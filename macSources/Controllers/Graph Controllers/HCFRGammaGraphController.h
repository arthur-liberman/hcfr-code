//  ColorHCFR
//  HCFRLuminanceGraphController.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 01/09/07.
//
//  $Rev: 119 $
//  $Date: 2008-09-15 14:26:39 +0100 (Mon, 15 Sep 2008) $
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

#import "SM2DGraphView/SM2DGraphView.h"
#import <HCFRPlugins/HCFRDataSerieListener.h>
#import <HCFRPlugins/HCFRUtilities.h>
#import "HCFRGraphController.h"

/*!
    @class
    @abstract    Controller pour la vue "gamma"
    @discussion  Cette classe est l'interface entre le data store et le graphique de gamma.
 Elle s'enregistre comme listener auprès du dataStore, puis à chaque modification des données, les transforme pour
 fournir au graph les valeurs à tracer.
*/
@interface HCFRGammaGraphController : HCFRGraphController {
  IBOutlet NSView         *optionsView;
  
  SM2DGraphView           *graphView;
  
  NSMutableArray          *referenceValues;

  NSMutableArray          *graphValues;
  NSMutableArray          *redValues;
  NSMutableArray          *greenValues;
  NSMutableArray          *blueValues;

  NSMutableArray          *referenceSerieGraphValues;
  NSMutableArray          *referenceSerieRedValues;
  NSMutableArray          *referenceSerieGreenValues;
  NSMutableArray          *referenceSerieBlueValues;
}
-(HCFRGammaGraphController*) init;

#pragma mark Surcharges des methodes de HCFRGraphController
-(NSString*) graphicName;
-(BOOL) hasAnOptionsView;
-(NSView*) optionsView;
-(NSView*) graphicView;

#pragma mark Implémentation de l'interface DataSource pour le graphique
- (unsigned int)numberOfLinesInTwoDGraphView:(SM2DGraphView *)inGraphView;
//- (NSDictionary *)twoDGraphView:(SM2DGraphView *)inGraphView attributesForLineIndex:(unsigned int)inLineIndex;
- (NSArray *)twoDGraphView:(SM2DGraphView *)inGraphView dataForLineIndex:(unsigned int)inLineIndex;
- (double)twoDGraphView:(SM2DGraphView *)inGraphView maximumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis;
- (double)twoDGraphView:(SM2DGraphView *)inGraphView minimumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis;

#pragma mark fonctions internes
-(void) computeCurrentSerieData;
-(void) computeReferenceSerieData;
-(void) computeDataForSerie:(HCFRDataSerie*)currentSerie
                 toDataArray:(NSMutableArray*)graphValuesArray
                redDataArray:(NSMutableArray*)redValuesArray
              greenDataArray:(NSMutableArray*)greenValuesArray
               blueDataArray:(NSMutableArray*)blueValuesArray;
-(void) updateReferenceValues;

#pragma mark IBActions
-(IBAction) refreshGraph:(id)sender;
@end
