//  ColorHCFR
//  HCFRLuminanceGraphController.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 07/09/07.
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

#import "HCFRRgbGraphController.h"

typedef enum
{
  kRGBGraphRedLine = 0,
  kRGBGraphGreenLine = 1,
  kRGBGraphBlueLine = 2,
  kRGBGraphReferenceSerieRedLine = 3,
  kRGBGraphReferenceSerieGreenLine = 4,
  kRGBGraphReferenceSerieBlueLine = 5,
  kRGBGraphReferenceLine = 6
} RGBGraphLinesId;

@implementation HCFRRgbGraphController
-(HCFRRgbGraphController*) init
{
  [super init];
  
  graphView = [[SM2DGraphView alloc] init];
  redGraphValues = [[NSMutableArray alloc] initWithCapacity:50];
  greenGraphValues = [[NSMutableArray alloc] initWithCapacity:50];
  blueGraphValues = [[NSMutableArray alloc] initWithCapacity:50];
    
  referenceSerieRedGraphValues = [[NSMutableArray alloc] initWithCapacity:50];
  referenceSerieGreenGraphValues = [[NSMutableArray alloc] initWithCapacity:50];
  referenceSerieBlueGraphValues = [[NSMutableArray alloc] initWithCapacity:50];
  
  // on construit la courbe de référence (ce qui n'est pas trop dur ...)
  NSString  *firstPoint = NSStringFromPoint(NSMakePoint(0,100));
  NSString  *lastPoint = NSStringFromPoint(NSMakePoint(100,100));
  referenceGraphValues = [[NSArray alloc] initWithObjects:firstPoint, lastPoint, nil];
  
  // les valeurs de min et de max
  // On les définie avant de règler le graph, car elles seront utilisées lors du reloadAttibutes
  maxDisplayedValue = 120;
  minDisplayedValue = 80;

  // configuration du graph
  [graphView setDataSource:self];
  [graphView setBackgroundColor:[NSColor blackColor]];
  [graphView setDrawsGrid:YES];
  [graphView setGridColor:[NSColor grayColor]];
  [graphView setNumberOfTickMarks:11 forAxis:kSM2DGraph_Axis_X];
  [graphView setNumberOfTickMarks:11 forAxis:kSM2DGraph_Axis_Y];
  [graphView setLabel:@"IRE" forAxis:kSM2DGraph_Axis_X];
  [graphView setLabel:@"%" forAxis:kSM2DGraph_Axis_Y];
  [graphView reloadAttributes];
  [graphView reloadData];
    
  return self;
}

-(void) dealloc
{
  if ([self currentSerie] != nil)
    [[self currentSerie] removeListener:self forType:kLuminanceDataType];

  [graphView release];
  
  [redGraphValues release];
  [greenGraphValues release];
  [blueGraphValues release];
  
  [referenceSerieRedGraphValues release];
  [referenceSerieGreenGraphValues release];
  [referenceSerieBlueGraphValues release];
    
  [referenceGraphValues release];
  
  [super dealloc];
}

#pragma mark Surcharges des methodes de HCFRGraphController
-(GraphType) graphicType
{
  return kCalibrationGraph;
}
-(NSString*) graphicName
{
  return @"RGB";
}
-(BOOL) hasAnOptionsView
{
  return NO;
}
-(NSView*) graphicView
{
  return graphView;
}

#pragma mark fonctions de calcul
-(void) computeCurrentData
{
  [self computeDataForSerie:[self currentSerie]
                 toRedArray:redGraphValues
                 greenArray:greenGraphValues
                  blueArray:blueGraphValues];
} 
-(void) computeReferenceData
{
  [self computeDataForSerie:[self referenceSerie]
                 toRedArray:referenceSerieRedGraphValues
                 greenArray:referenceSerieGreenGraphValues
                  blueArray:referenceSerieBlueGraphValues];
}
-(void) computeDataForSerie:(HCFRDataSerie*)serie
                 toRedArray:(NSMutableArray*)redArray
                 greenArray:(NSMutableArray*)greenArray
                  blueArray:(NSMutableArray*)blueArray
{
  // On ne peut pas se contenter d'ajouter le point au tableau :
  // il est possible que les valeurs ne viennent pas en ordre croissant,
  // et il est possible qu'une nouvelle valeur arrive pour un point
  // déja enregistré.
  // On pourrait ajouter dans le tableau en triant, mais le gain en perf
  // serait probablement inninterressant par rapport à l'effort fourni.
  // Donc, on recalcule toutes les données à chaque fois.
  
  int     nbOfEntries = [serie countForType:kLuminanceDataType];
  
  [redArray removeAllObjects];
  [greenArray removeAllObjects];
  [blueArray removeAllObjects];
  
  if (serie == nil)
    return;
    
  for (int index=0; index<nbOfEntries; index++)
  {
    HCFRDataStoreEntry  *entry = [serie entryAtIndex:index
                                                 forType:kLuminanceDataType];
    
    // on calcul la quantité de chaque primaire par rapport aux autres
    CColor measureXyY=[[entry value] xyYColor];
    CColor normColor;
    
    normColor[0]=(measureXyY[0]/measureXyY[1]);
    normColor[1]=(1.0);
    normColor[2]=((1.0-(measureXyY[0]+measureXyY[1]))/measureXyY[1]);
    
    // puis on converti le résultat en RGB pour affichage
    CColor tempColor(normColor);
    normColor=tempColor.GetRGBValue([[self colorReference] cColorReference]);
    
    [redArray addObject:NSStringFromPoint(NSMakePoint([[entry referenceNumber] doubleValue], normColor[0]*100))];
    [greenArray addObject:NSStringFromPoint(NSMakePoint([[entry referenceNumber] doubleValue], normColor[1]*100))];
    [blueArray addObject:NSStringFromPoint(NSMakePoint([[entry referenceNumber] doubleValue], normColor[2]*100))];
    
    // on met à jour le min et le max
    for (int i=0; i<3; i++)
    {
      maxDisplayedValue = fmax(maxDisplayedValue, normColor[i]*100);
      minDisplayedValue = fmin(minDisplayedValue, normColor[i]*100);
    }
  }
}

#pragma mark Surcharge des fonctions de listener de HCFRGraphController
-(void) entryAdded:(HCFRDataStoreEntry*)newEntry toSerie:(HCFRDataSerie*)serie forDataType:(HCFRDataType)dataType
{
  if (serie == [self currentSerie])
  {
    [self computeCurrentData];
    [graphView reloadData];
  }
}
-(void) serie:(HCFRDataSerie*)serie changedForDataType:(HCFRDataType)dataType
{
  if (serie == [self currentSerie])
  {
    [self computeCurrentData];
    [graphView reloadData];
  }
}
-(void) colorReferenceChanged:(HCFRColorReference*)oldReference
{
  [self computeCurrentData];
  [self computeReferenceData];
  [graphView reloadData];
}
-(void) currentDataSerieChanged:(HCFRDataSerie*)oldSerie
{
  if (oldSerie != nil)
    [oldSerie removeListener:self forType:kLuminanceDataType];
  if ([self currentSerie] != nil)
    [[self currentSerie] addListener:self forType:kLuminanceDataType];
  
  [self computeCurrentData];
  [graphView reloadData];
}
-(void) referenceDataSerieChanged:(HCFRDataSerie*)oldSerie
{
  [self computeReferenceData];
  [graphView reloadData];
}

#pragma mark dataSource pour le graphique
- (unsigned int)numberOfLinesInTwoDGraphView:(SM2DGraphView *)inGraphView
{
  return 7;
}

- (NSDictionary *)twoDGraphView:(SM2DGraphView *)inGraphView attributesForLineIndex:(unsigned int)inLineIndex
{
  switch (inLineIndex)
  {
    case kRGBGraphRedLine :
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor redColor], NSForegroundColorAttributeName,
                                                        nil];
      break;
    case kRGBGraphGreenLine :
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor greenColor], NSForegroundColorAttributeName,
                                                        nil];
      break;
    case kRGBGraphBlueLine :
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor blueColor], NSForegroundColorAttributeName,
                                                        nil];
      break;
    case kRGBGraphReferenceSerieRedLine :
      return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor redColor] colorWithAlphaComponent:0.2], NSForegroundColorAttributeName,
        nil];
      break;
    case kRGBGraphReferenceSerieGreenLine :
      return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor greenColor] colorWithAlphaComponent:0.2], NSForegroundColorAttributeName,
        nil];
      break;
    case kRGBGraphReferenceSerieBlueLine :
      return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor blueColor] colorWithAlphaComponent:0.2], NSForegroundColorAttributeName,
        nil];
      break;
    case kRGBGraphReferenceLine :
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor whiteColor], NSForegroundColorAttributeName,
                                                        [NSNumber numberWithInt:kSM2DGraph_Width_Fine], SM2DGraphLineWidthAttributeName,
                                                        nil];
      break;
  }

  NSAssert(NO, @"RGBGraph : attributes for graph -> unhandled line index !");
  
  return nil;
}

- (NSArray *)twoDGraphView:(SM2DGraphView *)inGraphView dataForLineIndex:(unsigned int)inLineIndex
{
  switch (inLineIndex)
  {
    case kRGBGraphRedLine :
      return redGraphValues;
      break;
    case kRGBGraphGreenLine :
      return greenGraphValues;
      break;
    case kRGBGraphBlueLine :
      return blueGraphValues;
      break;
    case kRGBGraphReferenceSerieRedLine :
      return referenceSerieRedGraphValues;
      break;
    case kRGBGraphReferenceSerieGreenLine :
      return referenceSerieGreenGraphValues;
      break;
    case kRGBGraphReferenceSerieBlueLine :
      return referenceSerieBlueGraphValues;
      break;
    case kRGBGraphReferenceLine :
      return referenceGraphValues;
      break;
  }

  NSAssert(NO, @"RGBGraph : data for graph -> unhandled line index !");

  return nil;
}

- (double)twoDGraphView:(SM2DGraphView *)inGraphView maximumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis
{
  if (inAxis == kSM2DGraph_Axis_X)
    return 100;
  else
  {
    // on utilise une échelle fixe, pour éviter les problèmes d'échelle délirante quand on a un point "hors normes"
    return 200.0;
    // on calcul la distance du point le plus éloigné de la courbe (+5),
    // et on utilisera cette valeur pour calculer le max et le min, ce qui 
    // permet de concerver le 100% centré verticalement.
//    double distance = fmax(100-minDisplayedValue+5, maxDisplayedValue-100+5);
//    distance = ((int)distance/5) * 5 + 5; // on arrondie à 5
//    return ceil(fmax (120, 100+distance));
  }
  
  return 0;
}
- (double)twoDGraphView:(SM2DGraphView *)inGraphView minimumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis;
{
  if (inAxis == kSM2DGraph_Axis_X)
    return 0;
  else
  {
    // on utilise une échelle fixe (cf valeur max)
    return 0.0;
    // on calcul la distance du point le plus éloigné de la courbe (+5),
    // et on utilisera cette valeur pour calculer le max et le min, ce qui 
    // permet de concerver le 100% centré verticalement.
//    double distance = fmax(100-minDisplayedValue+5, maxDisplayedValue-100+5);
//    distance = ((int)distance/5) * 5 + 5; // on arrondie à 5
//    return floor(fmin (80, 100-distance));
  }
  
  return 0;
}
@end
