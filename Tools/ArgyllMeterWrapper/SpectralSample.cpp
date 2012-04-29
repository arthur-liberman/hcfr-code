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

#include "SpectralSample.h"
#include "ArgyllMeterWrapper.h"
#include "SpectralSampleFiles.h"
#include "CGATSFile.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>

#define SALONEINSTLIB
#define ENABLE_USB
#define ENABLE_SERIAL
#include "xspect.h"
#include "ccss.h"
#undef SALONEINSTLIB


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


bool SpectralSample::Read(const std::string& samplePath)
{
    if (!samplePath.empty())
    {
        if (m_ccss->read_ccss(m_ccss, (char *)samplePath.c_str()))
        {
            return false;
        }
        m_Path = samplePath;
        m_Display = m_ccss->desc;
        m_Tech = m_ccss->tech;
        setDescription(m_ccss->tech, m_ccss->disp); 
        return true;
    }
    else
    {
        return false;
    }
}

bool SpectralSample::Create(const std::string& sampleCCSSPath, const std::string& sampleReadingsPath, const std::string& displayName, const std::string& displayTech, std::string& errorMessage)
{
	CGATSFile cgats;

	// Either displayname or displaytech should be non-null. There is no reason we should not enforce both

	if (displayName.empty() || displayTech.empty())
	{
		errorMessage = "Display name and tech must be supplied";
		return false;
	}

	if (cgats.Read(sampleReadingsPath))
	{
		if (cgats.getNumberOfTables() <= 0)
		{
			errorMessage = "Input file ";
			errorMessage += sampleReadingsPath;
			errorMessage += " must contain at least one table";
			return false;
		}
		if (cgats.getNumberOfDataSets(0) <= 0)
		{
			errorMessage = "Input file ";
			errorMessage += sampleReadingsPath;
			errorMessage += " must contain at least one data set";
			return false;
		}

		int indexInstrument, indexSpectralBands, indexSpectralStart, indexSpectralEnd;

		if (!cgats.FindKeyword("TARGET_INSTRUMENT", 0, indexInstrument))
		{			
			errorMessage = "Can't find keyword TARGET_INSTRUMENT in  ";
			errorMessage += sampleReadingsPath;
			return false;
		}
		if (!cgats.FindKeyword("SPECTRAL_BANDS", 0, indexSpectralBands))
		{			
			errorMessage = "Can't find keyword SPECTRAL_BANDS in  ";
			errorMessage += sampleReadingsPath;
			return false;
		}
		if (!cgats.FindKeyword("SPECTRAL_START_NM", 0, indexSpectralStart))
		{			
			errorMessage = "Can't find keyword SPECTRAL_START_NM in  ";
			errorMessage += sampleReadingsPath;
			return false;
		}
		if (!cgats.FindKeyword("SPECTRAL_END_NM", 0, indexSpectralEnd))
		{			
			errorMessage = "Can't find keyword SPECTRAL_END_NM in  ";
			errorMessage += sampleReadingsPath;
			return false;
		}
			
		xspect sp, *samples = NULL;
		samples = new xspect[cgats.getNumberOfDataSets(0)];

		std::string refInstrument = cgats.getStringKeywordValue(0, indexInstrument);

		sp.spec_n = cgats.getIntKeywordValue(0, indexSpectralBands);
		sp.spec_wl_short = cgats.getDoubleKeywordValue(0, indexSpectralStart);
		sp.spec_wl_long = cgats.getDoubleKeywordValue(0, indexSpectralEnd);
		sp.norm = 1.0;
		
		// Find the fields for spectral values 

		int  spi[XSPECT_MAX_BANDS];

		for (int j = 0; j < sp.spec_n; j++) 
		{
			int nm, fieldIndex;
	
			// Compute nearest integer wavelength 

			nm = (int)(sp.spec_wl_short + ((double)j/(sp.spec_n-1.0)) * (sp.spec_wl_long - sp.spec_wl_short) + 0.5);
			
			std::stringstream sFormatter (stringstream::in | stringstream::out);
			sFormatter << setw(3) << setfill('0') << nm;
			std::string sField = "SPEC_" + sFormatter.str();

			if (cgats.FindField(sField, 0, fieldIndex))
			{
				spi[j] = fieldIndex;
			}
			else
			{
				errorMessage = "Can't find field ";
				errorMessage += sField;
				errorMessage += " in input file ";
				errorMessage += sampleReadingsPath;
				delete [] samples;
				return false;
			}
		}

		// Transfer all the spectral values 

		for (int i = 0; i < cgats.getNumberOfDataSets(0); i++) 
		{

			XSPECT_COPY_INFO(&samples[i], &sp);
	
			for (int j = 0; j < sp.spec_n; j++) 
			{
				samples[i].spec[j] = cgats.getDoubleFieldValue(0, i, spi[j]);
			}
		}

		std::string displayDescription = displayTech + " (" + displayName + ")";

		// See what the highest value is
		
		double bigv = -1e60;
		
		for (int i = 0; i < cgats.getNumberOfDataSets(0); i++) // For all grid points 
		{	

			for (int j = 0; j < samples[i].spec_n; j++) 
			{
				if (samples[i].spec[j] > bigv)
				{
					bigv = samples[i].spec[j];
				}
			}
		}
			
		// Normalize the values 

		for (int i = 0; i < cgats.getNumberOfDataSets(0); i++) // For all grid points 
		{	
			double scale = 100.0;

			for (int j = 0; j < samples[i].spec_n; j++)
			{
				samples[i].spec[j] *= scale / bigv;
			}
		}		

		if (m_ccss->set_ccss(m_ccss, "HCFR ccss generator", NULL, 
									(char *)displayDescription.c_str(), 
									(char *)displayName.c_str(), 
									(char *)displayTech.c_str(), 
									(char *)refInstrument.c_str(), 
									samples, cgats.getNumberOfDataSets(0))) 
		{
			errorMessage = "set_ccss failed with '%s'";
			errorMessage += m_ccss->err;
		}
		else if(m_ccss->write_ccss(m_ccss, (char *)sampleCCSSPath.c_str()) == 0)
		{
			return true;
		}
		else
		{
			errorMessage = "Writing CCSS file failed";
			delete [] samples;
			return false;
		}

	}
	return false;
}

const char* SpectralSample::getDescription() const
{
    return m_Description.c_str();
}


const char* SpectralSample::getDisplay() const
{
    return m_Display.c_str();
}


const char* SpectralSample::getTech() const
{
    return m_Tech.c_str();
}


const char* SpectralSample::getPath() const
{
    return m_Path.c_str();
}


void SpectralSample::setDescription(const char* tech, const char *disp)
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


void SpectralSample::setDisplay(const char* disp)
{
    m_Display = disp;
}


void SpectralSample::setTech(const char* tech)
{
    m_Tech = tech;
}


void SpectralSample::setPath(const char* path)
{
    m_Path = path;
}


void SpectralSample::setAll(const char* tech, const char* disp, const char* path )
{
    m_Display = disp;
    m_Tech = tech;
    m_Path = path;
    setDescription(tech, disp);
}

ccss* SpectralSample::getCCSS() const
{
    return m_ccss;
}