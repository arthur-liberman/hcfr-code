//  ColorHCFR
//  HCFRDataStoreIOCategory.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 26/12/07.
//
//  $Rev: 115 $
//  $Date: 2008-09-03 14:05:12 +0100 (Wed, 03 Sep 2008) $
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
#import <list>

#define kHCFRDataStoreSaveFailureException @"DataStoreSaveFailure"
#define kHCFRFileFormatException @"FileFormatException"

/*!
    @class
    @abstract    (brief description)
    @discussion  (comprehensive description)
*/
@interface HCFRDataSerie  (IO) <NSCoding>
/*!
    @function 
    @abstract   Liste des extensions gérées par la fonction importFile:
    @discussion Cette fonction retourne un tableau contenant les extensions gérées par la fonction
 importFile:.
    @result     Un tableau contenant les extensions sous forme de NSString*
*/
+(NSArray*) importableExtensions;
/*!
    @function 
    @abstract   Importe un fichier
    @discussion Importe un fichier. Le format du fichier est reconnu grâce à son extension, et la fonction adequate est appelée.
 Une exception kHCFRFileFormatException sera levée si le format n'est pas connu, et les exceptions générées par les fonctions de lecture
 sont transmises sans traitement.
    @param      filePath Le path du fichier à lire
*/
-(void) importFile:(NSString*)filePath;
/*!
 @function 
 @abstract   Charge les données à partir d'un fichier CSV
 @discussion Cette fonction permet de charger les données à fournir lors des mesures
 d'un fichier dont l'URL est passée en argument.
 Le fichier est un fichier CVS, dont chaque ligne est au format suivant :
 type, reference, x, y, z
 type est un entier, toutes les autres données dont des double.
 Attention, ce format perd des informations : les matrices de transformation présentes dans les
 instances de Color, sont perdues, seul les valeurs XYZ sont sauvegardées. La matrice de transformation
 est la matrice unité après chargement.
 @param      filePath L'URL du fichier à charger
 */
-(void) loadDataFromCSVFile:(NSString*)filePath;  

/*!
   @function 
   @abstract   Enregistre les données dans un fichier CSV
   @discussion Cette fonction permet de sauvegarder les données stockées dans le dataStore
   dans un fichier dont l'URL est passée en argument.
   Le fichier est un fichier CVS, dont chaque ligne est au format suivant :
   type, reference, x, y, z
   type est un entier, toutes les autres données dont des double.
   Attention, ce format perd des informations : les matrices de transformation présentes dans les
   instances de Color, sont perdues, seul les valeurs XYZ sont sauvegardées. La matrice de transformation
   est la matrice unité après chargement.
   @param      filePath L'URL du fichier à charger
*/
-(void) saveDataToCSVFile:(NSString*)filePath;

/*!
  @function 
  @abstract   Charge les données à partir d'un fichier CHC
  @discussion Cette fonction permet d'importer les données d'un fichier CHC enregistré par la version PC du colorHCFR.
 Cette fonction lève une exception en cas d'echec de la lecture du fichier.
  @param      filePath L'URL du fichier à charger
*/
-(void) loadDataFromCHCFile:(NSString*)filePath;
#pragma fonctions utilitaires
-(void) loadColorsList:(list<CColor>)list ofType:(HCFRDataType)dataType
   referenceMultiplier:(int)multiplier referenceShift:(int)shift;
-(void) loadColorsList:(list<CColor>)list ofType:(HCFRDataType)dataType
          mappingArray:(int[])mapping;
@end
