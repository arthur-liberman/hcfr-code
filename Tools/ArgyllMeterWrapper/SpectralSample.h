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

class SpectralSample
{
    // ArgyllMeterWrapper needs access to the ccss data structure to load up a ccss file
    // which is provided by the protected member function getCCSS()

    friend class ArgyllMeterWrapper;

public:
    SpectralSample(void);
    SpectralSample(const SpectralSample& s);
    ~SpectralSample(void);

    bool operator==(const SpectralSample& s) const;
    SpectralSample& operator=(const SpectralSample& s);

    bool Read(std::string samplePath);
    bool Verify(std::string samplePath);

    // Get information about the sample. Currently we're only using getDescription()
    // getTech() and getDisplay() are included for future flexibility

    const char* getDescription();
    const char* getDisplay();    
    const char* getTech();	
    const char* getPath();

protected:
    void setDescription(char* tech, char* disp);
    void setDisplay(char* disp);    
    void setTech(char* tech);
    void setPath(char* path);
    void setAll(char* tech, char* disp, char* path );

    // ArgyllMeterWrapper may access these member functions. It needs to be able to retrieve
    // a ccss structure in order to load it into the meter. However, we really don't want any
    // other class touching this very implementation specific data structure, hence the protected
	// nature and the friend class declaration above

    ccss* getCCSS();

private:
    ccss* m_ccss;
    std::string m_Path;
    std::string m_Description;
    std::string m_Display;
    std::string m_Tech;
};


#endif