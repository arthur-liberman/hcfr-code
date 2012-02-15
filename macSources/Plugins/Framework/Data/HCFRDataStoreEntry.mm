//  ColorHCFR
//  HCFRDataStoreEntry.m
// -----------------------------------------------------------------------------
//  Created by JŽr™me Duquennoy on 31/08/07.
//
//  $Rev: 51 $
//  $Date: 2008-05-22 13:48:37 +0100 (Thu, 22 May 2008) $
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

#import <HCFRPlugins/HCFRDataStoreEntry.h>


@implementation HCFRDataStoreEntry
#pragma mark Constructeurs et destructeurs
-(HCFRDataStoreEntry*) initWithMeasure:(HCFRColor*)newValue referenceNumber:(NSNumber*)newReferenceNumber
{
  [super init]; 

  value = [newValue retain];
  referenceNumber = [newReferenceNumber retain];
  
  return self;
}

-(id) initWithCoder:(NSCoder*)coder
{  
  [super init];
  
  referenceNumber = [[coder decodeObjectForKey:@"referenceNumber"] retain];
  value = [[coder decodeObjectForKey:@"value"] retain];
  
  return self;
}

-(void) encodeWithCoder:(NSCoder*)coder
{
  [coder encodeObject:referenceNumber forKey:@"referenceNumber"];
  [coder encodeObject:value forKey:@"value"];
}

-(void) dealloc
{
  [value release];
  [referenceNumber release];
  
  [super dealloc]; 
}

#pragma mark Accesseurs
-(HCFRColor*) value
{
  return value;
}
-(NSNumber*) referenceNumber
{
  return referenceNumber;
}

#pragma mark Pour debug
-(NSString*) description
{
  return [NSString stringWithFormat:@"HCFRDataStoreEntry[value=%@ - reference=%@]", value, referenceNumber];
}

#pragma mark autres fonctions
- (NSComparisonResult)compare:(HCFRDataStoreEntry *)anEntry
{
  return [referenceNumber compare:[anEntry referenceNumber]];
}
@end
