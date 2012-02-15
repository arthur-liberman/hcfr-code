//  ColorHCFR
//  HCFRLuminanceGraphController.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 01/09/07.
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

#import "HCFRGammaGraphController.h"

const float referenceSerieCurvesAlpha = 0.2;

@implementation HCFRGammaGraphController
-(HCFRGammaGraphController*) init
{
  [super init];
  
  // on met les valeurs par defauts pour les prefs
  NSDictionary  *defaultPreferences = [NSDictionary dictionaryWithObjectsAndKeys:
                                          @"YES", @"GammaGraphDisplayReference",
                                          @"YES", @"GammaGraphDisplayLuminosity",
                                          @"YES", @"GammaGraphDisplayRed",
                                          @"NO", @"GammaGraphDisplayGreen",
                                          @"NO", @"GammaGraphDisplayBlue",
                                          nil];
                  
  [[NSUserDefaults standardUserDefaults] registerDefaults:defaultPreferences];
  
  // on charge le fichier nib qui contiend le panel d'options
  [NSBundle loadNibNamed:@"GammaGraph" owner:self];
  
  graphView = [[SM2DGraphView alloc] init];
  graphValues = [[NSMutableArray alloc] initWithCapacity:10];
  redValues = [[NSMutableArray alloc] initWithCapacity:10];
  greenValues = [[NSMutableArray alloc] initWithCapacity:10];
  blueValues = [[NSMutableArray alloc] initWithCapacity:10];

  referenceSerieGraphValues = [[NSMutableArray alloc] initWithCapacity:10];
  referenceSerieRedValues = [[NSMutableArray alloc] initWithCapacity:10];
  referenceSerieGreenValues = [[NSMutableArray alloc] initWithCapacity:10];
  referenceSerieBlueValues = [[NSMutableArray alloc] initWithCapacity:10];
  
  // on prépare le tableau des valeurs de référence
  referenceValues = [[NSMutableArray alloc] initWithCapacity:2];
  [self updateReferenceValues];
  
  // configuration du graph
  [graphView setDataSource:self];
  [graphView setBackgroundColor:[NSColor blackColor]];
  [graphView setDrawsGrid:YES];
  [graphView setGridColor:[NSColor grayColor]];
  [graphView setNumberOfTickMarks:11 forAxis:kSM2DGraph_Axis_X];
  [graphView setNumberOfTickMarks:11 forAxis:kSM2DGraph_Axis_Y];
  [graphView setLabel:@"IRE" forAxis:kSM2DGraph_Axis_X];
  [graphView setLabel:@"gamma" forAxis:kSM2DGraph_Axis_Y];
  [graphView reloadAttributes];
  [graphView reloadData];
  
  return self;
}

-(void) dealloc
{
  [graphView release];
  [graphValues release];
  [redValues release];
  [greenValues release];
  [blueValues release];

  [referenceSerieGraphValues release];
  [referenceSerieRedValues release];
  [referenceSerieGreenValues release];
  [referenceSerieBlueValues release];
  
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
  return HCFRLocalizedString(@"Gamma",@"Gamma");
}
-(BOOL) hasAnOptionsView
{
  return YES;
}
-(NSView*) optionsView
{
  return optionsView;
}
-(NSView*) graphicView
{
  return graphView;
}

#pragma mark Surcharge des fonctions de listener de HCFRGraphController
-(void) entryAdded:(HCFRDataStoreEntry*)newEntry toSerie:(HCFRDataSerie*)serie forDataType:(HCFRDataType)dataType
{
  if (serie == [self currentSerie])
  {
    [self computeCurrentSerieData];
    [graphView reloadData];
  }
}
-(void) serie:(HCFRDataSerie*)serie changedForDataType:(HCFRDataType)dataType
{
  if (serie == [self currentSerie])
  {
    [self computeReferenceSerieData];
    [graphView reloadData];
  }
}
-(void) colorReferenceChanged:(HCFRColorReference*)oldReference
{
//  [self computeCurrentSerieData];
//  [self computeReferenceSerieData];
//  [graphView reloadData];
}
-(void) gammaChanged:(float)oldGamma
{
  [self updateReferenceValues];
//  [self computeCurrentSerieData];
//  [self computeReferenceSerieData];
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
  // ça peut changer le nombre de courbes -> on recharge les attributs
  [graphView reloadAttributes];
  [graphView reloadData];
}

#pragma mark dataSource pour le graphique
- (unsigned int)numberOfLinesInTwoDGraphView:(SM2DGraphView *)inGraphView
{
  unsigned short  nbLines = 0;
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"GammaGraphDisplayReference"])
    nbLines ++;
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"GammaGraphDisplayLuminosity"])
  {
    nbLines ++;
    if ([self referenceSerie] != nil)
      nbLines ++;
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"GammaGraphDisplayRed"])
  {
    nbLines ++;
    if ([self referenceSerie] != nil)
      nbLines ++;
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"GammaGraphDisplayGreen"])
  {
    nbLines ++;
    if ([self referenceSerie] != nil)
      nbLines ++;
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"GammaGraphDisplayBlue"])
  {
    nbLines ++;
    if ([self referenceSerie] != nil)
      nbLines ++;
  }
  
  return nbLines;
}

- (NSDictionary *)twoDGraphView:(SM2DGraphView *)inGraphView attributesForLineIndex:(unsigned int)inLineIndex
{
  // L'index de la ligne dépend des données qui sont tracées.
  // On utilise la variable currentIndex, qu'on incrémente poru chaque type de données tracées
  unsigned short  currentIndex = 0;
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"GammaGraphDisplayReference"])
  {
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor grayColor], NSForegroundColorAttributeName,
//                                                        [NSNumber numberWithInt:kSM2DGraph_Width_Fine], SM2DGraphLineWidthAttributeName,
//                                                      [NSNumber numberWithInt:kSM2DGraph_Symbol_X], SM2DGraphLineSymbolAttributeName,
                                                        nil];
    currentIndex ++;
  }
  
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"GammaGraphDisplayLuminosity"])
  {
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor yellowColor], NSForegroundColorAttributeName,
                                                        nil];
    currentIndex ++;
    if ([self referenceSerie] != nil)
    {
      if (inLineIndex == currentIndex)
        return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor yellowColor] colorWithAlphaComponent:referenceSerieCurvesAlpha],NSForegroundColorAttributeName,
          nil];
      currentIndex ++;
    }
  }
    
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"GammaGraphDisplayRed"])
  {
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor redColor], NSForegroundColorAttributeName,
                                                        nil];
    currentIndex ++;

    if ([self referenceSerie] != nil)
    {
      if (inLineIndex == currentIndex)
        return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor redColor] colorWithAlphaComponent:referenceSerieCurvesAlpha],NSForegroundColorAttributeName,
          nil];
      currentIndex ++;
    }
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"GammaGraphDisplayGreen"])
  {
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor greenColor], NSForegroundColorAttributeName,
                                                        nil];
      currentIndex ++;
      if ([self referenceSerie] != nil)
      {
        if (inLineIndex == currentIndex)
          return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor greenColor] colorWithAlphaComponent:referenceSerieCurvesAlpha],NSForegroundColorAttributeName,
            nil];
        currentIndex ++;
      }
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"GammaGraphDisplayBlue"])
  {
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor blueColor], NSForegroundColorAttributeName,
                                                        nil];
    currentIndex ++;
    if ([self referenceSerie] != nil)
    {
      if (inLineIndex == currentIndex)
        return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor blueColor] colorWithAlphaComponent:referenceSerieCurvesAlpha],NSForegroundColorAttributeName,
          nil];
      currentIndex ++;
    }
  }
  
  NSAssert(NO, @"HCFRLuminanceGraphController:twoDGraphView:AttributesForLineIndex : no dict for graph !");
  return [NSDictionary dictionary];
}

- (NSArray *)twoDGraphView:(SM2DGraphView *)inGraphView dataForLineIndex:(unsigned int)inLineIndex
{
  unsigned short  currentIndex = 0;
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"GammaGraphDisplayReference"])
  {
    if (inLineIndex == currentIndex)
      return referenceValues;
    currentIndex ++;
  }
  
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"GammaGraphDisplayLuminosity"])
  {
    if (inLineIndex == currentIndex)
      return graphValues;
    currentIndex ++;
    if ([self referenceSerie] != nil)
    {
      if (inLineIndex == currentIndex)
        return referenceSerieGraphValues;
      currentIndex ++;
    }
  }
  
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"GammaGraphDisplayRed"])
  {
    if (inLineIndex == currentIndex)
      return redValues;
    currentIndex ++;
    if ([self referenceSerie] != nil)
    {
      if (inLineIndex == currentIndex)
        return referenceSerieRedValues;
      currentIndex ++;
    }
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"GammaGraphDisplayGreen"])
  {
    if (inLineIndex == currentIndex)
      return greenValues;
    currentIndex ++;
    if ([self referenceSerie] != nil)
    {
      if (inLineIndex == currentIndex)
        return referenceSerieGreenValues;
      currentIndex ++;
    }
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"GammaGraphDisplayBlue"])
  {
    if (inLineIndex == currentIndex)
      return blueValues;
    currentIndex ++;
    if ([self referenceSerie] != nil)
    {
      if (inLineIndex == currentIndex)
        return referenceSerieBlueValues;
      currentIndex ++;
    }
  }

  NSAssert(NO, @"HCFRLuminanceGraphController:twoDGraphView:dataForLineIndex: no array returned !");
  return [NSArray array];
}
- (double)twoDGraphView:(SM2DGraphView *)inGraphView maximumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis
{
  if (inAxis == kSM2DGraph_Axis_X)
      return 100.0;
  else if (inAxis == kSM2DGraph_Axis_Y)
      return 3.0;

  NSAssert(NO, @"RGBGraph : maximum value for graph -> unhandled graph axis !");

  return 100.0; // pour éviter un warning
}
- (double)twoDGraphView:(SM2DGraphView *)inGraphView minimumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis;
{
  if (inAxis == kSM2DGraph_Axis_X)
    return 0.0;
  else if (inAxis == kSM2DGraph_Axis_Y)
    return 1.0;
  
  NSAssert(NO, @"RGBGraph : maximum value for graph -> unhandled graph axis !");
  
  return 0.0; // pour éviter un warning
}

#pragma mark fonctions internes
-(void) computeCurrentSerieData
{
  if ([self colorReference] == nil)
  {
    NSLog (@"HCFRGammaGraphController : cannot compute data with null color reference. No data computed");
    return;
  }
  
  [self computeDataForSerie:[self currentSerie]
                toDataArray:graphValues
               redDataArray:redValues
             greenDataArray:greenValues
              blueDataArray:blueValues];
}
-(void) computeReferenceSerieData
{
  if ([self colorReference] == nil)
  {
    NSLog (@"HCFRGammaGraphController : cannot compute reference data with null color reference. No data computed");
    return;
  }
  
  [self computeDataForSerie:[self referenceSerie]
                toDataArray:referenceSerieGraphValues
               redDataArray:referenceSerieRedValues
             greenDataArray:referenceSerieGreenValues
              blueDataArray:referenceSerieBlueValues];
}

-(void) computeDataForSerie:(HCFRDataSerie*)currentSerie
                 toDataArray:(NSMutableArray*)graphValuesArray
                redDataArray:(NSMutableArray*)redValuesArray
                greenDataArray:(NSMutableArray*)greenValuesArray
                blueDataArray:(NSMutableArray*)blueValuesArray
{
  int     nbOfEntries;

  // Avant tout, on supprime les valeurs actuelles
  [graphValuesArray removeAllObjects];
  [redValuesArray removeAllObjects];
  [greenValuesArray removeAllObjects];
  [blueValuesArray removeAllObjects];
  
  if (currentSerie == nil)
    nbOfEntries = 0;
  else
    nbOfEntries = [currentSerie countForType:kLuminanceDataType];
  
  // Si on a moins de 3 points, on ne trace pas le graph (ça ne serait pas pertinent)
  if (nbOfEntries < 3)
  {
    return;
  }
  
  // On ne peut pas utiliser la méthode addDataPoint:toLineIndex du graph
  // car en ordonnée, on a le pourcentage de la valeur max.
  // Donc, lorsqu'un point est ajouté, la totalité des points doit être recalculé
  HCFRDataStoreEntry *lastEntry = [currentSerie lastEntryForType:kLuminanceDataType];
  HCFRDataStoreEntry *firstEntry = [currentSerie entryAtIndex:0 forType:kLuminanceDataType];
  double  maxLumValue, maxRedValue, maxGreenValue, maxBlueValue;
  double  minLumValue, minRedValue, minGreenValue, minBlueValue;
  CColor  minRGBColor = [[firstEntry value] RGBColorWithColorReference:[self colorReference]];
  CColor  maxRGBColor = [[lastEntry value] RGBColorWithColorReference:[self colorReference]];
  
  minLumValue = [[firstEntry value] luminance:true];
  minRedValue = minRGBColor.GetX();
  minGreenValue = minRGBColor.GetY();
  minBlueValue = minRGBColor.GetZ();
  
  if ([[lastEntry referenceNumber] doubleValue] < 99)
  {
    float maxRef = [[lastEntry referenceNumber] doubleValue];
    maxLumValue = ([[lastEntry value] luminance:true] - minLumValue) * 100 / maxRef;
    maxRedValue = (maxRGBColor.GetX() - minRedValue) * 100 / maxRef;
    maxGreenValue = (maxRGBColor.GetY() - minGreenValue) * 100 / maxRef;
    maxBlueValue = (maxRGBColor.GetZ() - minBlueValue) * 100 / maxRef;
  }
  else
  {
    maxLumValue = [[lastEntry value] luminance:true] - minLumValue;
    maxRedValue = maxRGBColor.GetX() - minRedValue;
    maxGreenValue = maxRGBColor.GetY() - minGreenValue;
    maxBlueValue = maxRGBColor.GetZ() - minBlueValue;
  }
  
  // on prépare les valeurs du graph des mesures
  for (int index=0; index<nbOfEntries; index++)
  {
    NSPoint             lumPoint, redPoint, greenPoint, bluePoint;
    HCFRDataStoreEntry* entry=[currentSerie entryAtIndex:index forType:kLuminanceDataType];
    
    lumPoint.x = [[entry referenceNumber] doubleValue];
    redPoint.x = lumPoint.x;
    greenPoint.x = lumPoint.x;
    bluePoint.x = lumPoint.x;
    
    if (lumPoint.x == 0.0 || lumPoint.x == 100.0)
      continue; // on ne calcul pas le gamma pour le premier ou le dernier point
    
    // calcul des valeurs
    lumPoint.y = ([[entry value] luminance:true] - minLumValue);
    if (lumPoint.y > 0.0001)
    {
      lumPoint.y = log(lumPoint.y/maxLumValue)/log(lumPoint.x/100.0);
      [graphValuesArray addObject:NSStringFromPoint(lumPoint)];
    }
    
    redPoint.y = ([[entry value] RGBColorWithColorReference:[self colorReference]][0] - minRedValue);
    if (lumPoint.y > 0.0001)
    {
      redPoint.y = log(redPoint.y/maxRedValue)/log(redPoint.x/100.0);
      [redValuesArray addObject:NSStringFromPoint(redPoint)];
    }
    
    greenPoint.y = ([[entry value] RGBColorWithColorReference:[self colorReference]][1] - minGreenValue);
    if (lumPoint.y > 0.0001)
    {
      greenPoint.y = log(greenPoint.y/maxGreenValue)/log(greenPoint.x/100.0);
      [greenValuesArray addObject:NSStringFromPoint(greenPoint)];
    }
    
    bluePoint.y = ([[entry value] RGBColorWithColorReference:[self colorReference]][2] - minBlueValue);
    if (lumPoint.y > 0.0001)
    {
      bluePoint.y = log(bluePoint.y/maxBlueValue)/log(bluePoint.x/100.0);
      [blueValuesArray addObject:NSStringFromPoint(bluePoint)];
    }
  }
}

-(void) updateReferenceValues
{
  [referenceValues removeAllObjects];
  
  [referenceValues addObject:NSStringFromPoint(NSMakePoint(0.0,[self gamma]))];
  [referenceValues addObject:NSStringFromPoint(NSMakePoint(100.0,[self gamma]))];
}

#pragma mark IB actions
-(IBAction) refreshGraph:(id)sender
{
  [graphView reloadAttributes];
  [graphView reloadData];
}
@end
