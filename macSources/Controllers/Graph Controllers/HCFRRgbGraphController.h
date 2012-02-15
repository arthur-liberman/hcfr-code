//  ColorHCFR
//  HCFRLuminanceGraphController.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 07/09/07.
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
#import "SM2DGraphView/SM2DGraphView.h"
#import "HCFRDataStore.h"
//#import "HCFRDataStoreListener.h"
#import "HCFRGraphController.h"

/*!
    @class
    @abstract    Controller pour la vue "RGB"
    @discussion  Cette classe est l'interface entre le data store et le graphique RGB.
 Elle s'enregistre comme listener auprès du dataStore, puis à chaque modification des données, les transforme pour
 fournir au graph les valeurs à tracer.
*/
@interface HCFRRgbGraphController : HCFRGraphController {
  SM2DGraphView   *graphView;
  
  NSMutableArray  *referenceGraphValues;
  NSMutableArray  *redGraphValues;
  NSMutableArray  *greenGraphValues;
  NSMutableArray  *blueGraphValues;
  NSMutableArray  *referenceSerieRedGraphValues;
  NSMutableArray  *referenceSerieGreenGraphValues;
  NSMutableArray  *referenceSerieBlueGraphValues;
  
  // pour pouvoir mettre simplement à jour l'echelle d'affichage, on mémorise les valeurs max et min à afficher
  double  maxDisplayedValue;
  double  minDisplayedValue;
}
-(HCFRRgbGraphController*) init;

#pragma mark Surcharges des methodes de HCFRGraphController
-(NSString*) graphicName;
-(BOOL) hasAnOptionsView;
//-(NSView*) optionsView; // pas de vue d'options pour le moment ...
-(NSView*) graphicView;

#pragma mark fonctions de calcul
-(void) computeCurrentData;
-(void) computeReferenceData;
-(void) computeDataForSerie:(HCFRDataSerie*)serie toRedArray:(NSMutableArray*)redArray greenArray:(NSMutableArray*)greenArray blueArray:(NSMutableArray*)blueArray;

#pragma mark Implémentation de l'interface DataSource pour le graphique
- (unsigned int)numberOfLinesInTwoDGraphView:(SM2DGraphView *)inGraphView;
//- (NSDictionary *)twoDGraphView:(SM2DGraphView *)inGraphView attributesForLineIndex:(unsigned int)inLineIndex;
- (NSArray *)twoDGraphView:(SM2DGraphView *)inGraphView dataForLineIndex:(unsigned int)inLineIndex;
- (double)twoDGraphView:(SM2DGraphView *)inGraphView maximumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis;
- (double)twoDGraphView:(SM2DGraphView *)inGraphView minimumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis;
@end
