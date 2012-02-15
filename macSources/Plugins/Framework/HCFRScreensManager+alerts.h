//  ColorHCFR
//  HCFRScreensManager+alerts.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 10/05/08.
//
//  $Rev: 47 $
//  $Date: 2008-05-20 17:54:26 +0100 (Tue, 20 May 2008) $
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
#import "HCFRScreensManager.h"

/*!
    @class
    @abstract    Gestion des alertes
    @discussion  Pour lever une alerte, les plugins doivent passer par cette classe :
 elle connait l'état des différents écrans, et peux donc déterminer où afficher l'alerte.
*/
@interface HCFRScreensManager (Alerts)
/*!
    @function 
    @abstract   Affiche une alerte à l'endroit le plus approprié, et retourne le code de retoure de l'alerte
    @param      alert L'alerte à afficher
    @param      delegate L'objet qui sera prévenue quand l'alerte sera finie
    @param      didEndSelector Le selecteur qui sera appelée quand l'alerte sera finie
    @param      calibrationWindow La fenêtre qui affiche les images de calibration si il y en a une.
 Si un seul écran est disponible et que cet argument n'est pas nil, la fenêtre recevra l'alerte.
    @result     Le résultat de l'alerte.
*/
-(void) displayAlert:(NSAlert*)alert modalDelegate:(id)delegate didEndSelector:(SEL)didEndSelector contextInfo:(void *)contextInfo;
@end
