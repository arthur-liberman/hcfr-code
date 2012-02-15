//  ColorHCFR
//  HCFRIRProfilesManager.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 02/05/08.
//
//  $Rev: 109 $
//  $Date: 2008-08-31 00:36:58 +0100 (Sun, 31 Aug 2008) $
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
#import <HCFRPlugins/HCFRTool.h>

/*!
    @class
    @abstract    Un outil permettant de gérer les différents profiles IR
    @discussion  Cet outil permet d'ajouter des profiles IR à la liste des profiles fournis avec le logiciel
*/
@interface HCFRIRProfilesManager : HCFRTool {
  NSString          *factoryItem;
  NSString          *customItem;

  IBOutlet NSWindow       *mainWindow;
  IBOutlet NSOutlineView  *outlineView;
  // la partie code IR
  IBOutlet NSPopUpButton  *codesPopup;
  IBOutlet NSTextView     *codeTextField;
  IBOutlet NSTextField    *irCodeValidityTextField;
  // les infos
  IBOutlet NSTextField    *authorTextField;
  IBOutlet NSTextView     *descriptionTextField;
  //delays
  IBOutlet NSTextField    *menuNavigationDelayTextField;
  IBOutlet NSTextField    *menuSelectionDelayTextField;
  IBOutlet NSTextField    *chapterNavigationDelayTextField;
}

#pragma mark bindings functions
-(BOOL) isCurrentProfileEditable;
-(BOOL) isCurrentCodeTestable;

#pragma mark IBActions
-(IBAction) addProfile:(id)sender;
-(IBAction) deleteProfile:(id)sender;
-(IBAction) importProfile:(id)sender;
-(IBAction) selectedCodeChanged:(id)sender;
-(IBAction) learnCode:(id)sender;
-(IBAction) testCode:(id)sender;
-(IBAction) authorChanged:(id)sender;
-(IBAction) delayChanged:(id)sender;
@end
