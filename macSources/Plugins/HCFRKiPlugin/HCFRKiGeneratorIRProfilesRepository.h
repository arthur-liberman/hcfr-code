//  ColorHCFR
//  HCFRKiGeneratorXMLParser.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/01/08.
//
//  $Rev: 114 $
//  $Date: 2008-08-31 19:29:29 +0100 (Sun, 31 Aug 2008) $
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
#import "HCFRKiGeneratorIRProfile.h"
#import "HCFRIrProfilesRepositoryListener.h"

typedef enum
{
  // les infos
  HCFRKiGeneratorXmlElementDescription,
  HCFRKiGeneratorXmlElementAuthor,
  // les codes ID
  HCFRKiGeneratorXmlElementOkCode,
  HCFRKiGeneratorXmlElementRightArrowCode,
  HCFRKiGeneratorXmlElementLeftArrowCode,
  HCFRKiGeneratorXmlElementDownArrowCode,
  HCFRKiGeneratorXmlElementUpArrowCode,
  HCFRKiGeneratorXmlElementNextChapterArrowCode,
  // les delais
  HCFRKiGeneratorXmlElementMenuNavigationDelay,
  HCFRKiGeneratorXmlElementMenuValidationDelay,
  HCFRKiGeneratorXmlElementChapterNavigationDelay,

  HCFRKiGeneratorXmlNoElement
} ElementType;
/*!
    @class
    @abstract    Repertoire partagé des profiles IR pour la sonde HCFR
    @discussion  Cette classe est un singleton qui gère les profiles IR pour la sonde HCFR.
 Elle fourni un tableau de profiles après avoir lu ces profiles dans les fichiers XML.
 Le fichier intégré au logiciel est lu en premier, puis un eventuel fichier trouvé dans le dossier library
 de l'utilisateur est utilisé pour les profiles perso.
 Les clients peuvent s'inscrire pour être tenus au courrant des changements sur les profiles. TODO
*/
@interface HCFRKiGeneratorIRProfilesRepository : NSObject {
  NSMutableArray            *factoryProfilesArray;
  NSMutableArray            *customProfilesArray;
  
  NSMutableArray            *listenersArray;

  HCFRKiGeneratorIRProfile  *currentProfile; // contiendra le profile en cours de création pendant la lecture
  NSMutableArray            *currentlyLoadedProfilesArray;  // le tableau ou seront stockés les profiles chargés par le parser
  ElementType               currentType;
  NSMutableString           *currentElementContent; // contiendra le contenu de la balise
}
#pragma mark fonction d'accès au singleton
+ (HCFRKiGeneratorIRProfilesRepository*)sharedRepository;

#pragma mark accesseurs divers
-(NSArray*) factoryProfilesArray;
-(NSArray*) customProfilesArray;
-(void) addCustomProfile:(HCFRKiGeneratorIRProfile*)newProfile;
-(void) removeCustomProfile:(HCFRKiGeneratorIRProfile*)profileToRemove;

#pragma mark Gestion des listeners
  /*!
  @function 
   @abstract   Ajoute un listener
   @discussion Ajoute l'objet newListener commme listener.
   L'objet sera retenu jusqu'a ce qu'il soit supprimé de la liste des listeners par un appel à la fonction
   removeLister:
   @param      newListener L'objet qui doit être ajouté comme listener
   */
-(void) addListener:(NSObject<HCFRIrProfilesRepositoryListener>*)newListener;
  /*!
  @function 
   @abstract   Supprime un listerner pour le type donné
   @discussion Supprime l'objet oldListener de la liste des listenenrs pour les données dy type dataType.
   l'objet sera libéré à cette occasion. Si l'objet n'a pas été enregistré au préalable, la fonction
   ne fait rien.
   @param      oldListener L'objet à supprimer de la liste des listeners
   @param      dataType Le type de données pour lequel l'objet doit être retiré
   */
-(void) removeListener:(NSObject<HCFRIrProfilesRepositoryListener>*)oldListener;

  /*!
  @function 
   @abstract   Notifie tous les listeners que la liste des profiles à changée
   */
-(void) fireProfilesListChanged;

#pragma mark sauvegarde
-(void) saveCustomProfiles;
-(BOOL) saveProfiles:(NSArray*)profilesArray toPath:(NSString*)destinationPath;

#pragma mark Import XML
-(BOOL) importXMLFile:(NSString*)filePath;
@end
