//  ColorHCFR
//  HCFRAboutWindowController.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 30/01/08.
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
// -----------------------------------------------------------------------------

#import <Cocoa/Cocoa.h>

@interface HCFRAboutWindowController : NSWindowController
{
    IBOutlet NSTextView   *creditsTextView;
    IBOutlet NSPanel      *licencePanel;
    IBOutlet NSTextView   *licenceTextView;
    IBOutlet NSTextField  *versionTextField;

    // Le défillement des remerciements
    NSTimer               *scrollTimer;
    NSTimer               *eventLoopScrollTimer;
    float                 scrollLocation;
    int                   maxScroll;
    float               	scrollRate;
}
+ (HCFRAboutWindowController *)sharedInstance;

- (IBAction)hideLicence:(id)sender;
- (IBAction)showLicence:(id)sender;
- (IBAction)visitWebsite:(id)sender;
@end
