//  ColorHCFR
//  HCFRDataStoreListener.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/08/07.
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

#import <HCFRPlugins/HCFRDataStoreEntry.h>
#import <HCFRPlugins/HCFRDataTypes.h>

@class HCFRDataSerie;

/*!
@protocol
@abstract    Protocol permettant à une classe de s'inscrire auprès du dataStore comme listener
@discussion  Une classe implémantant ce protocole pourra s'inscrire auprès du dataStore pour être
tenu au courant des modifications dans les données d'un type spécifié.
*/
@protocol HCFRDataSerieListener
/*!
    @function 
    @abstract    Une valeur à été ajoutée pour le type dataType
    @discussion  Une valeur à été ajoutée au dataStaore pour le type donné. La nouvelle entrée
 et le type associé sont fournis en argument
    @param       newEntry L'entrée qui à été ajoutée
    @param       dataType Le type de mesure qui à donné cette nouvelle entrée. Cette valeur est
 utile si l'objet s'est enregistré comme listener sur plusieurs types.
*/
-(void) entryAdded:(HCFRDataStoreEntry*)newEntry toSerie:(HCFRDataSerie*)serie forDataType:(HCFRDataType)dataType;
/*!
    @function 
    @abstract   La série à été modifiée
    @discussion Cette fonction sera appelée lorsque la série est modifiée, mais qu'il ne s'agit pas d'un
 simple ajout.
    @param       dataType Le type de mesure pour lequel une nouvelles série à commencée. Cette valeur est
 utile si l'objet s'est enregistré comme listener sur plusieurs types.
*/
-(void) serie:(HCFRDataSerie*)serie changedForDataType:(HCFRDataType)dataType;
@end