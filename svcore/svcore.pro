
TEMPLATE = lib

exists(config.pri) {
    include(config.pri)
}
win* {
    !exists(config.pri) {
        DEFINES += HAVE_BZ2 HAVE_FFTW3 HAVE_FFTW3F HAVE_SNDFILE HAVE_SAMPLERATE HAVE_VAMP HAVE_VAMPHOSTSDK HAVE_RUBBERBAND HAVE_DATAQUAY HAVE_LIBLO HAVE_MAD HAVE_ID3TAG
    }
}

CONFIG += staticlib qt thread warn_on stl rtti exceptions
QT += network xml
QT -= gui

TARGET = svcore

DEPENDPATH += . data plugin plugin/api/alsa
INCLUDEPATH += . data plugin plugin/api/alsa ../dataquay
OBJECTS_DIR = o
MOC_DIR = o

win32-g++ {
    INCLUDEPATH += ../sv-dependency-builds/win32-mingw/include
}
win32-msvc* {
    INCLUDEPATH += ../sv-dependency-builds/win32-msvc/include
}

# Doesn't work with this library, which contains C99 as well as C++
PRECOMPILED_HEADER =

# Set up suitable platform defines for RtMidi
linux*:   DEFINES += __LINUX_ALSASEQ__
macx*:    DEFINES += __MACOSX_CORE__
win*:     DEFINES += __WINDOWS_MM__
solaris*: DEFINES += __RTMIDI_DUMMY_ONLY__

HEADERS += base/AudioLevel.h \
           base/AudioPlaySource.h \
           base/Clipboard.h \
           base/Command.h \
           base/Debug.h \
           base/Exceptions.h \
           base/LogRange.h \
           base/Pitch.h \
           base/Playable.h \
           base/PlayParameterRepository.h \
           base/PlayParameters.h \
           base/Preferences.h \
           base/Profiler.h \
           base/ProgressPrinter.h \
           base/ProgressReporter.h \
           base/PropertyContainer.h \
           base/RangeMapper.h \
           base/RealTime.h \
           base/RecentFiles.h \
           base/Resampler.h \
           base/ResizeableBitset.h \
           base/ResourceFinder.h \
           base/RingBuffer.h \
           base/Scavenger.h \
           base/Selection.h \
           base/Serialiser.h \
           base/StorageAdviser.h \
           base/StringBits.h \
           base/TempDirectory.h \
           base/TempWriteFile.h \
           base/TextMatcher.h \
           base/Thread.h \
           base/UnitDatabase.h \
           base/ViewManagerBase.h \
           base/Window.h \
           base/XmlExportable.h \
           base/ZoomConstraint.h
SOURCES += base/AudioLevel.cpp \
           base/Clipboard.cpp \
           base/Command.cpp \
           base/Debug.cpp \
           base/Exceptions.cpp \
           base/LogRange.cpp \
           base/Pitch.cpp \
           base/PlayParameterRepository.cpp \
           base/PlayParameters.cpp \
           base/Preferences.cpp \
           base/Profiler.cpp \
           base/ProgressPrinter.cpp \
           base/ProgressReporter.cpp \
           base/PropertyContainer.cpp \
           base/RangeMapper.cpp \
           base/RealTime.cpp \
           base/RecentFiles.cpp \
           base/Resampler.cpp \
           base/ResourceFinder.cpp \
           base/Selection.cpp \
           base/Serialiser.cpp \
           base/StorageAdviser.cpp \
           base/StringBits.cpp \
           base/TempDirectory.cpp \
           base/TempWriteFile.cpp \
           base/TextMatcher.cpp \
           base/Thread.cpp \
           base/UnitDatabase.cpp \
           base/ViewManagerBase.cpp \
           base/XmlExportable.cpp

HEADERS += data/fft/FFTapi.h \
           data/fft/FFTCacheReader.h \
           data/fft/FFTCacheStorageType.h \
           data/fft/FFTCacheWriter.h \
           data/fft/FFTDataServer.h \
           data/fft/FFTFileCacheReader.h \
           data/fft/FFTFileCacheWriter.h \
           data/fft/FFTMemoryCache.h \
           data/fileio/AudioFileReader.h \
           data/fileio/AudioFileReaderFactory.h \
           data/fileio/BZipFileDevice.h \
           data/fileio/CachedFile.h \
           data/fileio/CodedAudioFileReader.h \
           data/fileio/CSVFileReader.h \
           data/fileio/CSVFileWriter.h \
           data/fileio/CSVFormat.h \
           data/fileio/DataFileReader.h \
           data/fileio/DataFileReaderFactory.h \
           data/fileio/FileFinder.h \
           data/fileio/FileReadThread.h \
           data/fileio/FileSource.h \
           data/fileio/MatchFileReader.h \
           data/fileio/MatrixFile.h \
           data/fileio/MIDIFileReader.h \
           data/fileio/MIDIFileWriter.h \
           data/fileio/MP3FileReader.h \
           data/fileio/OggVorbisFileReader.h \
           data/fileio/PlaylistFileReader.h \
           data/fileio/QuickTimeFileReader.h \
           data/fileio/CoreAudioFileReader.h \
           data/fileio/ResamplingWavFileReader.h \
           data/fileio/WavFileReader.h \
           data/fileio/WavFileWriter.h \
           data/midi/MIDIEvent.h \
           data/midi/MIDIInput.h \
           data/midi/rtmidi/RtError.h \
           data/midi/rtmidi/RtMidi.h \
           data/model/AggregateWaveModel.h \
           data/model/AlignmentModel.h \
           data/model/Dense3DModelPeakCache.h \
           data/model/DenseThreeDimensionalModel.h \
           data/model/DenseTimeValueModel.h \
           data/model/EditableDenseThreeDimensionalModel.h \
           data/model/FFTModel.h \
           data/model/ImageModel.h \
           data/model/IntervalModel.h \
           data/model/Labeller.h \
           data/model/Model.h \
           data/model/ModelDataTableModel.h \
           data/model/NoteModel.h \
           data/model/PathModel.h \
           data/model/PowerOfSqrtTwoZoomConstraint.h \
           data/model/PowerOfTwoZoomConstraint.h \
           data/model/RangeSummarisableTimeValueModel.h \
           data/model/RegionModel.h \
           data/model/SparseModel.h \
           data/model/SparseOneDimensionalModel.h \
           data/model/SparseTimeValueModel.h \
           data/model/SparseValueModel.h \
           data/model/TabularModel.h \
           data/model/TextModel.h \
           data/model/WaveFileModel.h \
           data/model/WritableWaveFileModel.h \
           data/osc/OSCMessage.h \
           data/osc/OSCQueue.h 
SOURCES += data/fft/FFTapi.cpp \
           data/fft/FFTDataServer.cpp \
           data/fft/FFTFileCacheReader.cpp \
           data/fft/FFTFileCacheWriter.cpp \
           data/fft/FFTMemoryCache.cpp \
           data/fileio/AudioFileReader.cpp \
           data/fileio/AudioFileReaderFactory.cpp \
           data/fileio/BZipFileDevice.cpp \
           data/fileio/CachedFile.cpp \
           data/fileio/CodedAudioFileReader.cpp \
           data/fileio/CSVFileReader.cpp \
           data/fileio/CSVFileWriter.cpp \
           data/fileio/CSVFormat.cpp \
           data/fileio/DataFileReaderFactory.cpp \
           data/fileio/FileReadThread.cpp \
           data/fileio/FileSource.cpp \
           data/fileio/MatchFileReader.cpp \
           data/fileio/MatrixFile.cpp \
           data/fileio/MIDIFileReader.cpp \
           data/fileio/MIDIFileWriter.cpp \
           data/fileio/MP3FileReader.cpp \
           data/fileio/OggVorbisFileReader.cpp \
           data/fileio/PlaylistFileReader.cpp \
           data/fileio/QuickTimeFileReader.cpp \
           data/fileio/CoreAudioFileReader.cpp \
           data/fileio/ResamplingWavFileReader.cpp \
           data/fileio/WavFileReader.cpp \
           data/fileio/WavFileWriter.cpp \
           data/midi/MIDIInput.cpp \
           data/midi/rtmidi/RtMidi.cpp \
           data/model/AggregateWaveModel.cpp \
           data/model/AlignmentModel.cpp \
           data/model/Dense3DModelPeakCache.cpp \
           data/model/DenseTimeValueModel.cpp \
           data/model/EditableDenseThreeDimensionalModel.cpp \
           data/model/FFTModel.cpp \
           data/model/Model.cpp \
           data/model/ModelDataTableModel.cpp \
           data/model/PowerOfSqrtTwoZoomConstraint.cpp \
           data/model/PowerOfTwoZoomConstraint.cpp \
           data/model/RangeSummarisableTimeValueModel.cpp \
           data/model/WaveFileModel.cpp \
           data/model/WritableWaveFileModel.cpp \
           data/osc/OSCMessage.cpp \
           data/osc/OSCQueue.cpp 

HEADERS += plugin/DSSIPluginFactory.h \
           plugin/DSSIPluginInstance.h \
           plugin/FeatureExtractionPluginFactory.h \
           plugin/LADSPAPluginFactory.h \
           plugin/LADSPAPluginInstance.h \
           plugin/PluginIdentifier.h \
           plugin/PluginXml.h \
           plugin/RealTimePluginFactory.h \
           plugin/RealTimePluginInstance.h \
           plugin/api/dssi.h \
           plugin/api/ladspa.h \
           plugin/plugins/SamplePlayer.h \
           plugin/api/alsa/asoundef.h \
           plugin/api/alsa/asoundlib.h \
           plugin/api/alsa/seq.h \
           plugin/api/alsa/seq_event.h \
           plugin/api/alsa/seq_midi_event.h \
           plugin/api/alsa/sound/asequencer.h
SOURCES += plugin/DSSIPluginFactory.cpp \
           plugin/DSSIPluginInstance.cpp \
           plugin/FeatureExtractionPluginFactory.cpp \
           plugin/LADSPAPluginFactory.cpp \
           plugin/LADSPAPluginInstance.cpp \
           plugin/PluginIdentifier.cpp \
           plugin/PluginXml.cpp \
           plugin/RealTimePluginFactory.cpp \
           plugin/RealTimePluginInstance.cpp \
           plugin/api/dssi_alsa_compat.c \
           plugin/plugins/SamplePlayer.cpp

HEADERS += rdf/PluginRDFIndexer.h \
           rdf/PluginRDFDescription.h \
           rdf/RDFExporter.h \
           rdf/RDFFeatureWriter.h \
           rdf/RDFImporter.h \
           rdf/RDFTransformFactory.h
SOURCES += rdf/PluginRDFIndexer.cpp \
           rdf/PluginRDFDescription.cpp \
           rdf/RDFExporter.cpp \
           rdf/RDFFeatureWriter.cpp \
           rdf/RDFImporter.cpp \
           rdf/RDFTransformFactory.cpp

HEADERS += system/Init.h \
           system/System.h
SOURCES += system/Init.cpp \
           system/System.cpp

HEADERS += transform/CSVFeatureWriter.h \
           transform/FeatureExtractionModelTransformer.h \
           transform/FeatureWriter.h \
           transform/FileFeatureWriter.h \
           transform/RealTimeEffectModelTransformer.h \
           transform/Transform.h \
           transform/TransformDescription.h \
           transform/TransformFactory.h \
           transform/ModelTransformer.h \
           transform/ModelTransformerFactory.h
SOURCES += transform/CSVFeatureWriter.cpp \
           transform/FeatureExtractionModelTransformer.cpp \
           transform/FileFeatureWriter.cpp \
           transform/RealTimeEffectModelTransformer.cpp \
           transform/Transform.cpp \
           transform/TransformFactory.cpp \
           transform/ModelTransformer.cpp \
           transform/ModelTransformerFactory.cpp
