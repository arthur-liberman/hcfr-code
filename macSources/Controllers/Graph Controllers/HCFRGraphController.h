//  ColorHCFR
//  HCFRGraphController.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 10/09/07.
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
#import <HCFRPlugins/HCFRDataSerie.h>
#import <HCFRPlugins/HCFRDataSerieListener.h>

/*!
    @enum 
    @abstract   Types de graphiques
    @discussion Défini les différents types de graphqiques disponibles
    @constant   kCalibrationGraph Graphique utilisé pour représenter des données de calibration
    @constant   kContinuousMeasuresGraph Graphique utilisé pour représenter des données d'acquisition continue
    @constant   kUndefinedGraph Graphique 
*/
typedef enum
{
  kCalibrationGraph,
  kContinuousMeasuresGraph,
  kUndefinedGraph
} GraphType;

/*!
    @class
    @abstract    Base class for all graph controllers
    @discussion  This is the class from which all graph controllers inherit.
*/
@interface HCFRGraphController : NSObject <HCFRDataSerieListener> {
  HCFRDataSerie       *currentDataSerie;
  HCFRDataSerie       *referenceDataSerie;
  
  float               gamma;
  HCFRColorReference  *colorReference;
}
#pragma mark Identification du générateur et Gestion du panneau de configuration
/*!
 @function 
 @abstract   Nom lisible du graphique géré
 @discussion Fournis un nom comprehensible par un humain pour le graphiqué géré.
 @param      (name) (description)
 @result     (description)
 */
-(GraphType) graphicType;
/*!
@function 
 @abstract   Nom lisible du graphique géré
 @discussion Fournis un nom comprehensible par un humain pour le graphiqué géré.
 @param      (name) (description)
 @result     (description)
 */
-(NSString*) graphicName;
  /*!
  @function 
   @abstract   Le graphique a-t-il un panneau d'options ?
   @discussion Si le graphique dispose d'un panneau d'options, surchargez
   cette methode pour qu'elle retourne YES. La methode optionView devra être définie
   */
-(BOOL) hasAnOptionsView;
  /*!
  @function 
   @abstract   Retourne la vue d'options du graph
   @discussion Cette methode doit être surchargée pour charger et retourner la vue
   de présentant les options du graphique. Elle ne sera appelée que si hasAnOptionView retourne YES.
   Par defaut, cette fonction retourne nil.
   */
-(NSView*) optionsView;
  /*!
  @function 
   @abstract   Retourne la vue contenant le graphique
   @discussion Cette methode doit être surchargée pour retourner la vue
   de contenant le graphique.
   */
-(NSView*) graphicView;

/*!
    @function 
    @abstract   Fournie une nouvelle série de données
    @discussion Cette fonction permet de fournir une nouvelle série de données.
 L'implémentation par défaut de cette fonction inscrit l'objet comme listener sur la nouvelle série,
 et appelle la fonction "currenDataSerieChanged".
 Les implémentations de cette classe sont invités à surcharger les fonctions de listener et "currenDataSerieChanged"
 pour gérer les changement de données à afficher.
    @param      newSerie La nouvelle série de données
*/
-(void) setCurrentSerie:(HCFRDataSerie*)newSerie;
/*!
    @function 
    @abstract   Fourni la série de données à afficher.
    @discussion Attention, si aucune série n'est disponible, cette fonction retourne nil !
    @result     La série de données à afficher, ou nil si il n'y en a pas.
*/
-(HCFRDataSerie*) currentSerie;
  /*!
  @function 
   @abstract   Fournie une nouvelle série de données de référence
   @discussion Cette fonction permet de fournir une nouvelle série de données.
   L'implémentation par défaut de cette fonction inscrit l'objet comme listener sur la nouvelle série,
   et appelle la fonction "referenceDataSerieChanged".
   Les implémentations de cette classe sont invités à surcharger les fonctions de listener et "referenceDataSerieChanged"
   pour gérer les changement de données à afficher.
   @param      newSerie La nouvelle série de référence
   */
-(void) setReferenceSerie:(HCFRDataSerie*)newSerie;
  /*!
  @function 
   @abstract   Fourni la série de données de référence à afficher.
   @discussion Attention, si aucune série n'est disponible, cette fonction retourne nil !
   @result     La série de données de référence à afficher, ou nil si il n'y en a pas.
   */
-(HCFRDataSerie*) referenceSerie;
  /*!
  @function 
   @abstract   Fournie une nouvelle référence de couleur
   @discussion Il n'est pas possible de fournir une référence nulle.
   @param      newReference La nouvelle référence de couleur
   */
-(void) setColorReference:(HCFRColorReference*)newReference;
  /*!
  @function 
   @abstract   Fourni la référence de couleur
   @result     Une instance de HCFRColorReference représentant la référence actuelle
   */
-(HCFRColorReference*) colorReference;
  /*!
  @function 
   @abstract   Modifie la valeur du gamma
   @param      newGamma La nouvelle valeur du gamma
   */
-(void) setGamma:(float)newGamma;
  /*!
  @function 
   @abstract   Fourni la série du gamma
   @result     La valeur du gamma
   */
-(float) gamma;

#pragma mark Implémentation de l'interface listener de la data serie et des fonctions d'avertissement des changements
// les implémentations sont invitées à surcharger ces fonctions.
-(void) entryAdded:(HCFRDataStoreEntry*)newEntry toSerie:(HCFRDataSerie*)serie forDataType:(HCFRDataType)dataType;
-(void) serie:(HCFRDataSerie*)serie changedForDataType:(HCFRDataType)dataType;
-(void) colorReferenceChanged:(HCFRColorReference*)oldVersion;
-(void) gammaChanged:(float)oldValue;
-(void) currentDataSerieChanged:(HCFRDataSerie*)oldSerie;
-(void) referenceDataSerieChanged:(HCFRDataSerie*)oldSerie;
@end
