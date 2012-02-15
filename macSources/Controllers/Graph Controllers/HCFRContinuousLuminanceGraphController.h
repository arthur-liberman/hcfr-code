//  ColorHCFR
//  HCFRContinuousLuminanceGraphController.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 24/01/11.
//
//  $Rev$
//  $Date$
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

#import "SM2DGraphView/SM2DGraphView.h"
#import <HCFRPlugins/HCFRDataSerieListener.h>
#import <HCFRPlugins/HCFRUtilities.h>
#import "HCFRContinuousMeasuresGraphController.h"

/*!
 @class
 @abstract    Controller pour le graph de mesures continues "luminance"
 @discussion  Cette classe est l'interface entre le data store et le graphique de mesures continue de luminance.
 Elle s'enregistre comme listener auprès du dataStore, puis à chaque modification des données, les transforme pour
 fournir au graph les valeurs à tracer.
 */
@interface HCFRContinuousLuminanceGraphController : HCFRContinuousMeasuresGraphController {
  SM2DGraphView           *graphView;
  
  NSMutableArray          *graphValues;
  NSMutableArray          *redValues;
  NSMutableArray          *greenValues;
  NSMutableArray          *blueValues;
  
  // Le maximum et le minimum pour l'axe Y est conservé dans cette variable une fois calculé.
  double                  yAxisMaxValue, yAxisMinValue;
  // si on mesure une valeur maximum supperieur au maximum de l'axe Y,
  // cette valeur supplentera le maximum de l'échelle tant qu'elle sera affichée.
  double                  maxDisplayedValue, minDisplayedValue;
}
-(HCFRContinuousLuminanceGraphController*) init;

#pragma mark Surcharges des methodes de HCFRGraphController
-(NSString*) graphicName;
-(BOOL) hasAnOptionsView;
-(NSView*) graphicView;

#pragma mark Implémentation de l'interface DataSource pour le graphique
- (unsigned int)numberOfLinesInTwoDGraphView:(SM2DGraphView *)inGraphView;
- (NSArray *)twoDGraphView:(SM2DGraphView *)inGraphView dataForLineIndex:(unsigned int)inLineIndex;
- (double)twoDGraphView:(SM2DGraphView *)inGraphView maximumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis;
- (double)twoDGraphView:(SM2DGraphView *)inGraphView minimumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis;

#pragma mark fonctions internes
-(void) computeCurrentSerieData;
-(void) computeDataForSerie:(HCFRDataSerie*)currentSerie
                toDataArray:(NSMutableArray*)graphValuesArray
               redDataArray:(NSMutableArray*)redValuesArray
             greenDataArray:(NSMutableArray*)greenValuesArray
              blueDataArray:(NSMutableArray*)blueValuesArray;
-(float) getCurrentGamma;

// Fonctions utilitaires, pour la phase de dev
/*!
 @function 
 @abstract   retourne la valeur du gamma à appliquer
 @discussion Cette fonction est la en attendant de savoir commen on vas gérer le gamma.
 A priori, à terme, le gamma sera stocké par le dataStore (vu qu'il s'applique à toutes les mesures enregistrées).
 @result     la valeur du gamma à appliquer
 */
-(float) getCurrentGamma;

#pragma mark IB actions
/*-(IBAction) showReferenceChanged:(NSButton*)sender;
 -(IBAction) showLuminosityChanged:(NSButton*)sender;
 -(IBAction) showRedChanged:(NSButton*)sender;
 -(IBAction) showGreenChanged:(NSButton*)sender;
 -(IBAction) showBlueChanged:(NSButton*)sender;
 -(IBAction) compensateChanged:(NSButton*)sender;*/
@end
