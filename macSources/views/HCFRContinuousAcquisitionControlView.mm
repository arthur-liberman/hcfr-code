//  ColorHCFR
//  HCFRContinuousAcquisitionControlView.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 21/01/11.
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

#import "HCFRContinuousAcquisitionControlView.h"
#import "HCFREmptyView.h"
#import "HCFRUtilities.h"

@implementation HCFRContinuousAcquisitionControlView

-(void) awakeFromNib
{
  graphicsArray = [[NSMutableArray alloc] init];

  // on vire les tabs présents
  while ([graphicsTabView numberOfTabViewItems]) {
    [graphicsTabView removeTabViewItem:[graphicsTabView tabViewItemAtIndex:0]];
  }

  // on met un placeholder comme vue de control de générateur
  [self setGeneratorControlView:nil];
}

- (void) dealloc
{
  [graphicsArray release];
  
  [super dealloc];
}

-(void) addGraph:(HCFRGraphController*)newGraph
{
  NSTabViewItem *newTabItem = [[NSTabViewItem alloc] init];
  NSView *newGraphView = [newGraph graphicView];
    
  [newTabItem setView:newGraphView];
  [newTabItem setLabel:[newGraph graphicName]];
  [graphicsTabView addTabViewItem:newTabItem];

  [newTabItem release];
  
  [graphicsArray addObject:newGraph];
}

-(void) drawRect:(NSRect)rect
{
  // on met le fond en noir
  [[NSColor blackColor] set];
  [NSBezierPath fillRect:rect];
  
  // puis on dessine les composants
  [super drawRect:rect];
}

-(void) setDataSerie:(HCFRDataSerie*)newDataSerie
{
  for (HCFRGraphController* currentController in graphicsArray) {
    [currentController setCurrentSerie:newDataSerie];
  }
}

-(void) setGeneratorControlView:(NSView*)newControlView
{
  if (newControlView == nil)
  {
    newControlView = [HCFREmptyView emptyViewWithString:HCFRLocalizedString(@"No controls available", @"No controls available")];
    [generatorControlViewContainer setContentView:newControlView];
  }
  else
    [generatorControlViewContainer setContentView:newControlView];
}
@end
