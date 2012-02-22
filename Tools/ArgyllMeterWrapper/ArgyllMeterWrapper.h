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
/////////////////////////////////////////////////////////////////////////////

// ArgyllMeterWrapper.h: Wrapper for the argyll spectro library.
//
//////////////////////////////////////////////////////////////////////

#if !defined(ARGYLLWAPPER_HEADER_INCLUDED)
#define ARGYLLWAPPER_HEADER_INCLUDED

#include "Color.h"
#include <string>

struct _inst;

class ArgyllMeterWrapper
{
public:
    typedef enum
    {
        AUTODETECT = 0,
        EYE_ONE,
    } eMeterType;

    typedef enum
    {
        CRT,
        LCD
    } eDisplayType;

    typedef enum
    {
        DISPLAY,
        PROJECTOR
    } eReadingType;

    /// ArgyllMeterWrapper constructor
    /// Create a USB meter object
    ArgyllMeterWrapper(eMeterType meterType, eDisplayType displayType, eReadingType readingType);

    /// ArgyllMeterWrapper constructor
    /// Create a serial meter object
    ArgyllMeterWrapper(eMeterType meterType, eDisplayType displayType, eReadingType readingType, int comPort, int baudRate, bool flowControl);

    ~ArgyllMeterWrapper();
    bool doesMeterNeedCalibration();
    CColor takeSpotXYZReading();
    void calibrate();
    std::string getCalibrationInstructions();

private:
    void createMeter(int comPort, int baudRate, bool flowControl);
    _inst* m_meter;
    eMeterType m_meterType;
    eDisplayType m_displayType;
    eReadingType m_readingType;
};

#endif
