//  ColorHCFR
//  HCFRANSIContrastView.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 10/09/07.
//
//  $Rev: 33 $
//  $Date: 2008-05-05 21:30:42 +0100 (Mon, 05 May 2008) $
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

typedef enum {
  kNoCross = 0,
  kCrossOnBlackRegions = 1,
  kCrossOnWhiteRegions = 2
} ANSIContrastViewCrossStyle;

/*!
    @class
    @abstract    (brief description)
    @discussion  (comprehensive description)
*/
@interface HCFRANSIContrastView : NSView {
  ANSIContrastViewCrossStyle    currentCrossStyle;
  BOOL                          blackRectFirst;
}

/*!
    @function 
    @abstract   Paramètre l'affichage des croix.
    @discussion La vue ANSIContrastView peut afficher des croix sur les zones blanches ou les zones noires
 afin d'indiquer à l'utilisateur où placer sa sonde. Cette fonction permet de paramétere ce comportement.
    @param      newStyle Le style de croix : kNoCross, kCrossOnBlackRegions ou kCrossOnWhiteRegions.
*/
-(void) setCrossStyle:(ANSIContrastViewCrossStyle)newStyle;

/*!
    @function 
    @abstract   Inverse les case noires et les cases blanches.
*/
-(void) invertBlackAndWhite;

-(void) drawRect:(NSRect)rect;

-(void) drawCrossInRect:(NSRect)rect size:(int)size;

@end
