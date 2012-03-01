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

#include "ArgyllMeterWrapper.h"

#define SALONEINSTLIB
#include "xspect.h"
#include "inst.h"
#undef SALONEINSTLIB
#include <stdexcept>

namespace
{
    baud_rate convertBaudRate(int baudRate)
    {
        switch(baudRate)
        {
        case 4800:
            return baud_4800;
        case 9600:
            return baud_9600;
        case 57600:
            return baud_57600;
        case 115200:
            return baud_115200;
        default:
            return baud_nc;
        }
    }
    
    instType convertMeterTypeToArgyllInst(ArgyllMeterWrapper::eMeterType meterType)
    {
        switch(meterType)
        {
        case ArgyllMeterWrapper::AUTODETECT:
            return instUnknown;
        case ArgyllMeterWrapper::DTP20:
            return instDTP20;
        case ArgyllMeterWrapper::DTP22:
            return instDTP22;
        case ArgyllMeterWrapper::DTP41:
            return instDTP41;
        case ArgyllMeterWrapper::DTP51:
            return instDTP51;
        case ArgyllMeterWrapper::DTP92:
            return instDTP92;
        case ArgyllMeterWrapper::DTP94:
            return instDTP94;
        case ArgyllMeterWrapper::SPECTROLINO:
            return instSpectrolino;
        case ArgyllMeterWrapper::SPECTROSCAN:
            return instSpectroScan;
        case ArgyllMeterWrapper::SPECTROSCANT:
            return instSpectroScanT;
        case ArgyllMeterWrapper::I1DISPLAY:
            return instI1Display;
        case ArgyllMeterWrapper::I1MONITOR:
            return instI1Monitor;
        case ArgyllMeterWrapper::I1PRO:
            return instI1Pro;
        case ArgyllMeterWrapper::I1DISP3:
            return instI1Disp3;
        case ArgyllMeterWrapper::COLORMUNKI:
            return instColorMunki;
        case ArgyllMeterWrapper::HCFR:
            return instHCFR;
        case ArgyllMeterWrapper::SPYDER2:
            return instSpyder2;
        case ArgyllMeterWrapper::SPYDER3:
            return instSpyder3;
        case ArgyllMeterWrapper::HUEY:
            return instHuey;
        default:
            return instUnknown;
        }
    }
    
    ArgyllMeterWrapper::eMeterType convertArgyllInstToMeterType(instType argyllType)
    {
        switch(argyllType)
        {
        case instUnknown:
            return ArgyllMeterWrapper::AUTODETECT;
        case instDTP20:
            return ArgyllMeterWrapper::DTP20;
        case instDTP22:
            return ArgyllMeterWrapper::DTP22;
        case instDTP41:
            return ArgyllMeterWrapper::DTP41;
        case instDTP51:
            return ArgyllMeterWrapper::DTP51;
        case instDTP92:
            return ArgyllMeterWrapper::DTP92;
        case instDTP94:
            return ArgyllMeterWrapper::DTP94;
        case instSpectrolino:
            return ArgyllMeterWrapper::SPECTROLINO;
        case instSpectroScan:
            return ArgyllMeterWrapper::SPECTROSCAN;
        case instSpectroScanT:
            return ArgyllMeterWrapper::SPECTROSCANT;
        case instSpectrocam:
            return ArgyllMeterWrapper::SPECTROCAM;
        case instI1Display:
            return ArgyllMeterWrapper::I1DISPLAY;
        case instI1Monitor:
            return ArgyllMeterWrapper::I1MONITOR;
        case instI1Pro:
            return ArgyllMeterWrapper::I1PRO;
        case instI1Disp3:
            return ArgyllMeterWrapper::I1DISP3;
        case instColorMunki:
            return ArgyllMeterWrapper::COLORMUNKI;
        case instHCFR:
            return ArgyllMeterWrapper::HCFR;
        case instSpyder2:
            return ArgyllMeterWrapper::SPYDER2;
        case instSpyder3:
            return ArgyllMeterWrapper::SPYDER3;
        case instHuey:
            return ArgyllMeterWrapper::HUEY;
        default:
            return ArgyllMeterWrapper::AUTODETECT;
        }
    }

    void warning_imp(char *fmt, ...) 
    {
        va_list args;
        va_start(args, fmt);
        ArgyllLogMessage("Warning", fmt, args);
        va_end(args);
    }

    void verbose_imp(int level, char *fmt, ...) 
    {
        va_list args;
        va_start(args, fmt);
        ArgyllLogMessage("Trace", fmt, args);
        va_end(args);
    }

    // get called for errors in argyll
    // can be called in normal situations
    // like no meters found or wrong drivers
    // convert to c++ exception
    void error_imp(char *fmt, ...) 
    {
        va_list args;
        va_start(args, fmt);
        ArgyllLogMessage("Error", fmt, args);
        va_end(args);
        throw std::logic_error("Argyll Error");
    }

    // check an actual inst code is a particular reason
    bool isInstCodeReason(inst_code fullInstCode, inst_code partialInstCode)
    {
        return ((fullInstCode & inst_mask) == partialInstCode);
    }
}

// define the error handlers so we can change them
extern "C"
{
    extern void (*error)(char *fmt, ...);
    extern void (*warning)(char *fmt, ...);
    extern void (*verbose)(int level, char *fmt, ...);
}


ArgyllMeterWrapper::ArgyllMeterWrapper(eMeterType meterType, eDisplayType displayType, eReadingType readingType, int meterNumber) :
    m_meterType(meterType),
    m_displayType(displayType),
    m_readingType(readingType),
    m_comPort(meterNumber),
    m_baudRate(9600),
    m_flowControl(false),
    m_meter(0),
    m_nextCalibration(0)
{
}

ArgyllMeterWrapper::ArgyllMeterWrapper(eMeterType meterType, eDisplayType displayType, eReadingType readingType, int comPort, int baudRate, bool flowControl) :
    m_meterType(meterType),
    m_displayType(displayType),
    m_readingType(readingType),
    m_comPort(comPort),
    m_baudRate(baudRate),
    m_flowControl(flowControl),
    m_meter(0),
    m_nextCalibration(0)
{
}

ArgyllMeterWrapper::~ArgyllMeterWrapper()
{
    if(m_meter)
    {
        m_meter->del(m_meter);
    }
}

bool ArgyllMeterWrapper::connectAndStartMeter(std::string& errorDescription)
{
    if(m_meter)
    {
        m_meter->del(m_meter);
        m_meter = 0;
    }

    error = error_imp;
    warning = warning_imp;
    verbose = verbose_imp;
    instType argyllMeterType(convertMeterTypeToArgyllInst(m_meterType));
    try
    {
        m_meter = new_inst(m_comPort, argyllMeterType, 1, 0);
    }
    catch(std::logic_error&)
    {
        errorDescription = "No meter found - Create new Argyll instrument failed with severe error";
        return false;
    }
    if(m_meter == 0)
    {
        errorDescription = "Create new Argyll instrument failed";
        return false;
    }
    baud_rate argyllBaudRate(convertBaudRate(m_baudRate));
    flow_control argyllFlowControl(m_flowControl?fc_Hardware:fc_none);

    inst_code instCode;
    try
    {
        instCode = m_meter->init_coms(m_meter, m_comPort, argyllBaudRate, argyllFlowControl, 15.0);
    }
    catch(std::logic_error&)
    {
        m_meter->del(m_meter);
        m_meter = 0;
        errorDescription = "Incorrect driver - Starting communications with the meter failed with severe error";
        return false;
    }
    if(instCode != inst_ok)
    {
        m_meter->del(m_meter);
        m_meter = 0;
        errorDescription = "Starting communications with the meter failed";
        return false;
    }

    instCode = m_meter->set_opt_mode(m_meter, inst_opt_set_filter, inst_opt_filter_none);

    instCode = m_meter->init_inst(m_meter);
    if(instCode != inst_ok)
    {
        m_meter->del(m_meter);
        m_meter = 0;
        errorDescription = "Failed to initialize the instrument";
        return false;
    }

    // get actual meter type
    m_meterType = convertArgyllInstToMeterType(m_meter->get_itype(m_meter));

    inst_mode displayMode = inst_mode_emis_spot;
    instCode = m_meter->set_mode(m_meter, displayMode);
    if(instCode != inst_ok)
    {
        m_meter->del(m_meter);
        m_meter = 0;
        errorDescription = "Couldn't set display mode";
        return false;
    }

    int capabilities = m_meter->capabilities(m_meter);
    inst_mode mode = inst_mode_emis_spot;

    if(m_readingType == PROJECTOR)
    {
        if(capabilities & (inst_emis_proj_crt | inst_emis_proj_lcd))
        {
            instCode = m_meter->set_opt_mode(m_meter, m_displayType == CRT?inst_opt_proj_crt:inst_opt_proj_lcd);
            mode = inst_mode_emis_proj;
        }
        if(capabilities & (inst_emis_tele))
        {
            mode = inst_mode_emis_tele;
        }
        else if(capabilities & (inst_emis_proj))
        {
            mode = inst_mode_emis_proj;
        }
    }
    else
    {
        if (capabilities & (inst_emis_disp_crt | inst_emis_disp_lcd)) 
        {
            instCode = m_meter->set_opt_mode(m_meter, m_displayType == CRT?inst_opt_disp_crt:inst_opt_disp_lcd);
        }
    }

    instCode = m_meter->set_mode(m_meter, mode);
    if(instCode == inst_unsupported && mode != inst_mode_emis_spot)
    {
        instCode = m_meter->set_mode(m_meter, inst_mode_emis_spot);
    }
    if(instCode != inst_ok)
    {
        m_meter->del(m_meter);
        m_meter = 0;
        errorDescription = "Couldn't set meter mode";
        return false;
    }

    instCode = m_meter->set_opt_mode(m_meter, inst_opt_trig_prog);
    if(instCode != inst_ok)
    {
        m_meter->del(m_meter);
        m_meter = 0;
        errorDescription = "Couldn't set trigger mode";
        return false;
    }
    return true;
}

bool ArgyllMeterWrapper::doesMeterSupportCalibration()
{
    checkMeterIsInitialized();
    int capabilities2 = m_meter->capabilities2(m_meter);
    return ((capabilities2 & (inst2_cal_ref_white |
                            inst2_cal_ref_dark |
                            inst2_cal_trans_white|
                            inst2_cal_trans_dark|
                            inst2_cal_disp_offset |
                            inst2_cal_disp_ratio |
                            inst2_cal_disp_int_time |
                            inst2_cal_proj_offset |
                            inst2_cal_proj_ratio |
                            inst2_cal_proj_int_time |
                            ((m_displayType == CRT)?inst2_cal_crt_freq:0))) != 0);
}

CColor ArgyllMeterWrapper::getLastReading() const
{
    checkMeterIsInitialized();
    return m_lastReading;
}

ArgyllMeterWrapper::eMeterState ArgyllMeterWrapper::takeReading()
{
    checkMeterIsInitialized();
    ipatch argyllReading;
    inst_code instCode = m_meter->read_sample(m_meter, "SPOT", &argyllReading);
    if(isInstCodeReason(instCode, inst_needs_cal))
    {
        // try autocalibration - we might get lucky
        m_nextCalibration = 0;
        instCode = m_meter->calibrate(m_meter, inst_calt_all, (inst_cal_cond*)&m_nextCalibration, m_calibrationMessage);
        // if that didn't work tell the user
        if(isInstCodeReason(instCode, inst_cal_setup))
        {
            return NEEDS_MANUAL_CALIBRATION;
        }
        if(instCode != inst_ok)
        {
            throw std::logic_error("Automatic calibration failed");
        }
        // otherwise try reading again
        instCode = m_meter->read_sample(m_meter, "SPOT", &argyllReading);
        if(isInstCodeReason(instCode, inst_needs_cal))
        {
            // this would be an odd situation and will need
            // further investigation to work out what to do about it
            throw std::logic_error("Automatic calibration succeed but reading then failed wanting calibration again");
        }
    }
    if(isInstCodeReason(instCode, inst_wrong_sensor_pos))
    {
        return INCORRECT_POSITION;
    }
    if(instCode != inst_ok || !argyllReading.aXYZ_v)
    {
        throw std::logic_error("Taking Reading failed");
    }

    m_lastReading = CColor(argyllReading.aXYZ[0], argyllReading.aXYZ[1], argyllReading.aXYZ[2]);
    return READY;
}

ArgyllMeterWrapper::eMeterState ArgyllMeterWrapper::calibrate()
{
    checkMeterIsInitialized();
    m_calibrationMessage[0] = '\0';
    inst_code instCode = m_meter->calibrate(m_meter, inst_calt_all, (inst_cal_cond*)&m_nextCalibration, m_calibrationMessage);
    if(isInstCodeReason(instCode, inst_cal_setup))
    {
        // special case for colormunki when calibrating the
        // error is a setup one if the meter is in wrong position
        if(m_meterType == COLORMUNKI && instCode == (inst_code)0xf42)
        {
            return INCORRECT_POSITION;
        }
        else
        {
            return NEEDS_MANUAL_CALIBRATION;
        }
    }
    if(instCode == inst_wrong_sensor_pos)
    {
        return INCORRECT_POSITION;
    }
    if(instCode != inst_ok)
    {
        throw std::logic_error("Calibration failed");
    }
    return READY;
}

std::string ArgyllMeterWrapper::getCalibrationInstructions()
{
    checkMeterIsInitialized();
    inst_code instCode(inst_ok);
    if(m_nextCalibration == 0)
    {
        instCode = m_meter->calibrate(m_meter, inst_calt_all, (inst_cal_cond*)&m_nextCalibration, m_calibrationMessage);
        if(instCode == inst_ok || isInstCodeReason(instCode, inst_unsupported))
        {
            // we don't need to do anything
            // and we are now calibrated
            return "";
        }
        if(!isInstCodeReason(instCode, inst_cal_setup))
        {
            throw std::logic_error("Automatic calibration failed");
        }
        // otherwise drop through here
    }
    switch(m_nextCalibration)
    {
        case inst_calc_none:
            return "No particular calibration setup or unknown";
        case inst_calc_uop_ref_white:
            return "user operated reflective white calibration";
        case inst_calc_uop_trans_white:
            return "user operated tranmissive white calibration";
        case inst_calc_uop_trans_dark:
            return "user operated tranmissive dark calibration";
        case inst_calc_uop_mask:
            return "user operated calibration mask";
        case inst_calc_man_ref_white:
            return "place instrument on reflective white reference";
        case inst_calc_man_ref_whitek:
            return "click instrument on reflective white reference";
        case inst_calc_man_ref_dark:
            return "place instrument in dark not close to anything";
        case inst_calc_man_em_dark:
            return "place cap on instrument put on dark surface or white ref.";
        case inst_calc_man_cal_smode:
            return "Put instrument sensor in calibration position";
        case inst_calc_man_trans_white:
            return "place instrument on transmissive white reference";
        case inst_calc_man_trans_dark:
            return "place instrument on transmissive dark reference";
        case inst_calc_man_man_mask:
            return "user configured calibration mask";
        case inst_calc_disp_white:
            return "Provide a white display test patch";
        case inst_calc_disp_grey:
            return "Provide a grey display test patch";
        case inst_calc_disp_grey_darker:
            return "Provide a darker grey display test patch";
        case inst_calc_disp_grey_ligher:
            return "Provide a darker grey display test patch";
        case inst_calc_disp_mask:
            return "Display provided reference patch";
        case inst_calc_proj_white
            :return "Provide a white projector test patch";
        case inst_calc_proj_grey:
            return "Provide a grey projector test patch";
        case inst_calc_proj_grey_darker:
            return "Provide a darker grey projector test patch";
        case inst_calc_proj_grey_ligher:
            return "Provide a darker grey projector test patch";
        case inst_calc_proj_mask:
            return "Projector provided reference patch";
        case inst_calc_change_filter:
            return std::string("Filter needs changing on device - ") + m_calibrationMessage;
        case inst_calc_message:
            return m_calibrationMessage;
    }
    return "Unknown state";
}

void ArgyllMeterWrapper::checkMeterIsInitialized() const
{
    if(!m_meter)
    {
        throw std::logic_error("Meter not initialized");
    }
}

std::string ArgyllMeterWrapper::getIncorrectPositionInstructions()
{
    return "Meter is in incorrect position";
}

void ArgyllMeterWrapper::setHiResMode(bool enableHiRes)
{
    checkMeterIsInitialized();
    // only suported on i1Pro and colormunki (photo not display)
    // but just do nothing otherwise
    if(m_meterType == I1PRO || m_meterType == COLORMUNKI)
    {
        m_meter->set_opt_mode(m_meter, enableHiRes?inst_opt_highres:inst_opt_stdres);
    }
}


ArgyllMeterWrapper::eMeterType ArgyllMeterWrapper::getType() const
{
    checkMeterIsInitialized();
    return m_meterType;
}

std::string ArgyllMeterWrapper::getMeterName(eMeterType meterType)
{
    switch(meterType)
    {
    case AUTODETECT:
        return "Auto-Detect";
    case DTP20:
        return "DTP20";
    case DTP22:
        return "DTP22";
    case DTP41:
        return "DTP41";
    case DTP51:
        return "DTP51";
    case DTP92:
        return "DTP92";
    case DTP94:
        return "DTP94";
    case SPECTROLINO:
        return "Spectrolino";
    case SPECTROSCAN:
        return "SpectroScan";
    case SPECTROSCANT:
        return "SpectroScanT";
    case SPECTROCAM:
        return "Spectrocam";
    case I1DISPLAY:
        return "I1Display";
    case I1MONITOR:
        return "I1Monitor";
    case I1PRO:
        return "I1Pro";
    case I1DISP3:
        return "I1Display3";
    case COLORMUNKI:
        return "ColorMunki";
    case HCFR:
        return "HCFR";
    case SPYDER2:
        return "Spyder2";
    case SPYDER3:
        return "Spyder3";
    case HUEY:
        return "Huey";
    default:
        return "Unknown";
    }
}

bool ArgyllMeterWrapper::isMeterUSB(eMeterType meterType)
{
    switch(meterType)
    {
    case DTP20:
    case DTP92:
    case DTP94:
    case I1DISPLAY:
    case I1MONITOR:
    case I1PRO:
    case I1DISP3:
    case COLORMUNKI:
    case HCFR:
    case SPYDER2:
    case SPYDER3:
    case HUEY:
        return true;
    default:
        return false;
    }
}
