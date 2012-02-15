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

#import "HCFRSatLumGraphController.h"


@implementation HCFRSatLumGraphController
-(HCFRSatLumGraphController*) init
{
  [super init];
  
  graphView = [[SM2DGraphView alloc] init];
  
  // on met les valeurs par defauts pour les prefs
  NSDictionary  *defaultPreferences = [NSDictionary dictionaryWithObjectsAndKeys:
    @"YES", @"SatLumGraphShowReference",
    @"YES", @"SatLumGraphShowRed",
    @"YES", @"SatLumGraphShowGreen",
    @"YES", @"SatLumGraphShowBlue",
    nil];
  [[NSUserDefaults standardUserDefaults] registerDefaults:defaultPreferences];
  
  // initialisation des tableaux de données des référence
  redReferenceData = nil;
  greenReferenceData = nil;
  blueReferenceData = nil;
  [self computeReferenceData];
  
  // initialisation des tableaux de données
  redData = [[NSMutableArray alloc] initWithCapacity:30];
  greenData = [[NSMutableArray alloc] initWithCapacity:30];
  blueData = [[NSMutableArray alloc] initWithCapacity:30];
  
  referenceSerieRedData = [[NSMutableArray alloc] initWithCapacity:30];
  referenceSerieGreenData = [[NSMutableArray alloc] initWithCapacity:30];
  referenceSerieBlueData = [[NSMutableArray alloc] initWithCapacity:30];
  
  // les options
  drawReferences = [[NSUserDefaults standardUserDefaults] boolForKey:@"SatLumGraphShowReference"];
  drawRed = [[NSUserDefaults standardUserDefaults] boolForKey:@"SatLumGraphShowRed"];
  drawGreen = [[NSUserDefaults standardUserDefaults] boolForKey:@"SatLumGraphShowGreen"];
  drawBlue = [[NSUserDefaults standardUserDefaults] boolForKey:@"SatLumGraphShowBlue"];

  // configuration du graph
  [graphView setDataSource:self];
  [graphView setBackgroundColor:[NSColor blackColor]];
  [graphView setDrawsGrid:YES];
  [graphView setGridColor:[NSColor grayColor]];
  [graphView setNumberOfTickMarks:11 forAxis:kSM2DGraph_Axis_X];
  [graphView setNumberOfTickMarks:11 forAxis:kSM2DGraph_Axis_Y];
  [graphView setLabel:HCFRLocalizedString(@"Saturation %", @"Saturation %") forAxis:kSM2DGraph_Axis_X];
  [graphView setLabel:HCFRLocalizedString(@"IRE",@"IRE") forAxis:kSM2DGraph_Axis_Y];
  [graphView reloadAttributes];
  [graphView reloadData];
  

  return self;
}
-(void) dealloc
{
  if (redReferenceData != nil)
    [redReferenceData release];
  if (greenReferenceData != nil)
    [greenReferenceData release];
  if (blueReferenceData != nil)
    [blueReferenceData release];

  [redData release];
  [greenData release];
  [blueData release];
  
  [referenceSerieRedData release];
  [referenceSerieGreenData release];
  [referenceSerieBlueData release];
  
  if ([self currentSerie])
  {
    [[self currentSerie] removeListener:self forType:kRedSaturationDataType];
    [[self currentSerie] removeListener:self forType:kGreenSaturationDataType];
    [[self currentSerie] removeListener:self forType:kBlueSaturationDataType];
  }
  
  [graphView release];
  
  [super dealloc];
}

#pragma mark Fonctions de calcul des données d'affichage
-(void) computeReferenceData
{
  NSPoint firstPoint;
  NSPoint lastPoint;
  
  double redReferenceLuma = [[self colorReference] redReferenceLuma];
  double greenReferenceLuma = [[self colorReference] greenReferenceLuma];
  double blueReferenceLuma = [[self colorReference] blueReferenceLuma];
  
  // les référence étant des droites, on utilise uniquement deux points
  
  // la référence rouge
  firstPoint = NSMakePoint(0, redReferenceLuma*100);
  lastPoint = NSMakePoint(100, redReferenceLuma*100);
  if (redReferenceData != nil)
    [redReferenceData release];
  redReferenceData = [[NSArray alloc] initWithObjects:NSStringFromPoint(firstPoint), NSStringFromPoint(lastPoint), nil];

  // la référence verte
  firstPoint = NSMakePoint(0, greenReferenceLuma*100);
  lastPoint = NSMakePoint(100, greenReferenceLuma*100);
  if (greenReferenceData != nil)
    [greenReferenceData release];
  greenReferenceData = [[NSArray alloc] initWithObjects:NSStringFromPoint(firstPoint), NSStringFromPoint(lastPoint), nil];

  // la référence bleue
  firstPoint = NSMakePoint(0, blueReferenceLuma*100);
  lastPoint = NSMakePoint(100, blueReferenceLuma*100);
  if (blueReferenceData != nil)
    [blueReferenceData release];
  blueReferenceData = [[NSArray alloc] initWithObjects:NSStringFromPoint(firstPoint), NSStringFromPoint(lastPoint), nil];
}

-(void) computeDataForSerie:(HCFRDataSerie*)serie withReference:(double)referenceLuma dataType:(HCFRDataType)dataType destinationArray:(NSMutableArray*)destination
{
  NSAssert (destination!=nil, @"HCFRSatLumGraphController:computDataForReference : nil destination !");
  
  [destination removeAllObjects];

  if (serie == nil)
    return;
  
  int nbEntries = [serie countForType:dataType];
  int index;
    
  double lastMeasureLuminance = [[(HCFRDataStoreEntry*)[serie lastEntryForType:dataType] value] luminance];
  if (lastMeasureLuminance == 0.0)
    return;
  double coef = referenceLuma*100.0/lastMeasureLuminance;
  
  for (index = 0; index < nbEntries; index ++)
  {
    HCFRDataStoreEntry  *currentEntry = [serie entryAtIndex:index forType:dataType];
    NSPoint             theNewPoint;
      
    theNewPoint.x=[[currentEntry referenceNumber] floatValue];
    theNewPoint.y=[[currentEntry value] luminance] * coef;
    [destination addObject:NSStringFromPoint(theNewPoint)];
  }
}

#pragma mark Surcharges des methodes de HCFRGraphController
-(GraphType) graphicType
{
  return kCalibrationGraph;
}
-(NSString*) graphicName
{
  return HCFRLocalizedString(@"Saturation/luminance",@"Saturation/luminance");
}
-(BOOL) hasAnOptionsView
{
  return NO; // TODO
}
-(NSView*) graphicView
{
  return graphView;
}

#pragma mark Implémentation de l'interface DataSource pour le graphique
- (unsigned int)numberOfLinesInTwoDGraphView:(SM2DGraphView *)inGraphView
{
  unsigned int numberOfLines = 0;
  
  if (drawReferences)
    numberOfLines += 3;
  if (drawRed)
    numberOfLines += 2; // la courbe courrante et la courbe de reference
  if (drawGreen)
    numberOfLines += 2; // la courbe courrante et la courbe de reference
  if (drawBlue)
    numberOfLines += 2; // la courbe courrante et la courbe de reference
    
  return numberOfLines;
}
- (NSDictionary *)twoDGraphView:(SM2DGraphView *)inGraphView attributesForLineIndex:(unsigned int)inLineIndex
{
  unsigned short  currentIndex = 0;
  if (drawReferences)
  {
    // on a trois références (rouge, vert et bleue) controlés par une seul variable
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor redColor] colorWithAlphaComponent:0.1], NSForegroundColorAttributeName,
//          [NSNumber numberWithInt:kSM2DGraph_Width_Fine], SM2DGraphLineWidthAttributeName,
        nil];
    currentIndex ++;
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor greenColor] colorWithAlphaComponent:0.1], NSForegroundColorAttributeName,
//        [NSNumber numberWithInt:kSM2DGraph_Width_Fine], SM2DGraphLineWidthAttributeName,
        nil];
    currentIndex ++;
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor blueColor] colorWithAlphaComponent:0.1], NSForegroundColorAttributeName,
//        [NSNumber numberWithInt:kSM2DGraph_Width_Fine], SM2DGraphLineWidthAttributeName,
        nil];
    currentIndex ++;
  }
  if (drawRed)
  {
    // la courbe courante
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor redColor], NSForegroundColorAttributeName,
        nil];
    currentIndex ++;
    
    // la courbe de ref
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor redColor] colorWithAlphaComponent:0.2], NSForegroundColorAttributeName,
        nil];
    currentIndex ++;
  }
  if (drawGreen)
  {
    // la courbe courante
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor greenColor], NSForegroundColorAttributeName,
        nil];
    currentIndex ++;

    // la courbe de ref
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor greenColor] colorWithAlphaComponent:0.2], NSForegroundColorAttributeName,
        nil];
    currentIndex ++;
  }
  if (drawBlue)
  {
    // la courbe courante
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[NSColor blueColor], NSForegroundColorAttributeName,
        nil];
    currentIndex ++;

    // la courbe de ref
    if (inLineIndex == currentIndex)
      return [NSDictionary dictionaryWithObjectsAndKeys:[[NSColor blueColor] colorWithAlphaComponent:0.2], NSForegroundColorAttributeName,
        nil];
    currentIndex ++;
  }
  
  NSAssert (NO, @"HCFRSatLumGraphController : no attributes for line !");
  return nil;
}
- (NSArray *)twoDGraphView:(SM2DGraphView *)inGraphView dataForLineIndex:(unsigned int)inLineIndex
{
  unsigned short  currentIndex = 0;
  if (drawReferences)
  {
    // on a trois références (rouge, vert et bleue) controlés par une seul variable
    if (inLineIndex == currentIndex)
      return redReferenceData;
    currentIndex ++;
    if (inLineIndex == currentIndex)
      return greenReferenceData;
    currentIndex ++;
    if (inLineIndex == currentIndex)
      return blueReferenceData;
    currentIndex ++;
  }
  if (drawRed)
  {
    if (inLineIndex == currentIndex)
      return redData;
    currentIndex ++;
    if (inLineIndex == currentIndex)
      return referenceSerieRedData;
    currentIndex ++;
  }
  if (drawGreen)
  {
    if (inLineIndex == currentIndex)
      return greenData;
    currentIndex ++;
    if (inLineIndex == currentIndex)
      return referenceSerieGreenData;
    currentIndex ++;
  }
  if (drawBlue)
  {
    if (inLineIndex == currentIndex)
      return blueData;
    currentIndex ++;
    if (inLineIndex == currentIndex)
      return referenceSerieBlueData;
    currentIndex ++;
  }

  NSAssert (NO, @"HCFRSatLumGraphController : no data for line !");
  return nil;
}
- (double)twoDGraphView:(SM2DGraphView *)inGraphView maximumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis
{
    return 100.0;
}
- (double)twoDGraphView:(SM2DGraphView *)inGraphView minimumValueForLineIndex:(unsigned int)inLineIndex
                forAxis:(SM2DGraphAxisEnum)inAxis
{
    return 0.0;
}

#pragma mark Surcharge des fonctions de listener de HCFRGraphController
-(void) entryAdded:(HCFRDataStoreEntry*)newEntry toSerie:(HCFRDataSerie*)serie forDataType:(HCFRDataType)dataType
{
//  NSLog (@"entryAdded");
  // Comme on calcule les valeur par rapport à la dernière valeur du graph, on doit retracer le
  // graph inégralement à chaque ajout.
  if (serie == [self currentSerie])
  {
    switch (dataType)
    {
      case kRedSaturationDataType:
        [self computeDataForSerie:[self currentSerie] withReference:[[self colorReference] redReferenceLuma] dataType:dataType destinationArray:redData];
        break;
      case kGreenSaturationDataType:
        [self computeDataForSerie:[self currentSerie] withReference:[[self colorReference] greenReferenceLuma] dataType:dataType destinationArray:greenData];
        break;
      case kBlueSaturationDataType:
        [self computeDataForSerie:[self currentSerie] withReference:[[self colorReference] blueReferenceLuma] dataType:dataType destinationArray:blueData];
        break;
      default:
        // rien à faire là -> on lève une exception
        NSAssert1 (NO, @"HCFRSatLumGraphController:entryAdded : invalide data type :%d", dataType);
        break;
    }
  }
  
  [graphView reloadData];
}
-(void) serie:(HCFRDataSerie*)serie changedForDataType:(HCFRDataType)dataType
{
//  NSLog (@"serie changed");
  if (serie == [self currentSerie])
  {
    // on supprime tous les objets
    switch (dataType)
    {
      case kRedSaturationDataType:
        [self computeDataForSerie:[self currentSerie] withReference:[[self colorReference] redReferenceLuma] dataType:dataType destinationArray:redData];
        break;
      case kGreenSaturationDataType:
        [self computeDataForSerie:[self currentSerie] withReference:[[self colorReference] greenReferenceLuma] dataType:dataType destinationArray:greenData];
        break;
      case kBlueSaturationDataType:
        [self computeDataForSerie:[self currentSerie] withReference:[[self colorReference] blueReferenceLuma] dataType:dataType destinationArray:blueData];
        break;
      default:
        // rien à faire là -> on lève une exception
        NSAssert1 (NO, @"HCFRSatLumGraphController:newSerieBegunForDataType : invalide data typae :%d", dataType);
        break;
    }
  }
  
  // et on efface le graph
  [graphView reloadData];
}
-(void) colorReferenceChanged:(HCFRColorReference*)oldReference
{
  [self computeReferenceData];
  
  // la série courante
  [self computeDataForSerie:[self currentSerie] withReference:[[self colorReference] redReferenceLuma] dataType:kRedSaturationDataType destinationArray:redData];
  [self computeDataForSerie:[self currentSerie] withReference:[[self colorReference] greenReferenceLuma] dataType:kGreenSaturationDataType destinationArray:greenData];
  [self computeDataForSerie:[self currentSerie] withReference:[[self colorReference] blueReferenceLuma] dataType:kBlueSaturationDataType destinationArray:blueData];

  // la série de référence
  [self computeDataForSerie:[self referenceSerie]
              withReference:[[self colorReference] redReferenceLuma]
                   dataType:kRedSaturationDataType
           destinationArray:referenceSerieRedData];
  [self computeDataForSerie:[self referenceSerie]
              withReference:[[self colorReference] greenReferenceLuma]
                   dataType:kGreenSaturationDataType
           destinationArray:referenceSerieGreenData];
  [self computeDataForSerie:[self referenceSerie]
              withReference:[[self colorReference] blueReferenceLuma]
                   dataType:kBlueSaturationDataType
           destinationArray:referenceSerieBlueData];

  [graphView reloadData];
}

-(void) currentDataSerieChanged:(HCFRDataSerie*)oldSerie
{
//  NSLog (@"current serie changed");
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
  
  [self computeDataForSerie:[self currentSerie] withReference:[[self colorReference] redReferenceLuma] dataType:kRedSaturationDataType destinationArray:redData];
  [self computeDataForSerie:[self currentSerie] withReference:[[self colorReference] greenReferenceLuma] dataType:kGreenSaturationDataType destinationArray:greenData];
  [self computeDataForSerie:[self currentSerie] withReference:[[self colorReference] blueReferenceLuma] dataType:kBlueSaturationDataType destinationArray:blueData];
  [graphView reloadData];
}
-(void) referenceDataSerieChanged:(HCFRDataSerie*)oldSerie
{
  [self computeDataForSerie:[self referenceSerie]
              withReference:[[self colorReference] redReferenceLuma]
                   dataType:kRedSaturationDataType
           destinationArray:referenceSerieRedData];
  [self computeDataForSerie:[self referenceSerie]
              withReference:[[self colorReference] greenReferenceLuma]
                   dataType:kGreenSaturationDataType
           destinationArray:referenceSerieGreenData];
  [self computeDataForSerie:[self referenceSerie]
              withReference:[[self colorReference] blueReferenceLuma]
                   dataType:kBlueSaturationDataType
           destinationArray:referenceSerieBlueData];

  [graphView reloadData];
}
@end
