//  ColorHCFR
//  HCFRAbstractEngine.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/08/07.
//
//  $Rev$
//  $Date$
//  $Author$
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
#import "HCFRConfiguration.h"
#import "HCFREngineDelegate.h"

/*!
    @class
    @abstract    Abstract classe for engines
    @discussion  This class is the base class for engines.
 Since abstract functions does not exist in objective-c 2, all methodes are implemented,
 but thoses that should be abstract simplly throws an exception
*/

@interface HCFRAbstractEngine : NSObject {
  NSObject<HCFREngineDelegate> *delegate;

  // pour la gestion des alertes
  BOOL waitForInteraction;
  int lastAlertReturnCode;
}

-(void) setConfiguration:(HCFRConfiguration*)configuration;

/*!
    @function
    @abstract   Get the calibrated screen
    @discussion Returns the screen that is being calibrated.
 It will return nil if no screen is being calibrated.
    @result     The calibrated screen, or nil if none is found.
*/
-(NSScreen*) getCalibratedScreen;

/*!
 @function 
 @abstract   Retourne la vue plein écran de controle du moteur.
 @discussion Fourni la vue de control du générateur destinée à être affichée en plein écran.
 Cette fonction retourne Nil si le moteur ne fournie pas de vue de contrôle plein écran.
 */
-(NSView*) getFullScreenControlView;

/*!
 @function 
 @abstract   Retourne la fenetre de controle à afficher si on n'est pas en plein écran..
 @discussion Fourni la fenetre de control du moteur destinée à être pendant son fonctionnement.
 C'est le pendant de getFullScreenControlView pour un affichage non plein écran.
 Cette fonction retourne Nil si le moteur ne fournie pas de fenetre de controle.
 */
-(NSWindow*) getControlWindow;

/*!
 @function 
 @abstract   Annule la procedure de calibration
 @discussion Un appel à cette procédure interrompra la série de mesures en cours. Les données récoltées jus'a présent
 seront concervées, mais pour le moment, il n'y a pas de reprise possible. Il faudra tout recommencer pour obtenir les mesures
 manquantes.
 */
-(void) abortMeasures;
/*!
 @function 
 @abstract   Indique si une série de mesures est en cours
 @result     YES si une mesure est en cours, NO sinon.
 */
-(bool) isRunning;

#pragma mark gestion du delegué
-(void) setDelegate:(NSObject<HCFREngineDelegate>*)newDelegate;

-(void) notifyEngineStart;
-(void) notifyEngineEnd;

#pragma Gestion des alertes
-(int) displayAlert:(NSAlert*)alert;
-(void) displayAlertInMainThread:(NSAlert*)alert;
@end
