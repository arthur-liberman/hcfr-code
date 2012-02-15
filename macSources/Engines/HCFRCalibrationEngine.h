//  ColorHCFR
//  HCFRCalibrationEngine.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/08/07.
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
#import <HCFRPlugins/HCFRGenerator.h>
#import <HCFRPlugins/HCFRSensor.h>
#import <HCFRPlugins/HCFRDataSerie.h>
#import <HCFRPlugins/HCFRUtilities.h>

#import "HCFRAbstractEngine.h"

#import "HCFREngineDelegate.h"
#import "HCFRControlView.h"

/*!
    @class
    @abstract    La moteur de d'acquisition
    @discussion  Cette classe est chargée de gérer l'acquisition des données de calibration.
 Il s'agit de gérer le générateur, pour chaque frame de lire les informations de la sonde, puis
 de les stocker dans le dataStore.
 Cette classe s'enregistrera en tant que délégué du générateur lors du lancment de la procédure.
*/
@interface HCFRCalibrationEngine : HCFRAbstractEngine {
  @private
    IBOutlet HCFRControlView *controlView;
  
    HCFRGenerator   *generator;
    HCFRSensor      *sensor;
    HCFRDataSerie   *dataSerie;
  
    BOOL            sensorFacingSource; // Pour afficher une alerte lors de la première mesure de contrast
    BOOL            abortRequested;
    NSLock          *measureLock; // Pour éviter des mesures concurentes.
    
    NSString        *engineStatus;
}

#pragma mark Accesseurs
/*!
    @function 
    @discussion Retourne l'instance de HCFRGenerator actuellement utilisée
    @result     L'instance de HCFRGenerator utilisée
*/
-(HCFRGenerator*) generator;
/*!
   @function 
   @discussion Modifie l'instance de HCFRGenerator à utiliser
   @param      newGenerator La nouvelle instance à utiliser 
*/
-(void) setGenerator:(HCFRGenerator*)newGenerator;
/*!
   @function 
   @discussion Retourne l'instance de HCFRSensor actuellement utilisée
   @result     L'instance de HCFRSensor utilisée
*/
-(HCFRSensor*) sensor;
/*!
   @function 
   @discussion Modifie l'instance de HCFRSensor à utiliser
   @param      newGenerator La nouvelle instance à utiliser 
*/
-(void) setSensor:(HCFRSensor*)newSensor;
/*!
   @function 
   @discussion Retourne l'instance de HCFRDataStore actuellement utilisée
   @result     l'instance de HCFRDataStore utilisée
*/
-(HCFRDataSerie*) dataSerie;
/*!
   @function 
   @discussion Modifie l'instance de HCFRDataStore à utiliser
   @param      newDataStore La nouvelle instance à utiliser 
*/
-(void) setDataSerie:(HCFRDataSerie*)newDataSerie;

#pragma mark control de la procédure de calibration
/*!
    @function 
    @abstract   Contrôle la validité de l'environnement.
    @discussion Cette fonction vérifie que un capteur et un générateur ont été séléctionnés, que un dataStore est disponible,
 et qu'ils sont tous prèt à fonctionner. Si tout est OK, la fonction retourne YES. Sinon, la fonction affiche
 un message d'erreur et retourne NO.
 Des exeptions sont levées si le générateur ou le capteur sont nulls (c'est un cas qui ne devrait pas arriver,
 qui met en évidence un erreur de programmation que le développeur doit corriger).
    @param      requiresASerie Indique si une série courrante est requise. Ce n'est pas le cas dans le cas d'une calibration, mais c'est le cas pour une série de mesures.
    @result     YES si l'environnemnent est valide, NO sinon.
*/
-(BOOL) checkEnvironment:(bool)requiresASerie;
/*!
    @function 
    @abstract   Calibration du capteur
    @discussion Cette fonction effectue (si necessaire) une calibration du capteur séléctionné.
    @result     YES si la calibration a effectivement été lancée, NO si le lancement à été annulé.
*/
-(bool) startSensorCalibration;
/*!
    @function 
    @abstract   Lancement de mesures
    @discussion Cette procédure vérifie que les paramètres en place sont correctes via un appel à checkEnvironment,
 puis lance les mesures demandées.
    @result     YES si les mesures ont effectivement été lancées, NO si le lancement à été annulé.
*/
-(bool) startMeasures:(HCFRMeasuresType)measuresToDo;

#pragma mark Actions d'interface
-(IBAction) abortMeasures:(id)sender;

#pragma mark Fonctions de feeback
-(NSString*) status;
-(NSString*) currentMeasureName;

#pragma mark Calcul de la matrice de calibration
/*!
    @function 
    @abstract   Calcul la matrice de calibration à partir des mesures fournies
    @discussion Cette fonction calcule à partir de mesures de mires la matrice permettant de passer
 des valeurs du capteur aux valeurs XYZ.
    @param      redMeasure La mesure pour la mire rouge
    @param      greenMeasure La mesure pour la mire verte
    @param      blueMeasure La mesure pour la mire bleu
    @param      blackMeasure La mesure pour la mire noire
    @param      whiteMeasure La mesure pour la mire blanche
    @result     La matrice de calibration obtenue, ou null si la calibration à échouée.
*/
-(Matrix) computeCalibrationMatrixWithRedMeasure:(HCFRColor*)redMeasure
                                    greenMeasure:(HCFRColor*)greenMeasure
                                     blueMeasure:(HCFRColor*)blueMeasure
                                    blackMeasure:(HCFRColor*)blackMeasure
                                    whiteMeasure:(HCFRColor*)whiteMeasure;

@end
