//
//  HCFRLuminanceGraphAnalyser.h
//  ColorHCFR
//
//  Created by Jérôme Duquennoy on 26/02/09.
//  Copyright 2009 SII. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "HCFRGraphAnalyser.h"

@interface HCFRLuminanceGraphAnalyser : HCFRGraphAnalyser {

}

-(void) initWithValueArrays:(NSArray*)luminosityValues
             redValuesArray:(NSArray*)redValues
           greenValuesArray:(NSArray*)greenValues
            blueValuesArray:(NSArray*)blueValues
                targetGamma:(float)targetGamma;
-(void) valuesHasChanged;
@end
