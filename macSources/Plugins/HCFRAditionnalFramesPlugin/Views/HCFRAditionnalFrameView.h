//
//  HCFRAditionnalFrameView.h
//  ColorHCFR
//
//  Created by Jérôme Duquennoy on 27/08/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface HCFRAditionnalFrameView : NSView {
  NSString      *frameDescription;
}
-(void) setFrameDescription:(NSString*)description;
-(NSString*) frameDescription;
-(void) startAnimation;
-(void) stopAnimation;
@end
