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

// ArgyllMeterWrapper.h: Wrapper for the argyll spectro library.
//
//////////////////////////////////////////////////////////////////////

#if !defined(ARGYLLWAPPER_HEADER_INCLUDED)
#define ARGYLLWAPPER_HEADER_INCLUDED

#include "libHCFR/Color.h"
#include <string>
#include <vector>
#include <stdarg.h>


#include "SpectralSampleFiles.h" 

struct _inst;


class ArgyllMeterWrapper
{
public:
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

    ~ArgyllMeterWrapper();

    /// initialize the meter
    /// returns true on success
    bool connectAndStartMeter(std::string& errorDescription, eReadingType readingType);

    /// see if the meter supports calibration
    bool doesMeterSupportCalibration();

    // try and do a reading
    // the client application will need to handle the
    // possible non success conditions
    eMeterState takeReading();

    CColor getLastReading() const;

    int getNumberOfDisplayTypes();
    const char* getDisplayTypeText(int displayModeIndex);
    int getDisplayType() const;
    void setDisplayType(int displayMode);

	// Determine if the meter supports spectral sample files
    bool doesMeterSupportSpectralSamples();

	// Load the supplied spectral sample 
    bool loadSpectralSample(SpectralSample& sample);

	// Determine if a spectral sample identified by a textual description is already loaded
    bool isSpectralSampleLoaded(const char* sampleDescription);

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

    /// Enable/Disable high resolution mode on i1Pro
    void setHiResMode(bool enableHiRes);
    /// Does device support high resolution mode on i1Pro
    bool doesSupportHiRes() const;

    /// get the name of the meter given the meter
    std::string getMeterName() const;

    // do the meter objects point to the same underlying meter
    // not expected to be useful to client code but harmless
    bool isSameMeter(ArgyllMeterWrapper* otherMeter) const;

    // do the meter objects point to the same underlying meter
    // not expected to be useful to client code but harmless
    bool isMeterStillValid() const;

    typedef std::vector<ArgyllMeterWrapper*> ArgyllMeterWrappers;

    // get a list of pointer to meters, the life time of the
    // pointers is handled at a global level and there is no need to free
    // or delete the returned objects
    static ArgyllMeterWrappers getDetectedMeters(std::string& errorMessage);

    // is the meter a colorimeter rather than a spectrometer
    bool isColorimeter();



private:
    /// ArgyllMeterWrapper constructor
    /// Create a USB meter object
    ArgyllMeterWrapper(_inst* meter);

    void checkMeterIsInitialized();
    _inst* m_meter;
    int m_displayType;
    int m_meterType;
    eReadingType m_readingType;
    CColor m_lastReading;
    int m_nextCalibration;
    char m_calibrationMessage[200];  

    SpectralSample m_Sample;
};

extern void ArgyllLogMessage(const char* messageType, char *fmt, va_list& args);


#endif
