//  ColorHCFR
//  HCFRConfiguration.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/12/10.
//
//  $Rev$
//  $Date$
//  $Author$
// -----------------------------------------------------------------------------
// This program is free software; you can redistribute it and/or modify it under the terms of the GNU
// General Public License as published by the Free Software Foundation; either version 2 of the License,
// or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
// the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
// Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program; if not,
// write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
// -----------------------------------------------------------------------------

#import "HCFRConfiguration.h"


@implementation HCFRConfiguration

-(void) dealloc
{
  if (sensor != Nil)
  {
    
    [sensor release];
  }
  
  if (generator != Nil)
    [generator release];
  
  [super dealloc];
}

-(HCFRSensor*) sensor
{
  return sensor;
}
  
-(void) setSensor:(HCFRSensor *)newSensor
{
  if (sensor != Nil)
  {
    [sensor deactivate];
    [sensor release];
  }
  if (newSensor == Nil)
    sensor = Nil;
  else
  {
    sensor = [newSensor retain];
    [sensor activate];
  }
}

-(HCFRGenerator*) generator
{
  return generator;
}

-(void) setGenerator:(HCFRGenerator *)newGenerator
{
  if (generator != Nil)
  {
    [generator deactivate];
    [generator release];
  }
  if (newGenerator == Nil)
    generator = Nil;
  else
  {
    generator = [newGenerator retain];
    [generator activate];
  }
}
@end
