//  ColorHCFR
//  HCFRSensorUpdater.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 29/07/08.
//
//  $Rev: 96 $
//  $Date: 2008-08-19 12:35:23 +0100 (Tue, 19 Aug 2008) $
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

#import <IOKit/usb/IOUSBLib.h>

#import <HCFRPlugins/HCFRTool.h>
#import <HCFRPlugins/HCFRUtilities.h>

#import "HCFRIntelHexRecord.h"

/*!
@class
@abstract    (brief description)
@discussion  (comprehensive description)
*/
@interface HCFRSensorUpdater : HCFRTool {
    IBOutlet NSWindow             *mainWindow;
    
    IBOutlet NSView               *step1;
    IBOutlet NSView               *step2;
    IBOutlet NSTextField          *filenameTextField;
    IBOutlet NSTextField          *step2Status;
    IBOutlet NSButton             *updateButton;
    IBOutlet NSView               *step3;
    IBOutlet NSProgressIndicator  *progressIndicator;
    IBOutlet NSView               *success;
    IBOutlet NSView               *failure;
    
    HCFRIntelHexRecord            *record;
    
    // variable pour la gestion des callback sur evenements USB
    IONotificationPortRef	notify_port;
    io_iterator_t         device_added_iterator;
    io_iterator_t         device_removed_iterator;
    
    // variable pour la gestion du périphérique USB
    IOUSBDeviceInterface  **device;
    IOUSBInterfaceInterface	**interface;
    
    BOOL                  toolStarted;
    BOOL                  updateRunning; // pour bloquer la fermeture de la fenêtre pendant la MAJ
    BOOL                  updateDone;
}
- (IBAction) performUpdate:(id)sender;
- (void) performThreadedUpdate:(id)object;
- (IBAction) selectFile:(id)sender;
- (IBAction) visitForums:(id)sender;

#pragma mark Window delegate
- (BOOL)windowShouldClose:(id)window;
- (void)windowWillClose:(NSNotification *)notification;
@end
