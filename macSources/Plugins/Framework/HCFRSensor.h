//  ColorHCFR
//  HCFRSensor.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/08/07.
//
//  $Rev: 135 $
//  $Date: 2011-02-27 10:48:51 +0000 (Sun, 27 Feb 2011) $
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
#import <HCFRPlugins/HCFRDataSource.h>

#define kHCFRSensorFailureException @"HCFRSensorFailurException"
#define kHCFRSensorInvalidePreferenceException @"HCFRSensorInvalidPreferenceException"

/*!
    @class
    @abstract    Interface d'accès aux capteurs
    @discussion  Cette classe permet d'accéder aux capteurs.
 C'est une couche d'abstraction qui uniformise l'accès aux données des capteurs.
 Une seul instance sera crée lors de l'execution. Si plusieurs sondes sont connectées,
 l'instance devra proposer à l'utilisateur de choisir la sonde à utiliser.
*/
@interface HCFRSensor : NSObject <HCFRDataSource> {
  Matrix    sensorToXYZMatrix;
  BOOL      calibrationPerformed;
  NSString  *currentCalibrationFile;

  BOOL      waitForInteraction; // utilisé pour attendre une interaction utilisateur.
  int       lastAlertReturnCode;
}
#pragma mark constructeur et desturcteur
/*!
    @function 
    @abstract   Initialisation
    @discussion Cette fonction sera appelée lors de la création de l'objet
 Il sera très probablement necessaire de redéfinir cette fonction pour chaque sous classe
 afin de s'enregistrer au niveau de l'OS pour être averti des connexion/déconnexion
 des sondes USB.
    @result     L'objet initialisé
*/
-(id) init;
  /*!
  @function 
   @abstract   Desctrucion
   @discussion Cette fonction sera appelée lors de la destruction de l'objet
   Il sera très probablement necessaire de redéfinir cette fonction pour chaque sous classe
   afin de se dés-inscrire pour l'écoute des evenements USB.
   @result     L'objet initialisé
   */
-(void) dealloc;

#pragma mark fonction divers
/*!
    @function 
    @abstract   Le nom du capteur
    @discussion Cette fonction retourne le nom du capteur pris en charge
    @result     Une chaine de caractères autoreleasée.
*/
+(NSString*) sensorName;
/*!
    @function 
    @abstract   L'identifiant unique du capteur
    @discussion Cet identifiant doit rester contant (pas de traduction, entre autre), il sera utilisé pour
 la sauvegarde et le chargement du capteur utilisé.
 LA REDEFINITION DE CETTE FONCTION EST OBLIGATOIRE !
 Le format de l'ID est libre, mais il est vivement conseillé d'utiliser un format similaire au Uniform Type Identifiers.
 Par exemple : homecinema-fr.sensors.kiSensor
    @result     L'identifiant unique
*/
+(NSString*) sensorId;
/*!
    @function 
    @abstract   Fonction appelée lorsque les paramètres du capteur sont affichés
    @discussion Cette fonction est prévue pour donner à la classe un oportunité de mettre à jour la liste
 des capteurs connectés avant affichage des paramètres.
 Elle sera appelée à chaque fois que le panneau de configuration sera affiché.
*/
-(void) refresh;


#pragma mark sauvegarde/chargement des préférences du capteur
/*!
    @function 
    @abstract   Retourne un dictionnaire contenant les préférences du capteur.
    @discussion Cette fonction sera appelée pour sauvegarder les paramètres du capteur.
 Le dictionnaire doit utiliser des objets NSString aussi bien pour les clés que pour les valeurs.
    @result     Le dictionnaire contenant les préférences du capteur.
*/
-(NSDictionary*) getPreferencesDictionary;
/*!
    @function 
    @abstract   Charge les préférences du capteur à partir d'un dictionnaire.
    @discussion Cette fonction sera appelée lors du chargement d'un fichier. Le dictionnaire contenant les paramètres
 du capteur est fourni en argument. Il correspond à celui qui aura été fourni par la fonction getPreferencesDictionary
 lors de la sauvegarde
 En cas de problème lors du chargement, la fonction peut lever une exception kHCFRSensorInvalidePreferenceException,
 en mettant un message explicite dans le champ "reason" (il sera affiché à l'utilisateur).
    @param      preferences Le dictionnaire de valeurs
*/
-(void) setPreferencesWithDictionary:(NSDictionary*)preferences;

#pragma mark Gestion du capteur
/*!
    @function 
    @abstract   Le capteur est-il utilisable ?
    @discussion Cette fonction permet au programme de savoir si le capteur représenté par la classe
 est utilisable (connecté, et en état de fonctionnement).
 Doit être surchargée pour définire un nouveau capteur.
    @result     YES si le capteur est utilisable, NO sinon.
*/
-(BOOL) isAvailable;
/*!
    @function 
    @abstract   Le capteur doit-il être calibré avant utilisation ?
    @discussion Cette fonction indique si le capteur nécéssite une calibration avant utilisation ou non.
 Doit être surchargée pour définire un nouveau capteur.
    @result     Un boolean indiquant si une calibration est requise.
*/
-(BOOL) needsCalibration;
/*!
    @function 
    @abstract   Les données de calibration sont elles valides ?
    @discussion Cette fonction indique si les données de calibration sont valides ou non.
    Si le capteur n'a pas besoin de calibration, cette fonction retournera toujours YES.
    Si cette fonction retourne NO, il faudra charger des données de calibration avant d'effectuer une mesure.
    @result     Un boolean indiquant si les données de calibration sont valides.
*/
-(BOOL) calibrationPerformed;
  /*!
    @function 
    @abstract   Met à jour la matrice de calibration du capteur.
    @discussion Cette fonction permet de mettre à jour la matrice de calibration du capteur.
 Cette matrice permet d'obtenir une couleur XYZ avec les données du capteur.
 Cette fonction n'a d'intéret que pour les capteurs necessitant une calibration !
    @param      newCalibrationMatrix La nouvelle matrice de calibration
*/
-(void) setSensorToXYZMatrix:(Matrix)newSensorToXYZMatrix;
/*!
    @function 
    @abstract   Retourne la matrice de calibration actuelle du capteur.
    @discussion Cette fonction n'a d'intéret que si le capteur necessite une calibration !
 Ne doit pas être surchargée.
    @result     La matrice de calibration.
*/
-(Matrix) getSensorToXYZMatrix;
/*!
    @function 
    @abstract   Retourne le nom du fichier de calibration actuellement chargé
    @discussion Retourne Nil si aucun fichier n'est chargé.
    @result     Le nom du fichier de calibration chargé
*/
-(NSString*) currentCalibrationFile;
/*!
    @function 
    @abstract   Appelé lorsque le capteur commence à être utilisé
    @discussion Cette fonction est appelée lors que le capteur commence à être utilisé.
 C'est le moment d'initialiser tout le necessaire !
*/
-(void) activate;
/*!
    @function 
    @abstract   Appelée lorsque la classe devient innutilisée
    @discussion Cette fonction est appelée lorsque le capteur devient innutilisé. Il
 peut alors se mettre en veille, et faire le necessaire pour limiter sa consommation de ressources.
*/
-(void) deactivate;

#pragma mark Gestion du fichier de calibration
/*!
   @function 
   @abstract   Supprime les informations de calibration
   @discussion Cette fonction est appelée quand on passe d'une calibration "fichier" à une calibration "a la volée"
*/
-(void) clearCalibrationData;
/*!
   @function 
   @abstract   Charge les données d'un fichier de calibration HCFR.
   @discussion Cette fonction n'a d'intéret que si le capteur necessite une calibration.
   @param      filePath Le chemin du fichier à charger
   @result     YES si le fichier à été chargé, NO si une erreur est survenue.
*/
-(BOOL) readCalibrationFile:(NSString *)filePath;

#pragma mark Gestion du panneau de configuration
/*!
@function 
 @abstract   Le capteur a-t-il un panneau de configuration ?
 @discussion Si le capteur dispose d'un panneau de configuration, surchargez
 cette methode pour qu'elle retourne YES. La methode showSetupPanel devra être définie
 Doit être surchargée pour définire un nouveau capteur.
 */
-(BOOL) hasASetupView;
  /*!
  @function 
   @abstract   Retourne la vue de configuration du capteur
   @discussion Cette methode doit être surchargée pour charger et retourner la vue
   de configuration du capteur. Elle ne sera appelée que si hasASetupView retourne YES.
   Par defaut, cette fonction ne fait rien, elle doit être surchargée pour définire un nouveau capteur.
   */
-(NSView*) setupView;

#pragma mark implementation du protocol DataSource
/*!
   @function 
   @abstract   Indique la prochaine valeur qui sera demandée
   @discussion Cette fonction n'a réellement d'intérêt que pour les capteurs virtuels, qui doivent
   retourner une valeur sans qu'il n'y ai de mesure effectuée. Pour les autres capteurs, il est innnutile
   de la redéfinir.
   */
-(void) nextMeasureWillBeType:(HCFRDataType)type referenceValue:(NSNumber*)referenceValue;
  /*!
  @function 
   @abstract   Effectue une mesure, et retourne le résultat
   @discussion Cette fonction charge une mesure du capteur, puis la retourne sous forme d'un objet
   HCFRColor autoreleasé. Si il n'est pas possible de faire une mesure, ou si la mesure à échouée
   (capteur déconnecté par exemple), la fonction retourne nil.
   Doit être surchargée pour définire un nouveau capteur.
   Cette methode doit lever une exception de nom kHCFRSensorFailureException si il est impossible de retourner la mesure.
   @result     Le résultat de la mesure. L'objet est autoreleasé. La fonction ne doit jamais retourner nil !
   */
-(HCFRColor*) currentMeasure;

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

-(void) displayAlertInMainThread:(NSAlert*)alert;

@end
