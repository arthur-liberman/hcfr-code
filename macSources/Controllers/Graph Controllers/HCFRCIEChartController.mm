//  ColorHCFR
//  HCFRCIEChartController.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 13/09/07.
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

#import "HCFRCIEChartController.h"
#import "HCFRConstants.h"

#define kHCFRCIEChartDisplayMode                    @"CIEChartDisplayMode"
#define kHCFRCIEChartShowGridValue                  @"CIEChartShowGrid"
#define kHCFRCIEChartShowBlackBodyValue             @"CIEChartShowBlackBody"

@implementation HCFRCIEChartController
#pragma mark Surcharges des methodes de HCFRGraphController
-(HCFRCIEChartController*) init
{
  [super init];
  
  // on met les valeurs par defauts pour les prefs
  NSDictionary  *defaultPreferences = [NSDictionary dictionaryWithObjectsAndKeys:
                                          [NSNumber numberWithInt:0], kHCFRCIEChartDisplayMode,
                                          @"YES", kHCFRCIEChartShowGridValue,
                                          @"NO", kHCFRCIEChartShowBlackBodyValue,
                                          nil];
  
  [[NSUserDefaults standardUserDefaults] registerDefaults:defaultPreferences];

  // on charge le fichier nib qui contient le panel d'options
  [NSBundle loadNibNamed:@"CIEChart" owner:self];

  int currentDisplayMode = [[NSUserDefaults standardUserDefaults] integerForKey:kHCFRCIEChartDisplayMode];
  
  // On crée la vue graphique avec une reference de couleur par défaut
  graphicView = [[HCFRCIEChartView alloc] initWithFrame:NSMakeRect(0,0,640,480)
                                         colorReference:[[[HCFRColorReference alloc] init] autorelease]
                                       useUVCoordinates:(currentDisplayMode==1)];
  [graphicView setShowGrid:[[NSUserDefaults standardUserDefaults] boolForKey:kHCFRCIEChartShowGridValue]];
  [graphicView setShowBlackBodyCurve:[[NSUserDefaults standardUserDefaults] boolForKey:kHCFRCIEChartShowBlackBodyValue]];
  
  return self;
}

-(void) dealloc
{
  if ([self currentSerie] != nil)
    [[self currentSerie] removeListener:self forType:kComponentsDataType];
  
  [graphicView release];
  [super dealloc];
}

#pragma mark Surcharges des methodes de HCFRGraphController
-(GraphType) graphicType
{
  return kCalibrationGraph;
}
-(NSString*) graphicName
{
  return @"CIE";
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
  return graphicView;
}

-(void) reloadData
{
  [graphicView setRedComponent:[[[self currentSerie] entryWithReferenceValue:kRedComponent forType:kComponentsDataType] value]];
  [graphicView setGreenComponent:[[[self currentSerie] entryWithReferenceValue:kGreenComponent forType:kComponentsDataType] value]];
  [graphicView setBlueComponent:[[[self currentSerie] entryWithReferenceValue:kBlueComponent forType:kComponentsDataType] value]];
  [graphicView setCyanComponent:[[[self currentSerie] entryWithReferenceValue:kCyanComponent forType:kComponentsDataType] value]];
  [graphicView setMagentaComponent:[[[self currentSerie] entryWithReferenceValue:kMagentaComponent forType:kComponentsDataType] value]];
  [graphicView setYellowComponent:[[[self currentSerie] entryWithReferenceValue:kYellowComponent forType:kComponentsDataType] value]];
}

#pragma mark Surcharge des fonctions de listener de HCFRGraphController
-(void) entryAdded:(HCFRDataStoreEntry*)newEntry toSerie:(HCFRDataSerie*)serie forDataType:(HCFRDataType)dataType
{
  if (serie == [self currentSerie])
  {
    // c'est forcément un ajout de composante, on ne s'est inscrit que pour ces evenements la.
    [self reloadData];
  }
}
-(void) serie:(HCFRDataSerie*)serie changedForDataType:(HCFRDataType)dataType
{
  if (serie == [self currentSerie])
  {
    [self reloadData];
  }
}
-(void) colorReferenceChanged:(HCFRColorReference*)oldReference
{
  [graphicView setReference:[self colorReference]];
}
-(void) currentDataSerieChanged:(HCFRDataSerie*)oldSerie
{
  if (oldSerie != nil)
    [oldSerie removeListener:self forType:kComponentsDataType];
  if ([self currentSerie] != nil)
    [[self currentSerie] addListener:self forType:kComponentsDataType];

  [self reloadData];
}

#pragma mark Actions IB
-(IBAction) displayModeChanged:(NSPopUpButton*)sender
{
  [graphicView setCoordinates:([sender selectedTag] == 1)];
}
-(IBAction) showGridChanged:(NSButton*)sender
{
  [graphicView setShowGrid:([sender state]==NSOnState)];
}
-(IBAction) showBlackBodyChanged:(NSButton*)sender
{
  [graphicView setShowBlackBodyCurve:([sender state]==NSOnState)];
}
@end
