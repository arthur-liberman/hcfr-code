//  ColorHCFR
//  HCFRMainController+toolbarDelegate.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 19/01/08.
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

#import "HCFRMainController+ToolbarAndMenu.h"
#import "KBPopUpToolbarItem.h"

//-------------------------------------------------------
// mesures
#define kHCFRToolbarItemMeasures @"measuresMenuItem"
#define kHCFRToolbarItemMeasureLuminance @"measureLuminanceItem"
#define kHCFRToolbarItemMeasureSaturation @"measureSaturationsItem"
#define kHCFRToolbarItemMeasureContrast @"measureContrastsItem"

#define kHCFRToolbarItemCancelMeasures @"stopItem"

//-------------------------------------------------------
// utilitaires
#define kHCFRToolbarItemConfigure @"showConfigurationItem"
#define kHCFRToolbarItemInspector @"showInspectorItem"

@implementation HCFRMainController (ToolbarAndMenuCategory)
- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar
     itemForItemIdentifier:(NSString *)itemIdentifier
 willBeInsertedIntoToolbar:(BOOL)flag
{
  NSToolbarItem *result;
  
  //---------------------------------------------------
  // les boutons divers
  //---------------------------------------------------
  if ([itemIdentifier isEqualToString:kHCFRToolbarItemConfigure])
  {
    result = [[[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier] autorelease];
    [result setImage:[NSImage imageNamed:@"config.png"]];
    [result setLabel:HCFRLocalizedString(@"Configuration",@"Configuration")];
    [result setPaletteLabel:[result label]];
    [result setAction:@selector(showSetupSheet:)];
  }
  else if ([itemIdentifier isEqualToString:kHCFRToolbarItemInspector])
  {
    result = [[[NSToolbarItem alloc] initWithItemIdentifier:kHCFRToolbarItemInspector] autorelease];
    [result setImage:[NSImage imageNamed:@"inspector.png"]];
    [result setLabel:HCFRLocalizedString(@"Inspector",@"Inspector")];
    [result setPaletteLabel:[result label]];
    [result setAction:@selector(showHideGraphInspector:)];
  }
  //---------------------------------------------------
  // les boutons de lancement de mesures
  //---------------------------------------------------
  // le boutton popup "mesures"
  else if ([itemIdentifier isEqualToString:kHCFRToolbarItemMeasures])
  {
    NSImage   *image = [NSImage imageNamed:@"measure.ico"];
    KBPopUpToolbarItem *popupItem = [[[KBPopUpToolbarItem alloc] initWithItemIdentifier:kHCFRToolbarItemMeasures] autorelease];
    [popupItem setLabel:HCFRLocalizedString(@"Measures",@"Measures")];
    [popupItem setPaletteLabel:[popupItem label]];
    if (image != nil)
      [popupItem setImage:image];
    [popupItem setTag:kAllMeasures];
    [popupItem setMenu:[self generateMeasuresMenu]];
    [popupItem setAction:@selector(startMesureFromTooblarItem:)];
    [popupItem setImage:image];
    result = popupItem;
  }
  // le boutton "grayscale"
  else if ([itemIdentifier isEqualToString:kHCFRToolbarItemMeasureLuminance])
  {
    NSImage   *image = [NSImage imageNamed:@"measure_lumi.ico"];
    KBPopUpToolbarItem *popupItem = [[[KBPopUpToolbarItem alloc] initWithItemIdentifier:kHCFRToolbarItemMeasureLuminance] autorelease];
    [popupItem setLabel:HCFRLocalizedString(@"Measure grayscale",@"Measure grayscale")];
    [popupItem setPaletteLabel:[popupItem label]];
    if (image != nil)
      [popupItem setImage:image];
    [popupItem setTag:kAllGrayscaleMeasure];
    [popupItem setMenu:[self generateGrayscaleMenu]];
    [popupItem setAction:@selector(startMesureFromTooblarItem:)];
    result = popupItem;
  }
  // le boutton "saturations"
  else if ([itemIdentifier isEqualToString:kHCFRToolbarItemMeasureSaturation])
  {
    NSImage   *image = [NSImage imageNamed:@"measure_rgb.ico"];
    KBPopUpToolbarItem *popupItem = [[[KBPopUpToolbarItem alloc] initWithItemIdentifier:kHCFRToolbarItemMeasureSaturation] autorelease];
    [popupItem setLabel:HCFRLocalizedString(@"Measure saturations",@"Measure saturations")];
    [popupItem setPaletteLabel:[popupItem label]];
    if (image != nil)
      [popupItem setImage:image];
    [popupItem setTag:kSaturationsMeasure];
    [popupItem setMenu:[self generateSaturationsMenu]];
    [popupItem setAction:@selector(startMesureFromTooblarItem:)];
    [popupItem setImage:image];
    result = popupItem;
  }    
  // le boutton "Contrasts"
  else if ([itemIdentifier isEqualToString:kHCFRToolbarItemMeasureContrast])
  {
    NSImage   *image = [NSImage imageNamed:@"contrast.bmp"];
    KBPopUpToolbarItem *popupItem = [[[KBPopUpToolbarItem alloc] initWithItemIdentifier:kHCFRToolbarItemMeasureContrast] autorelease];
    [popupItem setLabel:HCFRLocalizedString(@"Measure contrasts",@"Measure contrasts")];
    [popupItem setPaletteLabel:[popupItem label]];
    if (image != nil)
      [popupItem setImage:image];
    [popupItem setTag:kContrastsMeasure];
    [popupItem setMenu:[self generateContrastsMenu]];
    [popupItem setAction:@selector(startMesureFromTooblarItem:)];
    [popupItem setImage:image];
    result = popupItem;
  }
  else if ([itemIdentifier isEqualToString:kHCFRToolbarItemCancelMeasures])
  {
    NSImage   *image = [NSImage imageNamed:@"stop.png"];
    result = [[[NSToolbarItem alloc] initWithItemIdentifier:kHCFRToolbarItemCancelMeasures] autorelease];
    [result setImage:image];
    [result setLabel:HCFRLocalizedString(@"Cancel",@"Cancel")];
    [result setPaletteLabel:[result label]];
    [result setAction:@selector(cancelCalibration:)];
  }
  else
    NSAssert1(NO, @"HCFRMainController->toolbarItemForItemIdentifier : unknown identifier %@", itemIdentifier);
  
  [result setTarget:self];
  
  return result;
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar
{
  static NSArray  *toolbarItems = [[NSArray arrayWithObjects:kHCFRToolbarItemMeasures,
                                                            kHCFRToolbarItemMeasureLuminance,
                                                            kHCFRToolbarItemMeasureSaturation,
                                                            kHCFRToolbarItemMeasureContrast,
                                                            kHCFRToolbarItemCancelMeasures,
                                                            // les éléments divers
                                                            kHCFRToolbarItemConfigure,
                                                            kHCFRToolbarItemInspector,
                                                            // les éléments cocoa 
                                                            NSToolbarFlexibleSpaceItemIdentifier,
                                                            NSToolbarSpaceItemIdentifier,
                                                            NSToolbarSeparatorItemIdentifier,
                                                            nil] retain];
  
  return toolbarItems;
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar*)toolbar
{
  static NSArray  *defaultToolbarItems = [[NSArray arrayWithObjects:kHCFRToolbarItemMeasures,
                                                                   kHCFRToolbarItemCancelMeasures,
                                                                   NSToolbarFlexibleSpaceItemIdentifier,
                                                                   kHCFRToolbarItemConfigure,
                                                                   kHCFRToolbarItemInspector,
                                                                   nil] retain];

  return defaultToolbarItems;
}

#pragma mark Generation des menus
-(NSMenu*) generateMeasuresMenu
{
  NSMenu      *measuresMenu = [[NSMenu alloc] initWithTitle:HCFRLocalizedString(@"Measures", @"Measures")];
  NSMenuItem  *newItem;
  
  // luminance
  newItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"Everything",@"Everything")
                                       action:@selector(startMesureFromMenu:)
                                keyEquivalent:@""];
  [newItem setTarget:self];
  [newItem setTag:kAllMeasures];
  [measuresMenu addItem:newItem];
  [newItem release];

  // luminance
  newItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"Grayscales",@"Grayscales")
                                                 action:@selector(startMesureFromMenu:)
                                          keyEquivalent:@""];
  [newItem setTarget:self];
  [newItem setTag:kAllGrayscaleMeasure];
  [newItem setSubmenu:[self generateGrayscaleMenu]];
  [measuresMenu addItem:newItem];
  [newItem release];

  // saturation
  newItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"Saturations",@"Saturations")
                                       action:@selector(startMesureFromMenu:)
                                keyEquivalent:@""];
  [newItem setTarget:self];
  [newItem setTag:kSaturationsMeasure];
  [newItem setSubmenu:[self generateSaturationsMenu]];
  [measuresMenu addItem:newItem];
  [newItem release];
  
  // primaires
  newItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"Primary componnents",@"Primary componnents")
                                       action:@selector(startMesureFromMenu:)
                                keyEquivalent:@""];
  [newItem setTarget:self];
  [newItem setTag:kPrimaryComponentsMeasure];
  [measuresMenu addItem:newItem];
  [newItem release];

  // secondaires
  newItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"Secondary componnents",@"Secondary componnents")
                                       action:@selector(startMesureFromMenu:)
                                keyEquivalent:@""];
  [newItem setTarget:self];
  [newItem setTag:kSecondaryComponentsMeasure];
  [measuresMenu addItem:newItem];
  [newItem release];

  // contratsts
  newItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"Contrasts",@"Contrasts")
                                       action:@selector(startMesureFromMenu:)
                                keyEquivalent:@""];
  [newItem setTarget:self];
  [newItem setTag:kContrastsMeasure];
  [newItem setSubmenu:[self generateContrastsMenu]];
  [measuresMenu addItem:newItem];
  [newItem release];
  
  return [measuresMenu autorelease];
}

-(NSMenu*) generateGrayscaleMenu
{
  NSMenu      *menu = [[NSMenu alloc] initWithTitle:HCFRLocalizedString(@"Grayscale",@"Grayscale")];
  NSMenuItem  *newItem;
  
  // Les niveaux de gris sur toute l'echelle, gros grain
  newItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"Grayscale",@"Grayscale")
                                       action:@selector(startMesureFromMenu:)
                                keyEquivalent:@""];
  [newItem setTarget:self];
  [newItem setTag:kGrayscaleMeasure];
  [menu addItem:newItem];
  [newItem release];
  
  // Les niveaux de gris détaillés "near black"
  newItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"Detailed near black",@"Detailed near black")
                                       action:@selector(startMesureFromMenu:)
                                keyEquivalent:@""];
  [newItem setTarget:self];
  [newItem setTag:kGrayscaleNearBlackMeasure];
  [menu addItem:newItem];
  [newItem release];
  
  // Les niveaux de gris détaillés "near black"
  newItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"Detailed near white",@"Detailed near white")
                                       action:@selector(startMesureFromMenu:)
                                keyEquivalent:@""];
  [newItem setTarget:self];
  [newItem setTag:kGrayscaleNearWhiteMeasure];
  [menu addItem:newItem];
  [newItem release];
  
  return [menu  autorelease];
}

-(NSMenu*) generateSaturationsMenu
{
  NSMenu      *menu = [[NSMenu alloc] initWithTitle:HCFRLocalizedString(@"Saturations",@"Saturations")];
  NSMenuItem  *newItem;
  
  // saturation rouge
  newItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"Red",@"Red")
                                       action:@selector(startMesureFromMenu:)
                                keyEquivalent:@""];
  [newItem setTarget:self];
  [newItem setTag:kRedSaturationMeasure];
  [menu addItem:newItem];
  [newItem release];
  
  // saturation vert
  newItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"Green",@"Green")
                                       action:@selector(startMesureFromMenu:)
                                keyEquivalent:@""];
  [newItem setTarget:self];
  [newItem setTag:kGreenSaturationMeasure];
  [menu addItem:newItem];
  [newItem release];
  
  // saturation bleu
  newItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"Blue",@"Blue")
                                       action:@selector(startMesureFromMenu:)
                                keyEquivalent:@""];
  [newItem setTarget:self];
  [newItem setTag:kBlueSaturationMeasure];
  [menu addItem:newItem];
  [newItem release];
  
  return [menu  autorelease];
}

-(NSMenu*) generateContrastsMenu
{
  NSMenu      *menu = [[NSMenu alloc] initWithTitle:HCFRLocalizedString(@"Contrasts",@"Contrasts")];
  NSMenuItem  *newItem;
  
  // contrast plein écran
  newItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"Full screen",@"Full screen")
                                       action:@selector(startMesureFromMenu:)
                                keyEquivalent:@""];
  [newItem setTarget:self];
  [newItem setTag:kFullScreenContrastMeasure];
  [menu addItem:newItem];
  [newItem release];
  
  // contrast ANSI
  newItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"ANSI",@"ANSI")
                                       action:@selector(startMesureFromMenu:)
                                keyEquivalent:@""];
  [newItem setTarget:self];
  [newItem setTag:kANSIContrastMeasure];
  [menu addItem:newItem];
  [newItem release];
  
  return [menu  autorelease];
}

#pragma mark auto-activation des menus & des elements de la toolbar
- (BOOL)validateMenuItem:(NSMenuItem *)item
{
  if ([item action] == @selector(copy:))
    return [dataTableController canCopy];
  
  // en cours de calibration, on n'active aucun menu
  if ([calibrationEngine isRunning])
    return NO;
  
  // si le capteur n'a pas besoin de calibration, on désactive le menu
  if ([item tag] == kSensorCalibrationMeasures)
    return ([[configuration sensor] needsCalibration] && ![calibrationEngine isRunning]);
  // le éléments permettant de lancer des mesures ne sont pas actifs si une mesure tourne déja
  // ou si aucune série n'est disponible
  else if ([item tag] == kGrayscaleMeasure ||
           [item tag] == kGrayscaleNearBlackMeasure ||
           [item tag] == kGrayscaleNearWhiteMeasure ||
           [item tag] == kAllGrayscaleMeasure ||
           [item tag] == kRedSaturationMeasure ||
           [item tag] == kGreenSaturationMeasure ||
           [item tag] == kBlueSaturationMeasure ||
           [item tag] == kSaturationsMeasure ||
           [item tag] == kPrimaryComponentsMeasure ||
           [item tag] == kSecondaryComponentsMeasure ||
           [item tag] == kComponentsMeasure ||
           [item tag] == kFullScreenContrastMeasure ||
           [item tag] == kANSIContrastMeasure ||
           [item tag] == kContrastsMeasure ||
           [item tag] == kSensorCalibrationMeasures ||
           [item tag] == kAllMeasures ||
           [item tag] == kContinuousMeasures)
    return (![calibrationEngine isRunning]) && ([dataStore currentSerie] != nil);
           
  return YES;
}
- (BOOL)validateToolbarItem:(NSToolbarItem *)item
{
  NSString  *identifier = [item itemIdentifier];
  
  // pour le boutton cancel, on n'active que si il y a une mesure en cours.
  // Pour les bouttons de mesure, c'est l'inverse.
  // Les autres restent toujours valides.
  
  if ([identifier isEqual:kHCFRToolbarItemCancelMeasures])
    return [calibrationEngine isRunning];
  else if ([identifier isEqual:kHCFRToolbarItemConfigure])
    return ![calibrationEngine isRunning];
  else if ([identifier isEqual:kHCFRToolbarItemMeasures] ||
           [identifier isEqual:kHCFRToolbarItemMeasureLuminance] ||
           [identifier isEqual:kHCFRToolbarItemMeasureSaturation] ||
           [identifier isEqual:kHCFRToolbarItemMeasureContrast])
    return (![calibrationEngine isRunning]) && ([dataStore currentSerie] != nil);
  
  return YES;
}
@end
