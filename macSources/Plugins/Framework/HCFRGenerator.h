//  ColorHCFR
//  HCFRGenerator.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 30/08/07.
//
//  $Rev: 134 $
//  $Date: 2011-02-23 22:44:34 +0000 (Wed, 23 Feb 2011) $
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
#import <HCFRPlugins/HCFRGeneratorDelegate.h>
#import <HCFRPlugins/HCFRDataSource.h>
#import <HCFRPlugins/HCFRDataTypes.h>

// Les exceptions
#define kHCFRGeneratorDidNotStartException        @"HCFRGeneratorDidNotStartException"
#define kHCFRGeneratorInvalidePreferenceException @"HCFRGeneratorInvalidePreferenceException"

/*!
    @enum 
    @abstract   Décrit les différentes mesures disponibles
    @discussion Les valeurs de cette enumération représentent les différent types de mesures disponibles.
 Chaque type de mesure corresponde à une série de frame à afficher, et à un type de données dans le dataStore.
 Les valeurs de cette énumération sont des puissance de deux afin de pouvoir être combinées.
 Par exemple (kRedSaturationMeasure & kGreenSaturationMeasure & kBlueSaturationMeasure) représentera les
 mesures de saturation pour les trois couleurs
*/
typedef enum {
  kGrayscaleMeasure = 1, // currentStep = 0
  kGrayscaleNearBlackMeasure = 2, // currentStep = 1
  kGrayscaleNearWhiteMeasure = 4, // currentStep = 2
  kAllGrayscaleMeasure = kGrayscaleMeasure | kGrayscaleNearWhiteMeasure | kGrayscaleNearBlackMeasure,
  kRedSaturationMeasure = 8, // currentStep = 3
  kGreenSaturationMeasure = 16, // currentStep = 4
  kBlueSaturationMeasure = 32, // currentStep = 5
  kSaturationsMeasure = kRedSaturationMeasure | kGreenSaturationMeasure | kBlueSaturationMeasure,
  kPrimaryComponentsMeasure = 64, // currentStep = 6
  kSecondaryComponentsMeasure = 128, // currentStep = 7
  kComponentsMeasure = kPrimaryComponentsMeasure | kSecondaryComponentsMeasure,
  kFullScreenContrastMeasure = 256, // currentStep = 8
  kANSIContrastMeasure = 512, // currentStep = 9
  kContrastsMeasure = kFullScreenContrastMeasure | kANSIContrastMeasure,
  kSensorCalibrationMeasures = kPrimaryComponentsMeasure | kFullScreenContrastMeasure,
  kAllMeasures = kAllGrayscaleMeasure | kSaturationsMeasure | kComponentsMeasure | kContrastsMeasure,
  kContinuousMeasures = 65535
} HCFRMeasuresType;

/*!
    @class
    @abstract    Classe de base pour les générateurs
    @discussion  Cette class, qui aurait été abstraite si le language le permettait
  servira de base pour tous les générateurs qui seront définis.
 Le fonctionnement généra d'un générateur est le suivant :
 - lors d'un appel à startMeasures, il prépare ce qu'il veut, et affiche la première frame.
 - a chaque appel à nextFrame, il affiche la frame suivante, puis retourne.
 - Si une interraction utilisateur doit avoir lieu, il affiche l'alerte, puis positionne un semaphore (BOOL)
 et boucle tant que le semaphore n'a pas la valeur souhaité.
*/
@interface HCFRGenerator : NSResponder {
  // faut-il afficher un message avant les mesures de contrats pour dire
  // a l'utilisateur de placer la sonde face au projecteur ?
  BOOL  messageBeforeContrastsMeasure; 
                                       
  // Le générateur est-il en train de tourner ?
  bool  running;
  
  // pour la gestion des alertes modales pour le générateur
  BOOL  waitForInteraction;
  int   lastAlertReturnCode;
}

#pragma mark Identification du générateur et Gestion du panneau de configuration
/*!
    @function 
    @abstract   Nom lisible du générateur
    @discussion Fournis un nom comprehensible par un humain pour le générateur.
 Ce nom sera utilisé dans la liste des générateurs nottement.
    @result     (description)
*/
+(NSString*) generatorName;
/*!
    @function 
    @abstract   L'identifiant unique du generateur
    @discussion Cet identifiant doit rester contant (pas de traduction, entre autre), il sera utilisé pour
   la sauvegarde et le chargement du generateur utilisé.
   LA REDEFINITION DE CETTE FONCTION EST OBLIGATOIRE !
   Le format de l'ID est libre, mais il est vivement conseillé d'utiliser un format similaire au Uniform Type Identifiers.
   Par exemple : homecinema-fr.generators.kiGenerator
    @result     L'identifiant unique
*/
+(NSString*) generatorId;
/*!
 @function 
 @abstract   Le générateur a-t-il un panneau de configuration ?
 @discussion Si le générateur dispose d'un panneau de configuration, surchargez
 cette methode pour qu'elle retourne YES. La methode setupView devra alors être définie
 */
-(BOOL) hasASetupView;
/*!
 @function 
 @abstract   Retourne la vue de configuration du générateur
 @discussion Cette methode doit être surchargée pour charger et retourner la vue
 de configuration du générateur. Elle ne sera appelée que si hasASetupView retourne YES.
 Par defaut, cette fonction retourne nil.
 */
-(NSView*) setupView;

/*!
 @function 
 @abstract   Le générateur a-t-il un panneau de contrôle affichable durant l'utilisation ?
 @discussion Si le générateur dispose d'un panneau de controle, surchargez
 cette methode pour qu'elle retourne YES. La methode controlView devra alors être définie
 */
-(BOOL) hasAControlView;
/*!
 @function 
 @abstract   Retourne la vue de contrôle du générateur
 @discussion Cette methode doit être surchargée pour charger et retourner la vue
 de contrôle du générateur. Elle ne sera appelée que si hasAControlView retourne YES.
 Par defaut, cette fonction retourne nil.
 */
-(NSView*) controlView;

#pragma mark sauvegarde/chargement des préférences du capteur
/*!
    @function 
    @abstract   Retourne un dictionnaire contenant les préférences du generateur.
    @discussion Cette fonction sera appelée pour sauvegarder les paramètres du générateur.
 Le dictionnaire doit utiliser des objets NSString aussi bien pour les clés que pour les valeurs.
    @result     Le dictionnaire contenant les préférences du générateur.
*/
-(NSDictionary*) getPreferencesDictionary;
/*!
    @function 
    @abstract   Charge les préférences du générateur à partir d'un dictionnaire.
    @discussion Cette fonction sera appelée lors du chargement d'un fichier. Le dictionnaire contenant les paramètres
 du générateur est fourni en argument. Il correspond à celui qui aura été fourni par la fonction getPreferencesDictionary
 lors de la sauvegarde
 En cas de problème lors du chargement, la fonction peut lever une exception kHCFRGeneratorInvalidePreferenceException,
 en mettant un message explicite dans le champ "reason" (il sera affiché à l'utilisateur).
    @param      preferences Le dictionnaire de valeurs
*/
-(void) setPreferencesWithDictionary:(NSDictionary*)preferences;

#pragma mark Control du générateur
/*!
    @function 
    @abstract   Fourni une nouvelle référence de couleur au générateur.
    @discussion Le générateur n'a besoin de cette référence que dans le cas où in génère la couleur
 à afficher. Dans le cas contraire, il n'est pas necessaire de surcharger la fonction.
    @param      newReference la nouvelle référence
*/
-(void) setColorReference:(HCFRColorReference*)newReference;
/*!
  @function 
   @abstract   Le générateur est-il utilisable ?
   @discussion Cette fonction permet au programme de savoir si le générateur représenté par la classe
   est utilisable.
   @result     YES si le générateur est utilisable, NO sinon.
   */
-(BOOL) isGeneratorAvailable;
/*!
   @function 
   @abstract   Appelé lorsque le générateur commence à être utilisé
   @discussion Cette fonction est appelée lors que le générateur commence à être utilisé.
   C'est le moment d'initialiser tout le necessaire !
*/
-(void) activate;
/*!
   @function 
   @abstract   Appelée lorsque le générateur devient innutilisée
   @discussion Cette fonction est appelée lorsque le générateur devient innutilisé. Il
   peut alors se mettre en veille, et faire le necessaire pour limiter sa consommation de ressources.
*/
-(void) deactivate;
  /*!
   @function 
   @abstract   Lance le générateur.
   @discussion Une fois le générateur lancée, la fonction nextFrame permet d'afficher l'écran suivant..
   Le délégué doit être prévenu du chargement.
   Si le générateur n'a pas démarré pour une raison ou une autre, une exception sera levée
   avec comme nom kHCFRGeneratorDidNotStartException et avec une raison explicite (elle sera affichée à l'utilisateur)
   @param      measuresToPerforme Les mesures à effectuer. Doit être une combinaison d'entrées de HCFRMeasuresType.
*/
-(void) startMeasures:(HCFRMeasuresType)measuresToPerforme;
/*!
   @function 
   @abstract   Passe à la frame suivante.
   @discussion Cette fonction demande au générateur de passer à la frame suivante.
 Le changement de frame pouvant être long (nottament dans le cas d'un générateur manuel), 
 elle retourne instantanéement sans que la frame ait changée. Le délégué sera prévenu lorsque
 le changement de frame sera effectif.
   @result La fonction retourne YES si une frame à été affichée,
   NO sinon. Un retour à NO provoquera la fin de la série de mesures.
*/
-(BOOL) nextFrame;
/*!
   @function 
   @abstract   Clos le générateur.
   @discussion Provoque la fermeture du générateur. Il n'es pas désaloué, juste fermé.
*/
-(void) close;

#pragma mark Les fonctions de gestion des options
-(void) setMessageBeforeContrastsMeasures:(BOOL)newValue;
-(BOOL) getMessageBeforeContrastMeasures;

-(void) setRunning:(BOOL)newRunning;
-(BOOL) isRunning;

/*!
    @function 
    @abstract   Retourne l'écran utilisé par le générateur.
    @discussion Cette fonction retourne l'objet NSScreen utilisé par le générateur
 pour afficher ses données, ou nil si il n'en utilise pas.
 Elle est utilisée par le logiciel pour savoir quels écrans obscursir pendant les mesures.
 L'implémentation par défaut de cette fonction retourne nil.
 Le générateur ne peut réserver que un écran via cette fonction.
    @result     L'écran utilisée, ou nil si aucun n'est utilisée.
*/
-(NSScreen*) screenUsed;

#pragma mark Gestion du type de données
/*!
    @function 
    @abstract   Type de mesure actuelle
    @discussion En fonction de l'étape en cours, le type de mesure change. Il peut s'agir
 de mesure sur la luminosité, sur une des composante primaire ...
 Afin de classer les valeurs dans le HCFRDataStore, il faut que le generateur puisse indiquer
 le type de mesure actuellement en cours.
 Surchargez cette methode lors de l'écriture d'un générateur.
    @param      (name) (description)
    @result     (description)
*/
-(HCFRDataType) currentDataType;

/*!
    @function 
    @abstract   Retourne la valeur de référence pour la frame actuelle.
    @discussion Cette fonction indique la valeur de référence pour la frame actuellement affichée.
 Par exemple, pour une mesure de luminosité, la valeur IRE actuelle sera retournée.
 Pour un contrast, renvoie 0 pour la mesure du noir, 1 pour la mesure du blanc.
    @result     La valeur de référence actuelle
*/
-(double) currentReferenceValue;

#pragma mark Surcharges divers
-(NSString*) description;

#pragma mark les fonction d'alerte
/*!
    @function 
    @abstract   Affiche une alerte et attend qu'elle soit validée
    @discussion Le générateur tourne dans une thread séparée. Cette fonction permet d'afficher une alerte,
 et de bloquer la thread du générateur en attendant la réponse.
    @param      alert L'alert à afficher
    @result     le code de retour de l'alerte (NSAlert...Return)
*/
-(int) displayAlert:(NSAlert*) alert;
@end
