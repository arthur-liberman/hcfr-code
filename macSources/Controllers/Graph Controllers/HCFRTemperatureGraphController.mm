//  ColorHCFR
//  HCFRTemperatureGraphController.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 14/01/08.
//
//  $Rev: 134 $
//  $Date: 2011-02-23 22:44:34 +0000 (Wed, 23 Feb 2011) $
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

#import "HCFRTemperatureGraphController.h"

@interface HCFRTemperatureGraphController (Private)
-(void) computeAllData;
-(void) computeCurrentSerieData;
-(void) computeReferenceSerieData;
@end

@implementation HCFRTemperatureGraphController
-(HCFRTemperatureGraphController*) init
{
  [super init];
  
  graphView = [[SM2DGraphView alloc] init];
  currentSerieGraphValues = [[NSMutableArray alloc] initWithCapacity:50];
  referenceSerieGraphValues = [[NSMutableArray alloc] initWithCapacity:50];
  
  // on prépare le tableau des valeurs de référence
  referenceValues = [[NSArray alloc] initWithObjects:NSStringFromPoint(NSMakePoint(0.0,6502.0)),
                                                     NSStringFromPoint(NSMakePoint(100.0,6502.0)),
                                                                       nil];
  
  // configuration du graph
  [graphView setDataSource:self];
  [graphView setBackgroundColor:[NSColor blackColor]];
  [graphView setDrawsGrid:YES];
  [graphView setGridColor:[NSColor grayColor]];
  [graphView setNumberOfTickMarks:11 forAxis:kSM2DGraph_Axis_X];
  [graphView setNumberOfTickMarks:11 forAxis:kSM2DGraph_Axis_Y];
  [graphView setLabel:@"IRE" forAxis:kSM2DGraph_Axis_X];
  [graphView setLabel:@"K" forAxis:kSM2DGraph_Axis_Y];
  [graphView reloadAttributes];
  [graphView reloadData];
  
  return self;
}

-(void) dealloc
{
  if ([self currentSerie] != nil)
    [[self currentSerie] removeListener:self forType:kLuminanceDataType];

  [graphView release];
  [currentSerieGraphValues release];
  [referenceSerieGraphValues release];
  [referenceValues release];
  
  [super dealloc];
}

#pragma mark Surcharges des methodes de HCFRGraphController
-(GraphType) graphicType
{
  return kCalibrationGraph;
}
-(NSString*) graphicName
{
  return HCFRLocalizedString(@"Temperature",@"Temperature");
}
-(BOOL) hasAnOptionsView
{
  return NO;
}
-(NSView*) graphicView
{
  return graphView;
}

#pragma mark dataSource pour le graphique
- (unsigned int)numberOfLinesInTwoDGraphView:(SM2DGraphView *)inGraphView
{
  if ([self referenceSerie] == nil)
    return 2;
  else
    return 3;
}

- (NSDictionary *)twoDGraphView:(SM2DGraphView *)inGraphView attributesForLineIndex:(unsigned int)inLineIndex
{
  if (inLineIndex == 0)
  {
    // la référence
    return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor grayColor], NSForegroundColorAttributeName,
      nil];
  }
  else if (inLineIndex == 1)
  {
    return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor cyanColor], NSForegroundColorAttributeName,
      nil];
  }
  else if (inLineIndex == 2)
  {
    return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor cyanColor] colorWithAlphaComponent:0.2], NSForegroundColorAttributeName,
      nil];
  }
  
  NSAssert(NO, @"HCFRTemperatureGraphController:twoDGraphView:AttributesForLineIndex: no dict returned !");
  return [NSDictionary dictionary];
}

- (NSArray *)twoDGraphView:(SM2DGraphView *)inGraphView dataForLineIndex:(unsigned int)inLineIndex
{
  if (inLineIndex == 0)
    return referenceValues;
  else if (inLineIndex == 1)
    return currentSerieGraphValues;
  else if (inLineIndex == 2)
    return referenceSerieGraphValues;

  NSAssert(NO, @"HCFRTemperatureGraphController:twoDGraphView:dataForLineIndex: no array returned !");
  return [NSArray array];
}
- (double)twoDGraphView:(SM2DGraphView *)inGraphView maximumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis
{
  if (inAxis == kSM2DGraph_Axis_X)
    return 100.0;
  else if (inAxis == kSM2DGraph_Axis_Y)
    return 9000.0;
  
  return 0; // on n'est jamais sencé passer la, c'est juste pour éviter un warning à la compilation
}
- (double)twoDGraphView:(SM2DGraphView *)inGraphView minimumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis;
{
  if (inAxis == kSM2DGraph_Axis_X)
    return 0.0;
  else if (inAxis == kSM2DGraph_Axis_Y)
    return 3500.0;

  return 0; // on n'est jamais sencé passer la, c'est juste pour éviter un warning à la compilation
}
@end

#pragma Category pour les fonctions privées
@implementation HCFRTemperatureGraphController (Private)
-(void) computeAllData
{
  [self computeCurrentSerieData];
  [self computeReferenceSerieData];
}
-(void) computeCurrentSerieData
{
  NSAssert ([self colorReference] != nil, @"HCFRTemperatureGraphController : cannot compute data with nil color reference");
  
  // on vire tout
  [currentSerieGraphValues removeAllObjects];
  
  if ([self currentSerie] == nil)
    return;
  
  // puis on remet les points
  int nbEntries = [[self currentSerie] countForType:kLuminanceDataType];
  int index;
  
  for (index = 0; index < nbEntries; index ++)
  {
    HCFRDataStoreEntry  *currentEntry = [[self currentSerie] entryAtIndex:index forType:kLuminanceDataType];
    NSPoint             newPoint;

    newPoint.x = [[currentEntry referenceNumber] doubleValue];
    newPoint.y = [[currentEntry value] temperatureWithColorReference:[self  colorReference]];

    [currentSerieGraphValues addObject:NSStringFromPoint(newPoint)];
  }
}
-(void) computeReferenceSerieData
{
  NSAssert ([self colorReference] != nil, @"HCFRTemperatureGraphController : cannot compute reference data with nil color reference");
  
  // on vire tout
  [referenceSerieGraphValues removeAllObjects];
  
  if ([self currentSerie] == nil)
    return;

  // puis on remet les points
  int nbEntries = [[self referenceSerie] countForType:kLuminanceDataType];
  int index;
  
  for (index = 0; index < nbEntries; index ++)
  {
    HCFRDataStoreEntry  *currentEntry = [[self referenceSerie] entryAtIndex:index forType:kLuminanceDataType];
    NSPoint             newPoint;
    
    newPoint.x = [[currentEntry referenceNumber] doubleValue];
    newPoint.y = [[currentEntry value] temperatureWithColorReference:[self  colorReference]];
    
    [referenceSerieGraphValues addObject:NSStringFromPoint(newPoint)];
  }
}
#pragma mark Surcharge des fonctions de listener de HCFRGraphController
-(void) entryAdded:(HCFRDataStoreEntry*)newEntry toSerie:(HCFRDataSerie*)serie forDataType:(HCFRDataType)dataType
{
  if (serie == [self currentSerie])
  {
    // On ne peut pas se contenter d'ajouter le point au tableau :
    // il est possible que les valeurs ne viennent pas en ordre croissant,
    // et il est possible qu'une nouvelle valeur arrive pour un point
    // déja enregistré.
    // On pourrait ajouter dans le tableau en triant, mais le gain en perf
    // serait probablement inninterressant par rapport à l'effort fourni.
    // Donc, on recalcule toutes les données à chaque fois.
    [self computeCurrentSerieData];
    [graphView reloadData];
  }
}
-(void) serie:(HCFRDataSerie*)serie changedForDataType:(HCFRDataType)dataType
{
  if (serie == [self currentSerie])
  {
    [self computeCurrentSerieData];
    [graphView reloadData];
  }
}
-(void) colorReferenceChanged:(HCFRColorReference*)oldReference
{
  [self computeAllData];
  [graphView reloadData];
}
-(void) currentDataSerieChanged:(HCFRDataSerie*)oldSerie
{
  if (oldSerie != nil)
    [oldSerie removeListener:self forType:kLuminanceDataType];
  if ([self currentSerie] != nil)
    [[self currentSerie] addListener:self forType:kLuminanceDataType];
  
  [self computeCurrentSerieData];
  [graphView reloadData];
}
-(void) referenceDataSerieChanged:(HCFRDataSerie*)oldSerie
{
  [self computeReferenceSerieData];
  [graphView reloadAttributes];
  [graphView reloadData];
}
@end
