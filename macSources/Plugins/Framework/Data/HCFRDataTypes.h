//  ColorHCFR
//  HCFRDataTypes.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 11/09/07.
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

/*!
 @typedef 
 @abstract   Type de mesure
 @discussion Ce nouveau type est utilisé pour identifier les différents types de mesures.
 Il est défini afin d'amméliorer la lisibilité du programme, et est mis dans un fichier header
 à part, car le mettre dans HCFRDataStore.h aurait créé des dépendances circulaires entre HCFRDataStore
 et HCFRDataStoreListener.
*/
typedef enum {
  kLuminanceDataType,
  kFullScreenContrastDataType,
  kANSIContrastDataType,
  kRedSaturationDataType,
  kGreenSaturationDataType,
  kBlueSaturationDataType,
  
  kComponentsDataType,
  
//  kReferenceColorDataType, // il n'y a pas de dictionnaire associé à ces types
//  kGammaDataType,
  
  kContinuousDataType, // pour les mesures continues
  kUndefinedDataType // pour les cas d'erreur
} HCFRDataType;

/*!
@typedef 
 @abstract   Les valeurs des références pour les composantes
 @discussion Le DataStore associe les mesures à une valeur de référence.
 Pour la luminosité ou la saturation, pas de problème, c'est la valeur est le nombre d'IRE ou
 le pourcentage de saturation. Pour les composantes, c'est un peu plus complexe : il n'y a pas de valeurs logiques.
 Ce nouveau type énuméré défini les valeurs de référence pour le type de données kComponentsDataType
 ATTENTION : les valeurs de ce type de doivent pas être changées : elles sont considérées comme fixes par les générateurs !
 C'est pas bien, mais c'est comme ça. Donc, en cas de modif, il faudra reprendre les générateurs.
 */
typedef enum {
  kRedComponent=0,
  kGreenComponent=1,
  kBlueComponent=2,
  kCyanComponent=3,
  kMagentaComponent=4,
  kYellowComponent=5
} ComponenttReferenceValue;