//  ColorHCFR
//  HCFRProfilesManager.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 30/12/07.
//
//  $Rev: 69 $
//  $Date: 2008-07-22 09:59:08 +0100 (Tue, 22 Jul 2008) $
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
#import "libHCFR/ProfilesRepositoryIndexFileReader.h"
#import <HCFRPlugins/HCFRUtilities.h>

/*!
    @class
    @abstract    Gestion des profiles de calibration
    @discussion  Cette classe gère les profiles de calibration des capteurs.
 Elle permet nottament d'obtenir une liste des fichiers de calibration, et permet de mettre à jour
 ces profiles via internet.
*/
@interface HCFRProfilesManager : NSObject {
  IBOutlet          NSWindow            *progressWindow;
  IBOutlet          NSView              *progressView;
  IBOutlet          NSProgressIndicator *progressIndicator;
  IBOutlet          NSTextField         *progressTextField;
  IBOutlet          NSView              *reportView;
  IBOutlet          NSTextField         *reportTextField;
  IBOutlet          NSView              *messageView;
  IBOutlet          NSTextField         *messageTextField;
  
  // Pour s'adapter au fonctionnement de la classe NSURLDownload,
  // on utilise une machine à état. updateRunning permet d'empêcher que deux mises à jour soient
  // lancées en même temps, et updateState contiend l'état de la machine.
  // Il y a deux états :
  //   0 -> reception du fichier d'index
  //   1 -> reception des profiles
  bool                              updateRunning;
  unsigned short                    updateState;
  
  ProfilesRepositoryIndexFileReader *profilesIndex;
  
  NSArray                           *currentlyAvailableProfiles;
  NSString                          *downloadedFileName;

  // les tableaux pour les résultats
  NSMutableArray                    *addedProfiles;
}
/*
 @function 
 @abstract   Retourne une instance partagée du gestionnaire de profiles
 */
+(HCFRProfilesManager*) sharedinstance;
/*
   @function 
   @abstract   Retourne le path du fichier de profile correspondant au nom fourni
   @discussion Cette fonction recherche dans la liste de profiles retourné par getAvailableProfiles
   un profile correspondant au nom fourni.
   @return     Une NSString contenant le path du fichier de profile, ou nil si rien n'est trouvé.
*/
+(NSString*) getProfileWithName:(NSString*)profileName;
/*
 @function 
 @abstract   Charge la liste des profiles actuellement disponibles
 @discussion Cette fonction fourni un tableau contenant les path des fichiers de profile
 actuellement installés sous forme de NSURL.
 Les profiles sont recherchés dans les dossiers "calibrationProfiles" du bundle
*/
+(NSArray*) getAvailableProfiles;
/*
    @function 
    @abstract   Met à jour les profiles de calibration sur internet
    @discussion Cette fonction lance la procédure de mise à jour des fichiers de calibration sur intenet.
*/
// TODO : les parametres de connexion sont en dur, c'est mal !!!
-(void) updateProfiles;

#pragma mark les actions IB
-(IBAction) closeWindow:(id)sender;
@end
