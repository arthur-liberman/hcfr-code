//  ColorHCFR
//  HCFRDataSerie.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 13/05/08.
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
#import <HCFRPlugins/HCFRColor.h>
#import <HCFRPlugins/HCFRDataTypes.h>
#import <HCFRPlugins/HCFRDataSerieListener.h>
#import <HCFRPlugins/HCFRColorReference.h>

/*!
@class
@abstract    Stockage d'une série de données
@discussion  Cette classe est prévue pour stocker les informations générées lors d'une calibration.
Les données sotockées par paire (valeur théorique <-> valeur obtenue), et regroupées par type de mesure.
Un classe implémentant le protocol HCFRDataListener pourra s'enregistrer pour une série de mesure pour
être avertie lors des changements.
NOTE IMPORTANTE : La data serie ne fourni jamais de copie du tableau de donnée ni d'enumérateur
car il est impossible de suivre leur usage par la suite, ce qui pose de gros problème, nottament
au niveaux des verrous.
Si on faisait ça, le dataStore ne serait plus du tout prêt à gérer du multithread.
Donc, pour parcourir les entreés d'un dictionnaire, on demande le nombre d'éléments, puis on les parcours
avec une boucle for et la fonction getEntryAtIndex.
*/
@interface HCFRDataSerie : NSObject {
  @private
  NSString              *name;
  
  // dataStore contiendra un NSMutableArray par type de données possible
  // Chaque tableau contiendra les objest HCFRDataStoreEntry associés au type de mesure
  NSMutableDictionary   *dataStore;
  // listeners contiendra un NSMutableArray par type de données possible
  // Chaque tableau contiendra les listeners inscrits pour ce type
  NSMutableDictionary   *listeners;
  
  // On prévoie le cas multitache : on utilise des mutex pour empécher de lire pendant un ajout de donnée.
  // C'est rendu nécessaire par le fait que les NSMutableArray ne sont pas thread safe.
  // Histoire d'avoir un minimum de granularité, on prend un mutex par type de donnée.
  NSMutableDictionary  *locksArray;
    
  // Les notes
  NSAttributedString   *notes;
  
  BOOL                 dirty; // indique si la série à été modifiée depuis le dernier appel a encodeWithCoder
}

-(NSString*) name;
-(void) setName:(NSString*)newName;

#pragma mark Gestion des mesures
/*!
@function 
 @abstract   Enregistre un nouveau résultat pour le type de données spécifié
 @discussion Ajoute un nouvel objet HCFRDataStoreEntry initialisé avec la mesure et la valeur de référence fournis
 pour le type de mesure fournis. Les mesures ajoutées sont automatiquement triées en fonction de leur referenceNumber.
 @param      value La valeur HCFRColor fournis par le capteur
 @param      referenceNumber La valeur théorique qui sera utilisée comme abscisse sur les graphiques.
 Par exemple, dans le cas d'une mesure de luminance, il s'agira de la valeur théorique en IRE.
 @param      dataType Le type de mesure assocée aux résultats.
 */
-(void) addMeasure:(HCFRColor*)value withReferenceNumber:(NSNumber*)referenceNumber forType:(HCFRDataType)dataType;
/*!
 @function 
 @abstract   Enregistre un résultat de mesure continue.
 @discussion Ajoute une nouvelle mesure pour le type kContinuousMeasuresType.
 Le stockage est glissant : lorsque le tableau est plein, la première valeur est supprimée.
 @param      value La valeur HCFRColor fournis par le capteur
 @param      referenceNumber La valeur théorique qui sera utilisée comme abscisse sur les graphiques.
 Par exemple, dans le cas d'une mesure de luminance, il s'agira de la valeur théorique en IRE.
 @param      dataType Le type de mesure assocée aux résultats.
 */
-(void) appendContinuousMeasure:(HCFRColor*)value;
/*!
  @function 
   @abstract   Efface toutes les données pour le type fournis
   @discussion Efface toutes les mesures stockées pour le type fournis, afin de commencer une nouvelle série de mesures.
   @param      dataType Le type de mesure pour lequel il faut effacer les données
   */
-(void) clearForDataType:(HCFRDataType)dataType;

  /*!
  @function 
   @abstract   Supprime toutes les données stockées
   @discussion Cette fonction supprime toutes les données enregistrées dans le data store,
   quelque soit leur type. Cette manipulation est irreversible.
   */
-(void) clearStore;
#pragma mark accès aux mesures
  /*!
  @function 
   @abstract   Retourne ne nombre d'élements dans le dataStore
   @discussion Cette fonction retourne le nombre total de mesures stockées dans le dataStore.
   les données "colorReference" et "gamma" ne sont en toute logique pas comptés.
   */
-(int) count;
  /*!
  @function 
   @abstract   Compte le nombre d'entrées pour le type fournis.
   @discussion Cette fonction compte le nombre d'entrées enregistrées pour le type de données fournis.
   @param      dataType Le type de donnée
   @result     Le nombre d'entrée pour le type fournis
   */
-(int) countForType:(HCFRDataType)dataType;
  /*!
  @function 
   @abstract   Retourne une entrée du dataStore
   @discussion Cette fonction retourne l'entrée enregistrée pour un type de donnée et un index donné.
   Si l'index est supperieur au nombre d'entrées pour ce type -1, une exception est levée.
   @param      index L'index de l'entrée à fournir
   @param      dataType Le type de donnée recherché.
   @result     L'entrée du datastore. Si cette variable doit être concervée, il est necessaire de la retenir (retain).
   */
-(HCFRDataStoreEntry*) entryAtIndex:(int)index forType:(HCFRDataType)dataType;
  /*!
  @function 
   @abstract   Retourne l'entrée la plus élevée du tableau.
   @discussion Cette fonction retourne l'entrée dont la valeur de référence est la plus élevée
   (la dernière valeur du tableau d'entrées du type). Ce n'est pas forcément la dernière valeur ajoutée !
   @param      dataType Le type de donnée recherché.
   @result     L'entrée du datastore demandée
   */
-(HCFRDataStoreEntry*) lastEntryForType:(HCFRDataType)dataType;
  /*!
  @function 
   @abstract   Retourne une entrée pour un type et une valeur de référence donnée.
   @discussion Cette fonction recherche parmis toutes les entrées d'un type donné et retourne
   la première dont la valeur de référence correspond à celle demandée.
   La fonction retourne nil si aucune entrée n'est trouvée.
   @param      reference La valeur de référence à rechercher. Ce paramètre est un entier, car une égalité parfaite
   entre deux float est franchement improbable.
   @param      dataType Le type de donnée recherché.
   @result     L'entrée trouvée, ou nil si aucune entrée n'est trouvée.
   */
-(HCFRDataStoreEntry*) entryWithReferenceValue:(int)reference forType:(HCFRDataType)dataType;

#pragma mark Gestion des données aditionnelles
-(void) setNotes:(NSAttributedString*) newNotes; // <- utilisée par un binding sur le champ note
-(NSAttributedString*) notes; // <- utilisée par un binding sur le champ note
-(void) setDirty:(BOOL)newDirtyStatus;
/*!
    @function 
    @abstract   Indique si la série à été modifiée depuis le dernier appel à encodeWithCoder
    @result     Un boolean qui indique si la série à été modifiée depuis le dernier appel à encodeWithCoder
*/
-(BOOL) dirty;

#pragma mark Gestion des listeners
  /*!
  @function 
   @abstract   Ajoute un listener pour le type donné
   @discussion Ajoute l'objet newListener commme listener sur le données du type dataType.
   L'objet sera retenu jusqu'a ce qu'il soit supprimé de la liste des listeners par un appel à la fonction
   removeLister:forType:
   @param      newListener L'objet qui doit être ajouté comme listener
   @param      dataType Le type de données pour lequel l'objet doit être enregistré
   */
-(void) addListener:(NSObject<HCFRDataSerieListener>*)newListener forType:(HCFRDataType)dataType;
  /*!
  @function 
   @abstract   Supprime un listerner pour le type donné
   @discussion Supprime l'objet oldListener de la liste des listenenrs pour les données dy type dataType.
   l'objet sera libéré à cette occasion. Si l'objet n'a pas été enregistré au préalable, la fonction
   ne fait rien.
   @param      oldListener L'objet à supprimer de la liste des listeners
   @param      dataType Le type de données pour lequel l'objet doit être retiré
   */
-(void) removeListener:(NSObject<HCFRDataSerieListener>*)oldListener forType:(HCFRDataType)dataType;


@end
