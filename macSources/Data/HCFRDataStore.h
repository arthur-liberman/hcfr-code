//  ColorHCFR
//  HCFRDataStore.h
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
#import <HCFRPlugins/HCFRDataSerie.h>


@interface HCFRDataStore : NSObject <NSCoding> {
  NSMutableArray      *series;
  
  HCFRDataSerie       *currentSerie;
  HCFRDataSerie       *referenceSerie;
  
  float               gamma;
  HCFRColorReference  *colorReference;
  
  NSString            *filename; // le nom de fichier associé au dataStore
  
  BOOL                dirty; // indique si le dataStore doit être sauvegardé ou non.
}
-(int) countSeries;
-(NSArray*) series;
-(HCFRDataSerie*)serieAtIndex:(int)index;

/*!
    @function
    @abstract   Retourne l'index de la série sélectionnée
    @discussion Retourne l'index de la série sélectionnée si il y en a une, et NSNotFound sinon.
    @result     L'index ou NSNotFound.
*/
-(HCFRDataSerie*) currentSerie;
-(int) currentSerieIndex;
-(void) setCurrentSerieIndex:(int)index;
-(HCFRDataSerie*) referenceSerie;
/*!
    @function 
    @abstract   Fixe la série de référence à partir de son index dans la liste.
    @discussion Si l'index passé est -1, on n'a plus de série de référence
    @param      index L'inde de la série à sélectionner, -1 pour supprimer la référence.
*/
-(void) setReferenceSerieIndex:(int)index;

/*!
    @function 
    @abstract   Ajoute une nouvelle série, et la séléctionne comme série courante
    @param      selectTheNewSerie Faut-il sélectionner la nouvelle série ?
    @result     L'index de la série ajoutée
*/
-(int) addNewSerie:(BOOL)selectTheNewSerie;
/*!
    @function 
    @abstract   Ajoute la série founie, et la sélectionne comme série courante
    @param      selectTheNewSerie Faut-il sélectionner la nouvelle série ?
    @result     L'index de la série ajoutée
*/
-(int) addSerie:(HCFRDataSerie*)newSerie andSelectIt:(BOOL)selectTheNewSerie;
/*!
    @function 
    @abstract   Supprime la série à l'index donné
    @discussion Si la série est référence, il n'y a plus de reference après la suppression.
 Si la série est la série courante, une nouvelle série courante est choisie, et son index est retourné.
 Si il n'y à plus de série, on n'a plus de série courante.
    @param      index L'index de la série à effacer
    @result     L'index de la nouvelle série courante, -1 si il n'y a plus de série.
*/
-(int) removeSerieAtIndex:(int)index;

-(void) setColorReference:(HCFRColorReference*) newReference;
-(HCFRColorReference*) colorReference;
-(void) setGamma:(float) newGamma;
-(float) gamma;

-(NSString*) filename;
-(void) setFilename:(NSString*)newFileName;
-(void) setDirty:(BOOL)newDirtyStatus;
-(BOOL) dirty;
@end