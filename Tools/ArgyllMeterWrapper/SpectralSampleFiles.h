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

// SpectralSampleFiles.h: vector/iterator for the argyll ccss files
//
//////////////////////////////////////////////////////////////////////

#if !defined(SPECTRALSAMPLEFILES_HEADER_INCLUDED)
#define SPECTRALSAMPLEFILES_HEADER_INCLUDED

#include <string>
#include <vector>
#include <stdarg.h>
#include "SpectralSample.h"

class SpectralSampleFiles
{
public:
    SpectralSampleFiles(void);

    typedef std::vector<std::string> SpectralSampleDescriptions;

    SpectralSample getSample(std::string sampleDescription) const;
    const SpectralSampleDescriptions& getDescriptions() const;
    bool doesSampleDescriptionExist(std::string sampleDescription) const;

private:	
    typedef std::vector<std::string> SpectralSamplePaths;
    SpectralSampleDescriptions m_SampleDescriptions;
    SpectralSamplePaths m_SamplePaths;
};


#endif