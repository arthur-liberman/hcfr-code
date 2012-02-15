//  ColorHCFR
//  HCFRScreensManager.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 10/05/08.
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


/*!
    @class
    @abstract    Gestion des écrans
    @discussion  Cette classe à été créée pour gérer l'occultation des écrans.
*/
@interface HCFRScreensManager : NSObject {
  NSMutableArray  *displayedWindows;
  NSScreen        *calibratedScreen;
  NSView          *controlView;
  
  BOOL            screensDarkened;
}

#pragma mark Les fonctions de gestion des écrans
/*!
    @function 
    @abstract   Fournis l'instance partagée du manager
    @result     L'instance partagée du manager
*/
+ (HCFRScreensManager*)sharedManager;
/*!
    @function 
    @abstract   Indique l'écran en cours de calibration.
    @discussion L'écran indiqué à cette fonction ne sera pas occulté lorsque la fonction darken sera appelée.
 Si calibratedScreen vaut nil, c'est que tous les écrans doivent être occultés
    @param      calibratedScreen L'écran qui est calibré, ou nil si il n'y en a pas.
*/
-(void) setCalibratedScreen:(NSScreen*)newCalibratedScreen;
/*!
    @function 
    @abstract   Fourni la vue de contrôle
    @discussion La vue fournie lors de l'appel à cette fonction sera affichée sur le premier écran disponible
 lorsqu'une occultation sera demandée. Elle ne sera releasée que lorsque cette fonction sera appelée avec une autre
 vue ou nil.
    @param      controlView La vue a afficher sur le premier écran dispo, ou nil pour indiquer
 que aucune vue ne doit être affichée.
*/
-(void) setControlView:(NSView*)newControlView;
/*!
    @function 
    @abstract   Oculte les écrans.
    @discussion Occulte tous les écrans, à l'exception de l'écran calibré. Si une vue de contrôle est fournie,
 elle sera affichée sur ler premier moniteur disponible.
*/
-(void) darken;
/*!
    @function 
    @abstract   Stop l'occultation des écrans.
    @discussion Si les écrans sont occultés, ils seront . Sinon, la fonction ne fait rien.
    @param      (name) (description)
    @result     (description)
*/
-(void) enlight;
/*!
    @function 
    @abstract   Déplace une fenêtre pour la conserver visible pendant la calibration
    @discussion Si la fenêtre fournie est affichée sur l'écran calibré (fournis via setCalibratedScreen),
 elle est déplacée vers le centre d'un autre écran qui restera dispo pendant la calibration.
    @param      window La fenêtre à traiter.
*/
-(void) ensureWindowVisibility:(NSWindow*)window;
@end
