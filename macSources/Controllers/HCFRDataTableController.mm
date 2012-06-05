//  ColorHCFR
//  HCFRDataTableController.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 10/09/07.
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

#import "HCFRDataTableController.h"
#import "HCFRConstants.h"


@implementation HCFRDataTableController
-(HCFRDataTableController*) init
{
  [super init];
    
  currentSerie = nil;
  currentMode = kXYZDisplayMode;
  currentDataType = kLuminanceDataType;
  colorReference = nil;
  
  return self;
}

-(void) awakeFromNib
{
  [dataTypesPopup removeAllItems];
  
  // on ajoute les dataTypes en dur dans le code, pour pouvoir utiliser les constantes définies pour les types
  NSMenuItem  * newMenuItem;
  
  newMenuItem = [[NSMenuItem alloc] init];
  [newMenuItem setTitle:HCFRLocalizedString(@"Grayscale",@"Grayscale")];
  [newMenuItem setRepresentedObject:[NSNumber numberWithInt:kLuminanceDataType]];
  [[dataTypesPopup menu] addItem:newMenuItem];
  [newMenuItem release];
  
  newMenuItem = [[NSMenuItem alloc] init];
  [newMenuItem setTitle:HCFRLocalizedString(@"Fullscreen contrast",@"Fullscreen contrast")];
  [newMenuItem setRepresentedObject:[NSNumber numberWithInt:kFullScreenContrastDataType]];
  [[dataTypesPopup menu] addItem:newMenuItem];
  [newMenuItem release];
  
  newMenuItem = [[NSMenuItem alloc] init];
  [newMenuItem setTitle:HCFRLocalizedString(@"ANSI Contraste",@"ANSI Contraste")];
  [newMenuItem setRepresentedObject:[NSNumber numberWithInt:kANSIContrastDataType]];
  [[dataTypesPopup menu] addItem:newMenuItem];
  [newMenuItem release];
  
  newMenuItem = [[NSMenuItem alloc] init];
  [newMenuItem setTitle:HCFRLocalizedString(@"Red saturation",@"Red saturation")];
  [newMenuItem setRepresentedObject:[NSNumber numberWithInt:kRedSaturationDataType]];
  [[dataTypesPopup menu] addItem:newMenuItem];
  [newMenuItem release];
  
  newMenuItem = [[NSMenuItem alloc] init];
  [newMenuItem setTitle:HCFRLocalizedString(@"Green saturation",@"Green saturation")];
  [newMenuItem setRepresentedObject:[NSNumber numberWithInt:kGreenSaturationDataType]];
  [[dataTypesPopup menu] addItem:newMenuItem];
  [newMenuItem release];
  
  newMenuItem = [[NSMenuItem alloc] init];
  [newMenuItem setTitle:HCFRLocalizedString(@"Blue saturation",@"Blue saturation")];
  [newMenuItem setRepresentedObject:[NSNumber numberWithInt:kBlueSaturationDataType]];
  [[dataTypesPopup menu] addItem:newMenuItem];
  [newMenuItem release];
  
  newMenuItem = [[NSMenuItem alloc] init];
  [newMenuItem setTitle:HCFRLocalizedString(@"Componnents (RGBCMY)",@"Componnents (RGBCMY)")];
  [newMenuItem setRepresentedObject:[NSNumber numberWithInt:kComponentsDataType]];
  [[dataTypesPopup menu] addItem:newMenuItem];
  [newMenuItem release];
  
  [dataTypesPopup selectItemAtIndex:0];
  
  [grayscaleTable setAllowsMultipleSelection:YES];
  [grayscaleTable setDraggingSourceOperationMask:NSDragOperationCopy forLocal:NO];
  [grayscaleTable registerForDraggedTypes:[NSArray arrayWithObjects:NSStringPboardType, NSTabularTextPboardType, nil]];
  
  // on simule un changment de type de donnée
  // pour provoquer le suivi des valeurs
  [self changeDataToDisplay:dataTypesPopup];
}

-(void) dealloc
{
  if (currentSerie != nil)
  {
    [currentSerie removeListener:self forType:currentDataType];
    [currentSerie removeListener:self forType:kANSIContrastDataType];
    [currentSerie removeListener:self forType:kFullScreenContrastDataType];
    [currentSerie release];
    currentSerie = nil;
  }
  
  [colorReference release];
  
  [super dealloc];
}

-(void) setDataSerie:(HCFRDataSerie*)newSerie
{
  // On s'inscrit pour le suivi des données de contrast (pour mettre à jour
  // les valeurs de contrast calculées) ainsi que pour le type de données
  // actuellement séléctionnée.
  if (currentSerie != nil)
  {
    [currentSerie removeListener:self forType:currentDataType];
    [currentSerie removeListener:self forType:kFullScreenContrastDataType];
    [currentSerie removeListener:self forType:kANSIContrastDataType];
    [currentSerie release];
  }
  currentSerie = [newSerie retain];

  [newSerie addListener:self forType:currentDataType];
  [newSerie addListener:self forType:kFullScreenContrastDataType];
  [newSerie addListener:self forType:kANSIContrastDataType];
  
  [self reloadDataFromCurrentSerie];
}

-(void) updateColumnsForCurrentMode
{
  NSString  *xTitle, *yTitle, *zTitle, *value;
  
  // en fonction du mode d'affichage
  if (currentMode == kRGBDisplayMode)
  {
    xTitle = HCFRLocalizedString(@"R",@"R");
    yTitle = HCFRLocalizedString(@"G",@"G");
    zTitle = HCFRLocalizedString(@"B",@"B");
  }
  else if (currentMode == kXYZDisplayMode)
  {
    xTitle = @"X";
    yTitle = @"Y";
    zTitle = @"Z";
  }
  else if (currentMode == kxyYDisplayMode)
  {
    xTitle = @"x";
    yTitle = @"y";
    zTitle = @"Y";
  }
  else if (currentMode == kSensorDisplayMode)
  {
    xTitle = HCFRLocalizedString(@"R",@"R");
    yTitle = HCFRLocalizedString(@"G",@"G");
    zTitle = HCFRLocalizedString(@"B",@"B");
  }
  else
  {
    xTitle = @"?";
    yTitle = @"?";
    zTitle = @"?";
  }

  // en fonction du type de valeurs affiché
  switch (currentDataType)
  {
    case kLuminanceDataType :
      value = @"IRE";
      break;
    case kRedSaturationDataType :
    case kBlueSaturationDataType :
    case kGreenSaturationDataType :
      value = @"%";
      break;
    default :
      value = @"";
      break;
  }

  [[[grayscaleTable tableColumnWithIdentifier:@"ref"] headerCell] setStringValue:value];
  [[[grayscaleTable tableColumnWithIdentifier:@"X"] headerCell] setStringValue:xTitle];
  [[[grayscaleTable tableColumnWithIdentifier:@"Y"] headerCell] setStringValue:yTitle];
  [[[grayscaleTable tableColumnWithIdentifier:@"Z"] headerCell] setStringValue:zTitle];
}

#pragma mark Actions IB
- (IBAction)changeDataToDisplay:(NSPopUpButton*)sender
{
  // on observe en continue les données de contrast pour pouvoir mettre à jour
  // les valeurs de contraste calculées
  if (currentSerie != nil && currentDataType != kANSIContrastDataType && currentDataType != kFullScreenContrastDataType)
    [currentSerie removeListener:self forType:currentDataType];

  currentDataType = (HCFRDataType)[[[dataTypesPopup selectedItem] representedObject] intValue];
  
  // on n'a pas besoin de s'inscrire si on demande les valeurs de mesures pour les contrasts :
  // on y est toujours inscrit.
  if (currentSerie != nil && currentDataType != kANSIContrastDataType && currentDataType != kFullScreenContrastDataType)
    [currentSerie addListener:self forType:currentDataType];

  [self updateColumnsForCurrentMode];
  [grayscaleTable reloadData];
}
- (IBAction)changeDisplayMode:(NSPopUpButton*)sender
{
  currentMode = [[sender selectedItem] tag];
  [self updateColumnsForCurrentMode];
  [grayscaleTable reloadData];
}


#pragma mark TableView data source
- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
  if (currentSerie == nil)
    return 0;
  
  if (aTableView == grayscaleTable)
    return [currentSerie countForType:currentDataType];

  return 0;
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
  if (currentSerie == nil)
    return @"N/A";
  
  if (aTableView == grayscaleTable)
  {
    NSAssert2(rowIndex < [currentSerie countForType:currentDataType], @"Entry #%d not available for type %d", rowIndex, currentDataType);
    HCFRDataStoreEntry  *theEntry = [currentSerie entryAtIndex:rowIndex forType:currentDataType];

    Matrix theColor;
    if (currentMode == kXYZDisplayMode)
      theColor = [[theEntry value] XYZColor];
    else if (currentMode == kRGBDisplayMode)
      theColor = [[theEntry value] RGBColorWithColorReference:colorReference];
    else if (currentMode == kSensorDisplayMode)
      theColor = [[theEntry value] XYZColor];
    else if (currentMode == kxyYDisplayMode)
      theColor = [[theEntry value] xyYColor];
    
    // les identifiants des colonnes sont toujours les mêmes quel que soit le mode d'affichage : ref, X, Y et Z.
    // Petite incohérence, mais c'est pas trop grave vu que c'est expliqué :-)
    if ([@"ref" isEqualToString:[aTableColumn identifier]])
    {
      int refValue = [[theEntry referenceNumber] intValue];
      switch (currentDataType)
      {
        case kLuminanceDataType :
        case kRedSaturationDataType :
        case kBlueSaturationDataType :
        case kGreenSaturationDataType :
          return [theEntry referenceNumber];
          break;
        case kComponentsDataType :
          switch (refValue)
          {
            case 0 :
              return HCFRLocalizedString(@"Red",@"Red");
              break;
            case 1 :
              return HCFRLocalizedString(@"Green",@"Green");
              break;
            case 2 :
              return HCFRLocalizedString(@"Blue",@"Blue");
              break;
            case 3 :
              return HCFRLocalizedString(@"Cyan",@"Cyan");
              break;
            case 4 :
              return HCFRLocalizedString(@"Magenta",@"Magenta");
              break;
            case 5 :
              return HCFRLocalizedString(@"Yellow",@"Yellow");
              break;
          }
          break;
        case kANSIContrastDataType :
        case kFullScreenContrastDataType :
          switch (refValue)
          {
            case 0 :
              return HCFRLocalizedString(@"Black",@"Black");
              break;
            case 1 :
              return HCFRLocalizedString(@"White",@"White");
              break;
          }
          break;
        default :
          return @"???";
          break;
      }
    }
    else if ([@"X" isEqualToString:[aTableColumn identifier]])
      return [NSNumber numberWithDouble:theColor[0][0]];
    else if ([@"Y" isEqualToString:[aTableColumn identifier]])
      return [NSNumber numberWithDouble:theColor[1][0]];
    else if ([@"Z" isEqualToString:[aTableColumn identifier]])
      return [NSNumber numberWithDouble:theColor[2][0]];
  }
  
  return @"N/A";
}

-(void) reloadDataFromCurrentSerie
{
  [grayscaleTable reloadData];
  [self reloadFullscreenContrastData];
  [self reloadANSIContrastData];
}

-(void) reloadANSIContrastData
{
  if ( (currentSerie == nil) || ([currentSerie countForType:kFullScreenContrastDataType] == 0) )
  {
    [ansiContrastTextField setStringValue:@"N/A"];
    return;
  }
    
  HCFRDataStoreEntry  *firstEntry = [currentSerie entryAtIndex:0 forType:kANSIContrastDataType];
  HCFRDataStoreEntry  *lastEntry = [currentSerie lastEntryForType:kANSIContrastDataType];
  // n'utilise les valeurs que si on à les valeurs pour les réferences 0 et 100
  if ([[firstEntry referenceNumber] intValue] == 0 && [[lastEntry referenceNumber] intValue] == 1)
  {
    double max = [[lastEntry value] luminance:YES];
    double min = [[firstEntry value] luminance:YES];
    
    if (min == 0)
      [ansiContrastTextField setStringValue:@"inf"];
    else
    {
      int ratio = (int)(max/min);
      [ansiContrastTextField setStringValue:[NSString stringWithFormat:@"%i:1", ratio]];
    }
  }
  else
    [ansiContrastTextField setStringValue:@"N/A"];

}

-(void) reloadFullscreenContrastData
{
  if ( (currentSerie == nil) || ([currentSerie countForType:kFullScreenContrastDataType] == 0) )
  {
    [minLuminosityTextField setStringValue:@"N/A"];
    [maxLuminosityTextField setStringValue:@"N/A"];
    [onOffContrastTextField setStringValue:@"N/A"];

    return;
  }

  HCFRDataStoreEntry  *firstEntry = [currentSerie entryAtIndex:0 forType:kFullScreenContrastDataType];
  HCFRDataStoreEntry  *lastEntry = [currentSerie lastEntryForType:kFullScreenContrastDataType];
  
  if ([[firstEntry referenceNumber] intValue] == 0)
    [minLuminosityTextField setStringValue:[NSString stringWithFormat:@"%f", [[firstEntry value] luminance:YES]]];
  else
    [minLuminosityTextField setStringValue:@"N/A"];
  
  if ([[lastEntry referenceNumber] intValue] == 1)
    [maxLuminosityTextField setStringValue:[NSString stringWithFormat:@"%f", [[lastEntry value] luminance:YES]]];
  else
    [maxLuminosityTextField setStringValue:@"N/A"];
  
  // n'utilise les valeurs que si on à les valeurs pour les réferences 0 et 100
  if ([[firstEntry referenceNumber] intValue] == 0 && [[lastEntry referenceNumber] intValue] == 1)
  {
    double max = [[lastEntry value] luminance:YES];
    double min = [[firstEntry value] luminance:YES];
    
    if (min == 0)
      [onOffContrastTextField setStringValue:@"inf"];
    else
    {
      int ratio = (int)(max/min);
      [onOffContrastTextField setStringValue:[NSString stringWithFormat:@"%i:1", ratio]];
    }
  }
  else
    [onOffContrastTextField setStringValue:@"N/A"];
    
}

#pragma mark copier/coller & drag/drop pour la table 
- (BOOL)tableView:(NSTableView *)tableView writeRowsWithIndexes:(NSIndexSet *)rowIndexes toPasteboard:(NSPasteboard*)pasteboard
{
  NSArray         *tableColumns = [tableView tableColumns];
  NSMutableString *textBuf = [NSMutableString string];
  unsigned int    bufSize = [rowIndexes count];
  unsigned int    *buf = new unsigned int[bufSize];

  bufSize = [rowIndexes getIndexes:buf maxCount:bufSize inIndexRange:nil];
  
  for(int i = 0; i < bufSize; i++)
  {
    int row = buf[i];
    
    NSEnumerator *colEnum = [tableColumns objectEnumerator];
    NSTableColumn *col;
    while (nil != (col = [colEnum nextObject]) )
    {
      NSString *columnValue  = [self tableView:tableView objectValueForTableColumn:col row:row];
      NSString *columnString = @"";
      if (columnValue != nil)
      {
        columnString = [columnValue description];
      }
      [textBuf appendFormat:@"%@\t",columnString];
    }
    // delete the last tab.
    if ([textBuf length] > 0)
    {
      [textBuf deleteCharactersInRange: NSMakeRange([textBuf length]-1, 1)];
    }
    // Append newlines to both tabular and newline data
    [textBuf appendString:@"\n"];
  }
  // Delete the final newlines from the text and tabs buf.
  if ([textBuf length])
  {
    [textBuf deleteCharactersInRange:
      NSMakeRange([textBuf length]-1, 1)];
  }
  // Set up the pasteboard
  [pasteboard declareTypes: [NSArray arrayWithObjects:NSTabularTextPboardType, NSStringPboardType, nil] owner:nil];
  // Put the data into the pasteboard for our various types
  [pasteboard setString:[NSString stringWithString:textBuf] forType:NSStringPboardType];
  [pasteboard setString:[NSString stringWithString:textBuf] forType:NSTabularTextPboardType];

  delete(buf);
  
  return YES;
}

#pragma mark Implémentation de DataStoreListener
-(void) entryAdded:(HCFRDataStoreEntry*)newEntry toSerie:(HCFRDataSerie*)serie forDataType:(HCFRDataType)dataType
{
  if (serie != currentSerie)
    return;
  if (dataType == currentDataType)
    [grayscaleTable reloadData];
  else if (dataType == kANSIContrastDataType)
    [self reloadANSIContrastData];
  else if (dataType == kFullScreenContrastDataType)
    [self reloadFullscreenContrastData];
}
-(void) serie:(HCFRDataSerie*)serie changedForDataType:(HCFRDataType)dataType
{
  if (serie != currentSerie)
    return;
  if (dataType == currentDataType)
    [grayscaleTable reloadData];
  else if (dataType == kANSIContrastDataType)
    [self reloadANSIContrastData];
  else if (dataType == kFullScreenContrastDataType)
    [self reloadFullscreenContrastData];
}
-(void) setColorReference:(HCFRColorReference*)newReference
{
  NSAssert (newReference != nil, @"HCFRDataTableController : invalide nil color reference in setColorReference");
  [colorReference release];
  colorReference = [newReference retain];
  
  [grayscaleTable reloadData];
  [ansiContrastTextField setStringValue:@"N/A"];
  [ansiContrastTextField setStringValue:@"N/A"];
}
#pragma mark fonctions utilitaires
-(BOOL)canCopy
{
  return ([self numberOfRowsInTableView:grayscaleTable] > 0);
}
-(void) copyToPasteboard
{
  NSIndexSet  *selectedRows = [grayscaleTable selectedRowIndexes];
  if ([selectedRows count] > 0)
    [self tableView:grayscaleTable writeRowsWithIndexes:selectedRows toPasteboard:[NSPasteboard generalPasteboard]];
  else
  {
    NSIndexSet *allIndexesSet = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(0,[self numberOfRowsInTableView:grayscaleTable])];
    [self tableView:grayscaleTable writeRowsWithIndexes:allIndexesSet toPasteboard:[NSPasteboard generalPasteboard]];    
  }
}
@end
