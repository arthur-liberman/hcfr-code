//  ColorHCFR
//  HCFRSatLumGraphController.h
// -----------------------------------------------------------------------------
//  Created by J√©r√¥me Duquennoy on 25/09/07.
//
//  $Rev: 115 $
//  $Date: 2008-09-03 14:05:12 +0100 (Wed, 03 Sep 2008) $
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

#import <HCFRPlugins/HCFRUtilities.h>

#import "SM2DGraphView/SM2DGraphView.h"

#import "HCFRGraphController.h"
#import "HCFRDataStore.h"

/*!
    @class
    @abstract    Controlleur pour la vue "saturation/écarts"
    @discussion  Cette classe est l'interface entre le data store et le graphique saturation écarts.
 Elle s'enregistre comme listener auprès du dataStore, puis à chaque modification des données, les transforme pour
 fournir au graph les valeurs à tracer.
*/
@interface HCFRSatShiftGraphController : HCFRGraphController {
  IBOutlet NSView *optionsView;
  
  SM2DGraphView   *graphView1; // nom de variable à la con, mais je ne sais pas comment les nommer :-(
  SM2DGraphView   *graphView2; // nom de variable à la con, mais je ne sais pas comment les nommer :-(
  NSView          *graphsView; // deux graphs pour ce controller -> on crée une vue

  // le dictionnaire de tableaux de données
  // les index des tableaux sont stockés dans un enum privé
  NSMutableArray  *dataArrays;

  // Les max
  double maxShift, minShift, maxDelta;
}
-(HCFRSatShiftGraphController*) init;

#pragma mark Fonctions de calcul des données d'affichage
/*!
   @function 
   @abstract   Calcul les valeurs à tracer en fonction des valeurs mesurées
   @param      serie La série de données à tracer. Peut être nil, ce qui effacera les données actuelles sans en calculer de nouvelles.
   @param      dataType Le type de données à demander au dataStore
   @param      shiftArray Le tableau dans lequel les valeurs de décalage de la saturation seront stockées
   @param      shiftArray Le tableau dans lequel les valeurs de décalage de la saturation seront stockées
*/
-(void) computeDataForSerie:(HCFRDataSerie*)serie dataType:(HCFRDataType)dataType
         toShiftValuesArray:(NSMutableArray*)shiftArray andDeltaEArray:(NSMutableArray*)deltaEArray
     defaultSaturatedColor:(CColor)saturatedColor;

#pragma mark Surcharges des methodes de HCFRGraphController
-(NSString*) graphicName;
-(BOOL) hasAnOptionsView;
-(NSView*) optionsView;
-(NSView*) graphicView;

#pragma IBActions
-(IBAction) refreshGraph:(id)sender;

#pragma mark Implémentation de l'interface DataSource pour le graphique
- (unsigned int)numberOfLinesInTwoDGraphView:(SM2DGraphView *)inGraphView;
//- (NSDictionary *)twoDGraphView:(SM2DGraphView *)inGraphView attributesForLineIndex:(unsigned int)inLineIndex;
- (NSArray *)twoDGraphView:(SM2DGraphView *)inGraphView dataForLineIndex:(unsigned int)inLineIndex;
- (double)twoDGraphView:(SM2DGraphView *)inGraphView maximumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis;
- (double)twoDGraphView:(SM2DGraphView *)inGraphView minimumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis;
@end
