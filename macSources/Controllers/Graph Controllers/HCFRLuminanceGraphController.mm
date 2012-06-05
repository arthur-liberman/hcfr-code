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

#import "HCFRLuminanceGraphController.h"

const float referenceSerieCurvesGamma = 0.2;

@implementation HCFRLuminanceGraphController
-(HCFRLuminanceGraphController*) init
{
  [super init];
  
  // on met les valeurs par defauts pour les prefs
  NSDictionary  *defaultPreferences = [NSDictionary dictionaryWithObjectsAndKeys:
                                          @"YES", @"LuminanceGraphCompensate",
                                          @"YES", @"LuminanceGraphDisplayReference",
                                          @"YES", @"LuminanceGraphDisplayLuminosity",
                                          @"NO", @"LuminanceGraphDisplayRed",
                                          @"NO", @"LuminanceGraphDisplayGreen",
                                          @"NO", @"LuminanceGraphDisplayBlue",
                                          nil];
                  
  [[NSUserDefaults standardUserDefaults] registerDefaults:defaultPreferences];
  
  // on charge le fichier nib qui contiend le panel d'options
  [NSBundle loadNibNamed:@"LuminanceGraph" owner:self];
  
  graphView = [[SM2DGraphView alloc] init];
  graphValues = [[NSMutableArray alloc] initWithCapacity:50];
  redValues = [[NSMutableArray alloc] initWithCapacity:50];
  greenValues = [[NSMutableArray alloc] initWithCapacity:50];
  blueValues = [[NSMutableArray alloc] initWithCapacity:50];

  referenceSerieGraphValues = [[NSMutableArray alloc] initWithCapacity:50];
  referenceSerieRedValues = [[NSMutableArray alloc] initWithCapacity:50];
  referenceSerieGreenValues = [[NSMutableArray alloc] initWithCapacity:50];
  referenceSerieBlueValues = [[NSMutableArray alloc] initWithCapacity:50];
  
  // on prépare le tableau des valeurs de référence
  referenceValues = [[NSMutableArray alloc] initWithCapacity:10];
  [self updateReferenceValues];

  // le mode
  graphMode = kLuminanceGraphNormalMode;
  
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
  return HCFRLocalizedString(@"Luminance",@"Luminance");
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
  [self computeCurrentSerieData];
  [self computeReferenceSerieData];
  [graphView reloadData];
}
-(void) gammaChanged:(float)oldGamma
{
  [self updateReferenceValues];
  [self computeCurrentSerieData];
  [self computeReferenceSerieData];
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
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphDisplayReference"])
    nbLines ++;
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphDisplayLuminosity"])
  {
    nbLines ++;
    if ([self referenceSerie] != nil)
      nbLines ++;
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphDisplayRed"])
  {
    nbLines ++;
    if ([self referenceSerie] != nil)
      nbLines ++;
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphDisplayGreen"])
  {
    nbLines ++;
    if ([self referenceSerie] != nil)
      nbLines ++;
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphDisplayBlue"])
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
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphDisplayReference"])
  {
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor grayColor], NSForegroundColorAttributeName,
//                                                        [NSNumber numberWithInt:kSM2DGraph_Width_Fine], SM2DGraphLineWidthAttributeName,
//                                                      [NSNumber numberWithInt:kSM2DGraph_Symbol_X], SM2DGraphLineSymbolAttributeName,
                                                        nil];
    currentIndex ++;
  }
  
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphDisplayLuminosity"])
  {
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor yellowColor], NSForegroundColorAttributeName,
                                                        nil];
    currentIndex ++;
    if ([self referenceSerie] != nil)
    {
      if (inLineIndex == currentIndex)
        return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor yellowColor] colorWithAlphaComponent:referenceSerieCurvesGamma],NSForegroundColorAttributeName,
          nil];
      currentIndex ++;
    }
  }
    
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphDisplayRed"])
  {
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor redColor], NSForegroundColorAttributeName,
                                                        nil];
    currentIndex ++;

    if ([self referenceSerie] != nil)
    {
      if (inLineIndex == currentIndex)
        return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor redColor] colorWithAlphaComponent:referenceSerieCurvesGamma],NSForegroundColorAttributeName,
          nil];
      currentIndex ++;
    }
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphDisplayGreen"])
  {
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor greenColor], NSForegroundColorAttributeName,
                                                        nil];
      currentIndex ++;
      if ([self referenceSerie] != nil)
      {
        if (inLineIndex == currentIndex)
          return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor greenColor] colorWithAlphaComponent:referenceSerieCurvesGamma],NSForegroundColorAttributeName,
            nil];
        currentIndex ++;
      }
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphDisplayBlue"])
  {
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor blueColor], NSForegroundColorAttributeName,
                                                        nil];
    currentIndex ++;
    if ([self referenceSerie] != nil)
    {
      if (inLineIndex == currentIndex)
        return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor blueColor] colorWithAlphaComponent:referenceSerieCurvesGamma],NSForegroundColorAttributeName,
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
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphDisplayReference"])
  {
    if (inLineIndex == currentIndex)
      return referenceValues;
    currentIndex ++;
  }
  
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphDisplayLuminosity"])
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
  
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphDisplayRed"])
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
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphDisplayGreen"])
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
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphDisplayBlue"])
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
  {
    switch (graphMode)
    {
      case kLuminanceGraphBlackProximityMode :
        return 5.0;
        break;
      default :
        return 100.0;
        break;
    } // de switch (graphMode)
  } // de if axe X
  else if (inAxis == kSM2DGraph_Axis_Y)
  {
    switch (graphMode)
    {
      case kLuminanceGraphBlackProximityMode :
      {
        // on cherche le premier point dont la reference est > à 5 IRE, afin de prendre sa valeur
        // comme minimum pour l'axe Y
        int     index = 0;
        int     nbValues = [graphValues count];
        double  maxValue;
        
        while (index < nbValues)
        {
          NSPoint point = NSPointFromString([graphValues objectAtIndex:index]);
          if (point.x >= 5)
          {
            maxValue = point.y;
            break;
          }
          index ++;
        } // de while
        
        // si on n'a pas encore la valeur, on prend la valeur de la reference.
        // c'est simple : la reference à un pas de 1 IRE depuis 0. Donc, on prend la valeur à l'index 5.
        if (index == nbValues)
          maxValue = NSPointFromString([referenceValues objectAtIndex:5]).y;
        
        return maxValue;
        break;
      }
      default :
        return 100.0;
        break;
    } // de switch (graphMode)
  } // de if axe Y
  
  return 100.0;
}
- (double)twoDGraphView:(SM2DGraphView *)inGraphView minimumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis;
{
  if (inAxis == kSM2DGraph_Axis_X)
  {
    switch (graphMode)
    {
      case kLuminanceGraphWhiteProximityMode :
        return 95.0;
        break;
      default :
        return 0.0;
        break;
    }
  }
  else if (inAxis == kSM2DGraph_Axis_Y)
  {
    switch (graphMode)
    {
      case kLuminanceGraphWhiteProximityMode :
      {
            
        
        // on cherche le premier point dont la reference est > à 5 IRE, afin de prendre sa valeur
        // comme minimum pour l'axe Y
        int     index = [graphValues count]-1;
        double  minValue;
        
        while (index >= 0)
        {
          NSPoint point = NSPointFromString([graphValues objectAtIndex:index]);
          
          // si le premier point n'est pas au dessus de 95 IRE, on retourne la valeur de ref
          if ((index == [graphValues count]-1) && (point.x < 95))
            return NSPointFromString([referenceValues objectAtIndex:95]).y;

          if (point.x <= 95)
          {
            minValue = point.y;
            break;
          }
          index --;
        } // de while

        // si index = -1, c'est qu'on a aucune valeur -> on prend la référence
        if (index == -1)
        {
          return NSPointFromString([referenceValues objectAtIndex:95]).y;
        }

        return minValue;
        break;
      }
      default :
        return 0;
        break;
    }
  }
  
  NSAssert(NO, @"RGBGraph : maximum value for graph -> unhandled graph mode !");
  
  return 0.0;
}

#pragma mark fonctions internes
-(void) computeCurrentSerieData
{
  if ([self colorReference] == nil)
  {
    NSLog (@"HCFRLuminanceGraphController : cannot compute data with null color reference. No data computed");
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
    NSLog (@"HCFRLuminanceGraphController : cannot compute reference data with null color reference. No data computed");
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
  ColorRGB  minRGBColor = [[firstEntry value] RGBColorWithColorReference:[self colorReference]];
  ColorRGB  maxRGBColor = [[lastEntry value] RGBColorWithColorReference:[self colorReference]];
  
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LuminanceGraphCompensate"])
  {
    minLumValue = [[firstEntry value] luminance:true]; // utilisée si l'option "compensate" est active
    minRedValue = minRGBColor[0]; // utilisée si l'option "compensate" est active
    minGreenValue = minRGBColor[1]; // utilisée si l'option "compensate" est active
    minBlueValue = minRGBColor[2]; // utilisée si l'option "compensate" est active
  }
  else
  {
    minLumValue = 0;
    minRedValue = 0;
    minGreenValue = 0;
    minBlueValue = 0;
  }
  
  if ([[lastEntry referenceNumber] doubleValue] < 99)
  {
    float maxRef = [[lastEntry referenceNumber] doubleValue];
    maxLumValue = ([[lastEntry value] luminance:true] - minLumValue) * 100 / maxRef;
    maxRedValue = (maxRGBColor[0] - minRedValue) * 100 / maxRef;
    maxGreenValue = (maxRGBColor[1] - minGreenValue) * 100 / maxRef;
    maxBlueValue = (maxRGBColor[2] - minBlueValue) * 100 / maxRef;
  }
  else
  {
    maxLumValue = [[lastEntry value] luminance:true] - minLumValue;
    maxRedValue = maxRGBColor[0] - minRedValue;
    maxGreenValue = maxRGBColor[1] - minGreenValue;
    maxBlueValue = maxRGBColor[2] - minBlueValue;
  }
  
  // on applique la correction gamma à la valeur max
  //  maxLumValue = pow(maxLumValue, [self getCurrentGamma]);
  //  maxRedValue = pow(maxRedValue, [self getCurrentGamma]);
  //  maxGreenValue = pow(maxGreenValue, [self getCurrentGamma]);
  //  maxBlueValue = pow(maxBlueValue, [self getCurrentGamma]);
  
  // on prépare les valeurs du graph des mesures
  for (int index=0; index<nbOfEntries; index++)
  {
    NSPoint             lumPoint, redPoint, greenPoint, bluePoint;
    HCFRDataStoreEntry* entry=[currentSerie entryAtIndex:index forType:kLuminanceDataType];
    
    lumPoint.x = [[entry referenceNumber] doubleValue];
    redPoint.x = lumPoint.x;
    greenPoint.x = lumPoint.x;
    bluePoint.x = lumPoint.x;
    
    // on applique la compensation du noir (minLumValue vaut 0 si l'option est inactivée)
    lumPoint.y = [[entry value] luminance:true]-minLumValue; // en pourcentage de la valeur max
    
    ColorRGB  rgbColor = [[entry value] RGBColorWithColorReference:[self colorReference]];
    redPoint.y = rgbColor[0] - minRedValue;
    greenPoint.y = rgbColor[1] - minGreenValue;
    bluePoint.y = rgbColor[2] - minBlueValue;
    
    // et on applique le gamma
    //    newPoint.x = (newPoint.x+[self getCurrentGamma])/(1+[self getCurrentGamma]);
    //    lumPoint.y = pow(lumPoint.y, [self getCurrentGamma]);
    //    redPoint.y = pow(redPoint.y, [self getCurrentGamma]);
    //    greenPoint.y = pow(greenPoint.y, [self getCurrentGamma]);
    //    bluePoint.y = pow(bluePoint.y, [self getCurrentGamma]);
    
    // on transforme la valeur en pourcentage du maximum
    lumPoint.y = lumPoint.y*100/maxLumValue;
    redPoint.y = redPoint.y*100/maxRedValue;
    greenPoint.y = greenPoint.y*100/maxGreenValue;
    bluePoint.y = bluePoint.y*100/maxBlueValue;
    
    [graphValuesArray addObject:NSStringFromPoint(lumPoint)];
    [redValuesArray addObject:NSStringFromPoint(redPoint)];
    [greenValuesArray addObject:NSStringFromPoint(greenPoint)];
    [blueValuesArray addObject:NSStringFromPoint(bluePoint)];
  }
}

-(void) updateReferenceValues
{
  double maxValue = pow(100, [self getCurrentGamma]);
  
  [referenceValues removeAllObjects];
    
  for (int i=0; i<=100; i++)
  {
    NSPoint newPoint;
    
    newPoint.x = i;
    newPoint.y = pow(i, [self getCurrentGamma])*100/maxValue;
    
    [referenceValues addObject:NSStringFromPoint(newPoint)];
  }
}

-(float) getCurrentGamma
{
  return [self gamma];
}

-(void) setGraphMode:(HCFRLuminanceGraphMode)newMode
{
  graphMode = newMode;
  [graphView reloadData];
}

#pragma mark IB actions
-(IBAction) refreshGraph:(id)sender
{
  [graphView reloadAttributes];
  [graphView reloadData];
}
/*-(IBAction) showReferenceChanged:(NSButton*)sender
{
  bool oldVersion = drawReference;
  drawReference = ([sender state] == NSOnState);
  
  if (oldVersion != drawReference)
  {
    [graphView reloadAttributes];
    [graphView reloadData];
  }
}
-(IBAction) showLuminosityChanged:(NSButton*)sender
{
  bool oldVersion = drawLuminosity;
  drawLuminosity = ([sender state] == NSOnState);
  
  if (oldVersion != drawLuminosity)
  {
    [graphView reloadAttributes];
    [graphView reloadData];
  }
}
-(IBAction) showRedChanged:(NSButton*)sender
{
  bool oldVersion = drawRed;
  drawRed = ([sender state] == NSOnState);
  
  if (oldVersion != drawRed)
  {
    [graphView reloadAttributes];
    [graphView reloadData];
  }
}
-(IBAction) showGreenChanged:(NSButton*)sender
{
  bool oldVersion = drawGreen;
  drawGreen = ([sender state] == NSOnState);
  
  if (oldVersion != drawGreen)
  {
    [graphView reloadAttributes];
    [graphView reloadData];
  }
}
-(IBAction) showBlueChanged:(NSButton*)sender
{
  bool oldVersion = drawBlue;
  drawBlue = ([sender state] == NSOnState);
  
  if (oldVersion != drawBlue)
  {
    [graphView reloadAttributes];
    [graphView reloadData];
  }
}
-(IBAction)compensateChanged:(NSButton*)sender
{
  bool oldVersion = compensate;
  compensate = ([sender state] == NSOnState);
  
  if (oldVersion != compensate)
  {
    [self computeCurrentSerieData];
    [self computeReferenceSerieData];
    [graphView reloadData];
  }
}*/
-(IBAction) displayModeChanged:(NSPopUpButton*)sender
{
  int value = [sender selectedTag];
  switch (value)
  {
    case 0 : // affichage normal
      [self setGraphMode:kLuminanceGraphNormalMode];
      break;
    case 1 : // proximité du noir
      [self setGraphMode:kLuminanceGraphBlackProximityMode];
      break;
    case 2 : // proximité du blanc
      [self setGraphMode:kLuminanceGraphWhiteProximityMode];
      break;
  }
}
@end
