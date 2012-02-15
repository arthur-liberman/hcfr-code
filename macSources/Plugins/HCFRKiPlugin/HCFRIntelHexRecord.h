//  ColorHCFR
//  HCFRIntelHexRecord.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 29/07/08.
//
//  $Rev: 96 $
//  $Date: 2008-08-19 12:35:23 +0100 (Tue, 19 Aug 2008) $
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

// Les exceptions
#define HexRecordInvalidFormatException @"fileFormatException"
#define HexRecordErroneousChecksumException @"erroneousChecksumException"

typedef unsigned char byte;
/*!
  @class
  @abstract    Représente un bloc de 64 bits de mémoire
*/
@interface MemPage : NSObject {
  byte    *page;
  UInt32  baseAddress;
}
-(void) setData:(byte*)data ofLength:(int)length atPosition:(UInt8)position;
-(byte*)getData;
-(void) setBaseAddress:(UInt32)newBaseAddress;
-(UInt32) getBaseAddress;
@end

/*!
    @class
    @abstract    Représente un fichier au format intel hex
    @discussion  Cette classe permet de lire un fichier Hex, de vérifier sa cohérence
 et de générer le flux de données contenant le code à envoyer au microcontroller.
 Pour plus de détails sur le format Hex, voir http://www.keil.com/support/docs/1584.htm
*/
@interface HCFRIntelHexRecord : NSObject {
//  NSMutableArray      *entries;
  NSMutableDictionary *memoryPages;
  
  // block reading variables
  UInt32  currentAdress;
  int     currentEntryIndex;
}
-(void) loadFile:(NSString*)filePath;
-(int) pagesCount;
-(NSEnumerator*) pagesEnumerator;
@end
