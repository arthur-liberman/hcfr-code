//  ColorHCFR
//  HCFRGraphController.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 10/09/07.
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

#import "HCFRGraphController.h"


@implementation HCFRGraphController
- (HCFRGraphController*) init {
  self = [super init];
  if (self != nil) {
    currentDataSerie = nil;
    referenceDataSerie = nil;
  }
  return self;
}
- (void) dealloc {
  if (currentDataSerie != nil)
  {
    [currentDataSerie release];
  }
  if (referenceDataSerie != nil)
  {
    [referenceDataSerie release];
  }

  [super dealloc];
}

-(GraphType) graphicType
{
  NSAssert (NO, @"Unimplemented graphicType in graphic controller.");
  return kUndefinedGraph;
}
-(NSString*) graphicName
{
  return @"Not implemented graph";
}
-(BOOL) hasAnOptionsView
{
  return NO;
}
-(NSView*) optionsView
{
  NSAssert (NO, @"Unimplemented optionsView in graphic controller.");
  return nil;
}
-(NSView*) graphicView
{
  NSAssert (NO, @"Unimplemented graphicView in graphic controller.");
  return nil;
}

-(void) setCurrentSerie:(HCFRDataSerie*)newSerie
{
  HCFRDataSerie *oldSerie = currentDataSerie;
  
  currentDataSerie = newSerie;
  if (currentDataSerie != nil)
    [currentDataSerie retain];
  
  [self currentDataSerieChanged:oldSerie];
  
  if (oldSerie != nil)
    [oldSerie release];
}
-(HCFRDataSerie*) currentSerie
{
  return currentDataSerie;
}
-(void) setReferenceSerie:(HCFRDataSerie*)newSerie;
{
  HCFRDataSerie *oldSerie = referenceDataSerie;
  
  referenceDataSerie = newSerie;
  if (referenceDataSerie != nil)
    [referenceDataSerie retain];

  [self referenceDataSerieChanged:oldSerie];

  if (oldSerie != nil)
    [oldSerie release];
}
-(HCFRDataSerie*) referenceSerie
{
  return referenceDataSerie;
}
-(void) setColorReference:(HCFRColorReference*)newReference;
{
  NSAssert(newReference != nil, @"HCFRGraphController : invalide colorReference nil");
  
  HCFRColorReference  *oldRef = colorReference;
  
  colorReference = [newReference retain];

  [self colorReferenceChanged:oldRef];
  
  if (oldRef != nil)
    [oldRef release];
}
-(HCFRColorReference*) colorReference
{
  return colorReference;
}
-(void) setGamma:(float)newGamma;
{
  float oldGamma = gamma;
  gamma = newGamma;
  [self gammaChanged:oldGamma];
}
-(float) gamma
{
  return gamma;
}

#pragma mark Implémentation de l'interface listener de la data serie et des fonctions d'avertissement des changements
-(void) entryAdded:(HCFRDataStoreEntry*)newEntry toSerie:(HCFRDataSerie*)serie forDataType:(HCFRDataType)dataType {}
-(void) serie:(HCFRDataSerie*)serie changedForDataType:(HCFRDataType)dataType {}
-(void) colorReferenceChanged:(HCFRColorReference*)oldVersion {}
-(void) gammaChanged:(float)oldValue {}
-(void) currentDataSerieChanged:(HCFRDataSerie*)oldSerie {}
-(void) referenceDataSerieChanged:(HCFRDataSerie*)oldSerie {}
@end
