//  ColorHCFR
//  HCFREngineDelegate.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 26/01/08.
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

#import <HCFRPlugins/HCFRColor.h>
#import <HCFRPlugins/HCFRDataStoreEntry.h>
#import <HCFRPlugins/HCFRDataTypes.h>

/*!
@protocol
@abstract    Protocol permettant à une classe de s'inscrire aupràs du calibrationEngine comme listener
@discussion  Une classe implémantant ce protocole pourra s'inscrire auprès du calibration engine pour être tenu
au courrant de l'état.
*/
@protocol HCFREngineDelegate
/*!
 @function 
 @abstract    Une série de mesure commence
 @discussion  Cette fonction sera appelée lorsque une série de mesure sera lancée.
*/
-(void) engineStartMeasures:(id)startingEngine;
/*!
   @function 
   @abstract    Fin des mesures
   @discussion  Cette fonction sera appelée lorsque une série de mesure sera achevée.
*/
-(void) engineMeasuresEnded:(id)endingEngine;
@end