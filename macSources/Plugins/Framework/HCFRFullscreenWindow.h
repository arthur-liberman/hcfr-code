//  ColorHCFR
//  HCFRFullScreenWindow.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/08/07.
//
//  $Rev: 42 $
//  $Date: 2008-05-11 01:09:36 +0100 (Sun, 11 May 2008) $
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
@abstract    Fenetre plein écran
@discussion  Sous classe de NSWindow qui s'affiche en plein écran.
*/
@interface HCFRFullScreenWindow : NSWindow {
}
/*!
    @function 
    @abstract   Initialise une fenêtre en plein écran sur l'écran principal
*/
-(HCFRFullScreenWindow*) init;
/*!
  @function 
  @abstract   Initialise une fenêtre en plein écran sur l'écran spécifié
*/
-(HCFRFullScreenWindow*) initWithScreen:(NSScreen*)screen;
/*!
    @function 
    @abstract   Change l'écran à utiliser
    @discussion Modifie l'écran sur lequelle la fenetre s'affiche.
    @param      newScreen Le nouvel écran à utiliser.
*/
-(void) setScreen:(NSScreen*)newScreen;
@end
