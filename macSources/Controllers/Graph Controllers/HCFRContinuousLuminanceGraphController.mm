//  ColorHCFR
//  HCFRContinuousLuminanceGraphController.m
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

#import "HCFRContinuousLuminanceGraphController.h"


@implementation HCFRContinuousLuminanceGraphController
-(HCFRContinuousLuminanceGraphController*) init
{
  [super init];
  
  // on met les valeurs par defauts pour les prefs
  NSDictionary  *defaultPreferences = [NSDictionary dictionaryWithObjectsAndKeys:
                                       @"YES", @"ContinuousLuminanceGraphDisplayLuminosity",
                                       @"YES", @"ContinuousLuminanceGraphDisplayRed",
                                       @"YES", @"ContinuousLuminanceGraphDisplayGreen",
                                       @"YES", @"ContinuousLuminanceGraphDisplayBlue",
                                       nil];
  
  [[NSUserDefaults standardUserDefaults] registerDefaults:defaultPreferences];
  
  graphView = [[SM2DGraphView alloc] init];
  graphValues = [[NSMutableArray alloc] initWithCapacity:50];
  redValues = [[NSMutableArray alloc] initWithCapacity:50];
  greenValues = [[NSMutableArray alloc] initWithCapacity:50];
  blueValues = [[NSMutableArray alloc] initWithCapacity:50];
  
  yAxisMaxValue = 0;
  yAxisMinValue = 9999;
  
  // configuration du graph
  [graphView setDataSource:self];
  [graphView setBackgroundColor:[NSColor blackColor]];
  [graphView setDrawsGrid:YES];
  [graphView setGridColor:[NSColor grayColor]];
  [graphView setNumberOfTickMarks:11 forAxis:kSM2DGraph_Axis_Y];
  [graphView setLabel:@"IRE" forAxis:kSM2DGraph_Axis_X];
  [graphView setLabel:@"t" forAxis:kSM2DGraph_Axis_Y];
  [graphView setBorderColor:[NSColor grayColor]];

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
  
  [super dealloc];
}

#pragma mark Surcharges des methodes de HCFRGraphController
-(NSString*) graphicName
{
  return HCFRLocalizedString(@"Luminance",@"Luminance");
}
-(BOOL) hasAnOptionsView
{
  return NO;
}
//-(NSView*) optionsView
//{
//  return optionsView;
//}
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
    [graphView reloadData];
  }
}
-(void) colorReferenceChanged:(HCFRColorReference*)oldReference
{
  [self computeCurrentSerieData];
  [graphView reloadData];
}
-(void) gammaChanged:(float)oldGamma
{
  [self computeCurrentSerieData];
  [graphView reloadData];
}
-(void) currentDataSerieChanged:(HCFRDataSerie*)oldSerie
{
  if (oldSerie != nil)
    [oldSerie removeListener:self forType:kContinuousDataType];
  if ([self currentSerie] != nil)
    [[self currentSerie] addListener:self forType:kContinuousDataType];
  
  [self computeCurrentSerieData];
  [graphView reloadData];
}

#pragma mark dataSource pour le graphique
- (unsigned int)numberOfLinesInTwoDGraphView:(SM2DGraphView *)inGraphView
{
  unsigned short  nbLines = 4; // (luminosité, rouge, vert, bleu)

  // si on a une référence
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"ContinuousLuminanceGraphDisplayReference"])
    nbLines ++;
  
  return nbLines;
}

- (NSDictionary *)twoDGraphView:(SM2DGraphView *)inGraphView attributesForLineIndex:(unsigned int)inLineIndex
{
  // L'index de la ligne dépend des données qui sont tracées.
  // On utilise la variable currentIndex, qu'on incrémente pour chaque type de données tracées
  unsigned short  currentIndex = 0;
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"ContinuousLuminanceGraphDisplayReference"])
  {
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor grayColor], NSForegroundColorAttributeName,
              //                                                        [NSNumber numberWithInt:kSM2DGraph_Width_Fine], SM2DGraphLineWidthAttributeName,
              //                                                      [NSNumber numberWithInt:kSM2DGraph_Symbol_X], SM2DGraphLineSymbolAttributeName,
              nil];
    currentIndex ++;
  }
  
  if (inLineIndex == currentIndex)
    return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor yellowColor], NSForegroundColorAttributeName,
            nil];
  currentIndex ++;
  
  if (inLineIndex == currentIndex)
    return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor redColor], NSForegroundColorAttributeName,
            nil];
  currentIndex ++;

  if (inLineIndex == currentIndex)
    return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor greenColor], NSForegroundColorAttributeName,
            nil];
  currentIndex ++;

  if (inLineIndex == currentIndex)
    return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor blueColor], NSForegroundColorAttributeName,
            nil];
  currentIndex ++;
  
  NSAssert(NO, @"HCFRLuminanceGraphController:twoDGraphView:AttributesForLineIndex : no dict for graph !");
  return [NSDictionary dictionary];
}

- (NSArray *)twoDGraphView:(SM2DGraphView *)inGraphView dataForLineIndex:(unsigned int)inLineIndex
{
  unsigned short  currentIndex = 0;
  
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"ContinuousLuminanceGraphDisplayLuminosity"])
  {
    if (inLineIndex == currentIndex)
    {
      return graphValues;
    }
    currentIndex ++;
  }
  
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"ContinuousLuminanceGraphDisplayRed"])
  {
    if (inLineIndex == currentIndex)
      return redValues;
    currentIndex ++;
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"ContinuousLuminanceGraphDisplayGreen"])
  {
    if (inLineIndex == currentIndex)
      return greenValues;
    currentIndex ++;
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"ContinuousLuminanceGraphDisplayBlue"])
  {
    if (inLineIndex == currentIndex)
      return blueValues;
    currentIndex ++;
  }
  
  NSAssert(NO, @"HCFRLuminanceGraphController:twoDGraphView:dataForLineIndex: no array returned !");
  return [NSArray array];
}
- (double)twoDGraphView:(SM2DGraphView *)inGraphView maximumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis
{
  if (inAxis == kSM2DGraph_Axis_X)
  {
    return 50.0;
  } // de if axe X
  else if (inAxis == kSM2DGraph_Axis_Y)
  {
    // si on a une valeur stockée, on la ressort
    if (yAxisMaxValue != 0)
      return fmax(yAxisMaxValue, maxDisplayedValue);
    
    HCFRDataSerie   *currentSerie = [self currentSerie];
    int             continuousMeasuresCount = [currentSerie countForType:kContinuousDataType];

    // si il n'y a aucune mesure, on retourne une valeur par defaut
    if (continuousMeasuresCount == 0)
    {
      return 100.0;
    }
    
    HCFRDataStoreEntry *firstEntry = [currentSerie entryAtIndex:0 forType:kContinuousDataType];
    
    if (firstEntry == nil)
      return 100.0;

    // sinon, on prend 2* la valeur du premier point
//    yAxisMaxValue = [[firstEntry value] luminance:true]*2;
    
    // methode alternative, on prend la première valeur + 10%
    yAxisMaxValue = [[firstEntry value] luminance:true]*1.1;
    
    return fmax(yAxisMaxValue, maxDisplayedValue);
    
  } // de if axe Y
  
  return 100.0; // pour éviter un warning
}
- (double)twoDGraphView:(SM2DGraphView *)inGraphView minimumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis;
{
  if (inAxis == kSM2DGraph_Axis_X) {
    return 0.0;
  }
  else if (inAxis == kSM2DGraph_Axis_Y) {
    // si on a une valeur stockée, on l'utilise
    if (yAxisMinValue != 0)
      return fmin(yAxisMinValue, minDisplayedValue);
    
    HCFRDataSerie   *currentSerie = [self currentSerie];
    
    // si on n'a aucune mesures, on retourne 0
    if ([currentSerie countForType:kContinuousDataType] == 0)
      return 0.0;
    
    // sinon on peut retourner 0 ... facile
//    yAxisMinValue = 0.0;
    
    // ou bien on prend la première valeur - 10%
    HCFRDataStoreEntry *firstEntry = [currentSerie entryAtIndex:0 forType:kContinuousDataType];
    yAxisMinValue = [[firstEntry value] luminance:true]*0.9;

    return fmin(yAxisMinValue, minDisplayedValue);
  }
  return 0.0; // pour éviter un warning
}

#pragma mark fonctions internes
-(void) computeCurrentSerieData
{
  if ([self colorReference] == nil)
  {
    NSLog (@"HCFRContinuousLuminanceGraphController : cannot compute data with null color reference. No data computed");
    return;
  }
  
  [self computeDataForSerie:[self currentSerie]
                toDataArray:graphValues
               redDataArray:redValues
             greenDataArray:greenValues
              blueDataArray:blueValues];
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
  
  // et on reset le maximum
  maxDisplayedValue = 0.0;
  minDisplayedValue = 100.0;
  
  if (currentSerie == nil)
    nbOfEntries = 0;
  else
    nbOfEntries = [currentSerie countForType:kContinuousDataType];
  
  // if no value is found, do nothing.
  if (nbOfEntries == 0)
    return;
    
  // on prépare les valeurs du graph des mesures
  for (int index=0; index<nbOfEntries; index++)
  {
    NSPoint             lumPoint, redPoint, greenPoint, bluePoint;
    HCFRDataStoreEntry* entry=[currentSerie entryAtIndex:index forType:kContinuousDataType];
    
    lumPoint.x = index;
    redPoint.x = lumPoint.x;
    greenPoint.x = lumPoint.x;
    bluePoint.x = lumPoint.x;
    
    lumPoint.y = [[entry value] luminance:true];
    maxDisplayedValue = fmax(lumPoint.y, maxDisplayedValue);
    minDisplayedValue = fmin(lumPoint.y, minDisplayedValue);
    
    CColor  rgbColor = [[entry value] RGBColorWithColorReference:[self colorReference]];
    redPoint.y = rgbColor.GetX();
    maxDisplayedValue = fmax(redPoint.y, maxDisplayedValue);
    minDisplayedValue = fmin(redPoint.y, minDisplayedValue);
    greenPoint.y = rgbColor.GetY();
    maxDisplayedValue = fmax(greenPoint.y, maxDisplayedValue);
    minDisplayedValue = fmin(greenPoint.y, minDisplayedValue);
    bluePoint.y = rgbColor.GetZ();
    maxDisplayedValue = fmax(bluePoint.y, maxDisplayedValue);
    minDisplayedValue = fmin(bluePoint.y, minDisplayedValue);
    
    [graphValuesArray addObject:NSStringFromPoint(lumPoint)];
    [redValuesArray addObject:NSStringFromPoint(redPoint)];
    [greenValuesArray addObject:NSStringFromPoint(greenPoint)];
    [blueValuesArray addObject:NSStringFromPoint(bluePoint)];
  }
}

-(float) getCurrentGamma
{
  return [self gamma];
}

#pragma mark IB actions
-(IBAction) refreshGraph:(id)sender
{
  [graphView reloadAttributes];
  [graphView reloadData];
}
@end
