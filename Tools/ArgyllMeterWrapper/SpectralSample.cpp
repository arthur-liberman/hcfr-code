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

#include "Color.h"
#include "SpectralSample.h"
#include "ArgyllMeterWrapper.h"
#include "SpectralSampleFiles.h"
#include "CGATSFile.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>

#define SALONEINSTLIB
#define ENABLE_USB
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
	m_RefInstrument = s.m_RefInstrument;

    if (s.m_ccss != NULL)
    {
        m_ccss->set_ccss(m_ccss, s.m_ccss->orig, s.m_ccss->crdate, s.m_ccss->desc, s.m_ccss->disp, s.m_ccss->tech, s.m_ccss->refrmode, s.m_ccss->sel, s.m_ccss->ref, s.m_ccss->samples, s.m_ccss->no_samp);
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
		m_RefInstrument = s.m_RefInstrument;
    
        if (s.m_ccss != NULL)
        {
            m_ccss->set_ccss(m_ccss, s.m_ccss->orig, s.m_ccss->crdate, s.m_ccss->desc, s.m_ccss->disp, s.m_ccss->tech, s.m_ccss->refrmode, s.m_ccss->sel, s.m_ccss->ref, s.m_ccss->samples, s.m_ccss->no_samp);
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
        m_Display = m_ccss->disp ? m_ccss->disp : "";
        m_Tech = m_ccss->tech ? m_ccss->tech : "";
		m_RefInstrument = m_ccss->ref ? m_ccss->ref : "";
        setDescription(m_Tech.c_str(), m_Display.c_str()); 
        return true;
    }
    else
    {
        return false;
    }
}


bool SpectralSample::Write(const std::string& samplePath)
{	
	if(m_ccss->write_ccss(m_ccss, (char *)samplePath.c_str()) == 0)
	{
		return true;
	}
	else
	{
		throw std::logic_error("Writing CCSS file failed");
		return false;
	}
}

bool SpectralSample::loadMeasurementsFromTI3(const std::string& ti3Path, CColor** readings, int& nReadings)
{
	CGATSFile cgats;

	if (cgats.Read(ti3Path))
	{
		if (cgats.getNumberOfTables() <= 0)
		{
			std::string errorMessage = "Input file ";
			errorMessage += ti3Path;
			errorMessage += " must contain at least one table";
			throw std::logic_error(errorMessage);
		}
		if (cgats.getNumberOfDataSets(0) <= 0)
		{
			std::string errorMessage = "Input file ";
			errorMessage += ti3Path;
			errorMessage += " must contain at least one data set";
			throw std::logic_error(errorMessage);
		}

		int indexInstrument, indexSpectralBands, indexSpectralStart, indexSpectralEnd;

		if (!cgats.FindKeyword("TARGET_INSTRUMENT", 0, indexInstrument))
		{			
			std::string errorMessage = "Can't find keyword TARGET_INSTRUMENT in  ";
			errorMessage += ti3Path;
			throw std::logic_error(errorMessage);
		}
		if (!cgats.FindKeyword("SPECTRAL_BANDS", 0, indexSpectralBands))
		{			
			std::string errorMessage = "Can't find keyword SPECTRAL_BANDS in  ";
			errorMessage += ti3Path;
			throw std::logic_error(errorMessage);
		}
		if (!cgats.FindKeyword("SPECTRAL_START_NM", 0, indexSpectralStart))
		{			
			std::string errorMessage = "Can't find keyword SPECTRAL_START_NM in  ";
			errorMessage += ti3Path;
			return false;
		}
		if (!cgats.FindKeyword("SPECTRAL_END_NM", 0, indexSpectralEnd))
		{			
			std::string errorMessage = "Can't find keyword SPECTRAL_END_NM in  ";
			errorMessage += ti3Path;
			throw std::logic_error(errorMessage);
		}
			
		xspect sp, *samples = NULL;
		samples = new xspect[cgats.getNumberOfDataSets(0)];

		m_RefInstrument = cgats.getStringKeywordValue(0, indexInstrument);

		// Get colours index
		
		int xIndex, yIndex, zIndex;
		if (!cgats.FindField("XYZ_X", 0, xIndex))
		{
			std::string errorMessage = "Can't find keyword XYZ_X in  ";
			errorMessage += ti3Path;
			throw std::logic_error(errorMessage);
		}
		if (!cgats.FindField("XYZ_Y", 0, yIndex))
		{
			std::string errorMessage = "Can't find keyword XYZ_Y in  ";
			errorMessage += ti3Path;
			throw std::logic_error(errorMessage);
		}
		if (!cgats.FindField("XYZ_Z", 0, zIndex))
		{
			std::string errorMessage = "Can't find keyword XYZ_Z in  ";
			errorMessage += ti3Path;
			throw std::logic_error(errorMessage);
		}
		
		sp.spec_n = cgats.getIntKeywordValue(0, indexSpectralBands);
		sp.spec_wl_short = cgats.getDoubleKeywordValue(0, indexSpectralStart);
		sp.spec_wl_long = cgats.getDoubleKeywordValue(0, indexSpectralEnd);
		sp.norm = 1.0;

		int shortWavelength((int)(sp.spec_wl_short + 0.5));
		int longWavelength((int)(sp.spec_wl_long + 0.5));
		int bandWidth((int)((sp.spec_wl_long - sp.spec_wl_short) / (double)sp.spec_n + 0.5));
		
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
				std::string errorMessage = "Can't find field ";
				errorMessage += sField;
				errorMessage += " in input file ";
				errorMessage += ti3Path;
				delete [] samples;
				throw std::logic_error(errorMessage);
			}
		}

		// Transfer all the spectral values 
		*readings = new CColor[cgats.getNumberOfDataSets(0)];

		for (int i = 0; i < cgats.getNumberOfDataSets(0); i++) 
		{
			XSPECT_COPY_INFO(&samples[i], &sp);
	
			for (int j = 0; j < sp.spec_n; j++) 
			{
				samples[i].spec[j] = cgats.getDoubleFieldValue(0, i, spi[j]);
			}
			(*readings)[i] = CColor(cgats.getDoubleFieldValue(0, i, xIndex), cgats.getDoubleFieldValue(0, i, yIndex), cgats.getDoubleFieldValue(0, i, zIndex));
			CSpectrum spectrum(sp.spec_n, shortWavelength, longWavelength, bandWidth, samples[i].spec);
			(*readings)[i].SetSpectrum(spectrum);
		}


		delete [] samples;
		nReadings = cgats.getNumberOfDataSets(0);

		return true;
	}
	else
	{
		return false;
	}
}

bool SpectralSample::createFromTI3(const std::string& ti3Path)
{
	CGATSFile cgats;

	// Either displayname or displaytech should be non-null. There is no reason we should not enforce both

	if (m_Display.empty() || m_Tech.empty())
	{
		throw std::logic_error("Display name and tech must be supplied");
	}
	setDescription(m_Tech.c_str(), m_Display.c_str()); 

	if (cgats.Read(ti3Path))
	{
		if (cgats.getNumberOfTables() <= 0)
		{
			std::string errorMessage = "Input file ";
			errorMessage += ti3Path;
			errorMessage += " must contain at least one table";
			throw std::logic_error(errorMessage);
		}
		if (cgats.getNumberOfDataSets(0) <= 0)
		{
			std::string errorMessage = "Input file ";
			errorMessage += ti3Path;
			errorMessage += " must contain at least one data set";
			throw std::logic_error(errorMessage);
		}

		int indexInstrument, indexSpectralBands, indexSpectralStart, indexSpectralEnd;

		if (!cgats.FindKeyword("TARGET_INSTRUMENT", 0, indexInstrument))
		{			
			std::string errorMessage = "Can't find keyword TARGET_INSTRUMENT in  ";
			errorMessage += ti3Path;
			throw std::logic_error(errorMessage);
		}
		if (!cgats.FindKeyword("SPECTRAL_BANDS", 0, indexSpectralBands))
		{			
			std::string errorMessage = "Can't find keyword SPECTRAL_BANDS in  ";
			errorMessage += ti3Path;
			throw std::logic_error(errorMessage);
		}
		if (!cgats.FindKeyword("SPECTRAL_START_NM", 0, indexSpectralStart))
		{			
			std::string errorMessage = "Can't find keyword SPECTRAL_START_NM in  ";
			errorMessage += ti3Path;
			return false;
		}
		if (!cgats.FindKeyword("SPECTRAL_END_NM", 0, indexSpectralEnd))
		{			
			std::string errorMessage = "Can't find keyword SPECTRAL_END_NM in  ";
			errorMessage += ti3Path;
			throw std::logic_error(errorMessage);
		}
			
		xspect sp, *samples = NULL;
		samples = new xspect[cgats.getNumberOfDataSets(0)];

		m_RefInstrument = cgats.getStringKeywordValue(0, indexInstrument);
		
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
				std::string errorMessage = "Can't find field ";
				errorMessage += sField;
				errorMessage += " in input file ";
				errorMessage += ti3Path;
				delete [] samples;
				throw std::logic_error(errorMessage);
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

		bool bRet = !m_ccss->set_ccss(m_ccss, "HCFR ccss generator", NULL, 	
									(char *)m_Description.c_str(), 
									(char *)m_Display.c_str(), 
									(char *)m_Tech.c_str(), 
                                    -1,
                                    NULL,
									(char *)m_RefInstrument.c_str(), 
									samples, cgats.getNumberOfDataSets(0));

		delete [] samples;

		if (!bRet) 
		{
			std::string errorMessage = "set_ccss failed with '%s'";
			errorMessage += m_ccss->err;
			throw std::logic_error(errorMessage);
		}
		return bRet;
	}
	return false;
}



bool SpectralSample::createFromMeasurements(const CColor spectralReadings[], const int nReadings)
{
	// Either displayname or displaytech should be non-null. There is no reason we should not enforce both

	if (m_Display.empty() || m_Tech.empty())
	{
		std::string errorMessage = "Display name and tech must be supplied";
		throw std::logic_error(errorMessage);
	}
	setDescription(m_Tech.c_str(), m_Display.c_str()); 

	// First check to see what the highest value is

	double bigv = -1e60;

	for (int nColors = 0; nColors < nReadings; nColors++)
	{
		CSpectrum spectrum = spectralReadings[nColors].GetSpectrum();

		// Check if we have spectral data

		if (spectrum.GetRows() <= 0)
		{
			std::string errorMessage = "Did not manage to read any spectral values!";
			throw std::logic_error(errorMessage);
		}
		for (int i = 0; i < spectrum.GetRows(); i++)
		{
			if (spectrum[i] > bigv)
			{
				bigv = spectrum[i];
			}
		}
	}
	
	// Now normalize the values
	
	xspect *samples = new xspect[nReadings];

	for (int nColors = 0; nColors < nReadings; nColors++)
	{
		CSpectrum spectrum = spectralReadings[nColors].GetSpectrum();
		double scale = 100;

		// We can safely ignore the bandwidth and scale members of the xspect struct
		// as they are unused. The ccss algorithm only uses wl_short and wl_long and
		// the spectral values. 

		samples[nColors].spec_n = spectrum.GetRows();
		samples[nColors].spec_wl_short = spectrum.m_WaveLengthMin;
		samples[nColors].spec_wl_long = spectrum.m_WaveLengthMax;

		for (int nSpec = 0; nSpec < samples[nColors].spec_n && nSpec < XSPECT_MAX_BANDS; nSpec++)
		{
			samples[nColors].spec[nSpec] = spectrum[nSpec];
			samples[nColors].spec[nSpec] *= scale / bigv;
		}
	}

	bool bRet = !m_ccss->set_ccss(m_ccss, "HCFR ccss generator", NULL, 	
									(char *)m_Description.c_str(), 
									(char *)m_Display.c_str(), 
									(char *)m_Tech.c_str(), 
                                    -1,
                                    NULL,
									(char *)m_RefInstrument.c_str(), 
									samples, nReadings);

	delete [] samples;

	if (!bRet) 
	{
		std::string errorMessage = "set_ccss failed with '%s'";
		errorMessage += m_ccss->err;
		throw std::logic_error(errorMessage);
	}

	return bRet;
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


const char* SpectralSample::getReferenceInstrument() const
{
    return m_RefInstrument.c_str();
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



void SpectralSample::setReferenceInstrument(const char* instr)
{
    m_RefInstrument = instr;
}


void SpectralSample::setAll(const char* tech, const char* disp, const char *instr, const char* path )
{
    m_Display = disp;
    m_Tech = tech;
    m_Path = path;
	m_RefInstrument = instr;
    setDescription(tech, disp);
}

ccss* SpectralSample::getCCSS() const
{
    return m_ccss;
}

