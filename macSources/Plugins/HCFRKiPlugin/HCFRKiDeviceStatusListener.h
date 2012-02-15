//  ColorHCFR
//  HCFRIrProfilesRepositoryListener.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 05/05/08.
//
//  $Rev: 122 $
//  $Date: 2008-11-18 15:05:33 +0000 (Tue, 18 Nov 2008) $
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
@abstract    Protocol permettant à une classe de s'inscrire auprès du kiDevice comme listener
@discussion  Une classe implémantant ce protocole pourra s'inscrire auprès du kiDevice pour être
tenu au courant des changement d'état du capteur (dispo / indispo)
*/
@protocol HCFRKiDeviceStatusListener
-(void) kiSensorAvailabilityChanged;
@end