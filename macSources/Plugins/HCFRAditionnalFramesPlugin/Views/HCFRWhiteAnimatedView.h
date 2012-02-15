//
//  HCFRWhiteAnimatedView.h
//  ColorHCFR
//
//  Created by Jérôme Duquennoy on 27/08/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <HCFRPlugins/HCFRUtilities.h>
#import "HCFRAditionnalFrameView.h"

@interface HCFRWhiteAnimatedView : HCFRAditionnalFrameView {
  NSTimer   *animTimer;
  float     timeIndex; // pour savoir ou dessiner les bandes. Valeur incrémentée par timer, entre 0 et 1.
  int       level;
  int       step; 
}
-(HCFRWhiteAnimatedView*) initWithWhiteLevel:(int)level step:(int)step;
/*!
    @function 
    @abstract   "lisse" un index pour rendre un mouvement souple
    @discussion on fournie un index entre 0 et 1, la fonction retourne un index entre 0 et 1 qui suit une sinusoïde
    @param      index La position entre 0 et 1
    @result     la valeur lissée
*/
-(float) ease:(float)index;

/*!
 @function
 @abstract Le callback du timer d'animation
*/
- (void)animationTimerCallback:(NSTimer*)theTimer;
@end
