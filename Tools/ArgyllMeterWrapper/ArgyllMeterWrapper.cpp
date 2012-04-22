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


#include "ArgyllMeterWrapper.h"
#include "CriticalSection.h"
#include "LockWhileInScope.h"
#include <stdexcept>

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
#include "libusb.h"
#include "libusbi.h"
#include "conv.h"
#include "ccss.h"
#undef SALONEINSTLIB




namespace
{
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
    
    class ArgyllMeters
    {
    private:
        CriticalSection m_MeterCritSection;
        std::vector<ArgyllMeterWrapper*> m_meters;
        ArgyllMeters()
        {
            // technically we're supposed to call libusb_init() before calling any routines in libusb
            libusb_init(0);
        }
        ~ArgyllMeters()
        {
            for(size_t i(0); i < m_meters.size(); ++i)
            {
                delete m_meters[i];
            }
            m_meters.clear();
            // good as place as any to clear up usb
            libusb_exit(NULL);
            
        }
    public:
        static ArgyllMeters& getInstance()
        {
            static ArgyllMeters theInstance;
            return theInstance;
        }
        void addMeter(ArgyllMeterWrapper* newMeter)
        {
            CLockWhileInScope usingThis(m_MeterCritSection);
            for(size_t i(0); i < m_meters.size(); ++i)
            {
                if(m_meters[i]->isSameMeter(newMeter))
                {
                    delete newMeter;
                    return;
                }
            }
            m_meters.push_back(newMeter);
        }
        std::vector<ArgyllMeterWrapper*> getDetectedMeters()
        {
            CLockWhileInScope usingThis(m_MeterCritSection);
            std::vector<ArgyllMeterWrapper*> result(m_meters);
            return result;
        }
    };
}

// define the error handlers so we can change them
extern "C"
{
    extern void (*error)(char *fmt, ...);
    extern void (*warning)(char *fmt, ...);
    extern void (*verbose)(int level, char *fmt, ...);
}

ArgyllMeterWrapper::ArgyllMeterWrapper(_inst* meter) :
    m_readingType(DISPLAY),
    m_meter(meter),
    m_nextCalibration(0),
    m_meterType(meter->get_itype(meter))
    
{
}

ArgyllMeterWrapper::~ArgyllMeterWrapper()
{
    if(m_meter)
    {
        m_meter->del(m_meter);
        m_meter = 0;
    }
}

bool ArgyllMeterWrapper::connectAndStartMeter(std::string& errorDescription, eReadingType readingType)
{
    inst_code instCode;

    instCode = m_meter->set_opt_mode(m_meter, inst_opt_set_filter, inst_opt_filter_none);

    // allow this function to be called repeatedly on a meter
    if(!m_meter->inited)
    {
        instCode = m_meter->init_inst(m_meter);
        if(instCode != inst_ok)
        {
            m_meter->del(m_meter);
            m_meter = 0;
            errorDescription = "Failed to initialize the instrument";
            return false;
        }
    }

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
    int mode = inst_mode_emis_spot;

    if(m_readingType == PROJECTOR)
    {
        if(capabilities & (inst_emis_tele))
        {
            mode = inst_mode_emis_tele;
        }
        else if(capabilities & (inst_emis_proj))
        {
            mode = inst_mode_emis_proj;
        }
    }

    if ((capabilities & inst_spectral) != 0)
    {
        mode |= inst_mode_spectral;
    }

    instCode = m_meter->set_mode(m_meter, (inst_mode)mode);
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
    // reset the calibration
    m_nextCalibration = 0;
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
                            inst2_cal_crt_freq)) != 0);
}

CColor ArgyllMeterWrapper::getLastReading() const
{
    return m_lastReading;
}

int ArgyllMeterWrapper::getNumberOfDisplayTypes()
{
    checkMeterIsInitialized();
    if(m_meter->capabilities(m_meter) & inst_emis_disptype)
    {
        inst_disptypesel* displayTypes;
        int numItems;
        if (m_meter->get_opt_details(m_meter, inst_optdet_disptypesel, &numItems, &displayTypes) != inst_ok) 
        {
            return 0;
        }
        return numItems;
    }
    else
    {
        return 0;
    }
}

const char* ArgyllMeterWrapper::getDisplayTypeText(int displayModeIndex)
{
    checkMeterIsInitialized();
    if(m_meter->capabilities(m_meter) & inst_emis_disptype)
    {
        inst_disptypesel* displayTypes;
        int numItems;
        if (m_meter->get_opt_details(m_meter, inst_optdet_disptypesel, &numItems, &displayTypes) != inst_ok) 
        {
            return "Invalid Display Type";
        }
        if(displayModeIndex >= numItems)
        {
            return "Invalid Display Type";
        }
        return displayTypes[displayModeIndex].desc;
    }
    else
    {
        return "Invalid Display Type";
    }
}


int ArgyllMeterWrapper::getDisplayType() const
{
    return m_displayType;
}

void ArgyllMeterWrapper::setDisplayType(int displayMode)
{
    int numTypes(getNumberOfDisplayTypes());
    if(numTypes > 0 && displayMode < numTypes)
    {
        m_displayType = displayMode;
        inst_code instCode = m_meter->set_opt_mode(m_meter, inst_opt_disp_type, displayMode + 1);
        if(instCode != inst_ok)
        {
            throw std::logic_error("Set Display Type failed");
        }
    }
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

    m_lastReading.ResetSpectrum();
    m_lastReading = CColor(argyllReading.aXYZ[0], argyllReading.aXYZ[1], argyllReading.aXYZ[2]);
    if(argyllReading.sp.spec_n > 0)
    {
        int shortWavelength((int)(argyllReading.sp.spec_wl_short + 0.5));
        int longWavelength((int)(argyllReading.sp.spec_wl_long + 0.5));
        int bandWidth((int)((argyllReading.sp.spec_wl_long - argyllReading.sp.spec_wl_short) / (double)argyllReading.sp.spec_n + 0.5));
        CSpectrum spectrum(argyllReading.sp.spec_n, shortWavelength, longWavelength, bandWidth, argyllReading.sp.spec);
        m_lastReading.SetSpectrum(spectrum);
    }
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
        if(m_meterType == instColorMunki && instCode == (inst_code)0xf42)
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

void ArgyllMeterWrapper::checkMeterIsInitialized()
{
    if(!m_meter)
    {
        throw std::logic_error("Meter not initialized");
    }
    if(!m_meter->inited)
    {
        std::string errorDescription;
        if(!connectAndStartMeter(errorDescription, m_readingType))
        {
            throw std::logic_error(errorDescription);
        }
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
    if(m_meterType == instI1Pro || m_meterType == instColorMunki)
    {
        m_meter->set_opt_mode(m_meter, enableHiRes?inst_opt_highres:inst_opt_stdres);
    }
}

bool ArgyllMeterWrapper::doesSupportHiRes() const
{
    return (m_meterType == instI1Pro || m_meterType == instColorMunki);
}


std::string ArgyllMeterWrapper::getMeterName() const
{
    return inst_name((instType)m_meterType);
}

bool ArgyllMeterWrapper::isSameMeter(ArgyllMeterWrapper* otherMeter) const
{
    if(!isMeterStillValid())
    {
        return false;
    }
    if(!otherMeter->isMeterStillValid())
    {
        return false;
    }
    if(m_meter->icom->is_hid && otherMeter->m_meter->icom->is_hid)
    {
#if defined(NT)
        if(m_meter->icom->hidd && otherMeter->m_meter->icom->hidd &&
            m_meter->icom->hidd->dpath && otherMeter->m_meter->icom->hidd->dpath)
        {
            // a guess at a unique thing
            return (strcmp(m_meter->icom->hidd->dpath, otherMeter->m_meter->icom->hidd->dpath) == 0);
        }
        else
        {
            return false;
        }
#endif
#if defined (__APPLE__)
        return (m_meter->icom->hidd->ioob == otherMeter->m_meter->icom->hidd->ioob);
#endif
#if !defined (__APPLE__) && !defined(NT)
        return ((m_meter->icom->vid == otherMeter->m_meter->icom->vid) &&
                (m_meter->icom->pid == otherMeter->m_meter->icom->pid));
#endif
    }
    else if(m_meter->icom->is_usb && otherMeter->m_meter->icom->is_usb && 
         m_meter->icom->usbd && otherMeter->m_meter->icom->usbd)
    {
        return (m_meter->icom->vid == otherMeter->m_meter->icom->vid) &&
                (m_meter->icom->pid == otherMeter->m_meter->icom->pid) &&
                (m_meter->icom->usbd->bus_number == otherMeter->m_meter->icom->usbd->bus_number) &&
                (m_meter->icom->usbd->device_address == otherMeter->m_meter->icom->usbd->device_address);
    }
    else
    {
        return (m_meter->icom->port == otherMeter->m_meter->icom->port);
    }
}

bool ArgyllMeterWrapper::isMeterStillValid() const
{
    return m_meter && m_meter->icom;
}

ArgyllMeterWrapper::ArgyllMeterWrappers ArgyllMeterWrapper::getDetectedMeters(std::string& errorMessage)
{
    error = error_imp;
    warning = warning_imp;
    verbose = verbose_imp;
    errorMessage = "";

    // only detect meters once if some are found
    // means that we don't support an extra meter being adding during the
    // run at the moment, but I can live with this
    if(ArgyllMeters::getInstance().getDetectedMeters().empty())
    {
        icoms *icom = 0;
        if ((icom = new_icoms()) != NULL) 
        {
            icompath **paths;
            if ((paths = icom->get_paths(icom)) != NULL)
            {
                for (int i(0); paths[i] != NULL; ++i) 
                {
                    // avoid COM ports for now until we work out how to handle 
                    // them properly
                    if(strncmp("COM", paths[i]->path, 3) != 0 && strncmp("com", paths[i]->path, 3) != 0)
                    {
                        _inst* meter = 0;
                        try
                        {
                            meter = new_inst(i + 1, 0, 1, 0);
                        }
                        catch(std::logic_error&)
                        {
                            throw std::logic_error("No meter found at detected port- Create new Argyll instrument failed with severe error");
                        }
                        try
                        {
                            inst_code instCode = meter->init_coms(meter, i + 1, baud_38400, fc_nc, 15.0);
                            if(instCode == inst_ok)
                            {
                                ArgyllMeters::getInstance().addMeter(new ArgyllMeterWrapper(meter));
                            }
                            else
                            {
                                meter->del(meter);
                                errorMessage += "Starting communications with the meter failed. ";
                            }
                        }
                        catch(std::logic_error& e)
                        {
                            meter->del(meter);
                            errorMessage += "Incorrect driver - Starting communications with the meter failed with severe error. ";
                            errorMessage += e.what();
                        }
                    }
                }
            }
            icom->del(icom);
        }
    }
    return ArgyllMeters::getInstance().getDetectedMeters();
}

bool ArgyllMeterWrapper::isColorimeter()
{
    checkMeterIsInitialized();
    return !!(m_meter->capabilities(m_meter) & inst_colorimeter);
}



bool ArgyllMeterWrapper::doesMeterSupportSpectralSamples()
{
    checkMeterIsInitialized();
    return !!(m_meter->capabilities(m_meter) & inst_ccss);
}

bool ArgyllMeterWrapper::loadSpectralSample(const SpectralSample &sample)
{
    if (!doesMeterSupportSpectralSamples())
    {
        throw std::logic_error("Instrument doesn't have Colorimeter Calibration Spectral Sample capability");
    }

    if (m_SampleDescription == sample.getDescription())
    {
        // We're already loaded
        return true;
    }
    
    inst_code instCode = m_meter->col_cal_spec_set(m_meter, icxOT_default, NULL, sample.getCCSS()->samples, sample.getCCSS()->no_samp);
    if (instCode != inst_ok) 
    {
        std::string errorMessage("Setting Colorimeter Calibration Spectral Samples failed with error :'");
        errorMessage += m_meter->inst_interp_error(m_meter, instCode);
        errorMessage += "' (";
        errorMessage += m_meter->interp_error(m_meter, instCode);
        errorMessage += ")";
        throw std::logic_error(errorMessage);
    }

    // Keep a copy of the loaded sample description so we don't reload it unnecessarily
    m_SampleDescription = sample.getDescription();
    return true;
}

const std::string& ArgyllMeterWrapper::currentSpectralSampleDescription()
{
    return m_SampleDescription;
}

void ArgyllMeterWrapper::resetSpectralSample()
{
    // reset the current description
    m_SampleDescription.clear();

    inst_code instCode = m_meter->col_cal_spec_set(m_meter, icxOT_default, NULL, NULL, NULL);
    if (instCode != inst_ok) 
    {
        std::string errorMessage("Resetting Colorimeter Calibration Spectral Samples failed with error :'");
        errorMessage += m_meter->inst_interp_error(m_meter, instCode);
        errorMessage += "' (";
        errorMessage += m_meter->interp_error(m_meter, instCode);
        errorMessage += ")";
        throw std::logic_error(errorMessage);
    }
}
