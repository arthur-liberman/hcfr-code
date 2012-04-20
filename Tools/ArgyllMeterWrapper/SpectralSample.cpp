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

// SpectralSample.cpp: Wrapper for the argyll ccss type
//
//////////////////////////////////////////////////////////////////////

#include "ArgyllMeterWrapper.h"

#define SALONEINSTLIB
#define ENABLE_USB
#define ENABLE_SERIAL
#if defined(_MSC_VER)
#pragma warning(disable:4200)
#include <winsock.h>
#endif
#include "xspect.h"
#include "inst.h"
#include "hidio.h"
#include "conv.h"
#include "ccss.h"
#undef SALONEINSTLIB
#include <stdexcept>
#include "SpectralSampleFiles.h"
#include "SpectralSample.h" 


SpectralSample::SpectralSample(void) :
    m_ccss(NULL)
{
    if ((m_ccss = new_ccss()) == NULL)
    {
        throw std::logic_error("Error allocating data structure for spectral sample");
    }
}

SpectralSample::SpectralSample(const SpectralSample& s) :
    m_ccss(NULL)
{
    if ((m_ccss = new_ccss()) == NULL)
    {
        throw std::logic_error("Error allocating data structure for spectral sample");
    }

    m_Path = s.m_Path;
    m_Display = s.m_Display;
    m_Tech = s.m_Tech;
    m_Description = s.m_Description;

    if (s.m_ccss != NULL)
    {
        m_ccss->set_ccss(m_ccss, s.m_ccss->orig, s.m_ccss->crdate, s.m_ccss->desc, s.m_ccss->disp, s.m_ccss->tech, s.m_ccss->ref, s.m_ccss->samples, s.m_ccss->no_samp);
    }
}

SpectralSample::~SpectralSample(void)
{
    if (m_ccss != NULL)
    {
        m_ccss->del(m_ccss);
    }
}


bool SpectralSample::operator==(const SpectralSample& s) const
{
    if (m_Path == s.m_Path) 
    {
        return true;
    }
    return false;
}

SpectralSample& SpectralSample::operator=(const SpectralSample& s)
{
    if (this != &s)
    {
        m_Path = s.m_Path;
        m_Display = s.m_Display;
        m_Tech = s.m_Tech;
        m_Description = s.m_Description;
    
        if (s.m_ccss != NULL)
        {
            m_ccss->set_ccss(m_ccss, s.m_ccss->orig, s.m_ccss->crdate, s.m_ccss->desc, s.m_ccss->disp, s.m_ccss->tech, s.m_ccss->ref, s.m_ccss->samples, s.m_ccss->no_samp);
        }
    }
    return *this;
}


bool SpectralSample::Read(std::string samplePath)
{
    if (samplePath != "")
    {
        if (m_ccss->read_ccss(m_ccss, (char *)samplePath.c_str()))
        {                
                return false;
        }
    
        m_Path = samplePath;
        m_Display = m_ccss->desc;
        m_Tech = m_ccss->tech;
        setDescription(m_ccss->tech, m_ccss->disp); 
    }	
    return true;
}


bool SpectralSample::Verify(std::string samplePath)
{
    if (samplePath != "")
    {
        ccss *cs = NULL;
    
        if ((cs = new_ccss()) == NULL)
        {
            return false;
        }
  
        if (cs->read_ccss(cs, (char *)samplePath.c_str()))
        {                
            return false;
        }	
    
        setAll(cs->tech, cs->disp, (char *)samplePath.c_str());
        cs->del(cs);
    }	
    return true;
}



const char* SpectralSample::getDescription()
{
    return (const char*)m_Description.c_str();
}


const char* SpectralSample::getDisplay()
{
    return (const char*)m_Display.c_str();
}


const char* SpectralSample::getTech()
{
    return (const char*)m_Tech.c_str();
}


const char* SpectralSample::getPath()
{
    return (const char*)m_Path.c_str();
}


void SpectralSample::setDescription(char* tech, char *disp)
{
    // we use exactly the same convention as argyll
    // ccss files are not required to have both the display or the tech specified,
    // however they must have one specified. It's best to identify the file 
    // using a description field composed of the tech and the display. This is
    // how argyll does it in the output of their programs (see spotread.c for example)

    m_Description = tech;
    m_Description += " (";
    m_Description += disp;
    m_Description += ")";
}


void SpectralSample::setDisplay(char* disp)
{
    m_Display = disp;
}


void SpectralSample::setTech(char* tech)
{
    m_Tech = tech;
}


void SpectralSample::setPath(char* path)
{
    m_Path = path;
}


void SpectralSample::setAll(char* tech, char* disp, char* path )
{
    m_Display = disp;
    m_Tech = tech;
    m_Path = path;
    setDescription(tech, disp);
}

ccss* SpectralSample::getCCSS()
{
    return m_ccss;
}