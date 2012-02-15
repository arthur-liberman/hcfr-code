//  ColorHCFR
//  HCFRAditionnalFramesTool.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 27/08/08.
//
//  $Rev: 105 $
//  $Date: 2008-08-28 22:53:12 +0100 (Thu, 28 Aug 2008) $
//  $Author$
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
#import <HCFRplugins/HCFRTool.h>
#import <HCFRPlugins/HCFRFullScreenWindow.h>
#import "HCFRAditionnalFrameView.h"

@interface AditionalFrameWindow : HCFRFullScreenWindow
{}
@end

/*!
    @class
    @abstract    L'outil d'affichage de mires supplémentaires
*/
@interface HCFRAditionnalFramesTool : HCFRTool {
  IBOutlet NSWindow       *mainWindow;
  IBOutlet NSPopUpButton  *screensPopup;
  IBOutlet NSPopUpButton  *framesPopup;
  IBOutlet NSTextView     *descriptionTextView;
  
  AditionalFrameWindow    *fullscreenWindow;
  HCFRAditionnalFrameView *currentView;
  
  
}
-(IBAction) displayFrame:(id)sender;
-(IBAction) frameChanged:(id)sender;

-(void) setupFrame:(HCFRAditionnalFrameView*)newFrame;
@end
