//  ColorHCFR
//  HCFRUnicolorView.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/08/07.
//
//  $Rev: 82 $
//  $Date: 2008-07-26 12:33:20 +0100 (Sat, 26 Jul 2008) $
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


/*!
    @class
    @abstract    Vue unicolore
    @discussion  Cette vue ne sait faire qu'une chosen mais elle le fait bien : afficher une couleur unie.
 La fonction drawRect de NSView est surchargée pour afficher la couleur, et une fonction setColor est
 ajoutée pour modifier la couleur.
*/
@interface HCFRUnicolorView : NSView {
  NSColor       *color;
  int           screenCoverage;
  BOOL          forceFullscreen;  // supplente screenCoverage (utilisé pour les mesures de contrast)
  
  NSString      *description;
  NSDictionary  *lightDescriptionAttributes, *darkDescriptionAttributes;
}

/*!
    @function 
    @abstract   Change la couleur, et force cette couleur a etre affichée en plein écran, quel que soit le screenCoverage
*/
-(void) setFullscreenColor:(NSColor*)newColor;
-(void) setColor:(NSColor*)newColor;
-(int) screenCoverage;
-(void) setScreenCoverage:(int)newCoverage;
-(void) setDescription:(NSString*)newDescription;
@end
