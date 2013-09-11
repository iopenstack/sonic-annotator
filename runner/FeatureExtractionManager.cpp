/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Annotator
    A utility for batch feature extraction from audio files.
    Mark Levy, Chris Sutton and Chris Cannam, Queen Mary, University of London.
    Copyright 2007-2008 QMUL.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "FeatureExtractionManager.h"

#include <vamp-hostsdk/PluginChannelAdapter.h>
#include <vamp-hostsdk/PluginBufferingAdapter.h>
#include <vamp-hostsdk/PluginInputDomainAdapter.h>
#include <vamp-hostsdk/PluginSummarisingAdapter.h>
#include <vamp-hostsdk/PluginWrapper.h>
#include <vamp-hostsdk/PluginLoader.h>

#include "base/Exceptions.h"

#include <iostream>

using namespace std;

using Vamp::Plugin;
using Vamp::PluginBase;
using Vamp::HostExt::PluginLoader;
using Vamp::HostExt::PluginChannelAdapter;
using Vamp::HostExt::PluginBufferingAdapter;
using Vamp::HostExt::PluginInputDomainAdapter;
using Vamp::HostExt::PluginSummarisingAdapter;
using Vamp::HostExt::PluginWrapper;

#include "data/fileio/FileSource.h"
#include "data/fileio/AudioFileReader.h"
#include "data/fileio/AudioFileReaderFactory.h"
#include "data/fileio/PlaylistFileReader.h"
#include "base/TempDirectory.h"
#include "base/ProgressPrinter.h"
#include "transform/TransformFactory.h"
#include "rdf/RDFTransformFactory.h"
#include "transform/FeatureWriter.h"

#include <QTextStream>
#include <QFile>
#include <QFileInfo>

FeatureExtractionManager::FeatureExtractionManager() :
    m_summariesOnly(false),
    // We can read using an arbitrary fixed block size --
    // PluginBufferingAdapter handles this for us.  It's likely to be
    // quicker to use larger sizes than smallish ones like 1024
    m_blockSize(16384),
    m_defaultSampleRate(0),
    m_sampleRate(0),
    m_channels(0)
{
}

FeatureExtractionManager::~FeatureExtractionManager()
{
    for (PluginMap::iterator pi = m_plugins.begin();
         pi != m_plugins.end(); ++pi) {
        delete pi->first;
    }
    foreach (AudioFileReader *r, m_readyReaders) {
        delete r;
    }
}

void FeatureExtractionManager::setChannels(int channels)
{
    m_channels = channels;
}

void FeatureExtractionManager::setDefaultSampleRate(int sampleRate)
{
    m_defaultSampleRate = sampleRate;
}

static PluginSummarisingAdapter::SummaryType
getSummaryType(string name)
{
    if (name == "min")      return PluginSummarisingAdapter::Minimum;
    if (name == "max")      return PluginSummarisingAdapter::Maximum;
    if (name == "mean")     return PluginSummarisingAdapter::Mean;
    if (name == "median")   return PluginSummarisingAdapter::Median;
    if (name == "mode")     return PluginSummarisingAdapter::Mode;
    if (name == "sum")      return PluginSummarisingAdapter::Sum;
    if (name == "variance") return PluginSummarisingAdapter::Variance;
    if (name == "sd")       return PluginSummarisingAdapter::StandardDeviation;
    if (name == "count")    return PluginSummarisingAdapter::Count;
    return PluginSummarisingAdapter::UnknownSummaryType;
}

bool FeatureExtractionManager::setSummaryTypes(const set<string> &names,
                                               bool summariesOnly,
                                               const PluginSummarisingAdapter::SegmentBoundaries &boundaries)
{
    for (SummaryNameSet::const_iterator i = names.begin();
         i != names.end(); ++i) {
        if (getSummaryType(*i) == PluginSummarisingAdapter::UnknownSummaryType) {
            cerr << "ERROR: Unknown summary type \"" << *i << "\"" << endl;
            return false;
        }
    }
    m_summaries = names;
    m_summariesOnly = summariesOnly;
    m_boundaries = boundaries;
    return true;
}

static PluginInputDomainAdapter::WindowType
convertWindowType(WindowType t)
{
    switch (t) {
    case RectangularWindow:
        return PluginInputDomainAdapter::RectangularWindow;
    case BartlettWindow:
        return PluginInputDomainAdapter::BartlettWindow;
    case HammingWindow:
        return PluginInputDomainAdapter::HammingWindow;
    case HanningWindow:
        return PluginInputDomainAdapter::HanningWindow;
    case BlackmanWindow:
        return PluginInputDomainAdapter::BlackmanWindow;
    case NuttallWindow:
        return PluginInputDomainAdapter::NuttallWindow;
    case BlackmanHarrisWindow:
        return PluginInputDomainAdapter::BlackmanHarrisWindow;
    default:
        cerr << "ERROR: Unknown or unsupported window type \"" << t << "\", using Hann (\"" << HanningWindow << "\")" << endl;
        return PluginInputDomainAdapter::HanningWindow;
    }
}

bool FeatureExtractionManager::addFeatureExtractor
(Transform transform, const vector<FeatureWriter*> &writers)
{
    //!!! exceptions rather than return values?

    if (transform.getSampleRate() == 0) {
        if (m_sampleRate == 0) {
            cerr << "NOTE: Transform does not specify a sample rate, using default rate of " << m_defaultSampleRate << endl;
            transform.setSampleRate(m_defaultSampleRate);
            m_sampleRate = m_defaultSampleRate;
        } else {
            cerr << "NOTE: Transform does not specify a sample rate, using previous transform's rate of " << m_sampleRate << endl;
            transform.setSampleRate(m_sampleRate);
        }
    }

    if (m_sampleRate == 0) {
        m_sampleRate = transform.getSampleRate();
    }

    if (transform.getSampleRate() != m_sampleRate) {
        cerr << "WARNING: Transform sample rate " << transform.getSampleRate() << " does not match previously specified transform rate of " << m_sampleRate << " -- only a single rate is supported for each run" << endl;
        cerr << "WARNING: Using previous rate of " << m_sampleRate << " for this transform as well" << endl;
        transform.setSampleRate(m_sampleRate);
    }

    Plugin *plugin = 0;

    // Remember what the original transform looked like, and index
    // based on this -- because we may be about to fill in the zeros
    // for step and block size, but we want any further copies with
    // the same zeros to match this one
    Transform originalTransform = transform;
    
    if (m_transformPluginMap.find(transform) == m_transformPluginMap.end()) {

        // Test whether we already have a transform that is identical
        // to this, except for the output requested and/or the summary
        // type -- if so, they should share plugin instances (a vital
        // optimisation)

        for (TransformPluginMap::iterator i = m_transformPluginMap.begin();
             i != m_transformPluginMap.end(); ++i) {
            Transform test = i->first;
            test.setOutput(transform.getOutput());
            test.setSummaryType(transform.getSummaryType());
            if (transform == test) {
                cerr << "NOTE: Already have transform identical to this one (for \""
                     << transform.getIdentifier().toStdString()
                     << "\") in every detail except output identifier and/or "
                     << "summary type; sharing its plugin instance" << endl;
                plugin = i->second;
                if (transform.getSummaryType() != Transform::NoSummary &&
                    !dynamic_cast<PluginSummarisingAdapter *>(plugin)) {
                    plugin = new PluginSummarisingAdapter(plugin);
                    i->second = plugin;
                }
                break;
            }
        }

        if (!plugin) {

            TransformFactory *tf = TransformFactory::getInstance();

            PluginBase *pb = tf->instantiatePluginFor(transform);
            plugin = tf->downcastVampPlugin(pb);
            if (!plugin) {
                //!!! todo: handle non-Vamp plugins too, or make the main --list
                // option print out only Vamp transforms
                cerr << "ERROR: Failed to load plugin for transform \""
                     << transform.getIdentifier().toStdString() << "\"" << endl;
                delete pb;
                return false;
            }
            
            // We will provide the plugin with arbitrary step and
            // block sizes (so that we can use the same read/write
            // block size for all transforms), and to that end we use
            // a PluginBufferingAdapter.  However, we need to know the
            // underlying step size so that we can provide the right
            // context for dense outputs.  (Although, don't forget
            // that the PluginBufferingAdapter rewrites
            // OneSamplePerStep outputs so as to use FixedSampleRate
            // -- so it supplies the sample rate in the output
            // feature.  I'm not sure whether we can easily use that.)

            size_t pluginStepSize = plugin->getPreferredStepSize();
            size_t pluginBlockSize = plugin->getPreferredBlockSize();

            PluginInputDomainAdapter *pida = 0;

            // adapt the plugin for buffering, channels, etc.
            if (plugin->getInputDomain() == Plugin::FrequencyDomain) {

                pida = new PluginInputDomainAdapter(plugin);
                pida->setProcessTimestampMethod(PluginInputDomainAdapter::ShiftData);

                PluginInputDomainAdapter::WindowType wtype =
                    convertWindowType(transform.getWindowType());
                pida->setWindowType(wtype);
                plugin = pida;
            }

            PluginBufferingAdapter *pba = new PluginBufferingAdapter(plugin);
            plugin = pba;

            if (transform.getStepSize() != 0) {
                pba->setPluginStepSize(transform.getStepSize());
            } else {
                transform.setStepSize(pluginStepSize);
            }

            if (transform.getBlockSize() != 0) {
                pba->setPluginBlockSize(transform.getBlockSize());
            } else {
                transform.setBlockSize(pluginBlockSize);
            }

            plugin = new PluginChannelAdapter(plugin);

            if (!m_summaries.empty() ||
                transform.getSummaryType() != Transform::NoSummary) {
                PluginSummarisingAdapter *adapter =
                    new PluginSummarisingAdapter(plugin);
                adapter->setSummarySegmentBoundaries(m_boundaries);
                plugin = adapter;
            }

            if (!plugin->initialise(m_channels, m_blockSize, m_blockSize)) {
                cerr << "ERROR: Plugin initialise (channels = " << m_channels << ", stepSize = " << m_blockSize << ", blockSize = " << m_blockSize << ") failed." << endl;    
                delete plugin;
                return false;
            }

//            cerr << "Initialised plugin" << endl;

            size_t actualStepSize = 0;
            size_t actualBlockSize = 0;
            pba->getActualStepAndBlockSizes(actualStepSize, actualBlockSize);
            transform.setStepSize(actualStepSize);
            transform.setBlockSize(actualBlockSize);

            Plugin::OutputList outputs = plugin->getOutputDescriptors();
            for (int i = 0; i < (int)outputs.size(); ++i) {

//                cerr << "Newly initialised plugin output " << i << " has bin count " << outputs[i].binCount << endl;

                m_pluginOutputs[plugin][outputs[i].identifier] = outputs[i];
                m_pluginOutputIndices[outputs[i].identifier] = i;
            }

            cerr << "NOTE: Loaded and initialised plugin for transform \""
                 << transform.getIdentifier().toStdString()
                 << "\" with plugin step size " << actualStepSize
                 << " and block size " << actualBlockSize
                 << " (adapter step and block size " << m_blockSize << ")"
                 << endl;

            if (pida) {
                cerr << "NOTE: PluginInputDomainAdapter timestamp adjustment is "

                     << pida->getTimestampAdjustment() << endl;
            }

        } else {

            if (transform.getStepSize() == 0 || transform.getBlockSize() == 0) {

                PluginWrapper *pw = dynamic_cast<PluginWrapper *>(plugin);
                if (pw) {
                    PluginBufferingAdapter *pba =
                        pw->getWrapper<PluginBufferingAdapter>();
                    if (pba) {
                        size_t actualStepSize = 0;
                        size_t actualBlockSize = 0;
                        pba->getActualStepAndBlockSizes(actualStepSize,
                                                        actualBlockSize);
                        if (transform.getStepSize() == 0) {
                            transform.setStepSize(actualStepSize);
                        }
                        if (transform.getBlockSize() == 0) {
                            transform.setBlockSize(actualBlockSize);
                        }
                    }
                }
            }
        }

        if (transform.getOutput() == "") {
            transform.setOutput
                (plugin->getOutputDescriptors()[0].identifier.c_str());
        }

        m_transformPluginMap[transform] = plugin;

        if (!(originalTransform == transform)) {
            m_transformPluginMap[originalTransform] = plugin;
        }

    } else {
        
        plugin = m_transformPluginMap[transform];
    }

    m_plugins[plugin][transform] = writers;

    return true;
}

bool FeatureExtractionManager::addDefaultFeatureExtractor
(TransformId transformId, const vector<FeatureWriter*> &writers)
{
    TransformFactory *tf = TransformFactory::getInstance();

    if (m_sampleRate == 0) {
        if (m_defaultSampleRate == 0) {
            cerr << "ERROR: Default transform requested, but no default sample rate available" << endl;
            return false;
        } else {
            cerr << "NOTE: Using default sample rate of " << m_defaultSampleRate << " for default transform" << endl;
            m_sampleRate = m_defaultSampleRate;
        }
    }

    Transform transform = tf->getDefaultTransformFor(transformId, m_sampleRate);

    return addFeatureExtractor(transform, writers);
}

bool FeatureExtractionManager::addFeatureExtractorFromFile
(QString transformXmlFile, const vector<FeatureWriter*> &writers)
{
    RDFTransformFactory factory
        (QUrl::fromLocalFile(QFileInfo(transformXmlFile).absoluteFilePath())
         .toString());
    ProgressPrinter printer("Parsing transforms RDF file");
    std::vector<Transform> transforms = factory.getTransforms(&printer);
    if (!factory.isOK()) {
        cerr << "WARNING: FeatureExtractionManager::addFeatureExtractorFromFile: Failed to parse transforms file: " << factory.getErrorString().toStdString() << endl;
        if (factory.isRDF()) {
            return false; // no point trying it as XML
        }
    }
    if (!transforms.empty()) {
        bool success = true;
        for (int i = 0; i < (int)transforms.size(); ++i) {
            if (!addFeatureExtractor(transforms[i], writers)) {
                success = false;
            }
        }
        return success;
    }

    QFile file(transformXmlFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        cerr << "ERROR: Failed to open transform XML file \""
             << transformXmlFile.toStdString() << "\" for reading" << endl;
        return false;
    }

    QTextStream *qts = new QTextStream(&file);
    QString qs = qts->readAll();
    delete qts;
    file.close();

    Transform transform(qs);

    return addFeatureExtractor(transform, writers);
}

void FeatureExtractionManager::addSource(QString audioSource)
{
    if (QFileInfo(audioSource).suffix().toLower() == "m3u") {
        ProgressPrinter retrievalProgress("Opening playlist file...");
        FileSource source(audioSource, &retrievalProgress);
        if (!source.isAvailable()) {
            cerr << "ERROR: File or URL \"" << audioSource.toStdString()
                 << "\" could not be located" << endl;
            throw FileNotFound(audioSource);
        }
        source.waitForData();
        PlaylistFileReader reader(source);
        if (reader.isOK()) {
            vector<QString> files = reader.load();
            for (int i = 0; i < (int)files.size(); ++i) {
                addSource(files[i]);
            }
            return;
        } else {
            cerr << "ERROR: Playlist \"" << audioSource.toStdString()
                 << "\" could not be opened" << endl;
            throw FileNotFound(audioSource);
        }
    }

    std::cerr << "Have audio source: \"" << audioSource.toStdString() << "\"" << std::endl;

    // We don't actually do anything with it here, unless it's the
    // first audio source and we need it to establish default channel
    // count and sample rate

    if (m_channels == 0 || m_defaultSampleRate == 0) {

        ProgressPrinter retrievalProgress("Determining default rate and channel count from first input file...");

        FileSource source(audioSource, &retrievalProgress);
        if (!source.isAvailable()) {
            cerr << "ERROR: File or URL \"" << audioSource.toStdString()
                 << "\" could not be located" << endl;
            throw FileNotFound(audioSource);
        }
    
        source.waitForData();

        // Open to determine validity, channel count, sample rate only
        // (then close, and open again later with actual desired rate &c)

        AudioFileReader *reader =
            AudioFileReaderFactory::createReader(source, 0, &retrievalProgress);
    
        if (!reader) {
            throw FailedToOpenFile(audioSource);
        }

        retrievalProgress.done();

        cerr << "File or URL \"" << audioSource.toStdString() << "\" opened successfully" << endl;

        if (m_channels == 0) {
            m_channels = reader->getChannelCount();
            cerr << "Taking default channel count of "
                 << reader->getChannelCount() << " from file" << endl;
        }

        if (m_defaultSampleRate == 0) {
            m_defaultSampleRate = reader->getNativeRate();
            cerr << "Taking default sample rate of "
                 << reader->getNativeRate() << "Hz from file" << endl;
            cerr << "(Note: Default may be overridden by transforms)" << endl;
        }

        m_readyReaders[audioSource] = reader;
    }
}

void FeatureExtractionManager::extractFeatures(QString audioSource, bool force)
{
    if (m_plugins.empty()) return;

    if (QFileInfo(audioSource).suffix().toLower() == "m3u") {
        FileSource source(audioSource);
        PlaylistFileReader reader(source);
        if (reader.isOK()) {
            vector<QString> files = reader.load();
            for (int i = 0; i < (int)files.size(); ++i) {
                try {
                    extractFeatures(files[i], force);
                } catch (const std::exception &e) {
                    if (!force) throw;
                    cerr << "ERROR: Feature extraction failed for playlist entry \""
                         << files[i].toStdString()
                         << "\": " << e.what() << endl;
                    // print a note only if we have more files to process
                    if (++i != files.size()) {
                        cerr << "NOTE: \"--force\" option was provided, continuing (more errors may occur)" << endl;
                    }
                }
            }
            return;
        } else {
            cerr << "ERROR: Playlist \"" << audioSource.toStdString()
                 << "\" could not be opened" << endl;
            throw FileNotFound(audioSource);
        }
    }

    testOutputFiles(audioSource);

    if (m_sampleRate == 0) {
        throw FileOperationFailed
            (audioSource, "internal error: have sources and plugins, but no sample rate");
    }
    if (m_channels == 0) {
        throw FileOperationFailed
            (audioSource, "internal error: have sources and plugins, but no channel count");
    }

    AudioFileReader *reader = 0;

    if (m_readyReaders.contains(audioSource)) {
        reader = m_readyReaders[audioSource];
        m_readyReaders.remove(audioSource);
        if (reader->getChannelCount() != m_channels ||
            reader->getSampleRate() != m_sampleRate) {
            // can't use this; open it again
            delete reader;
            reader = 0;
        }
    }
    if (!reader) {
        ProgressPrinter retrievalProgress("Retrieving audio data...");
        FileSource source(audioSource, &retrievalProgress);
        source.waitForData();
        reader = AudioFileReaderFactory::createReader
            (source, m_sampleRate, &retrievalProgress);
        retrievalProgress.done();
    }

    if (!reader) {
        throw FailedToOpenFile(audioSource);
    }

    cerr << "Audio file \"" << audioSource.toStdString() << "\": "
         << reader->getChannelCount() << "ch at " 
         << reader->getNativeRate() << "Hz" << endl;
    if (reader->getChannelCount() != m_channels ||
        reader->getNativeRate() != m_sampleRate) {
        cerr << "NOTE: File will be mixed or resampled for processing: "
             << m_channels << "ch at " 
             << m_sampleRate << "Hz" << endl;
    }

    // allocate audio buffers
    float **data = new float *[m_channels];
    for (int c = 0; c < m_channels; ++c) {
        data[c] = new float[m_blockSize];
    }
    
    struct LifespanMgr { // unintrusive hack introduced to ensure
                         // destruction on exceptions
        AudioFileReader *m_r;
        int m_c;
        float **m_d;
        LifespanMgr(AudioFileReader *r, int c, float **d) :
            m_r(r), m_c(c), m_d(d) { }
        ~LifespanMgr() { destroy(); }
        void destroy() {
            if (!m_r) return;
            delete m_r;
            for (int i = 0; i < m_c; ++i) delete[] m_d[i];
            delete[] m_d;
            m_r = 0;
        }
    };
    LifespanMgr lifemgr(reader, m_channels, data);

    size_t frameCount = reader->getFrameCount();
    
//    cerr << "file has " << frameCount << " frames" << endl;

    for (PluginMap::iterator pi = m_plugins.begin();
         pi != m_plugins.end(); ++pi) {

        Plugin *plugin = pi->first;

//        std::cerr << "Calling reset on " << plugin << std::endl;
        plugin->reset();

        for (TransformWriterMap::iterator ti = pi->second.begin();
             ti != pi->second.end(); ++ti) {

            const Transform &transform = ti->first;

            //!!! we may want to set the start and duration times for extraction
            // in the transform record (defaults of zero indicate extraction
            // from the whole file)
//            transform.setStartTime(RealTime::zeroTime);
//            transform.setDuration
//                (RealTime::frame2RealTime(reader->getFrameCount(), m_sampleRate));

            string outputId = transform.getOutput().toStdString();
            if (m_pluginOutputs[plugin].find(outputId) ==
                m_pluginOutputs[plugin].end()) {
                //!!! throw?
                cerr << "WARNING: Nonexistent plugin output \"" << outputId << "\" requested for transform \""
                     << transform.getIdentifier().toStdString() << "\", ignoring this transform"
                     << endl;
/*
                cerr << "Known outputs for all plugins are as follows:" << endl;
                for (PluginOutputMap::const_iterator k = m_pluginOutputs.begin();
                     k != m_pluginOutputs.end(); ++k) {
                    cerr << "Plugin " << k->first << ": ";
                    if (k->second.empty()) {
                        cerr << "(none)";
                    }
                    for (OutputMap::const_iterator i = k->second.begin();
                         i != k->second.end(); ++i) {
                        cerr << "\"" << i->first << "\" ";
                    }
                    cerr << endl;
                }
*/
            }
        }
    }
    
    long startFrame = 0;
    long endFrame = frameCount;

/*!!! No -- there is no single transform to pull this stuff from --
 * the transforms may have various start and end times, need to be far
 * cleverer about this if we're going to support them

    RealTime trStartRT = transform.getStartTime();
    RealTime trDurationRT = transform.getDuration();

    long trStart = RealTime::realTime2Frame(trStartRT, m_sampleRate);
    long trDuration = RealTime::realTime2Frame(trDurationRT, m_sampleRate);

    if (trStart == 0 || trStart < startFrame) {
        trStart = startFrame;
    }

    if (trDuration == 0) {
        trDuration = endFrame - trStart;
    }
    if (trStart + trDuration > endFrame) {
        trDuration = endFrame - trStart;
    }

    startFrame = trStart;
    endFrame = trStart + trDuration;
*/
    
    for (PluginMap::iterator pi = m_plugins.begin();
         pi != m_plugins.end(); ++pi) { 

        for (TransformWriterMap::const_iterator ti = pi->second.begin();
             ti != pi->second.end(); ++ti) {
        
            const vector<FeatureWriter *> &writers = ti->second;
            
            for (int j = 0; j < (int)writers.size(); ++j) {
                FeatureWriter::TrackMetadata m;
                m.title = reader->getTitle();
                m.maker = reader->getMaker();
                if (m.title != "" && m.maker != "") {
                    writers[j]->setTrackMetadata(audioSource, m);
                }
            }
        }
    }

    ProgressPrinter extractionProgress("Extracting and writing features...");
    int progress = 0;

    for (long i = startFrame; i < endFrame; i += m_blockSize) {
        
        //!!! inefficient, although much of the inefficiency may be
        // susceptible to optimisation
        
        SampleBlock frames;
        reader->getInterleavedFrames(i, m_blockSize, frames);
        
        // We have to do our own channel handling here; we can't just
        // leave it to the plugin adapter because the same plugin
        // adapter may have to serve for input files with various
        // numbers of channels (so the adapter is simply configured
        // with a fixed channel count).

        int rc = reader->getChannelCount();

        // m_channels is the number of channels we need for the plugin

        int index;
        int fc = (int)frames.size();

        if (m_channels == 1) { // only case in which we can sensibly mix down
            for (int j = 0; j < m_blockSize; ++j) {
                data[0][j] = 0.f;
            }
            for (int c = 0; c < rc; ++c) {
                for (int j = 0; j < m_blockSize; ++j) {
                    index = j * rc + c;
                    if (index < fc) data[0][j] += frames[index];
                }
            }
            for (int j = 0; j < m_blockSize; ++j) {
                data[0][j] /= rc;
            }
        } else {                
            for (int c = 0; c < m_channels; ++c) {
                for (int j = 0; j < m_blockSize; ++j) {
                    data[c][j] = 0.f;
                }
                if (c < rc) {
                    for (int j = 0; j < m_blockSize; ++j) {
                        index = j * rc + c;
                        if (index < fc) data[c][j] += frames[index];
                    }
                }
            }
        }                

        Vamp::RealTime timestamp = Vamp::RealTime::frame2RealTime
            (i, m_sampleRate);
        
        for (PluginMap::iterator pi = m_plugins.begin();
             pi != m_plugins.end(); ++pi) {

            Plugin *plugin = pi->first;
            Plugin::FeatureSet featureSet = plugin->process(data, timestamp);

            if (!m_summariesOnly) {
                writeFeatures(audioSource, plugin, featureSet);
            }
        }

        int pp = progress;
        progress = int(((i - startFrame) * 100.0) / (endFrame - startFrame) + 0.1);
        if (progress > pp) extractionProgress.setProgress(progress);
    }

//    std::cerr << "FeatureExtractionManager: deleting audio file reader" << std::endl;

    lifemgr.destroy(); // deletes reader, data
        
    // In order to ensure our results are written to the output in a
    // fixed order (and not one that depends on the pointer value of
    // each plugin on the heap in any given run of the program) we
    // take the plugins' entries from the plugin map and sort them
    // into a new, temporary map that is indexed by the first
    // transform for each plugin. We then iterate over than instead of
    // over m_plugins in order to get the right ordering.

    // This is not the most elegant way to do this -- it would be more
    // elegant to impose an ordering directly on the plugins that are
    // used as keys to m_plugins. But the plugin type comes from the
    // Vamp SDK, so this change is more localised.

    // Thanks to Matthias for this.

    typedef map<Transform, PluginMap::value_type> OrderedPluginMap;
    OrderedPluginMap orderedPlugins;

    for (PluginMap::iterator pi = m_plugins.begin();
         pi != m_plugins.end(); ++pi) { 
        Transform firstForPlugin = (pi->second).begin()->first;
        orderedPlugins.insert(OrderedPluginMap::value_type(firstForPlugin, *pi));
    }

    for (OrderedPluginMap::iterator superPi = orderedPlugins.begin();
         superPi != orderedPlugins.end(); ++superPi) {

        // The value we extract from this map is just the same as the
        // value_type we get from iterating over our PluginMap
        // directly -- but we happen to get them in the right order
        // now because the map iterator is ordered by the Transform
        // key type ordering
        PluginMap::value_type pi = superPi->second;

        Plugin *plugin = pi.first;
        Plugin::FeatureSet featureSet = plugin->getRemainingFeatures();

        if (!m_summariesOnly) {
            writeFeatures(audioSource, plugin, featureSet);
        }

        if (!m_summaries.empty()) {
            PluginSummarisingAdapter *adapter =
                dynamic_cast<PluginSummarisingAdapter *>(plugin);
            if (!adapter) {
                cerr << "WARNING: Summaries requested, but plugin is not a summarising adapter" << endl;
            } else {
                for (SummaryNameSet::const_iterator sni = m_summaries.begin();
                     sni != m_summaries.end(); ++sni) {
                    featureSet.clear();
                    //!!! problem here -- we are requesting summaries
                    //!!! for all outputs, but they in principle have
                    //!!! different averaging requirements depending
                    //!!! on whether their features have duration or
                    //!!! not
                    featureSet = adapter->getSummaryForAllOutputs
                        (getSummaryType(*sni),
                         PluginSummarisingAdapter::ContinuousTimeAverage);
                    writeFeatures(audioSource, plugin, featureSet,//!!! *sni);
                                  Transform::stringToSummaryType(sni->c_str()));
                }
            }
        }

        writeSummaries(audioSource, plugin);
    }

    extractionProgress.done();

    finish();
    
    TempDirectory::getInstance()->cleanup();
}

void
FeatureExtractionManager::writeSummaries(QString audioSource, Plugin *plugin)
{
    // caller should have ensured plugin is in m_plugins
    PluginMap::iterator pi = m_plugins.find(plugin);

    for (TransformWriterMap::const_iterator ti = pi->second.begin();
         ti != pi->second.end(); ++ti) {
        
        const Transform &transform = ti->first;
        const vector<FeatureWriter *> &writers = ti->second;

        Transform::SummaryType summaryType = transform.getSummaryType();
        PluginSummarisingAdapter::SummaryType pType =
            (PluginSummarisingAdapter::SummaryType)summaryType;

        if (transform.getSummaryType() == Transform::NoSummary) {
            continue;
        }

        PluginSummarisingAdapter *adapter =
            dynamic_cast<PluginSummarisingAdapter *>(plugin);
        if (!adapter) {
            cerr << "FeatureExtractionManager::writeSummaries: INTERNAL ERROR: Summary requested for transform, but plugin is not a summarising adapter" << endl;
            continue;
        }

        Plugin::FeatureSet featureSet = adapter->getSummaryForAllOutputs
            (pType, PluginSummarisingAdapter::ContinuousTimeAverage);

//        cout << "summary type " << int(pType) << " for transform:" << endl << transform.toXmlString().toStdString()<< endl << "... feature set with " << featureSet.size() << " elts" << endl;

        writeFeatures(audioSource, plugin, featureSet, summaryType);
    }
}

void FeatureExtractionManager::writeFeatures(QString audioSource,
                                             Plugin *plugin,
                                             const Plugin::FeatureSet &features,
                                             Transform::SummaryType summaryType)
{
    // caller should have ensured plugin is in m_plugins
    PluginMap::iterator pi = m_plugins.find(plugin);

    for (TransformWriterMap::const_iterator ti = pi->second.begin();
         ti != pi->second.end(); ++ti) {
        
        const Transform &transform = ti->first;
        const vector<FeatureWriter *> &writers = ti->second;
        
        if (transform.getSummaryType() != Transform::NoSummary &&
            m_summaries.empty() &&
            summaryType == Transform::NoSummary) {
            continue;
        }

        if (transform.getSummaryType() != Transform::NoSummary &&
            summaryType != Transform::NoSummary &&
            transform.getSummaryType() != summaryType) {
            continue;
        }

        string outputId = transform.getOutput().toStdString();

        if (m_pluginOutputs[plugin].find(outputId) ==
            m_pluginOutputs[plugin].end()) {
            continue;
        }
        
        const Plugin::OutputDescriptor &desc =
            m_pluginOutputs[plugin][outputId];
        
        int outputIndex = m_pluginOutputIndices[outputId];
        Plugin::FeatureSet::const_iterator fsi = features.find(outputIndex);
        if (fsi == features.end()) continue;

        for (int j = 0; j < (int)writers.size(); ++j) {
            writers[j]->write
                (audioSource, transform, desc, fsi->second,
                 Transform::summaryTypeToString(summaryType).toStdString());
        }
    }
}

void FeatureExtractionManager::testOutputFiles(QString audioSource)
{
    for (PluginMap::iterator pi = m_plugins.begin();
         pi != m_plugins.end(); ++pi) {

        for (TransformWriterMap::iterator ti = pi->second.begin();
             ti != pi->second.end(); ++ti) {
        
            vector<FeatureWriter *> &writers = ti->second;

            for (int i = 0; i < (int)writers.size(); ++i) {
                writers[i]->testOutputFile(audioSource, ti->first.getIdentifier());
            }
        }
    }
}

void FeatureExtractionManager::finish()
{
    for (PluginMap::iterator pi = m_plugins.begin();
         pi != m_plugins.end(); ++pi) {

        for (TransformWriterMap::iterator ti = pi->second.begin();
             ti != pi->second.end(); ++ti) {
        
            vector<FeatureWriter *> &writers = ti->second;

            for (int i = 0; i < (int)writers.size(); ++i) {
                writers[i]->flush();
                writers[i]->finish();
            }
        }
    }
}

void FeatureExtractionManager::print(Transform transform) const
{
    QString qs;
    QTextStream qts(&qs);
    transform.toXml(qts);
    cerr << qs.toStdString() << endl;
}
