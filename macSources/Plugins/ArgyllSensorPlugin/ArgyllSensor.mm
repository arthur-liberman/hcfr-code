//
//  ArgyllSensor.m
//  ColorHCFR
//
//  Created by Nick on 25/02/12.
//  Copyright 2012 Homecinema-fr.com. All rights reserved.
//

#include <stdexcept>
#import "ArgyllSensor.h"

@implementation ArgyllSensor (Private)
-(void) nibDidLoad
{
	
}
@end

@implementation ArgyllSensor
-(id)init
{
	[super init];
	
	m_DisplayType = 0;
	m_ReadingType = ArgyllMeterWrapper::DISPLAY;
	m_PortNumber  = 1;
	m_meter		  = nil;
	
	return self;
}



-(void) dealloc
{
	m_meter = NULL;
	[super dealloc];
}

+(NSString*) sensorName
{
	return HCFRLocalizedString(@"Argyll sensor", @"Argyll sensor");
}

+(NSString*) sensorId
{
	return @"homecinema-fr.sensors.argyllSensor";
}

-(BOOL) allocSensorWrapper:(BOOL) forSimultaneousMeasures
{
	if(!m_meter)
	{
        std::string errorMessage;
        std::vector<ArgyllMeterWrapper*> meters = ArgyllMeterWrapper::getDetectedMeters(errorMessage);
        if(!meters.empty())
        {
            m_meter = meters[0];
        }
        else
		{
			[[NSException exceptionWithName:kHCFRSensorFailureException
									 reason:HCFRLocalizedString(@"The sensor has not been activated.",@"The sensor has not been activated.")
								   userInfo:nil] raise];
			m_meter = NULL;
			return NO;
		}
		m_meter->setHiResMode(m_HiRes);
	}
	
	return YES;
}

-(BOOL) isAvailable
{
	return YES;
}

-(BOOL) needsCalibration
{
	if(!m_meter && ![self allocSensorWrapper: NO]) 
			return NO;
	
	return m_meter->doesMeterSupportCalibration();
}

-(BOOL) hasASetupView
{
	return NO; // for now
}

-(void) calibrate
{
	if(!m_meter && ![self allocSensorWrapper: NO]) return;
    if(!m_meter->doesMeterSupportCalibration()) return;
	
    ArgyllMeterWrapper::eMeterState state(ArgyllMeterWrapper::NEEDS_MANUAL_CALIBRATION);
    while(state != ArgyllMeterWrapper::READY)
    {
        std::string meterInstructions(m_meter->getCalibrationInstructions());
        if(meterInstructions.empty())
        {
            break;
        }
      //  MessageBox(NULL, meterInstructions.c_str(), "Calibration Instructions", MB_OK);
        state = m_meter->calibrate();
        if(state == ArgyllMeterWrapper::INCORRECT_POSITION)
        {
        //    MessageBox(NULL, m_meter->getIncorrectPositionInstructions().c_str(), "Incorrect Position", MB_OK+MB_ICONHAND);
        }
    }
   // MessageBox(NULL, "Device is now calibrated.  If the device requires it return to the correct measurement position.", "Calibration Complete", MB_OK);
}

-(HCFRColor*) currentMeasure
{
	if(!m_meter && ![self allocSensorWrapper: NO]) 
	{
		
			[[NSException exceptionWithName:kHCFRSensorFailureException
									 reason:HCFRLocalizedString(@"The sensor has not been activated.",@"The sensor has not been activated.")
								   userInfo:nil] raise];
	}
	
	ArgyllMeterWrapper::eMeterState state(ArgyllMeterWrapper::NEEDS_MANUAL_CALIBRATION);
    while(state != ArgyllMeterWrapper::READY)
    {
        try
        {
            state = m_meter->takeReading();
        }
        catch(std::logic_error&)
        {
            return nil;
        }
        if(state == ArgyllMeterWrapper::NEEDS_MANUAL_CALIBRATION)
        {
            [self calibrate];
        }
        if(state == ArgyllMeterWrapper::INCORRECT_POSITION)
        {
			[[NSException exceptionWithName:kHCFRSensorFailureException
									 reason:HCFRLocalizedString(@"The sensor has not been activated.",@"The sensor has not been activated.")
								   userInfo:nil] raise];
		}
    }
	
	CColor color = m_meter->getLastReading();
	HCFRColor* measure = [(HCFRColor*)[HCFRColor alloc] initWithColor:color];
	
	return [measure autorelease];
}
-(NSView*)setupView
{
	return nil; // for now
}

@end
