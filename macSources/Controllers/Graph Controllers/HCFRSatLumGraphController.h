//  ColorHCFR
//  HCFRSatLumGraphController.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 25/09/07.
//
//  $Rev: 67 $
//  $Date: 2008-07-20 20:02:36 +0100 (Sun, 20 Jul 2008) $
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
    @abstract    Controlleur pour la vue "saturation/luminance
    @discussion  Cette classe est l'interface entre le data store et le graphique saturation luminance.
 Elle s'enregistre comme listener auprès du dataStore, puis à chaque modification des données, les transforme pour
 fournir au graph les valeurs à tracer.
*/
@interface HCFRSatLumGraphController : HCFRGraphController {
  SM2DGraphView   *graphView;

  // les tableaux de données
  NSArray  *redReferenceData;
  NSArray  *greenReferenceData;
  NSArray  *blueReferenceData;

  NSMutableArray  *redData;
  NSMutableArray  *greenData;
  NSMutableArray  *blueData;
  
  NSMutableArray  *referenceSerieRedData;
  NSMutableArray  *referenceSerieGreenData;
  NSMutableArray  *referenceSerieBlueData;
  
  // Les options
  BOOL            drawReferences; // affichage des références
  BOOL            drawRed;
  BOOL            drawGreen;
  BOOL            drawBlue;
}
-(HCFRSatLumGraphController*) init;

#pragma mark Fonctions de calcul des données d'affichage
-(void) computeReferenceData;
/*!
   @function 
   @abstract   Calcul les valeurs à tracer en fonction des valeurs mesurées
   @discussion Cette fonction permet de factoriser le code de calcul des valeurs à afficher
   pour les différentes couleurs.
   @param      serie La série de données à tracer. Peut être nil, ce qui effacera les données actuelles sans en calculer de nouvelles.
   @param      reference La valeur de luma de couleur dans la référence
   @param      dataType Le type de données à demander au dataStore
   @param      destination Le tableau dans lequel les valeurs à tracer seront stockées
*/
-(void) computeDataForSerie:(HCFRDataSerie*)serie withReference:(double)referenceLuma dataType:(HCFRDataType)dataType destinationArray:(NSMutableArray*)destination;

#pragma mark Surcharges des methodes de HCFRGraphController
-(NSString*) graphicName;
-(BOOL) hasAnOptionsView;
-(NSView*) graphicView;

#pragma mark Implémentation de l'interface DataSource pour le graphique
- (unsigned int)numberOfLinesInTwoDGraphView:(SM2DGraphView *)inGraphView;
//- (NSDictionary *)twoDGraphView:(SM2DGraphView *)inGraphView attributesForLineIndex:(unsigned int)inLineIndex;
- (NSArray *)twoDGraphView:(SM2DGraphView *)inGraphView dataForLineIndex:(unsigned int)inLineIndex;
- (double)twoDGraphView:(SM2DGraphView *)inGraphView maximumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis;
- (double)twoDGraphView:(SM2DGraphView *)inGraphView minimumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis;
@end
