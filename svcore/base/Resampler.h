/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

/*
   This is a modified version of a source file from the 
   Rubber Band audio timestretcher library.
   This file copyright 2007 Chris Cannam.
*/

#ifndef _RESAMPLER_H_
#define _RESAMPLER_H_

#include <sys/types.h>

class Resampler
{
public:
    enum Quality { Best, FastestTolerable, Fastest };

    Resampler(Quality quality, size_t channels, size_t chunkSize = 0);
    ~Resampler();

    size_t resample(float **in, float **out,
                    size_t incount, float ratio,
                    bool final = false);

    size_t resampleInterleaved(float *in, float *out,
                               size_t incount, float ratio,
                               bool final = false);

    void reset();

protected:
    class D;
    D *m_d;
};

#endif
