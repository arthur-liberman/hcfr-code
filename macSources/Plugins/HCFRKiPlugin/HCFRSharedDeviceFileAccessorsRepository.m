//  ColorHCFR
//  HCFRSharedDeviceFileAccessorsRepository.m
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

#import "HCFRSharedDeviceFileAccessorsRepository.h"

@interface HCFRSharedDeviceFileAccessorsRepositoryEntry : NSObject
{
  HCFRDeviceFileAccessor  *fileAccessor;
  int                     usageCounter;
}
-(HCFRSharedDeviceFileAccessorsRepositoryEntry*)initWithFileAccessor:(HCFRDeviceFileAccessor*)newFileAccessor;
-(void) incrementUsageCounter;
-(bool) decrementUsageCounter;
-(HCFRDeviceFileAccessor*)fileAccessor;
@end

@implementation HCFRSharedDeviceFileAccessorsRepositoryEntry
-(HCFRSharedDeviceFileAccessorsRepositoryEntry*)initWithFileAccessor:(HCFRDeviceFileAccessor*)newFileAccessor
{
  self = [super init];
  if (self != nil) {
    fileAccessor = [newFileAccessor retain];
    usageCounter = 1;
  }
  return self;
}
- (void) dealloc {
  [fileAccessor release];
  [super dealloc];
}

-(void) incrementUsageCounter
{usageCounter ++;}
-(bool) decrementUsageCounter;
{
  usageCounter --;
  return usageCounter == 0;
}
-(HCFRDeviceFileAccessor*)fileAccessor
{return fileAccessor;}
@end

@implementation HCFRSharedDeviceFileAccessorsRepository
+(HCFRSharedDeviceFileAccessorsRepository*) sharedInstance
{
  static HCFRSharedDeviceFileAccessorsRepository *sharedInstance = nil;
  
  if (sharedInstance == nil)
    sharedInstance = [[HCFRSharedDeviceFileAccessorsRepository alloc] init];
  
  return sharedInstance;
}

- (HCFRSharedDeviceFileAccessorsRepository*) init {
  self = [super init];
  if (self != nil) {
    accessors = [[NSMutableDictionary alloc] init];
  }
  return self;
}
- (void) dealloc {
  [accessors release];
  [super dealloc];
}

-(HCFRDeviceFileAccessor*) getDeviceFileAccessorForPath:(NSString*)path
{
  HCFRSharedDeviceFileAccessorsRepositoryEntry *result = [accessors objectForKey:path];
  
  if (result == nil)
  {
    HCFRDeviceFileAccessor  *newFileAccessor = [[HCFRDeviceFileAccessor alloc] initWithPath:[path cString]];
    
    result = [[HCFRSharedDeviceFileAccessorsRepositoryEntry alloc] initWithFileAccessor:newFileAccessor];
    [accessors setObject:result forKey:path];
    
    // le file accessor est retenu par le HCFRSharedDeviceFileAccessorsRepositoryEntry
    // et le HCFRSharedDeviceFileAccessorsRepositoryEntry est retenu par le dictionnaire
    // donc, on peut tout releaser.
    [newFileAccessor release];
    [result release];
  }
  else
  {
    [result incrementUsageCounter];
  } 
  
  return [result fileAccessor];
}
-(void) releaseFileAccessorForPath:(NSString*)path
{
  HCFRSharedDeviceFileAccessorsRepositoryEntry *entry = [accessors objectForKey:path];
  
  if (entry == nil)
    return;
  
  // si il n'y a plus personne d'enregistré, on supprime l'entrée
  if ([entry decrementUsageCounter])
  {
    [accessors removeObjectForKey:path];
  }
}
@end
