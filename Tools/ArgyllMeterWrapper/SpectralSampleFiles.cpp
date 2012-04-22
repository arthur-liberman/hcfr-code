/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2012 HCFR Project.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.htm. If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//    John Adcock
//    Ian C
/////////////////////////////////////////////////////////////////////////////

// SpectralSampleFiles.cpp vector/iterator for the argyll ccss files
//
//////////////////////////////////////////////////////////////////////

#include "SpectralSampleFiles.h"
#include "ArgyllMeterWrapper.h"
#include <stdexcept>

#define SALONEINSTLIB
#define ENABLE_USB
#define ENABLE_SERIAL
#include "xspect.h"
#include "ccss.h"
#undef SALONEINSTLIB



SpectralSampleFiles::SpectralSampleFiles(void)
{
    iccss *cl = NULL;
    int num = 0;

    cl = list_iccss(&num);
    if (cl == NULL)
    {
        throw std::logic_error("Error retrieving list of installed spectral sample files");
    }

    // I prefer to verify that the files are readable and parseable here. It makes for less
    // explicit error checking later on. We just silently ignore those that are not
    for (int i = 0; i < num; i++)
    {
        SpectralSample newSample;
        if(newSample.Read(cl[i].path))
        {
            m_Samples.push_back(newSample);
        }
    }
    free_iccss(cl);
}


SpectralSampleFiles::SpectralSampleFiles(const SpectralSampleFiles &s)
{
    m_Samples = s.m_Samples;
}

SpectralSampleFiles::~SpectralSampleFiles(void)
{
}


const SpectralSample& SpectralSampleFiles::getSample(std::string sampleDescription)
{
    std::vector<SpectralSample>::iterator iter;

    for (iter = m_Samples.begin(); iter != m_Samples.end(); iter++)
    {
        if (sampleDescription == iter->getDescription())
        {
            return *iter;
        }
    }
    throw std::logic_error("Spectral sample not found in list");
}

const SpectralSampleFiles::SpectralSamples& SpectralSampleFiles::getList() const
{
    return m_Samples;
}