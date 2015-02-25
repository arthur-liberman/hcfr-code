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
//      John Adcock
//      Ian C 
/////////////////////////////////////////////////////////////////////////////

// SpectralSample.h: Wrapper for the argyll ccss type
//
//////////////////////////////////////////////////////////////////////

#if !defined(SPECTRALSAMPLE_HEADER_INCLUDED)
#define SPECTRALSAMPLE_HEADER_INCLUDED

#include <string>
#include <vector>
#include <stdarg.h>

struct _ccss;
typedef _ccss ccss;
class CColor;

class SpectralSample
{
public:
    SpectralSample(void);
    SpectralSample(const SpectralSample& s);
    ~SpectralSample(void);

    bool operator==(const SpectralSample& s) const;
    SpectralSample& operator=(const SpectralSample& s);

    bool Read(const std::string& samplePath);
	bool Write(const std::string& samplePath);
	bool loadMeasurementsFromTI3(const std::string& ti3Path, CColor** readings, int& nReadings);
	bool createFromTI3(const std::string& ti3Path);
	bool createFromMeasurements(const CColor spectralReadings[], const int nReadings);

    // Get information about the sample. Currently we're only using getDescription()
    // getTech() and getDisplay() are included for future flexibility
    const char* getDescription() const;
    const char* getDisplay() const;
    const char* getTech() const;
    const char* getPath() const;
	const char* getReferenceInstrument() const;

    void setDescription(const char* tech, const char* disp);
    void setDisplay(const char* disp);
    void setTech(const char* tech);
	void setReferenceInstrument(const char* instr);
    void setPath(const char* path);
    void setAll(const char* tech, const char* disp, const char* instr, const char* path);

    // ArgyllMeterWrapper may access this member functions. It needs to be able to retrieve
    // a ccss structure in order to load it into the meter.
    // This is really implementation specific and is only of any use if you have loaded 
    // up the argyll header and so use should be limited to the ArgyllMeterWrapper library
    ccss* getCCSS() const;

private:
    ccss* m_ccss;
    std::string m_Path;
    std::string m_Description;
    std::string m_Display;
    std::string m_Tech;
	std::string m_RefInstrument;
	int	m_refrmode;
};


#endif