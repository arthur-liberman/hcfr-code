//  ColorHCFR
//  HCFRMeasure.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 30/08/07.
//
//  $Rev: 51 $
//  $Date: 2008-05-22 13:48:37 +0100 (Thu, 22 May 2008) $
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

#import <HCFRPlugins/HCFRColor.h>
#import <HCFRPlugins/HCFRDataTypes.h>

/*!
@protocol
@abstract    Protocol s'appliquant aux classes capables de fournir des données de mesure
@discussion  Dans ce logiciel, plusieurs classes peuvent être ammenées à fournir des données.
             Ce sont par exemple les "drivers" de sonde ou les générateurs d'image, qui peuvent
             ainsi fournir la valeur attendue.
*/
@protocol HCFRDataSource
/*!
 @function 
 @abstract   Indique la prochaine valeur qui sera demandée
 @discussion Cette fonction n'a réellement d'intérêt que pour les capteurs virtuels, qui doivent
 retourner une valeur sans qu'il n'y ai de mesure effectuée. Pour les autres capteurs, il est innnutile
 d'en tenir compte.
 */
-(void) nextMeasureWillBeType:(HCFRDataType)type referenceValue:(NSNumber*)referenceValue;

/*!
    @function 
    @abstract   Retourne la mesure actuelle.
    @discussion L'objet retournée doit être autoreleasé par toutes les implémentations. Par conséquent,
 il faut retenir l'objet (retain) si on souhaite le concerver !
    @result     Une instance de HCFRColor représentant la dernière mesure.
*/
-(HCFRColor*) currentMeasure;
@end