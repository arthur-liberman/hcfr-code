//  ColorHCFR
//  HCFRIntelHexRecord.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 29/07/08.
//
//  $Rev: 122 $
//  $Date: 2008-11-18 15:05:33 +0000 (Tue, 18 Nov 2008) $
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

#import <stdio.h>
#import "HCFRIntelHexRecord.h"

@implementation MemPage
- (id) init {
  self = [super init];
  if (self != nil) {
    page = malloc(64);
    memset(page,0xFF,64);
    baseAddress = 0;
  }
  return self;
}
- (void) dealloc {
  free (page);
  [super dealloc];
}

-(void) setData:(byte*)data ofLength:(int)length atPosition:(UInt8)position
{
  NSAssert2((position+length)<=64, @"MemPage : setting data : position(%d) + length(%d) will overflow page size", position, length);
  
  memcpy(page+position,data,length);
}
-(byte*) getData
{
  return page;
}
-(void) setBaseAddress:(UInt32)newBaseAddress
{
  baseAddress = newBaseAddress;
}
-(UInt32) getBaseAddress
{
  return baseAddress;
}
@end

@implementation HCFRIntelHexRecord
- (id) init {
  self = [super init];
  if (self != nil) {
    memoryPages = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (void) dealloc {
  [memoryPages release];
  [super dealloc];
}


-(void) loadFile:(NSString*)filePath
{
  UInt32 extendedAddress = 0;
  UInt32 linearAddress = 0;
  
  // on efface les pages actuelles
  [memoryPages removeAllObjects];
  
  FILE   *hexFile = fopen ([filePath cStringUsingEncoding:NSUTF8StringEncoding], "r");
  char   currentLine[523]; // 10 bytes overhead, max 255 bytes represented by 2 char max + \0
  BOOL   endOfFileReceived = NO;
  
  while (fgets(currentLine, 523, hexFile) && !endOfFileReceived)
  {
    int     scanStorage;
    int     scanResult;

    UInt8   dataLength;
    UInt16  address;
    UInt8   entryType;
    byte    data[256]; // pas d'alloc dynamique par pur fleme ...
    UInt8   computedChecksum = 0;
    UInt8   readChecksum;
    
    // décodage de la ligne -------------------
    // le ':' initial
    if (currentLine[0] != ':')
    {        
      fclose (hexFile);
      [[NSException exceptionWithName:HexRecordInvalidFormatException
                               reason:@"':' not found at the beginning of line"
                             userInfo:nil] raise];
    }
    
    // le champ length
    scanResult = sscanf (currentLine+1, "%2X", &scanStorage);
    if (scanResult != 1)
    {        
      fclose (hexFile);
      [[NSException exceptionWithName:HexRecordInvalidFormatException
                               reason:@"Could not read data length"
                             userInfo:nil] raise];
    }
    dataLength = (UInt8)scanStorage;
    computedChecksum += dataLength;
    
    // le champ address
    scanResult = sscanf (currentLine+3, "%4x", &scanStorage);
    if (scanResult != 1)
    {        
      fclose (hexFile);
      [[NSException exceptionWithName:HexRecordInvalidFormatException
                               reason:@"Could not read address"
                             userInfo:nil] raise];
    }
    address = (UInt16)scanStorage;
    computedChecksum = computedChecksum + (address & 0xFF) + (address >> 8);
    
    // le champ entry type
    scanResult = sscanf (currentLine+7, "%2x", &scanStorage);
    if (scanResult != 1)
    {        
      fclose (hexFile);
      [[NSException exceptionWithName:HexRecordInvalidFormatException
                               reason:@"Could not read entry type"
                             userInfo:nil] raise];
    }
    entryType = (UInt8)scanStorage;
    computedChecksum += entryType;
    
    // data
    int loopIndex;
    for (loopIndex = 0; loopIndex < dataLength; loopIndex++)
    {
      scanResult = sscanf (currentLine+9+loopIndex*2, "%2x", &scanStorage);
      if (scanResult != 1)
      {        
        fclose (hexFile);
        [[NSException exceptionWithName:HexRecordInvalidFormatException
                                 reason:@"Could not reading data"
                               userInfo:nil] raise];
      }
      data[loopIndex] = scanStorage;
      computedChecksum += data[loopIndex];
    }
    
    // checksum
    // on finalise le calcul de la checksum
    computedChecksum = computedChecksum ^ 0xFF; computedChecksum = computedChecksum + 1;// complement à 2 de la checksum
    // et on lit la checksum du fichier
    scanResult = sscanf (currentLine+9+dataLength*2, "%2x", &scanStorage);
    if (scanResult != 1)
    {        
      fclose (hexFile);
      [[NSException exceptionWithName:HexRecordInvalidFormatException
                               reason:@"Could not read checksum"
                             userInfo:nil] raise];
    }
    readChecksum = (UInt8)scanStorage;
    
    // vérification de la checksum
    if (readChecksum != computedChecksum)
    {        
      fclose (hexFile);
      [[NSException exceptionWithName:HexRecordErroneousChecksumException
                               reason:@"Checksum do not match"
                             userInfo:nil] raise];
    }
    
    // traitement de l'entrée
    switch (entryType)
    {
      case 00 : // ce sont des data
      {
        UInt32 dataAddress = address+extendedAddress+linearAddress;
        
        // on stoque les données dans la page adequate
        NSNumber  *pageAddress = [NSNumber numberWithInt:(dataAddress&0xFFFFFFC0)];
        MemPage   *currentPage = [memoryPages objectForKey:pageAddress];
        if (currentPage == Nil)
        {
//          NSLog (@"page not found for address %04x -> creating", [pageAddress intValue]);
          currentPage = [[MemPage alloc] init];
          [currentPage setBaseAddress:[pageAddress intValue]];
          [memoryPages setObject:currentPage forKey:pageAddress];
          [currentPage release];
        }
        [currentPage setData:data ofLength:dataLength atPosition:(dataAddress&0x3F)];

        break;
      }
      case 01 : // end of file
        endOfFileReceived = YES;
        break;
      case 02 : // extended segment address
        if (dataLength != 2)
        {        
          fclose (hexFile);
          [[NSException exceptionWithName:HexRecordInvalidFormatException
                                   reason:@"Invalid data length for extended segement address record"
                                 userInfo:nil] raise];
        }
        extendedAddress = ((data[0]<<8) + data[1]) << 4;
        break;
      case 04 : // extended linear address
        if (dataLength != 2)
        {        
          fclose (hexFile);
          [[NSException exceptionWithName:HexRecordInvalidFormatException
                                   reason:@"Invalid data length for extended linear address record"
                                 userInfo:nil] raise];
        }
        linearAddress = ((data[0]<<8) + data[1]) << 16;
        break;
    }
    
  } // de while (currentLine)
  
  fclose (hexFile);

  if (!endOfFileReceived)
    [[NSException exceptionWithName:HexRecordInvalidFormatException
                             reason:@"No end of file record found. File may be truncated."
                           userInfo:nil] raise];
}

-(int) pagesCount
{
  return [memoryPages count];
}

-(NSEnumerator*) pagesEnumerator
{
  return [memoryPages objectEnumerator];
}
@end
