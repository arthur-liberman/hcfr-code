//  ColorHCFR
//  HCFRAboutWindowController.mm
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 30/01/08.
//
//  $Rev: 135 $
//  $Date: 2011-02-27 10:48:51 +0000 (Sun, 27 Feb 2011) $
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
#import "HCFRAboutWindowController.h"

#define ABOUT_SCROLL_FPS	30.0
#define ABOUT_SCROLL_RATE	1.0

@interface HCFRAboutWindowController (Private)
- (void)windowDidLoad;
- (void)windowWillClose:(id)sender;
- (void)scrollTimer:(NSTimer *)scrollTimer;
@end

@implementation HCFRAboutWindowController

HCFRAboutWindowController *sharedController = nil;
+ (HCFRAboutWindowController *)sharedInstance
{
  if (sharedController == nil) {
    sharedController = [[self alloc] initWithWindowNibName:@"about"];
  }
  return sharedController;
}

#pragma mark window delegate
- (void)windowDidLoad
{
  NSAttributedString		*creditsString;
  
  //Credits
  creditsString = [[[NSAttributedString alloc] initWithPath:[[NSBundle mainBundle] pathForResource:@"Credits" ofType:@"rtf"]
                                         documentAttributes:nil] autorelease];
  [[creditsTextView textStorage] setAttributedString:creditsString];
  [[creditsTextView enclosingScrollView] setLineScroll:0.0];
  [[creditsTextView enclosingScrollView] setPageScroll:0.0];
	[[creditsTextView enclosingScrollView] setVerticalScroller:nil];
  
  //Start scrolling    
  scrollLocation = 0; 
  scrollRate = ABOUT_SCROLL_RATE;
  maxScroll = [[creditsTextView textStorage] size].height - [[creditsTextView enclosingScrollView] documentVisibleRect].size.height;
  scrollTimer = [[NSTimer scheduledTimerWithTimeInterval:(1.0/ABOUT_SCROLL_FPS)
                                                  target:self
                                                selector:@selector(scrollTimer:)
                                                userInfo:nil
                                                 repeats:YES] retain];

  //Setup the build date / version
  NSString *version = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];
  [versionTextField setStringValue:[NSString stringWithFormat:@"Color HCFR Mac v%@",version]];

  [[self window] center];
}
- (void)windowWillClose:(id)sender
{
  [sharedController autorelease];
  sharedController = nil;
  
  [scrollTimer invalidate];
  [scrollTimer release];
  scrollTimer = nil;
}

#pragma mark actions IB
- (IBAction)showLicence:(id)sender
{
	NSString	*licensePath = [[NSBundle bundleForClass:[self class]] pathForResource:@"License" ofType:@"txt"];
    [licenceTextView setString:[NSString stringWithContentsOfFile:licensePath encoding:NSUTF8StringEncoding error:nil]];
     	
	[NSApp beginSheet:licencePanel
	   modalForWindow:[self window]
      modalDelegate:nil
	   didEndSelector:nil
        contextInfo:nil];
}

- (IBAction)hideLicence:(id)sender
{
  [licencePanel orderOut:nil];
  [NSApp endSheet:licencePanel returnCode:0];
}

- (IBAction)visitWebsite:(id)sender
{
  [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://www.homecinema-fr.com"]];
}

#pragma mark Action du timer
- (void)scrollTimer:(NSTimer *)scrollTimer
{    
	scrollLocation += scrollRate;
	
	if (scrollLocation > maxScroll)
    scrollLocation = 0;    
	if (scrollLocation < 0)
    scrollLocation = maxScroll;
	
	[creditsTextView scrollPoint:NSMakePoint(0, scrollLocation)];
}

@end
