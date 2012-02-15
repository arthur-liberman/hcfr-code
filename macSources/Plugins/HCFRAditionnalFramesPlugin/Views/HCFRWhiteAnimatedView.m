//
//  HCFRWhiteAnimatedView.m
//  ColorHCFR
//
//  Created by Jérôme Duquennoy on 27/08/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//


#import "math.h"
#import "HCFRWhiteAnimatedView.h"


@implementation HCFRWhiteAnimatedView

-(HCFRWhiteAnimatedView*) initWithWhiteLevel:(int)newLevel step:(int)newStep {
    self = [super init];
    if (self) {
      level = newLevel;
      step = newStep;
    }
    return self;
}
- (void) dealloc
{
  [super dealloc];
}

-(void) startAnimation
{
  if (animTimer != nil)
    return;
  animTimer = [[NSTimer scheduledTimerWithTimeInterval:0.05
                                                target:self
                                              selector:@selector(animationTimerCallback:)
                                              userInfo:nil
                                               repeats:YES] retain];
}
-(void) stopAnimation
{
  [animTimer invalidate];
  [animTimer release];
  animTimer = nil;
}


// sur un fond (100%), on dessine deux bandes, qui font au total 1/3 de la largeur de l'écran :
// - une bande à 99% de 1/9 de large
// - un espace de 1/9
// - une bande à 98% de 1/9
- (void)drawRect:(NSRect)rect
{
  const float   screenBandRatio = 9.0;
  NSRect        frame = [self frame];
  
  [[NSColor colorWithCalibratedWhite:level/100.0 alpha:1.0] set];
  [NSBezierPath fillRect:frame];
  
  // la position à laquelle on dessinne les bande est
  // une fraction des deux tiers de l'écran, qui varie selon le timeIndex.
  // La fonction ease permet d'avoir un mouvement plus "souple",
  float   bandPosition;
  NSRect  band;
  
  // la bande à 99%
  bandPosition = frame.size.width* (1.0- 3.0/screenBandRatio) * [self ease:timeIndex];
  band = NSMakeRect(bandPosition,0,frame.size.width/screenBandRatio,frame.size.height);
  [[NSColor colorWithCalibratedWhite:(level+step)/100.0 alpha:1.0] set];
  [NSBezierPath fillRect:band];
  // la bande à 98%
  bandPosition += 2*frame.size.width/screenBandRatio;
  band = NSMakeRect(bandPosition,0,frame.size.width/screenBandRatio,frame.size.height);
  [[NSColor colorWithCalibratedWhite:(level+2*step)/100.0 alpha:1.0] set];
  [NSBezierPath fillRect:band];
}

-(float) ease:(float)index
{
  return (sin(M_PI*2*index)+1)/2.0;
}

- (void)animationTimerCallback:(NSTimer*)theTimer
{
  timeIndex += 0.005;
  if (timeIndex > 1)
    timeIndex -= 1;
  [self setNeedsDisplay:YES];
}
@end
