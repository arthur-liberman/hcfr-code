//  ColorHCFR
//  HCFREmptyView.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 27/11/07.
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


/*!
    @class
    @abstract    Une NSView a afficher quand il n'y a rien à afficher ...
    @discussion  Cette vue contiend juste un label centré indiquant qu'il n'y a rien.
*/
@interface HCFREmptyView : NSView {
  NSTextField *theLabel;
}
+(HCFREmptyView*) emptyViewWithString:(NSString*)initialString;

-(HCFREmptyView*) init;
-(HCFREmptyView*) initWithString:(NSString*)theNewString;

-(void) setString:(NSString*)theNewString;
@end
