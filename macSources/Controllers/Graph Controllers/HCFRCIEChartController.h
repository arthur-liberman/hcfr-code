//  ColorHCFR
//  HCFRCIEChartController.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 13/09/07.
//
//  $Rev: 49 $
//  $Date: 2008-05-22 10:02:01 +0100 (Thu, 22 May 2008) $
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


#import <Cocoa/Cocoa.h>
#import <HCFRPlugins/HCFRDataSerie.h>
#import <HCFRPlugins/HCFRDataSerieListener.h>
#import "HCFRGraphController.h"
#import "HCFRCIEChartView.h"

/*!
  @class
  @abstract    Controller pour la vue "CIE"
  @discussion  Cette classe est l'interface entre le data store et le graphique CIE.
Elle s'enregistre comme listener auprès du dataStore, puis à chaque modification des données, les transforme pour
fournir au graph les valeurs à tracer.
*/
@interface HCFRCIEChartController : HCFRGraphController {
  IBOutlet NSView   *optionsView;
  IBOutlet NSButton *showGridButton;
  IBOutlet NSButton *showBlackBodyButton;
  
  HCFRCIEChartView  *graphicView;
}
-(HCFRCIEChartController*) init;

#pragma mark Surcharges des methodes de HCFRGraphController
-(NSString*) graphicName;
-(BOOL) hasAnOptionsView;
-(NSView*) optionsView;
-(NSView*) graphicView;

#pragma mark IBactions
-(IBAction) displayModeChanged:(NSPopUpButton*)sender;
-(IBAction) showGridChanged:(NSButton*)sender;
-(IBAction) showBlackBodyChanged:(NSButton*)sender;
@end
