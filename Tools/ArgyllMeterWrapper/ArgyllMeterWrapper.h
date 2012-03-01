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

#include "libHCFR/Color.h"
#include <string>
#include <vadefs.h>

struct _inst;

class ArgyllMeterWrapper
{
public:
    // we maintain our our meter list
    // annoyingly it doesn't quite line up with
    // the argyll internal list as there are some
    // unsupported meters in that list
    typedef enum
    {
        AUTODETECT = 0,
        DTP20,
        DTP22,
        DTP41,
        DTP51,
        DTP92,
        DTP94,
        SPECTROLINO,
        SPECTROSCAN,
        SPECTROSCANT,
        SPECTROCAM,
        I1DISPLAY,
        I1MONITOR,
        I1PRO,
        I1DISP3,
        COLORMUNKI,
        HCFR,
        SPYDER2,
        SPYDER3,
        HUEY,
        // this isn't really a meter, it intended to be used in
        // loops e.g for populating combo boxes
        LAST_METER
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

    typedef enum
    {
        READY,
        NEEDS_MANUAL_CALIBRATION,
        INCORRECT_POSITION,
    } eMeterState;

    /// ArgyllMeterWrapper constructor
    /// Create a USB meter object
    ArgyllMeterWrapper(eMeterType meterType, eDisplayType displayType, eReadingType readingType, int meterNumber);

    /// ArgyllMeterWrapper constructor
    /// Create a serial meter object
    ArgyllMeterWrapper(eMeterType meterType, eDisplayType displayType, eReadingType readingType, int comPort, int baudRate, bool flowControl);

    ~ArgyllMeterWrapper();

    /// initialize the meter
    /// returns true on success
    bool connectAndStartMeter(std::string& errorDescription);

    /// see if the meter supports calibration
    bool doesMeterSupportCalibration();

    // try and do a reading
    // the client application will need to handle the
    // possible non success conditions
    eMeterState takeReading();

    CColor getLastReading() const;

    // calibrate the meter
    // this should be called 
    // after the user has been told what to do
    // and presumably followed them
    // return true on success
    // false if the display is in wrong state or for 
    /// anything else oddd
    eMeterState calibrate();

    /// if either take reading or calibrate returns 
    /// NEEDS_MANUAL_CALIBRATION then call this to find out
    /// what the user needs to do next
    std::string getCalibrationInstructions();

    /// if either take reading or calibrate returns 
    /// INCORRECT_POSITION then call this to find out
    /// what the user needs to do next
    std::string getIncorrectPositionInstructions();

    /// gets the actual type of the meter
    eMeterType getType() const;

    /// Enable/Disable hi resolution mode on i1Pro
    void setHiResMode(bool enableHiRes);
    /// get the name of the meter given the type
    static std::string getMeterName(eMeterType meterType);

    /// is a given meter USB assume serial if false
    static bool isMeterUSB(eMeterType meterType);

private:
    void checkMeterIsInitialized() const;
    _inst* m_meter;
    eMeterType m_meterType;
    eDisplayType m_displayType;
    eReadingType m_readingType;
    int m_comPort;
    int m_baudRate;
    bool m_flowControl;
    CColor m_lastReading;
    int m_nextCalibration;
    char m_calibrationMessage[200];
};

extern void ArgyllLogMessage(const char* messageType, char *fmt, va_list& args);


#endif
