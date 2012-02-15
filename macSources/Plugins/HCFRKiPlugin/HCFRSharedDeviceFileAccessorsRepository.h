//  ColorHCFR
//  HCFRSharedDeviceFileAccessorsRepository.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 26/01/08.
//
//  $Rev: 30 $
//  $Date: 2008-05-01 18:01:19 +0100 (Thu, 01 May 2008) $
//  $Author: jerome $
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


#import <Cocoa/Cocoa.h>
#import "HCFRDeviceFileAccessor.h"

/*!
    @class
    @abstract    Un repertoire des instances partagées de deviceAccessor
*/
@interface HCFRSharedDeviceFileAccessorsRepository : NSObject
{
  NSMutableDictionary *accessors;
  NSMutableDictionary *accessorsUsageCount;
}

+(HCFRSharedDeviceFileAccessorsRepository*) sharedInstance;
-(HCFRDeviceFileAccessor*) getDeviceFileAccessorForPath:(NSString*)path;
-(void) releaseFileAccessorForPath:(NSString*)path;
@end
