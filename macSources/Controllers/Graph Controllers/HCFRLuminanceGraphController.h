//  ColorHCFR
//  HCFRLuminanceGraphController.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 01/09/07.
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

#import "SM2DGraphView/SM2DGraphView.h"
#import <HCFRPlugins/HCFRDataSerieListener.h>
#import <HCFRPlugins/HCFRUtilities.h>
#import "HCFRGraphController.h"

typedef enum
{
  kLuminanceGraphNormalMode,          // affichage normal du graphique sur une echelle de 0-100 IRE
  kLuminanceGraphBlackProximityMode,  // affichage sur une echelle de 0-5 IRE
  kLuminanceGraphWhiteProximityMode   // affichage sur une echelle de 95-100 IRE
} HCFRLuminanceGraphMode;

/*!
    @class
    @abstract    Controller pour la vue "luminance"
    @discussion  Cette classe est l'interface entre le data store et le graphique de luminance.
 Elle s'enregistre comme listener auprès du dataStore, puis à chaque modification des données, les transforme pour
 fournir au graph les valeurs à tracer.
*/
@interface HCFRLuminanceGraphController : HCFRGraphController {
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
  
  HCFRLuminanceGraphMode  graphMode;
  
  // Les options
//  bool                    drawReference;
//  bool                    drawLuminosity;
//  bool                    drawRed;
//  bool                    drawGreen;
//  bool                    drawBlue;
  // l'option compensate permet de soustraire à chaque valeur la valeur du point noir.
//  bool                    compensate;
}
-(HCFRLuminanceGraphController*) init;

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
-(float) getCurrentGamma;
-(void) setGraphMode:(HCFRLuminanceGraphMode)newMode;

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
-(IBAction) displayModeChanged:(NSPopUpButton*)sender;
@end
