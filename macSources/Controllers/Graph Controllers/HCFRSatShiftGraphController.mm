//  ColorHCFR
//  HCFRSatLumGraphController.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 25/09/07.
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

#import "HCFRSatShiftGraphController.h"

// Vu que ce n'est accessible que en local, on ne préfix pas le nom des constantes
#define kShowReference   @"SatShiftGraphShowReference"
#define kShowRed         @"SatShiftGraphShowRed"
#define kShowBlue        @"SatShiftGraphShowBlue"
#define kShowGreen       @"SatShiftGraphShowGreen"
#define kShowCyan        @"SatShiftGraphShowCyan"
#define kShowMagenta     @"SatShiftGraphShowMagenta"
#define kShowYellow      @"SatShiftGraphShowYellow"

// Les différents graphiques.
enum {
  // les ref du graph 1
  kGraph1RedReferenceValues,      // 0
  kGraph1GreenReferenceValues,
  kGraph1BlueReferenceValues,
  kGraph1CyanReferenceValues,
  kGraph1MagentaReferenceValues,
  kGraph1YellowReferenceValues,   // 5

  // les valeurs du graph 1
  kGraph1RedValues,               
  kGraph1GreenValues,
  kGraph1BlueValues,
  kGraph1CyanValues,
  kGraph1MagentaValues,           // 10
  kGraph1YellowValues,
  
  // La courbe de reference pour le graph 1 (une ligne à 0)
  kGraph1Reference,
  
  // les ref du graph 2
  kGraph2RedReferenceValues,
  kGraph2GreenReferenceValues,
  kGraph2BlueReferenceValues,
  kGraph2CyanReferenceValues,     // 15
  kGraph2MagentaReferenceValues,
  kGraph2YellowReferenceValues,
  
  // les valeurs du graph 2
  kGraph2RedValues,               
  kGraph2GreenValues,
  kGraph2BlueValues,              // 20
  kGraph2CyanValues,
  kGraph2MagentaValues,           
  kGraph2YellowValues,
  
  
  kNumberOfGraphs   // cette valeur correspondra au nombre de tableaux de valeurs
};

@implementation HCFRSatShiftGraphController
-(HCFRSatShiftGraphController*) init
{
  [super init];
  
  // Les prefs par defaut
  NSDictionary  *defaults = [[NSDictionary alloc] initWithObjectsAndKeys:
    @"NO", kShowReference,
    @"YES", kShowRed,
    @"YES", kShowBlue,
    @"YES", kShowGreen,
    @"NO", kShowCyan,
    @"NO", kShowMagenta,
    @"NO", kShowYellow,
    nil];
  [[NSUserDefaults standardUserDefaults] registerDefaults:defaults];
  [defaults release];
  
  // on charge le fichier nib qui contiend le panel d'options
  [NSBundle loadNibNamed:@"SatShiftGraph" owner:self];

  // les valeurs min et max des graphs
  maxShift = 0.0;
  minShift = 0.0;
  maxDelta = 0.0;
  
  // initialisation des tableaux de données
  dataArrays = [[NSMutableArray alloc] initWithCapacity:kNumberOfGraphs];
  int dataArrayIndex;
  for (dataArrayIndex = 0; dataArrayIndex < kNumberOfGraphs; dataArrayIndex ++)
  {
    NSMutableArray  *newArray = [[NSMutableArray alloc] initWithCapacity:10];
    [dataArrays addObject:newArray];
    [newArray release];
  }
  
  // on crée la référence du graph 1
  [[dataArrays objectAtIndex:kGraph1Reference] addObject:NSStringFromPoint(NSMakePoint(0.0,0.0))];
  [[dataArrays objectAtIndex:kGraph1Reference] addObject:NSStringFromPoint(NSMakePoint(100.0,0.0))];

  // configuration des graphs
  graphView1 = [[SM2DGraphView alloc] init];
  [graphView1 setDataSource:self];
  [graphView1 setBackgroundColor:[NSColor blackColor]];
  [graphView1 setDrawsGrid:YES];
  [graphView1 setGridColor:[NSColor grayColor]];
  [graphView1 setNumberOfTickMarks:11 forAxis:kSM2DGraph_Axis_X];
  [graphView1 setNumberOfTickMarks:11 forAxis:kSM2DGraph_Axis_Y];
//  [graphView1 setLabel:HCFRLocalizedString(@"Saturation %", @"Saturation %") forAxis:kSM2DGraph_Axis_X];
  [graphView1 setLabel:HCFRLocalizedString(@"Shift %",@"Shift %") forAxis:kSM2DGraph_Axis_Y];
  [graphView1 reloadAttributes];
  [graphView1 reloadData];

  graphView2 = [[SM2DGraphView alloc] init];
  [graphView2 setDataSource:self];
  [graphView2 setBackgroundColor:[NSColor blackColor]];
  [graphView2 setDrawsGrid:YES];
  [graphView2 setGridColor:[NSColor grayColor]];
  [graphView2 setNumberOfTickMarks:11 forAxis:kSM2DGraph_Axis_X];
  [graphView2 setNumberOfTickMarks:11 forAxis:kSM2DGraph_Axis_Y];
  [graphView2 setLabel:HCFRLocalizedString(@"Saturation %", @"Saturation %") forAxis:kSM2DGraph_Axis_X];
  [graphView2 setLabel:HCFRLocalizedString(@"Delta E",@"Delta E") forAxis:kSM2DGraph_Axis_Y];
  [graphView2 reloadAttributes];
  [graphView2 reloadData];
  
  // construction de la vue
  graphsView = [[NSView alloc] initWithFrame:NSMakeRect(0.0,0.0,100.0,100.0)];
  [graphsView setAutoresizesSubviews:YES];
  // le graph 1
  [graphView1 setAutoresizingMask:NSViewWidthSizable|NSViewMinYMargin|NSViewHeightSizable];
  [graphView1 setFrame:NSMakeRect(00.0,50.0,100.0,50.0)];
  [graphsView addSubview:graphView1];
  // le graph 2
  [graphView2 setAutoresizingMask:NSViewWidthSizable|NSViewMaxYMargin|NSViewHeightSizable];
  [graphView2 setFrame:NSMakeRect(0.0,0.0,100.0,50.0)];
  [graphsView addSubview:graphView2];
  
  return self;
}
-(void) dealloc
{
  [dataArrays release];
  
  if ([self currentSerie])
  {
    [[self currentSerie] removeListener:self forType:kRedSaturationDataType];
    [[self currentSerie] removeListener:self forType:kGreenSaturationDataType];
    [[self currentSerie] removeListener:self forType:kBlueSaturationDataType];
  }
  
  [graphView1 release];
  [graphView2 release];
  [graphsView release];
  
  [super dealloc];
}

#pragma mark Fonctions de calcul des données d'affichage
-(void) computeDataForSerie:(HCFRDataSerie*)serie dataType:(HCFRDataType)dataType
                  toShiftValuesArray:(NSMutableArray*)shiftArray andDeltaEArray:(NSMutableArray*)deltaEArray
      defaultSaturatedColor:(CColor)saturatedColor
{
  NSAssert (shiftArray!=nil, @"HCFRSatShiftGraphController:computDataForSerie : nil shift destination !");
  NSAssert (deltaEArray!=nil, @"HCFRSatShiftGraphController:computDataForSerie : nil deltaE destination !");
  
  [shiftArray removeAllObjects];
  [deltaEArray removeAllObjects];

  if ([serie count] == 0)
    return;
  
  // On charge les coordonnées du blanc de référence
	double xStart = [[self colorReference] white].GetxyYValue()[0];
	double yStart = [[self colorReference] white].GetxyYValue()[1];
  
  // on prend le point à 100% de saturation, ou une couleur theorique si il n'a pas encore été mesurée.
  ColorxyY endColor;
  double xEnd;
  double yEnd;
  if ([[[serie lastEntryForType:kRedSaturationDataType] referenceNumber] intValue] == 100)
  {
    HCFRColor *saturatedColor = [[serie lastEntryForType:dataType] value];
    endColor = [saturatedColor xyYColor];
//    endColor = [saturatedColor XYZColor];
  }
  else
  {
		endColor=saturatedColor.GetxyYValue();
  }
  xEnd = endColor[0];
  yEnd = endColor[1];
  
  // Enfin, on calcul les valeurs
  int nbValues = [serie countForType:dataType];
  int index;
  for (index=0; index < nbValues; index ++)
  {
    HCFRDataStoreEntry  *currentEntry = [serie entryAtIndex:index forType:dataType];
    // la mesure
    ColorxyY              xyYEntry = [[currentEntry value] xyYColor];
    double              xMeasure = xyYEntry[0];
    double              yMeasure = xyYEntry[1];
        
    // la cible : on interpole linérairement entre le premier et le dernier point
    double xTarget = xStart + (xEnd - xStart) * [[currentEntry referenceNumber] intValue] / 100.0;
//    double yTarget = yStart + (yEnd - yStart) * [[currentEntry referenceNumber] intValue] / 100.0;
    // le décalage
    double dx = xEnd - xStart;
    double dy = yEnd - yStart;
    double k = ( (xMeasure - xStart) + (yStart - yMeasure) * (dx/dy) ) / ( (dx*dx/dy)+dy );
    double xPoint = xMeasure - k*dy;
    double yPoint = yMeasure + k*dx;
    
    NSPoint shiftPoint = NSMakePoint([[currentEntry referenceNumber] intValue],(xPoint-xTarget)/(xEnd-xStart)*100);
    maxShift = fmax(shiftPoint.y, maxShift);
    minShift = fmin(shiftPoint.y, minShift);
    [shiftArray addObject:NSStringFromPoint(shiftPoint)];
    
    //le delta E
    double uMeasure = 4.0*xMeasure / (-2.0*xMeasure + 12.0*yMeasure + 3.0);
    double vMeasure = 9.0*yMeasure / (-2.0*xMeasure + 12.0*yMeasure + 3.0);
    double uPoint   = 4.0*xPoint / (-2.0*xPoint + 12.0*yPoint + 3.0);
    double vPoint   = 9.0*yPoint / (-2.0*xPoint + 12.0*yPoint + 3.0);
    
    NSPoint deltaEPoint = NSMakePoint([[currentEntry referenceNumber] intValue],1300.0*sqrt(pow(uPoint-uMeasure, 2)+pow(vPoint-vMeasure, 2)));
    maxDelta = fmax(deltaEPoint.y, maxDelta);
    [deltaEArray addObject:NSStringFromPoint(deltaEPoint)];
  }
}

#pragma mark Surcharges des methodes de HCFRGraphController
-(GraphType) graphicType
{
  return kCalibrationGraph;
}
-(NSString*) graphicName
{
  return HCFRLocalizedString(@"Sat/shift",@"Sat/shift");
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
  return graphsView;
}

#pragma IBActions
-(IBAction) refreshGraph:(id)sender
{
  [graphView1 reloadAttributes];
  [graphView1 reloadData];
  [graphView2 reloadAttributes];
  [graphView2 reloadData];  
}

#pragma mark Implémentation de l'interface DataSource pour le graphique
- (unsigned int)numberOfLinesInTwoDGraphView:(SM2DGraphView *)inGraphView
{
  unsigned int numberOfLines = 0;
  
  // il y a une courbe de reference pour le graph 1
  if (inGraphView == graphView1)
    numberOfLines += 1;    
  
  if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowRed])
  {
    numberOfLines += 1; // la courbe courrante et la courbe de reference
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      numberOfLines += 1;
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowGreen])
  {
    numberOfLines += 1; // la courbe courrante et la courbe de reference
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      numberOfLines += 1;
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowBlue])
  {
    numberOfLines += 1; // la courbe courrante et la courbe de reference
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      numberOfLines += 1;
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowCyan])
  {
    numberOfLines += 1; // la courbe courrante et la courbe de reference
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      numberOfLines += 1;
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowMagenta])
  {
    numberOfLines += 1; // la courbe courrante et la courbe de reference
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      numberOfLines += 1;
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowYellow])
  {
    numberOfLines += 1; // la courbe courrante et la courbe de reference
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      numberOfLines += 1;
  }
  
  return numberOfLines;
}
- (NSDictionary *)twoDGraphView:(SM2DGraphView *)inGraphView attributesForLineIndex:(unsigned int)inLineIndex
{
  unsigned short  currentIndex = 0;

  // la première courbe du graph 1 est la reference
  if (inGraphView == graphView1)
  {
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor whiteColor], NSForegroundColorAttributeName,
        nil];
    currentIndex ++;
  }

  // les courbes sont les mêmes sur les deux graphs
  if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowRed])
  {
    // la courbe courante
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor redColor], NSForegroundColorAttributeName,
        nil];
    currentIndex ++;
    
    // la courbe de ref
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
    {
      if (inLineIndex == currentIndex)
        return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor redColor] colorWithAlphaComponent:0.2], NSForegroundColorAttributeName,
          nil];
      currentIndex ++;
    }
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowGreen])
  {
    // la courbe courante
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor greenColor], NSForegroundColorAttributeName,
        nil];
    currentIndex ++;

    // la courbe de ref
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
    {
      if (inLineIndex == currentIndex)
        return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor greenColor] colorWithAlphaComponent:0.2], NSForegroundColorAttributeName,
          nil];
      currentIndex ++;
    }
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowBlue])
  {
    // la courbe courante
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor blueColor], NSForegroundColorAttributeName,
        nil];
    currentIndex ++;

    // la courbe de ref
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
    {
      if (inLineIndex == currentIndex)
        return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor blueColor] colorWithAlphaComponent:0.2], NSForegroundColorAttributeName,
          nil];
      currentIndex ++;
    }
  }
  NSAssert (NO, @"HCFRSatShiftGraphController : no attributes for line !");
  return nil;
}
- (NSArray *)twoDGraphView:(SM2DGraphView *)inGraphView dataForLineIndex:(unsigned int)inLineIndex
{
  unsigned short  currentIndex = 0;

  // Pour trouver de quelle courbe il s'agit en fonction de l'index, on parcours les graphs
  // qui doivent être affichés en incrémentant currentIndex.
  // Quand currentIndex vaut inLineIndex, c'est qu'on à la bonne courbe.
  
  if (inGraphView == graphView1)
  {
    // la courbe de référence, toujours affichée
    if (inLineIndex == currentIndex)
      return [dataArrays objectAtIndex:kGraph1Reference];
    currentIndex ++;

    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowRed])
    {
      if (inLineIndex == currentIndex)
        return [dataArrays objectAtIndex:kGraph1RedValues];
      currentIndex ++;
      if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      {
        if (inLineIndex == currentIndex)
          return [dataArrays objectAtIndex:kGraph1RedReferenceValues];
        currentIndex ++;
      }
    }
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowGreen])
    {
      if (inLineIndex == currentIndex)
        return [dataArrays objectAtIndex:kGraph1GreenValues];
      currentIndex ++;
      if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      {
        if (inLineIndex == currentIndex)
          return [dataArrays objectAtIndex:kGraph1GreenReferenceValues];
        currentIndex ++;
      }
    }
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowBlue])
    {
      if (inLineIndex == currentIndex)
        return [dataArrays objectAtIndex:kGraph1BlueValues];
      currentIndex ++;
      if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      {
        if (inLineIndex == currentIndex)
          return [dataArrays objectAtIndex:kGraph1BlueReferenceValues];
        currentIndex ++;
      }
    }
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowCyan])
    {
      if (inLineIndex == currentIndex)
        return [dataArrays objectAtIndex:kGraph1CyanValues];
      currentIndex ++;
      if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      {
        if (inLineIndex == currentIndex)
          return [dataArrays objectAtIndex:kGraph1CyanReferenceValues];
        currentIndex ++;
      }
    }
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowMagenta])
    {
      if (inLineIndex == currentIndex)
        return [dataArrays objectAtIndex:kGraph1MagentaValues];
      currentIndex ++;
      if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      {
        if (inLineIndex == currentIndex)
          return [dataArrays objectAtIndex:kGraph1MagentaReferenceValues];
        currentIndex ++;
      }
    }
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowYellow])
    {
      if (inLineIndex == currentIndex)
        return [dataArrays objectAtIndex:kGraph1YellowValues];
      currentIndex ++;
      if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      {
        if (inLineIndex == currentIndex)
          return [dataArrays objectAtIndex:kGraph1YellowReferenceValues];
        currentIndex ++;
      }
    }
  }
  else if (inGraphView == graphView2)
  {
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowRed])
    {
      if (inLineIndex == currentIndex)
        return [dataArrays objectAtIndex:kGraph2RedValues];
      currentIndex ++;
      if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      {
        if (inLineIndex == currentIndex)
          return [dataArrays objectAtIndex:kGraph2RedReferenceValues];
        currentIndex ++;
      }
    }
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowGreen])
    {
      if (inLineIndex == currentIndex)
        return [dataArrays objectAtIndex:kGraph2GreenValues];
      currentIndex ++;
      if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      {
        if (inLineIndex == currentIndex)
          return [dataArrays objectAtIndex:kGraph2GreenReferenceValues];
        currentIndex ++;
      }
    }
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowBlue])
    {
      if (inLineIndex == currentIndex)
        return [dataArrays objectAtIndex:kGraph2BlueValues];
      currentIndex ++;
      if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      {
        if (inLineIndex == currentIndex)
          return [dataArrays objectAtIndex:kGraph2BlueReferenceValues];
        currentIndex ++;
      }
    }
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowCyan])
    {
      if (inLineIndex == currentIndex)
        return [dataArrays objectAtIndex:kGraph2CyanValues];
      currentIndex ++;
      if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      {
        if (inLineIndex == currentIndex)
          return [dataArrays objectAtIndex:kGraph2CyanReferenceValues];
        currentIndex ++;
      }
    }
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowMagenta])
    {
      if (inLineIndex == currentIndex)
        return [dataArrays objectAtIndex:kGraph2MagentaValues];
      currentIndex ++;
      if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      {
        if (inLineIndex == currentIndex)
          return [dataArrays objectAtIndex:kGraph2MagentaReferenceValues];
        currentIndex ++;
      }
    }
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowYellow])
    {
      if (inLineIndex == currentIndex)
        return [dataArrays objectAtIndex:kGraph2YellowValues];
      currentIndex ++;
      if ([[NSUserDefaults standardUserDefaults] boolForKey:kShowReference])
      {
        if (inLineIndex == currentIndex)
          return [dataArrays objectAtIndex:kGraph2YellowReferenceValues];
        currentIndex ++;
      }
    }
  }
  
  return [NSArray array];
  NSAssert (NO, @"HCFRSatShiftGraphController : no data for line !");
  return nil;
}
- (double)twoDGraphView:(SM2DGraphView *)inGraphView maximumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis
{
  if (inAxis == kSM2DGraph_Axis_X)
    return 100.0;
  else
  {
    if (inGraphView == graphView1)
    {
      float value = fmax(20.0, fmax(fabs(maxShift), fabs(minShift)));
      value = ((int)value/5) * 5 + 5; // on arrondie à 5
      return value;
    }
    else
    {
      float value = fmax(20.0, ((int)maxDelta/5) * 5 + 5);
      return value;
    }
  }
}
- (double)twoDGraphView:(SM2DGraphView *)inGraphView minimumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis
{
  if (inAxis == kSM2DGraph_Axis_X)
    return 0.0;
  else
  {
    if (inGraphView == graphView1)
    {
      float value = fmax(20.0, fmax(fabs(maxShift), fabs(minShift)));
      value = ((int)value/5) * 5 + 5; // on arrondie à 5
      return -1*value;
    }
    else 
      return 0.0;
  }
}

#pragma mark Surcharge des fonctions de listener de HCFRGraphController
-(void) entryAdded:(HCFRDataStoreEntry*)newEntry toSerie:(HCFRDataSerie*)serie forDataType:(HCFRDataType)dataType
{
  ColorRGB rgbMatrix;
  CColor tempColor;
    
   // Comme on calcule les valeur par rapport à la dernière valeur du graph, on doit retracer le
  // graph inégralement à chaque ajout.
  if (serie == [self currentSerie])
  {
    switch (dataType)
    {
      case kRedSaturationDataType:
        rgbMatrix[0]=255; rgbMatrix[1]=0; rgbMatrix[2]=0;
        tempColor.SetRGBValue(rgbMatrix, [[self colorReference] cColorReference]);
        
        [self computeDataForSerie:[self currentSerie] dataType:dataType
                toShiftValuesArray:[dataArrays objectAtIndex:kGraph1RedValues]
                   andDeltaEArray:[dataArrays objectAtIndex:kGraph2RedValues]
            defaultSaturatedColor:tempColor];
        break;
      case kGreenSaturationDataType:
        rgbMatrix[0]=0; rgbMatrix[1]=255; rgbMatrix[2]=0;
        tempColor.SetRGBValue(rgbMatrix, [[self colorReference] cColorReference]);

        [self computeDataForSerie:[self currentSerie] dataType:dataType
                toShiftValuesArray:[dataArrays objectAtIndex:kGraph1GreenValues]
                   andDeltaEArray:[dataArrays objectAtIndex:kGraph2GreenValues]
            defaultSaturatedColor:tempColor];
        break;
      case kBlueSaturationDataType:
        rgbMatrix[0]=0; rgbMatrix[1]=0; rgbMatrix[2]=255;
        tempColor.SetRGBValue(rgbMatrix, [[self colorReference] cColorReference]);
        
        [self computeDataForSerie:[self currentSerie] dataType:dataType
                toShiftValuesArray:[dataArrays objectAtIndex:kGraph1BlueValues]
                   andDeltaEArray:[dataArrays objectAtIndex:kGraph2BlueValues]
            defaultSaturatedColor:tempColor];
        break;
      default:
        // rien à faire là -> on lève une exception
        NSAssert1 (NO, @"HCFRSatShiftGraphController:entryAdded : invalide data typae :%d", dataType);
        break;
    }
  }
  // on ne gère pas l'ajout de valeurs à la courbe de référence : ça ne devrait pas arriver
  
  [graphView1 reloadData];
  [graphView2 reloadData];
}
-(void) serie:(HCFRDataSerie*)serie changedForDataType:(HCFRDataType)dataType
{
  ColorRGB rgbMatrix;
  CColor tempColor;
  
  if (serie == [self currentSerie])
  {
    switch (dataType)
    {
      case kRedSaturationDataType:
        rgbMatrix[0]=255; rgbMatrix[1]=0; rgbMatrix[2]=0;
        tempColor.SetRGBValue(rgbMatrix, [[self colorReference] cColorReference]);
        [self computeDataForSerie:[self currentSerie] dataType:dataType
                toShiftValuesArray:[dataArrays objectAtIndex:kGraph1RedValues]
                   andDeltaEArray:[dataArrays objectAtIndex:kGraph2RedValues]
            defaultSaturatedColor:tempColor];
        break;
      case kGreenSaturationDataType:
        rgbMatrix[0]=0; rgbMatrix[1]=255; rgbMatrix[2]=0;
        tempColor.SetRGBValue(rgbMatrix, [[self colorReference] cColorReference]);
        [self computeDataForSerie:[self currentSerie] dataType:dataType
                toShiftValuesArray:[dataArrays objectAtIndex:kGraph1GreenValues]
                   andDeltaEArray:[dataArrays objectAtIndex:kGraph2GreenValues]
            defaultSaturatedColor:tempColor];
        break;
      case kBlueSaturationDataType:
        rgbMatrix[0]=0; rgbMatrix[1]=0; rgbMatrix[2]=255;
        tempColor.SetRGBValue(rgbMatrix, [[self colorReference] cColorReference]);
        [self computeDataForSerie:[self currentSerie] dataType:dataType
                toShiftValuesArray:[dataArrays objectAtIndex:kGraph1BlueValues]
                   andDeltaEArray:[dataArrays objectAtIndex:kGraph2BlueValues]
            defaultSaturatedColor:tempColor];
        break;
      default:
        // rien à faire là -> on lève une exception
        NSAssert1 (NO, @"HCFRSatShiftGraphController:newSerieBegunForDataType : invalide data type :%d", dataType);
        break;
    }
  }
  
  // et on efface le graph
  [graphView1 reloadData];
  [graphView2 reloadData];
}
-(void) colorReferenceChanged:(HCFRColorReference*)oldReference
{
  ColorRGB rgbMatrix;
  CColor tempColor;

  // On recalcule les deux série (la série courante et la série de référence
  
  // Le rouge
  rgbMatrix[0]=255; rgbMatrix[1]=0; rgbMatrix[2]=0;
  tempColor.SetRGBValue(rgbMatrix, [[self colorReference] cColorReference]);
  [self computeDataForSerie:[self currentSerie] dataType:kRedSaturationDataType
          toShiftValuesArray:[dataArrays objectAtIndex:kGraph1RedValues]
             andDeltaEArray:[dataArrays objectAtIndex:kGraph2RedValues]
      defaultSaturatedColor:tempColor];
  [self computeDataForSerie:[self referenceSerie] dataType:kRedSaturationDataType
         toShiftValuesArray:[dataArrays objectAtIndex:kGraph1RedReferenceValues]
             andDeltaEArray:[dataArrays objectAtIndex:kGraph2RedReferenceValues]
      defaultSaturatedColor:tempColor];
  
  // Le vert
  rgbMatrix[0]=0; rgbMatrix[1]=255; rgbMatrix[2]=0;
  tempColor.SetRGBValue(rgbMatrix, [[self colorReference] cColorReference]);
  [self computeDataForSerie:[self currentSerie] dataType:kGreenSaturationDataType
          toShiftValuesArray:[dataArrays objectAtIndex:kGraph1GreenValues]
             andDeltaEArray:[dataArrays objectAtIndex:kGraph2GreenValues]
      defaultSaturatedColor:tempColor];
  [self computeDataForSerie:[self referenceSerie] dataType:kGreenSaturationDataType
         toShiftValuesArray:[dataArrays objectAtIndex:kGraph1GreenReferenceValues]
             andDeltaEArray:[dataArrays objectAtIndex:kGraph2GreenReferenceValues]
      defaultSaturatedColor:tempColor];

  // Le bleu
  rgbMatrix[0]=0; rgbMatrix[1]=0; rgbMatrix[2]=255;
  tempColor.SetRGBValue(rgbMatrix, [[self colorReference] cColorReference]);
  [self computeDataForSerie:[self currentSerie] dataType:kBlueSaturationDataType
          toShiftValuesArray:[dataArrays objectAtIndex:kGraph1BlueValues]
             andDeltaEArray:[dataArrays objectAtIndex:kGraph2BlueValues]
      defaultSaturatedColor:tempColor];
  [self computeDataForSerie:[self referenceSerie] dataType:kBlueSaturationDataType
         toShiftValuesArray:[dataArrays objectAtIndex:kGraph1BlueReferenceValues]
             andDeltaEArray:[dataArrays objectAtIndex:kGraph2BlueReferenceValues]
      defaultSaturatedColor:tempColor];
  
  [graphView1 reloadData];
  [graphView2 reloadData];
}

-(void) currentDataSerieChanged:(HCFRDataSerie*)oldSerie
{
  ColorRGB rgbMatrix;
  CColor tempColor;

  if (oldSerie != nil)
  {
    [oldSerie removeListener:self forType:kRedSaturationDataType];
    [oldSerie removeListener:self forType:kGreenSaturationDataType];
    [oldSerie removeListener:self forType:kBlueSaturationDataType];
  }
  if ([self currentSerie] != nil)
  {
    [[self currentSerie] addListener:self forType:kRedSaturationDataType];
    [[self currentSerie] addListener:self forType:kGreenSaturationDataType];
    [[self currentSerie] addListener:self forType:kBlueSaturationDataType];
  }
  
  rgbMatrix[0]=255; rgbMatrix[1]=0; rgbMatrix[2]=0;
  tempColor.SetRGBValue(rgbMatrix, [[self colorReference] cColorReference]);
  [self computeDataForSerie:[self currentSerie] dataType:kRedSaturationDataType
          toShiftValuesArray:[dataArrays objectAtIndex:kGraph1RedValues]
             andDeltaEArray:[dataArrays objectAtIndex:kGraph2RedValues]
      defaultSaturatedColor:tempColor];
  rgbMatrix[0]=0; rgbMatrix[1]=255; rgbMatrix[2]=0;
  tempColor.SetRGBValue(rgbMatrix, [[self colorReference] cColorReference]);
  [self computeDataForSerie:[self currentSerie] dataType:kGreenSaturationDataType
          toShiftValuesArray:[dataArrays objectAtIndex:kGraph1GreenValues]
             andDeltaEArray:[dataArrays objectAtIndex:kGraph2GreenValues]
      defaultSaturatedColor:tempColor];
  rgbMatrix[0]=0; rgbMatrix[1]=0; rgbMatrix[2]=255;
  tempColor.SetRGBValue(rgbMatrix, [[self colorReference] cColorReference]);
  [self computeDataForSerie:[self currentSerie] dataType:kBlueSaturationDataType
          toShiftValuesArray:[dataArrays objectAtIndex:kGraph1BlueValues]
             andDeltaEArray:[dataArrays objectAtIndex:kGraph2BlueValues]
      defaultSaturatedColor:tempColor];

  [graphView1 reloadData];
  [graphView2 reloadData];
}
-(void) referenceDataSerieChanged:(HCFRDataSerie*)oldSerie
{
  ColorRGB rgbMatrix;
  CColor tempColor;

  rgbMatrix[0]=255; rgbMatrix[1]=0; rgbMatrix[2]=0;
  tempColor.SetRGBValue(rgbMatrix, [[self colorReference] cColorReference]);
  [self computeDataForSerie:[self referenceSerie] dataType:kRedSaturationDataType
          toShiftValuesArray:[dataArrays objectAtIndex:kGraph1RedReferenceValues]
             andDeltaEArray:[dataArrays objectAtIndex:kGraph2RedReferenceValues]
      defaultSaturatedColor:tempColor];
  rgbMatrix[0]=255; rgbMatrix[1]=0; rgbMatrix[2]=0;
  tempColor.SetRGBValue(rgbMatrix, [[self colorReference] cColorReference]);
  [self computeDataForSerie:[self referenceSerie] dataType:kGreenSaturationDataType
          toShiftValuesArray:[dataArrays objectAtIndex:kGraph1GreenReferenceValues]
             andDeltaEArray:[dataArrays objectAtIndex:kGraph2GreenReferenceValues]
      defaultSaturatedColor:tempColor];
  rgbMatrix[0]=255; rgbMatrix[1]=0; rgbMatrix[2]=0;
  tempColor.SetRGBValue(rgbMatrix, [[self colorReference] cColorReference]);
  [self computeDataForSerie:[self referenceSerie] dataType:kBlueSaturationDataType
          toShiftValuesArray:[dataArrays objectAtIndex:kGraph1BlueReferenceValues]
             andDeltaEArray:[dataArrays objectAtIndex:kGraph2BlueReferenceValues]
      defaultSaturatedColor:tempColor];
  
  [graphView1 reloadData];
  [graphView2 reloadData];
}
@end
