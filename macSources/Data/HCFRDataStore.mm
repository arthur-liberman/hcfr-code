//  ColorHCFR
//  HCFRDataStore.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/08/07.
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

#import "HCFRDataStore.h"
#import "HCFRConstants.h"

@implementation HCFRDataStore
- (id) init {
  self = [super init];
  if (self != nil) {
    series = [[NSMutableArray alloc] init];

    currentSerie = nil;
    referenceSerie = nil;
    filename = nil;
    
    colorReference = [[HCFRColorReference alloc] init];

    gamma=2.2;
    
    dirty = YES; // quand on crée un dataStore, il n'est pas sauvegardé -> on le marque dirty
  }
  return self;
}

-(id) initWithCoder:(NSCoder*)coder
{  
  self = [super init];
  if (self != nil) {
    filename = nil;
    
    colorReference = [[coder decodeObjectForKey:@"colorReference"] retain];
    gamma = [coder decodeFloatForKey:@"gamma"];
    series = [[coder decodeObjectForKey:@"series"] retain];
    
    int currentSerieIndex = [coder decodeIntForKey:@"currentSerieIndex"];
    if (currentSerieIndex != -1)
      currentSerie = [[series objectAtIndex:currentSerieIndex] retain];
    else
      currentSerie = nil;
    
    int referenceSerieIndex = [coder decodeIntForKey:@"referenceSerieIndex"];
    if (referenceSerieIndex != -1)
      referenceSerie = [[series objectAtIndex:referenceSerieIndex] retain];
    else
      currentSerie = nil;
    
    dirty = NO;
  }
  return self;
}

-(void) encodeWithCoder:(NSCoder*)coder
{
  [coder encodeObject:colorReference forKey:@"colorReference"];
  [coder encodeFloat:gamma forKey:@"gamma"];
  [coder encodeObject:series forKey:@"series"];
  if (currentSerie == nil)
    [coder encodeInt:-1 forKey:@"currentSerieIndex"];
  else      
    [coder encodeInt:[series indexOfObject:currentSerie] forKey:@"currentSerieIndex"];
  if (referenceSerie == nil)
    [coder encodeInt:-1 forKey:@"referenceSerieIndex"];
  else
    [coder encodeInt:[series indexOfObject:referenceSerie] forKey:@"referenceSerieIndex"];

  [self setDirty:NO];
}

- (void) dealloc {
  if (currentSerie != nil)
    [currentSerie release];
  if (referenceSerie != nil)
    [referenceSerie release];
  
  [series release];
  [colorReference release];
  
  if (filename != nil)
    [filename release];

  [super dealloc];
}

-(int) countSeries
{
  return [series count];
}
-(NSArray*) series
{
  return series;
}
-(HCFRDataSerie*)serieAtIndex:(int)index
{
  return [series objectAtIndex:index];
}

-(HCFRDataSerie*) currentSerie
{
  return currentSerie;
}
-(int) currentSerieIndex
{
  return [series indexOfObject:currentSerie];
}
-(void) setCurrentSerieIndex:(int)index
{
  HCFRDataSerie *newSerie = nil;
  if (index != -1)
    newSerie = [series objectAtIndex:index];

  // si il n'y a pas de changements, on quitte tout de suite
  if (newSerie == currentSerie)
    return;
  
  [self willChangeValueForKey:@"currentSerie"];
  if (currentSerie != nil)
  {
    [currentSerie release];
    currentSerie = nil;
  }
  
  if (newSerie != nil)
    currentSerie = [newSerie retain];

  [self didChangeValueForKey:@"currentSerie"];

//  [self setDirty:YES];
}
-(HCFRDataSerie*) referenceSerie
{
  return referenceSerie;
}
-(void) setReferenceSerieIndex:(int)index
{
  HCFRDataSerie *newSerie = nil;
  if (index != -1)
    newSerie = [series objectAtIndex:index];
  
  // si il n'y a pas de changements, on quitte tout de suite
  if (newSerie == referenceSerie)
    return;

  [self willChangeValueForKey:@"referenceSerie"];
  if (referenceSerie != nil)
  {
    [referenceSerie release];
    referenceSerie = nil;
  }
  
  if (newSerie != nil)
    referenceSerie = [newSerie retain];
  
  [self didChangeValueForKey:@"referenceSerie"];
  
//  [self setDirty:YES];
}
-(int) addNewSerie:(BOOL)selectTheNewSerie
{
  [self willChangeValueForKey:@"dataSeries"];
  HCFRDataSerie* newSerie = [[HCFRDataSerie alloc] init];
  [series addObject:newSerie];
  [newSerie release];
  [self didChangeValueForKey:@"dataSeries"];
  
  int newIndex = [series indexOfObject:newSerie];

  if (selectTheNewSerie)
    [self setCurrentSerieIndex:newIndex];
  
  [self setDirty:YES];

  return newIndex;
}
-(int) addSerie:(HCFRDataSerie*)newSerie andSelectIt:(BOOL)selectTheNewSerie
{
  [self willChangeValueForKey:@"dataSeries"];
  [series addObject:newSerie];
  [self didChangeValueForKey:@"dataSeries"];
  
  int newIndex = [series indexOfObject:newSerie];
  
  if (selectTheNewSerie)
    [self setCurrentSerieIndex:newIndex];
  
  [self setDirty:YES];
  
  return newIndex;
}
-(int) removeSerieAtIndex:(int)index
{
  HCFRDataSerie *serieToDelete = [[series objectAtIndex:index] retain];
  
  [self willChangeValueForKey:@"dataSeries"];
  [series removeObject:serieToDelete];
  [self didChangeValueForKey:@"dataSeries"];
  
  [self setDirty:YES];
  
  // si la série effacée était référence, on n'a plus de référence
  if (serieToDelete == referenceSerie)
    [self setReferenceSerieIndex:-1];
  
  // si on est la série courante, on séléctionne celle qui prend sa place au même index si possible,
  // la précédente sinon, aucune si il n'y en a plus.
  int newCurrentIndex = NSNotFound;
  if (serieToDelete == currentSerie)
  {
    if ([series count] > index)
      newCurrentIndex = index;
    else if ([series count] > 0)
      newCurrentIndex = [series count]-1;
    
    [self setCurrentSerieIndex:newCurrentIndex];
  }
  else
    newCurrentIndex = [series indexOfObject:currentSerie];
  
  [serieToDelete release];
  
  return newCurrentIndex;
}

#pragma mark Gestion des données aditionnelles
-(void) setColorReference:(HCFRColorReference*) newReference
{
  if (colorReference != nil)
    [colorReference release];
  
  colorReference = [newReference retain];

  [self setDirty:YES];
}
-(HCFRColorReference*) colorReference
{
  return colorReference;
}
-(void) setGamma:(float) newGamma
{
  gamma = newGamma;
  
  [self setDirty:YES];
}
-(float) gamma
{
  return gamma;
}

-(NSString*) filename
{
  return filename;
}
-(void) setFilename:(NSString*)newFilename
{
  if (newFilename == filename)
    return; // sinon, on release le filename, donc le newFilename, et on plante juste après pour cause d'accès à un objet detruit.
  
  if (filename != nil)
  {
    [filename release];
    filename = nil;
  }
  if (newFilename != nil)
    filename = [newFilename retain];
}

-(void) setDirty:(BOOL)newDirtyStatus
{
  dirty = newDirtyStatus;
}
-(BOOL) dirty
{
  if (dirty)
    return YES;
  
  int nbSeries = [series count];
  int i;
    
  for (i=0; i < nbSeries; i ++)
  {
    if ([[series objectAtIndex:i] dirty])
      return YES;
  }
  
  return NO;
}
@end