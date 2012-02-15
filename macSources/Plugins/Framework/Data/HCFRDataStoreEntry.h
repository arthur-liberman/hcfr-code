//  ColorHCFR
//  HCFRDataStoreEntry.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/08/07.
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


#import <Cocoa/Cocoa.h>
#import <HCFRPlugins/HCFRColor.h>

/*!
    @class
    @abstract    Structure élémentaire de stockage du HCFRDataStore
    @discussion  Cette structure contiend deux objest HCFRMeasure : la valeure théorique
 fournie par le générateur et la valeur lue par la sonde.
*/
@interface HCFRDataStoreEntry : NSObject <NSCoding> {
  @private
    HCFRColor     *value;
    NSNumber      *referenceNumber;
}

-(HCFRDataStoreEntry*) initWithMeasure:(HCFRColor*)newValue referenceNumber:(NSNumber*)newReferenceNumber;

#pragma mark Accesseurs
/*!
    @function 
    @discussion Retourne un objet HCFRColor représentant la valeur mesurée par la sonde.
    @result     La valeur mesurée par la sonde sous forme d'objet HCFRColor
*/
-(HCFRColor*) value;
/*!
   @function 
   @discussion Retourne un objet HCFRMeasure représentant la valeur de référence fournie par le générateur.
   @result     La valeur de référence fournie par le générateur sous forme de NSNumber représentant un double.
*/
-(NSNumber*) referenceNumber;

#pragma mark autres fonctions
- (NSComparisonResult)compare:(HCFRDataStoreEntry *)aNumber;
@end
