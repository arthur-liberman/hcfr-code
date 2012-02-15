//
//  HCFRAditionnalFrameView.m
//  ColorHCFR
//
//  Created by Jérôme Duquennoy on 27/08/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "HCFRAditionnalFrameView.h"
#import <HCFRPlugins/HCFRUtilities.h>

@implementation HCFRAditionnalFrameView
- (id) init {
  self = [super init];
  if (self != nil) {
    frameDescription = [HCFRLocalizedString(@"No description available",
                                            @"No description available") retain];
  }
  return self;
}


-(void) setFrameDescription:(NSString*)description
{
  [frameDescription release];
  frameDescription = [description retain];
}
-(NSString*) frameDescription
{
  return frameDescription;
}

-(void) startAnimation
{}
-(void) stopAnimation
{}
@end
