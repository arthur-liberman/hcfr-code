//  ColorHCFR
//  HCFRDataStoreIOCategory.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 26/12/07.
//
//  $Rev: 135 $
//  $Date: 2011-02-27 10:48:51 +0000 (Sun, 27 Feb 2011) $
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

#import <HCFRPlugins/HCFRUtilities.h>
#import <libHCFR/CHCFile.h>
#import "HCFRDataSerie+IO.h"

@implementation HCFRDataSerie (IO)

-(id) initWithCoder:(NSCoder*)coder
{
  // on initialize
  self = [self init];
  if (self != nil) {
    name = [[coder decodeObjectForKey:@"name"] retain];
    notes = [[coder decodeObjectForKey:@"notes"] retain];
    
    [dataStore setObject:[coder decodeObjectForKey:@"luminanceDataArray"]
                  forKey:[NSNumber numberWithInt:kLuminanceDataType]];
    [dataStore setObject:[coder decodeObjectForKey:@"fullscreenContrastDataArray"]
                  forKey:[NSNumber numberWithInt:kFullScreenContrastDataType]];
    [dataStore setObject:[coder decodeObjectForKey:@"ANSIContrastDataArray"]
                  forKey:[NSNumber numberWithInt:kANSIContrastDataType]];
    [dataStore setObject:[coder decodeObjectForKey:@"redSaturationDataArray"]
                  forKey:[NSNumber numberWithInt:kRedSaturationDataType]];
    [dataStore setObject:[coder decodeObjectForKey:@"greenSaturationDataArray"]
                  forKey:[NSNumber numberWithInt:kGreenSaturationDataType]];
    [dataStore setObject:[coder decodeObjectForKey:@"blueSaturationDataArray"]
                  forKey:[NSNumber numberWithInt:kBlueSaturationDataType]];
    [dataStore setObject:[coder decodeObjectForKey:@"componentsDataArray"]
                  forKey:[NSNumber numberWithInt:kComponentsDataType]];
    if ([coder containsValueForKey:@"continuousDataArray"])
    {
      [dataStore setObject:[coder decodeObjectForKey:@"continuousDataArray"]
                    forKey:[NSNumber numberWithInt:kContinuousDataType]];
    }
    else {
      [dataStore setObject:[NSMutableArray arrayWithCapacity:25] forKey:[NSNumber numberWithInt:kContinuousDataType]];

    }

    dirty = NO;
  }
  return self;
}

-(void) encodeWithCoder:(NSCoder*)coder
{
  [coder encodeObject:name forKey:@"name"];
  [coder encodeObject:notes forKey:@"notes"];

  [coder encodeObject:[dataStore objectForKey:[NSNumber numberWithInt:kLuminanceDataType]]
               forKey:@"luminanceDataArray"];
  [coder encodeObject:[dataStore objectForKey:[NSNumber numberWithInt:kFullScreenContrastDataType]]
               forKey:@"fullscreenContrastDataArray"];
  [coder encodeObject:[dataStore objectForKey:[NSNumber numberWithInt:kANSIContrastDataType]]
               forKey:@"ANSIContrastDataArray"];
  [coder encodeObject:[dataStore objectForKey:[NSNumber numberWithInt:kRedSaturationDataType]]
               forKey:@"redSaturationDataArray"];
  [coder encodeObject:[dataStore objectForKey:[NSNumber numberWithInt:kGreenSaturationDataType]]
               forKey:@"greenSaturationDataArray"];
  [coder encodeObject:[dataStore objectForKey:[NSNumber numberWithInt:kBlueSaturationDataType]]
               forKey:@"blueSaturationDataArray"];
  [coder encodeObject:[dataStore objectForKey:[NSNumber numberWithInt:kComponentsDataType]]
               forKey:@"componentsDataArray"];
  [coder encodeObject:[dataStore objectForKey:[NSNumber numberWithInt:kContinuousDataType]]
               forKey:@"continuousDataArray"];

  [self setDirty:NO];
}
+(NSArray*) importableExtensions
{
  return [NSArray arrayWithObjects:@"csv", @"chc", nil];
}

-(void) importFile:(NSString*)filePath
{
  if ([filePath hasSuffix:@"csv"])
    [self loadDataFromCSVFile:filePath];
  else if ([filePath hasSuffix:@"chc"])
    [self loadDataFromCHCFile:filePath];
  else
    [[NSException exceptionWithName:kHCFRFileFormatException
                             reason:HCFRLocalizedString(@"Unknown file format.", @"Unknown file format.")
                           userInfo:nil] raise];
}

-(void) loadDataFromCSVFile:(NSString*)filePath
{
  unsigned short  importedData = 0;
  FILE            *dataFile = fopen ([filePath cStringUsingEncoding:NSUTF8StringEncoding], "r");
  if (dataFile == NULL)
    return;
  char  currentLine[255];
  
  while (fgets(currentLine, 255, dataFile))
  {
    unsigned short  elementIndex = 0;
    char            *string = currentLine;
    char            *currentElement;
    int             type = 0;
    double          refValue;
    double          x, y, z;
    int             result = 0;
    
    while ((currentElement = strsep (&string, ",")))
    {
      switch (elementIndex)
      {
        case 0 :
          result += sscanf (currentElement, "%d", &type);
          break;
        case 1 :
          result += sscanf (currentElement, "%lf", &refValue);
          break;
        case 2 :
          result += sscanf (currentElement, "%lf", &x);
          break;
        case 3 :
          result += sscanf (currentElement, "%lf", &y);
          break;
        case 4 :
          result += sscanf (currentElement, "%lf", &z);
          break;
      }
      elementIndex ++;
    }
    
    if (result == 5)
    {
      // on a tous les elements, on crée l'entrée
      Matrix    newColorMatrix = Matrix(0.1, 3, 1);
      newColorMatrix[0][0] = x;
      newColorMatrix[1][0] = y;
      newColorMatrix[2][0] = z;
      HCFRColor           *color = [[HCFRColor alloc] initWithMatrix:newColorMatrix];
      
      [self addMeasure:color
                 withReferenceNumber:[NSNumber numberWithDouble:refValue]
                             forType:(HCFRDataType)type];
      importedData ++;
      
//      NSLog (@"HCFRDataStore-IO : loading value for type %d, reference %lf", type, refValue);
      
      [color release];
    }
    
  }

  fclose (dataFile);
//  NSLog (@"Datastore IO->loadDataFromCSVFile : %d donnees chargees", importedData);
}

-(void) loadDataFromCHCFile:(NSString*)filePath
{
  NSAssert([filePath hasSuffix:@".chc"], @"HCFRDataSerie->loadDataFromCHCFile : file do have a chc extension");
  try
  {
    CHCFile fileReader;
    
    fileReader.readFile([filePath cStringUsingEncoding:NSUTF8StringEncoding]);
    
//    HCFRDataSerie *importedSerie = [seriesTableController addNewSerieWithName:HCFRLocalizedString(@"Imported serie", @"Imported serie")];
    
    [self loadColorsList:fileReader.getGrayColors()
                  ofType:kLuminanceDataType
     referenceMultiplier:10
          referenceShift:0];
    [self loadColorsList:fileReader.getNearBlackColors()
                  ofType:kLuminanceDataType
     referenceMultiplier:1
          referenceShift:0];
    [self loadColorsList:fileReader.getNearWhiteColors()
                  ofType:kLuminanceDataType
     referenceMultiplier:1
          referenceShift:96];
    [self loadColorsList:fileReader.getRedSaturationColors()
                  ofType:kRedSaturationDataType
     referenceMultiplier:25
          referenceShift:0];    
    [self loadColorsList:fileReader.getGreenSaturationColors()
                  ofType:kGreenSaturationDataType
     referenceMultiplier:25
          referenceShift:0];        
    [self loadColorsList:fileReader.getBlueSaturationColors()
                  ofType:kBlueSaturationDataType
     referenceMultiplier:25
          referenceShift:0];        
    /*    [self loadColorsList:fileReader.getCyanSaturationColors()
ofType:kCyanSaturationDataType
toDataSerie:importedSerie];
[self loadColorsList:fileReader.getMagentaSaturationColors()
              ofType:kMagentaSaturationDataType
[self loadColorsList:fileReader.getYellowSaturationColors()
              ofType:kYellowSaturationDataType
         toDataSerie:importedSerie];*/
    
    int componnentsMappingArray[6] = {kRedComponent, kGreenComponent, kBlueComponent, kYellowComponent, kCyanComponent, kMagentaComponent};
    [self loadColorsList:fileReader.getComponnentsColors()
                  ofType:kComponentsDataType
            mappingArray:componnentsMappingArray];        
    [self loadColorsList:fileReader.getFullscreenContrastColors()
                  ofType:kFullScreenContrastDataType
     referenceMultiplier:1
          referenceShift:0];        
    [self loadColorsList:fileReader.getANSIContrastColors()
                  ofType:kANSIContrastDataType
     referenceMultiplier:1
          referenceShift:0];
  }
  catch (...)
  {
    // TODO : catchall vide -> go to hell !
  }
}

-(void) saveDataToCSVFile:(NSString*)filePath
{
  FILE  *dataFile = fopen ([filePath cStringUsingEncoding:NSUTF8StringEncoding], "w+");
  int   nbEntries;
  int   typeIndex, index; // pour les boucles for
  
  if (dataFile == NULL)
  {
    NSString *reason = [NSString stringWithFormat:HCFRLocalizedString(@"The destination file (%@) could not be opened for writing.",
                                                                      @"The destination file (%@) could not be opened for writing."), filePath];
    [[NSException exceptionWithName:kHCFRDataStoreSaveFailureException
                            reason:reason
                           userInfo:nil] raise];
  }
  
  const HCFRDataType types[] = {kLuminanceDataType, kFullScreenContrastDataType, kANSIContrastDataType, kRedSaturationDataType, kGreenSaturationDataType, kBlueSaturationDataType, kComponentsDataType, kContinuousDataType};
  const unsigned short nbTypes = 8;  
  
  for (typeIndex = 0; typeIndex < nbTypes; typeIndex ++)
  {
    nbEntries =[self countForType:types[typeIndex]];
    for (index = 0; index < nbEntries; index ++)
    {
      HCFRDataStoreEntry *currentEntry = [self entryAtIndex:index forType:types[typeIndex]];
      fprintf(dataFile, "%d,%lf,%lf,%lf,%lf\n", types[typeIndex],
                                      [[currentEntry referenceNumber] doubleValue],
                                      [[currentEntry value] X],
                                      [[currentEntry value] Y],
                                      [[currentEntry value] Z]);
    }
  }

  fclose (dataFile);
}

#pragma fonctions utilitaires
-(void) loadColorsList:(list<CColor>)list ofType:(HCFRDataType)dataType
   referenceMultiplier:(int)multiplier referenceShift:(int)shift
{
  std::list<CColor>::iterator iterator;
  int loopIndex = 0;
  for(iterator=list.begin(); iterator != list.end(); iterator++)
  {
    HCFRColor *newHCFRColor = [(HCFRColor*)[HCFRColor alloc] initWithColor:(*iterator)];
    [self addMeasure:newHCFRColor withReferenceNumber:[NSNumber numberWithInt:loopIndex*multiplier+shift] forType:dataType];
    [newHCFRColor release];
    loopIndex ++;
  }
}
-(void) loadColorsList:(list<CColor>)list ofType:(HCFRDataType)dataType
   mappingArray:(int[])mapping
{
  std::list<CColor>::iterator iterator;
  int loopIndex = 0;
  for(iterator=list.begin(); iterator != list.end(); iterator++)
  {
    HCFRColor *newHCFRColor = [(HCFRColor*)[HCFRColor alloc] initWithColor:(*iterator)];
    [self addMeasure:newHCFRColor withReferenceNumber:[NSNumber numberWithInt:mapping[loopIndex]] forType:dataType];
    [newHCFRColor release];
    loopIndex ++;
  }
}
@end
