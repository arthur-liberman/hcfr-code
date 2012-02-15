//  ColorHCFR
//  HCFRColorReference.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 07/09/07.
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
#import <libHCFR/Color.h>

/*!
    @class
    @abstract    Represente
    @discussion  Cette classe sert d'interface pour la classe CColorReference de la librairie libHCFR.
*/
@interface HCFRColorReference : NSObject <NSCoding> {
  CColorReference *colorReference;
}
/*!
    @function 
    @abstract   Crée un object colorReference par defaut.
    @discussion Les valeurs utilisées doivent être cohérentes avec les param par défaut de l'interface !
*/
-(HCFRColorReference*) init;
-(HCFRColorReference*) initWithColorStandard:(ColorStandard)standard whiteTarget:(WhiteTarget)white;
-(void) dealloc;

-(CColorReference) cColorReference;
-(ColorStandard) colorStandard;
-(WhiteTarget) whiteTarget;
-(CColor) white;
-(CColor) redPrimary;
-(CColor) greenPrimary;
-(CColor) bluePrimary;

-(double) redReferenceLuma;
-(double) greenReferenceLuma;
-(double) blueReferenceLuma;
@end
