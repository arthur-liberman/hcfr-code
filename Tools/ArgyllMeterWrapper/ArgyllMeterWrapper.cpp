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
}

ArgyllMeterWrapper::ArgyllMeterWrapper(eMeterType meterType, eDisplayType displayType, eReadingType readingType) :
    m_meterType(meterType),
    m_displayType(displayType),
    m_readingType(readingType)
{
    createMeter(1, 9600, false);
}

ArgyllMeterWrapper::ArgyllMeterWrapper(eMeterType meterType, eDisplayType displayType, eReadingType readingType, int comPort, int baudRate, bool flowControl) :
    m_meterType(meterType),
    m_displayType(displayType),
    m_readingType(readingType)
{
    createMeter(comPort, baudRate, flowControl);
}

ArgyllMeterWrapper::~ArgyllMeterWrapper()
{
    if(m_meter)
    {
        m_meter->del(m_meter);
    }
}

void ArgyllMeterWrapper::createMeter(int comPort, int baudRate, bool flowControl)
{
    m_meter = new_inst(comPort, (instType)(m_meterType - 1), 1, 1);
    if(m_meter == 0)
    {
        throw std::logic_error("Create new Argyll instrument failed");
    }
    baud_rate argyllBaudRate(convertBaudRate(baudRate));
    flow_control argyllFlowControl(flowControl?fc_Hardware:fc_none);

    inst_code instCode = m_meter->init_coms(m_meter, comPort, argyllBaudRate, argyllFlowControl, 15.0);
    if(instCode != inst_ok)
    {
        m_meter->del(m_meter);
        m_meter = 0;
        throw std::logic_error("Starting communications with the meter failed");
    }

    instCode = m_meter->set_opt_mode(m_meter, inst_opt_set_filter, inst_opt_filter_none);

    instCode = m_meter->init_inst(m_meter);
    if(instCode != inst_ok)
    {
        m_meter->del(m_meter);
        m_meter = 0;
        throw std::logic_error("Failed to initialize the instrument");
    }

    // get actual meter type
    m_meterType = (eMeterType)(m_meter->get_itype(m_meter) + 1);

    inst_mode displayMode = inst_mode_emis_spot;
    instCode = m_meter->set_mode(m_meter, displayMode);
    if(instCode != inst_ok)
    {
        m_meter->del(m_meter);
        m_meter = 0;
        throw std::logic_error("Couldn't set display mode");
    }

    int capabilities = m_meter->capabilities(m_meter);
    inst_mode mode = inst_mode_ref_spot;

    if(m_readingType == PROJECTOR && capabilities & (inst_emis_proj_crt | inst_emis_proj_lcd))
    {
        instCode = m_meter->set_opt_mode(m_meter, m_displayType == CRT?inst_opt_proj_crt:inst_opt_proj_lcd);
        mode = inst_mode_emis_proj;
    }
    else if (capabilities & (inst_emis_disp_crt | inst_emis_disp_lcd)) 
    {
        instCode = m_meter->set_opt_mode(m_meter, m_displayType == CRT?inst_opt_disp_crt:inst_opt_disp_lcd);
        mode = inst_mode_emis_disp;
    }

    instCode = m_meter->set_mode(m_meter, mode);
    if(instCode != inst_ok)
    {
        m_meter->del(m_meter);
        m_meter = 0;
        throw std::logic_error("Couldn't set meter mode");
    }

    instCode = m_meter->set_opt_mode(m_meter, inst_opt_trig_prog);
    if(instCode != inst_ok)
    {
        m_meter->del(m_meter);
        m_meter = 0;
        throw std::logic_error("Couldn't set trigger mode");
    }

}

bool ArgyllMeterWrapper::doesMeterNeedCalibration()
{
    return true;
}

CColor ArgyllMeterWrapper::takeSpotXYZReading()
{
    ipatch argyllReading;
    inst_code instCode = m_meter->read_sample(m_meter, "SPOT", &argyllReading);
    if(instCode != inst_ok)
    {
        throw std::logic_error("Taking Reading failed");
    }
    CColor result(argyllReading.aXYZ[0], argyllReading.aXYZ[1], argyllReading.aXYZ[2]);
    return result;
}

void ArgyllMeterWrapper::calibrate()
{
    //m_meter->calibrate(m_meter);
}

std::string ArgyllMeterWrapper::getCalibrationInstructions()
{
    return "";
}
