//  ColorHCFR
//  HCFRIrProfilesRepositoryListener.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 05/05/08.
//
//  $Rev: 33 $
//  $Date: 2008-05-05 21:30:42 +0100 (Mon, 05 May 2008) $
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

/*!
@protocol
@abstract    Protocol permettant à une classe de s'inscrire auprès du IRProfilesRepository comme listener
@discussion  Une classe implémantant ce protocole pourra s'inscrire auprès du repository pour être
tenu au courant des modifications sur la liste des profiles IR.
*/
@protocol HCFRIrProfilesRepositoryListener
/*!
@function 
 @abstract    La liste des profiles à changée
 @discussion  Cette fonction est également appelée lorsqu'un profile est modifié.
 */
-(void) irProfilesListChanged;
@end