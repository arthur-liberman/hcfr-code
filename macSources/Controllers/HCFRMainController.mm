//  ColorHCFR
//  HCFRMeasure.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 30/08/07.
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

#import "HCFRMainController.h"
#import "HCFRMainController+ToolbarAndMenu.h"
#import "HCFRGraphController.h"
#import "HCFRProfilesManager.h"
#import <HCFRPlugins/HCFRTool.h>
#import <HCFRPlugins/HCFRScreensManager.h>
#import "HCFREmptyView.h"
#import "HCFRAboutWindowController.h"
#import "HCFRConstants.h"
#import <HCFRPlugins/HCFRDataSerie+io.h>
#import "HCFRContinuousAcquisitionEngine.h"

// imports de la libHCFR
#import <libHCFR/CalibrationFile.h>

@implementation HCFRMainController
-(HCFRMainController*) init
{
  [super init];

  [self showSplash];

  pluginsController = [[HCFRPluginsController alloc] init];
  
  configuration = [[HCFRConfiguration alloc] init];
 
  return self;
}

-(void) awakeFromNib
{
  //-------------------------------------
  // Les variables de la toolbar
  //-------------------------------------
  mainToolbar = [[NSToolbar alloc] initWithIdentifier:@"mainToolbar"];
  [mainToolbar setDelegate:self]; // les fonctions du delegué sont dans la category toolbarDelegate
  [mainToolbar setAllowsUserCustomization:YES];
  [mainToolbar setAutosavesConfiguration:YES];
  [mainWindow setToolbar:mainToolbar];

  //-------------------------------------
  // On ajoute les NSNumberFormatter aus champs gamma et delay
  // Ce n'est pas fait via interface builder,
  // car il semble qu'on obtienne alors une ancienne version,
  // legerement buggée : http://www.cocoabuilder.com/archive/message/cocoa/2007/3/30/181167
  //-------------------------------------
  NSNumberFormatter *formatter = [[NSNumberFormatter alloc] init];
  [formatter setMinimum:[NSNumber numberWithFloat:0.1]];
  [gammaTextField setFormatter:formatter];
  [formatter release];
  
  formatter = [[NSNumberFormatter alloc] init];
  [formatter setMinimum:[NSNumber numberWithFloat:0]];
  [formatter setMaximum:[NSNumber numberWithFloat:10]];
  [delayTextField setFormatter:formatter];
  [formatter release];
  
  [colorStandardPopup setAutoenablesItems:NO];
  
  //-------------------------------------
  // On ajoute les menus "mesure" construits logiciellement
  // pour utiliser les constantes
  //-------------------------------------
  [measuresMenuItem setSubmenu:[self generateMeasuresMenu]];
  // et on ajoute l'élément "calibrate sensor"
  NSMenuItem  *calibrateItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"Calibrate sensor",@"Calibrate sensor")
                                                          action:@selector(startSensorCalibration:)
                                                   keyEquivalent:@""];
  [calibrateItem setTarget:self];
  [calibrateItem setTag:kSensorCalibrationMeasures];
  [[measuresMenuItem submenu] insertItem:calibrateItem atIndex:0];
  // puis l'élément "continuous measures"
  NSMenuItem  *continuousItem = [[NSMenuItem alloc] initWithTitle:HCFRLocalizedString(@"Continuous measures",@"Continuous measures")
                                                          action:@selector(startContinuousMesure:)
                                                   keyEquivalent:@""];
  [continuousItem setTarget:self];
  [continuousItem setTag:kContinuousMeasures];
  [[measuresMenuItem submenu] insertItem:continuousItem atIndex:1];
  
  [[measuresMenuItem submenu] insertItem:[NSMenuItem separatorItem] atIndex:2];
  
  //-------------------------------------
  // on charge les outils
  //-------------------------------------
  // on charge les outils proposés par les plugins
  NSEnumerator  *toolClassesEnumerator = [[pluginsController toolsPlugins] objectEnumerator];
  Class         currentTool;
  while (currentTool = [toolClassesEnumerator nextObject])
  {
    NSMenuItem *newMenuItem = [[NSMenuItem alloc] init];
    [newMenuItem setTitle:[currentTool toolName]]; // currentTool est de type Class, je en sais pas comment indiquer que c'est une classe HCFRTool
                                                   // A cause de ça, on a un warning ...
    [newMenuItem setRepresentedObject:currentTool];
    [newMenuItem setTarget:self];
    [newMenuItem setAction:@selector(startTool:)];
    
    [[toolsMenuItem submenu] addItem:newMenuItem];
    
    [newMenuItem release];
  }

  //-------------------------------------
  // on charge les différents graphs de calibration
  //-------------------------------------
  NSEnumerator *pluginsEnumerator;
  pluginsEnumerator = [[pluginsController graphicPlugins] objectEnumerator];
  HCFRGraphController *currentGraph;

  [graphsTabView removeTabViewItem:[graphsTabView tabViewItemAtIndex:0]];

  while (currentGraph = [pluginsEnumerator nextObject])
  {
    // on saute tous les graphiques qui ne sont pas des graphiques de calibration :
    // seul les graphs de calibration sont ajoutés à la fenêtre principale.
    if ([currentGraph graphicType] != kCalibrationGraph)
      continue;
    
    NSTabViewItem *newTabItem = [[NSTabViewItem alloc] init];
    NSView *newGraphView = [currentGraph graphicView];

    [newTabItem setView:newGraphView];
    [newTabItem setLabel:[currentGraph graphicName]];
    [graphsTabView addTabViewItem:newTabItem];

    [newTabItem release];
  }
  // on simule un clic sur le premier tab pour charger les options du graph
  [self tabView:graphsTabView didSelectTabViewItem:[graphsTabView tabViewItemAtIndex:0]];
  // et on s'enregistre comme delegué pour savoir quand on change de tab
  [graphsTabView setDelegate:self];  
  
  //-------------------------------------
  // on charge les profiles de calibration
  //-------------------------------------
  [self loadCalibrationFilesInPopup];
  
  //-------------------------------------
  // on charge les capteurs et les generateurs dans les popup pour la fenetre de setup
  //-------------------------------------
  // on ne peut pas tourner sans générateur ou sans capteur
  if ([[pluginsController sensorPlugins] count] == 0 ||
      [[pluginsController generatorPlugins] count] == 0)
  {
    [[NSAlert alertWithMessageText:HCFRLocalizedString(@"Incomplete ColorHCFR installation",
                                                       @"Incomplete ColorHCFR installation")
                     defaultButton:@"Ok" alternateButton:nil otherButton:nil
         informativeTextWithFormat:HCFRLocalizedString(@"ColorHCFR cannot run without at least a sensor and a generator.",
                                                       @"ColorHCFR cannot run without at least a sensor and a generator.")] runModal];
    [[NSApplication sharedApplication] terminate:self];
  }
      
  [sensorsPopupButton removeAllItems];
  pluginsEnumerator = [[pluginsController sensorPlugins] objectEnumerator];
  Class  currentSensorClass;
  while (currentSensorClass = [pluginsEnumerator nextObject])
  {
    NSMenuItem *menuItem = [[NSMenuItem alloc] init];
    [menuItem setTitle:[currentSensorClass sensorName]];
    [menuItem setRepresentedObject:[currentSensorClass sensorId]];
    [[sensorsPopupButton menu] addItem:menuItem];
    [menuItem release];
  }

  [generatorsPopupButton removeAllItems];
  pluginsEnumerator = [[pluginsController generatorPlugins] objectEnumerator];
  Class currentGeneratorClass;
  while (currentGeneratorClass = [pluginsEnumerator nextObject])
  {
    NSMenuItem *menuItem = [[NSMenuItem alloc] init];
    [menuItem setTitle:[currentGeneratorClass generatorName]];
    [menuItem setRepresentedObject:[currentGeneratorClass generatorId]];
    [[generatorsPopupButton menu] addItem:menuItem];
    [menuItem release];
  }
  
  
  [self closeSplash];
  // on affiche la fenetre
  [mainWindow makeKeyAndOrderFront:self];

  //-------------------------------------
  // On charge le dataStore
  //-------------------------------------
  // on essaye de charger le dernier dataStore si possible, sinon, on charge les valeurs par défaut
  // et si on a créé un nouveau dataStore, on charge le capteur et le générateur par défaut
  // On n'utilise pas le menu "documents recents" pour pouvoir ouvrir le dernier doc même si le menu à été nettoyé.
  NSString  *lastFilepath = [[NSUserDefaults standardUserDefaults] objectForKey:kHCFRLastDocumentFilepath];
  if (lastFilepath != nil)
  {
    [self loadFileAtPath:lastFilepath];
  }
  if (dataStore == nil)
  {
    // on charge les capteurs et générateurs par defaut
    // par defaut, on selectionne ceux stockés dans les prefs ou les premiers des listes
    NSString      *defaultSensorId = [[NSUserDefaults standardUserDefaults] objectForKey:kHCFRDefaultSensorId];
    HCFRSensor    *newSensor = Nil;
    if (defaultSensorId != Nil)
      newSensor = [pluginsController sensorWithId:defaultSensorId];
    if (newSensor == Nil) //  si on n'a pas pu charger le capteur, on prend le premier de la liste
      newSensor = [pluginsController sensorWithId:[[[pluginsController sensorPlugins] objectAtIndex:0] sensorId]];
    [configuration setSensor:newSensor];
//    [calibrationEngine setSensor:newSensor];
    
    NSString        *defaultGeneratorId = [[NSUserDefaults standardUserDefaults] objectForKey:kHCFRDefaultGeneratorId];
    HCFRGenerator   *newGenerator = Nil;
    if (defaultGeneratorId != Nil)
      newGenerator = [pluginsController generatorWithId:defaultGeneratorId];
    if (newGenerator == Nil) // si on n'a pas pu charger le generateur, on prend le premier de la liste
      newGenerator = [pluginsController generatorWithId:[[[pluginsController generatorPlugins] objectAtIndex:0] generatorId]];
    [configuration setGenerator:newGenerator];
//    [calibrationEngine setGenerator:newGenerator];

    // pas de dataStore -> on en crée un nouveau
    [self createNewDataStore:nil];
  }
}

-(void) dealloc
{
  [pluginsController release];
  [dataStore removeObserver:self forKeyPath:@"currentSerie"];
  [dataStore removeObserver:self forKeyPath:@"referenceSerie"];
  [dataStore removeObserver:self forKeyPath:@"colorReference"];
  [dataStore removeObserver:self forKeyPath:@"gamma"];
  [dataStore release];
  if (calibrationEngine != nil)
    [calibrationEngine release];
  [configuration release];

  [mainToolbar release];
    
  [super dealloc];
}

#pragma mark fonctions utilitaires d'interface
-(void) loadCalibrationFilesInPopup
{
  NSArray       *profiles = [HCFRProfilesManager getAvailableProfiles];
  NSEnumerator  *fileEnumerator = [profiles objectEnumerator];
  NSString      *filePath;
  BOOL          itemsAvailable=NO;
  
  // on mémorise le profile chargé pour le re-selectionner après
  NSString *lastSelectedProfile = [[configuration sensor] currentCalibrationFile];
  
  [calibrationFilesPopup removeAllItems];

  while (filePath = [fileEnumerator nextObject])
  {
    NSMenuItem  *newMenuItem = [[NSMenuItem alloc] init];
    NSString    *fileName = [filePath lastPathComponent];
    
    [newMenuItem setTitle:[[filePath lastPathComponent] substringToIndex:[fileName length]-4]];
    [newMenuItem setRepresentedObject:filePath];
    
    [[calibrationFilesPopup menu] addItem:newMenuItem];
    itemsAvailable=YES;
  }
  
  if (!itemsAvailable)
  {
    [calibrationTypeMatrix selectCellAtRow:1 column:0];
    [[calibrationTypeMatrix cellAtRow:0 column:0] setEnabled:NO];
    [calibrationFilesPopup setEnabled:NO];
  }
  else
  {
    if (lastSelectedProfile != nil)
      [self selectCalibrationFileWithFilename:lastSelectedProfile advertise:NO];
    [[calibrationTypeMatrix cellAtRow:0 column:0] setEnabled:YES];
    [calibrationFilesPopup setEnabled:YES];
  }
}

-(bool) selectSensorInPopup:(HCFRSensor*)sensor
{
  NSString      *sensorId = [[sensor class] sensorId];
  NSEnumerator  *itemsEnumerator = [[[sensorsPopupButton menu] itemArray] objectEnumerator];
  NSMenuItem    *currentItem;

  while (currentItem = [itemsEnumerator nextObject])
  {
    if ([sensorId isEqualToString:[currentItem representedObject]])
    {
      [sensorsPopupButton selectItem:currentItem];
      [self setSetupViewForSensor:sensor];
      return YES;
    }
  }
  return NO;
}
-(void) setSetupViewForSensor:(HCFRSensor*)selectedSensor
{
  NSView  *viewToDisplay = nil;
  
  if ([selectedSensor hasASetupView])
    viewToDisplay = [selectedSensor setupView];
  else
    viewToDisplay = [HCFREmptyView emptyViewWithString:HCFRLocalizedString(@"No option for this sensor",
                                                                           @"No option for this sensor")];
  [[sensorOptionsTabView tabViewItemAtIndex:0] setView:viewToDisplay];
  
  [sensorOptionsTabView selectTabViewItemAtIndex:0];
  if ([selectedSensor needsCalibration])
    viewToDisplay = calibrationView;
  else
    viewToDisplay = [HCFREmptyView emptyViewWithString:HCFRLocalizedString(@"No calibration needed for this sensor",
                                                                           @"No calibration needed for this sensor")];
  
  [[sensorOptionsTabView tabViewItemAtIndex:1] setView:viewToDisplay];
  
}
-(bool) selectGeneratorInPopup:(HCFRGenerator*)generator
{
  NSString      *generatorId = [[generator class] generatorId];
  NSEnumerator  *itemsEnumerator = [[[generatorsPopupButton menu] itemArray] objectEnumerator];
  NSMenuItem    *currentItem;

  while (currentItem = [itemsEnumerator nextObject])
  {
    if ([generatorId isEqualToString:[currentItem representedObject]])
    {
      [generatorsPopupButton selectItem:currentItem];
      [self setSetupViewForGenerator:generator];
      return YES;
    }
  }
  return NO;
}
-(void) setSetupViewForGenerator:(HCFRGenerator*)selectedGenerator
{
  NSView  *viewToDisplay;
  
  // on charge la vue de setup
  if ([selectedGenerator hasASetupView])
    viewToDisplay = [selectedGenerator setupView];
  else
    viewToDisplay = [HCFREmptyView emptyViewWithString:HCFRLocalizedString(@"No option for this generator",
                                                                           @"No option for this generator")];
  
  [generatorSetupBox setContentView:viewToDisplay];
  
}
-(bool) selectCalibrationFileWithFilename:(NSString*)filename advertise:(BOOL)advertise
{
  NSEnumerator  *calibrationFilesEnumerator = [[calibrationFilesPopup itemArray] objectEnumerator];
  NSMenuItem    *currentItem;
  int           index = 0;
  
  while (currentItem = [calibrationFilesEnumerator nextObject])
  {
    if ([[currentItem representedObject] hasSuffix:filename])
    {
      [calibrationFilesPopup selectItemAtIndex:index];
      if (advertise)
        [self calibrationFileChanged:calibrationFilesPopup];
      return YES;
    }
    index ++;
  }
  return NO;
}

#pragma mark IBActions et fonctions associées
-(IBAction) startSensorCalibration:(id)Sender
{
  if (calibrationEngine != nil)
    return;
  HCFRCalibrationEngine *newEngine = [[HCFRCalibrationEngine alloc] init];
  calibrationEngine = newEngine;
  [newEngine setDataSerie:[dataStore currentSerie]];
  [calibrationEngine setDelegate:self];
  [calibrationEngine setConfiguration:configuration];
  if (![newEngine startSensorCalibration])
  {
    [calibrationEngine release];
    calibrationEngine = nil;
  }
}
-(IBAction) startMeasures:(id)Sender
{
  [self startMeasuresForMask:kAllMeasures];
}
-(IBAction) startMesureFromMenu:(NSMenuItem*)menuItem
{
  [self startMeasuresForMask:(HCFRMeasuresType)[menuItem tag]];
}
-(IBAction) startMesureFromTooblarItem:(NSToolbarItem*)toolbarItem
{
  [self startMeasuresForMask:(HCFRMeasuresType)[toolbarItem tag]];
}
-(IBAction) startContinuousMesure:(id)Sender
{
  if (calibrationEngine != nil)
    return;

  [self createNewSerieIfNeeded];
  
  HCFRContinuousAcquisitionEngine *newEngine = [[HCFRContinuousAcquisitionEngine alloc] initWithPluginController:pluginsController];
  calibrationEngine = newEngine;
  [newEngine setDataSerie:[dataStore currentSerie]];
  [calibrationEngine setDelegate:self];
  [calibrationEngine setConfiguration:configuration];
  // on lance les mesures.
  // Si le lancement est annulé, on détruit le moteur.
  if (![newEngine startMeasuresWithColorReference:[dataStore colorReference]])
  {
    [calibrationEngine release];
    calibrationEngine = nil;
  }
}
-(IBAction) cancelCalibration:(id)sender
{
  [calibrationEngine abortMeasures];
}
-(IBAction) showSetupSheet:(id)sender
{
  // pas de blague, on ne modifie pas la config en cours de mesure !
  if ([calibrationEngine isRunning])
    return;
  
  // on charge le gamma
  [gammaTextField setFloatValue:[dataStore gamma]];
  
  // la référence colorimétrique
  
  // et on charge les param de calibration
  if ([[configuration sensor] needsCalibration])
  {
    NSString *currentCalibrationFile = [[configuration sensor] currentCalibrationFile];
    if (currentCalibrationFile == Nil)
      [calibrationTypeMatrix selectCellAtRow:1 column:0];
    else
    {
      [calibrationTypeMatrix selectCellAtRow:0 column:0];
      [self selectCalibrationFileWithFilename:currentCalibrationFile advertise:NO];
    }
  }
  
  // on charge le capteur actuel
  [self selectSensorInPopup:[configuration sensor]];
  
  // on charge le generateur actuel
  [self selectGeneratorInPopup:[configuration generator]];

  // on recharge les profiles au cas ou la liste aurait changée
  [self loadCalibrationFilesInPopup];
  
  [NSApp beginSheet:setupWindow
     modalForWindow:mainWindow
      modalDelegate:nil
     didEndSelector:nil
        contextInfo:nil];
}
-(IBAction) endSetupSheet:(id)sender
{
  [NSApp endSheet:setupWindow];
  [setupWindow orderOut:self];
}
-(IBAction) sensorChanged:(id)sender
{
  NSString        *selectedSensorId = [[sensorsPopupButton selectedItem] representedObject];
  HCFRSensor      *selectedSensor = nil;
  
  NSLog (@"selectedSensorId = %@", selectedSensorId);
  
  @try
  {
    // Si le capteur selectionné n'est pas le capteur actuel, on l'instancie
    if ([configuration sensor] == nil ||
        ![[[[configuration sensor] class] sensorId] isEqualToString:selectedSensorId])
    {
      selectedSensor = [pluginsController sensorWithId:selectedSensorId];
      NSAssert (selectedSensor != nil, @"sensorWithId returned nil -> incoherence between sensor popup and pluginsController");

      NSLog (@"selectedSensor = %@", selectedSensor);

      // on charge le capteur
      [configuration setSensor:selectedSensor];
      
      // On simule un changement de fichier de calibration, pour charger celui qui est selectionne
      if (([selectedSensor needsCalibration]) && ([calibrationTypeMatrix selectedRow] == 0))
      {
        [self calibrationFileChanged:calibrationFilesPopup];
      }
    }
    // sinon, on reprend le capteur actuel, et on lui envoi un refresh
    else
    {
      selectedSensor = [configuration sensor];
      [selectedSensor refresh]; // pour vérifier les capteurs connectés
    }
  }
  @catch (NSException *e)
  {
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Cannot select this sensor",
                                                                       @"Cannot select this sensor")
                                     defaultButton:@"Ok" alternateButton:nil otherButton:nil
                         informativeTextWithFormat:[e reason]];
    [alert beginSheetModalForWindow:mainWindow modalDelegate:nil didEndSelector:nil contextInfo:nil];
    return;
  }

  [self setSetupViewForSensor:selectedSensor];
  
  // on stock l'ID du capteur dans les préférences
  [[NSUserDefaults standardUserDefaults] setObject:selectedSensorId forKey:kHCFRDefaultSensorId];
}
-(IBAction) generatorChanged:(id)sender
{
//  NSView          *viewToDisplay = nil;
  
  NSString        *selectedGeneratorId = [[generatorsPopupButton selectedItem] representedObject];
  HCFRGenerator   *selectedGenerator = nil;
  
  @try
  {
    // Si le capteur selectionné n'est pas le capteur actuel, on l'instancie
    if ([configuration generator] == nil ||
        ![[[[configuration generator] class] generatorId] isEqualToString:selectedGeneratorId])
    {
      selectedGenerator = [pluginsController generatorWithId:selectedGeneratorId];
      NSAssert (selectedGenerator != nil, @"sensorWithId returned nil -> incoherence between generators popup and pluginsController");
      
      // on charge le générateur
      [configuration setGenerator:selectedGenerator];
      [selectedGenerator setColorReference:[dataStore colorReference]];
    }
    // sinon, on reprend le capteur actuel, et on lui envoi un refresh
    else
    {
      selectedGenerator = [configuration generator];
    }
  }
  @catch (NSException *e)
  {
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Cannot select this generator",
                                                                       @"Cannot select this generator")
                                     defaultButton:@"Ok" alternateButton:nil otherButton:nil
                         informativeTextWithFormat:[e reason]];
    [alert beginSheetModalForWindow:mainWindow modalDelegate:nil didEndSelector:nil contextInfo:nil];
    return;
  }
  
  [self setSetupViewForGenerator:selectedGenerator];
    
  // on stock l'ID du capteur dans les préférences
  [[NSUserDefaults standardUserDefaults] setObject:[[selectedGenerator class] generatorId] forKey:kHCFRDefaultGeneratorId];
}
-(IBAction) colorReferenceChanged:(NSPopUpButton*)sender
{
  int tag = [sender selectedTag];
  ColorStandard standard = (ColorStandard)tag; // un peu brutal ...
  
  HCFRColorReference  *colorReference = [[HCFRColorReference alloc] initWithColorStandard:standard whiteTarget:D65];
  
  [dataStore setColorReference:colorReference];
  
  // on stock le colorStandard dans les prefs
  [[NSUserDefaults standardUserDefaults] setInteger:tag forKey:kHCFRDefaultColorStandard];
  
  [colorReference release];
}
-(IBAction) gammaChanged:(NSTextField*)sender
{
  float newGamma = [sender floatValue];
  [dataStore setGamma:newGamma];
  // on enregistre la nouvelle valeur comme valeur par defaut
  [[NSUserDefaults standardUserDefaults] setFloat:newGamma forKey:kHCFRDefaultGammaValue];
}
-(IBAction) calibrationModeChanged:(NSMatrix*)sender
{
  // on active le popup de séléction du fichier si nécessaire.
  [calibrationFilesPopup setEnabled:([calibrationTypeMatrix selectedRow] == 0)];
  
  // et si on choisie un fichier, on lit celui sélectionné
  if ([calibrationTypeMatrix selectedRow] == 0)
    [self calibrationFileChanged:calibrationFilesPopup];
  else
    [[configuration sensor] clearCalibrationData];
}
-(IBAction) calibrationFileChanged:(NSPopUpButton*)sender
{
  NSString *filename = [[calibrationFilesPopup selectedItem] representedObject];

  if (filename == Nil)
  {
    NSLog (@"calibrationFileChanged : nil filename !");
    return;
  }

  [self loadCalibrationFile:filename];
}
-(IBAction) save:(id)sender
{
  NSString  *filename = [dataStore filename];
  
  if (filename == nil)
  {
    NSSavePanel *savePanel = [NSSavePanel savePanel];
    int         result;
    
    [savePanel setRequiredFileType:@"hcfrdata"];
    result = [savePanel runModalForDirectory:NSHomeDirectory() file:HCFRLocalizedString(@"calibration data",
                                                                                  @"calibration data")];
    
    if (result != NSOKButton)
      return;

    filename = [savePanel filename];
  }
  
  [self saveToPath:filename];
}
-(IBAction) load:(id)sender
{
  NSArray     *fileTypes = [NSArray arrayWithObject:@"hcfrdata"];
  NSOpenPanel *openPanel = [NSOpenPanel openPanel];
  int         result;
  
  [openPanel setRequiredFileType:@"hcfrdata"];
  result = [openPanel runModalForDirectory:NSHomeDirectory() file:nil types:fileTypes];
  
  if (result == NSOKButton)
  {
    [self loadFileAtPath:[openPanel filename]];
  }
}
-(IBAction) importFile:(id)sender
{
  NSOpenPanel *openPanel = [NSOpenPanel openPanel];
  
  [openPanel beginSheetForDirectory:nil
                               file:nil
                              types:[HCFRDataSerie importableExtensions]
                     modalForWindow:mainWindow
                      modalDelegate:self
                     didEndSelector:@selector(importFileSheetDidEnd:returnCode:contextInfo:)
                        contextInfo:nil];
}
-(IBAction) updateProfiles:(id)sender
{
  [[HCFRProfilesManager sharedinstance] updateProfiles];
}
-(IBAction) showHideGraphInspector:(id)sender
{
  if ([graphInspectorWindow isVisible])
    [graphInspectorWindow close];
  else
    [graphInspectorWindow makeKeyAndOrderFront:sender];
}
-(IBAction) showAboutPanel:(id)sender
{
  [[HCFRAboutWindowController sharedInstance] showWindow:self];
}

-(IBAction) startTool:(id)sender
{
  Class toolClassToStart = [sender representedObject];
  @try
  {
    [pluginsController startTool:toolClassToStart];
  }
  @catch (NSException *exception)
  {
    [[NSAlert alertWithMessageText:HCFRLocalizedString(@"The tool could not be started", @"The tool could not be started")
                     defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                   alternateButton:nil
                       otherButton:nil
         informativeTextWithFormat:[exception reason]] runModal];
  }
}
-(IBAction) createNewDataStore:(id)sender
{
  HCFRDataStore *newDataStore = [[HCFRDataStore alloc] init];
  if (![[NSUserDefaults standardUserDefaults] objectForKey:kHCFRDefaultGammaValue])
    [newDataStore setGamma:2.2];
  else
    [newDataStore setGamma:[[NSUserDefaults standardUserDefaults] floatForKey:kHCFRDefaultGammaValue]];
  
  // on charge le color Standard des preferences
  HCFRColorReference *reference;
  if (![[NSUserDefaults standardUserDefaults] objectForKey:kHCFRDefaultColorStandard])
    reference = [[HCFRColorReference alloc] initWithColorStandard:(ColorStandard)[colorStandardPopup selectedTag] whiteTarget:D65];
  else
  {
    ColorStandard standard = (ColorStandard)[[NSUserDefaults standardUserDefaults] integerForKey:kHCFRDefaultColorStandard];
    [colorStandardPopup selectItemWithTag:standard];
    reference = [[HCFRColorReference alloc] initWithColorStandard:standard whiteTarget:D65];
  }
  [newDataStore setColorReference:reference];
  [reference release];
  
  [self setDataStore:newDataStore];
  
  [newDataStore release];

  [self showSetupSheet:self];    
}

#pragma mark Divers
- (void)copy:(id)sender
{
  // Si on arrive là, c'est que aucun responder n'a géré le copier.
  // On n'est donc pas dans un champ text
  [dataTableController copyToPasteboard];
}
-(void) startMeasuresForMask:(HCFRMeasuresType)measures
{
  [self createNewSerieIfNeeded];
  
  HCFRCalibrationEngine *newEngine = [[HCFRCalibrationEngine alloc] init];
  calibrationEngine = newEngine;
  [newEngine setDataSerie:[dataStore currentSerie]];
  [calibrationEngine setDelegate:self];
  [calibrationEngine setConfiguration:configuration];
  
  // on lance les mesures, en cas d'annulation, on détruit le moteur.
  if (![newEngine startMeasures:measures])
  {
    [calibrationEngine release];
    calibrationEngine = nil;
  }
  
}
-(void) loadCalibrationFile:(NSString*)filename
{
  if (![[configuration sensor] readCalibrationFile:filename])
    [NSAlert alertWithMessageText:HCFRLocalizedString(@"An error occured while reading the calibration file.",
                                                      @"An error occured while reading the calibration file.")
                    defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                  alternateButton:nil otherButton:nil informativeTextWithFormat:HCFRLocalizedString(@"The calibration file %@ could not be loaded.",
                                                                                                    @"The calibration file %@ could not be loaded."), filename];
}
-(void) createNewSerieIfNeeded
{
  int   action = [[NSUserDefaults standardUserDefaults] integerForKey:kHCFRNewMeasuresWithNonEmptySerieAction];
  BOOL  createNewSerie = NO;
  if (action == askTheUserAction)
  {
    if ([[dataStore currentSerie] count] > 0)
    {
      NSAlert* alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Do you want to start a new data serie ?",
                                                                         @"Do you want to start a new data serie ?")
                                       defaultButton:HCFRLocalizedString(@"Yes",@"Yes")
                                     alternateButton:HCFRLocalizedString(@"No",@"No")
                                         otherButton:nil
                           informativeTextWithFormat:HCFRLocalizedString(@"If you do not start a new serie, the new measures will be appended to the old ones .",
                                                                         @"If you do not start a new serie, the new measures will be appended to the old ones.")];
      
      createNewSerie = ([alert runModal] == NSAlertDefaultReturn);
    }
  }
  else if (action == createNewSerieAction) {
    createNewSerie = YES;
  }
  
  if (createNewSerie) {
    [seriesTableController addNewSerie:self];
  }
}

#pragma mark chargement/sauvegarde du dataStore
-(BOOL) loadFileAtPath:(NSString*)path
{
  NSData            *data = [NSData dataWithContentsOfFile:path];
  if (data == nil)
    return NO;
  NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingWithData:data];
  
  NSString  *sensorId = [unarchiver decodeObjectForKey:@"sensorId"];
  NSString  *generatorId = [unarchiver decodeObjectForKey:@"generatorId"];
  HCFRDataStore *newDataStore = [unarchiver decodeObjectForKey:@"dataStore"];

  // on met le capteur
  HCFRSensor  *newSensor = [pluginsController sensorWithId:sensorId];
  if (newSensor == nil)
  {
    newSensor = [pluginsController sensorWithId:[[[pluginsController sensorPlugins] objectAtIndex:0] sensorId]];
    [[NSAlert alertWithMessageText:HCFRLocalizedString(@"Sensor could not be found",
                                                     @"Sensor could not be found")
                     defaultButton:@"Ok" alternateButton:Nil otherButton:Nil
         informativeTextWithFormat:HCFRLocalizedString(@"The sensor specified in the file is not available.\nA default sensor is used instead.",
                                                     @"The sensor specified in the file is not available.\nA default sensor is used instead.")] runModal];
  }
  [configuration setSensor:newSensor];
  // et ses options
  NSDictionary *sensorPreferences = [unarchiver decodeObjectForKey:@"generatorPreferences"];
  if (sensorPreferences != nil)
    [newSensor setPreferencesWithDictionary:sensorPreferences];
  
  // on met le generateur
  HCFRGenerator  *newGenerator = [pluginsController generatorWithId:generatorId];
  if (newGenerator == nil)
  {
    newGenerator = [pluginsController generatorWithId:[[[pluginsController generatorPlugins] objectAtIndex:0] generatorId]];
    [[NSAlert alertWithMessageText:HCFRLocalizedString(@"Generator could not be found",
                                                     @"Generator could not be found")
                     defaultButton:@"Ok" alternateButton:Nil otherButton:Nil
         informativeTextWithFormat:HCFRLocalizedString(@"The generator specified in the file is not available.\nA default generator is used instead.",
                                                     @"The generator specified in the file is not available.\nA default generator is used instead.")] runModal];
  }
  [configuration setGenerator:newGenerator];
  // et ses options
  NSDictionary *generatorPreferences = [unarchiver decodeObjectForKey:@"generatorPreferences"];
  if (generatorPreferences != nil)
    [newGenerator setPreferencesWithDictionary:generatorPreferences];
  
  // le fichier de calibration
  if ([unarchiver decodeBoolForKey:@"useCalibrationFile"])
  {
    [calibrationTypeMatrix selectCellAtRow:0 column:0];
    NSString *profilename = [unarchiver decodeObjectForKey:@"calibrationFileName"];
//    NSLog (@"Loading profile : %@", profilename);
    NSString *filename = [HCFRProfilesManager getProfileWithName:profilename];
    if (filename == nil)
    {
      [[NSAlert alertWithMessageText:HCFRLocalizedString(@"Saved calibration file could not be found.",
                                                      @"Saved calibration file could not be found.")
                      defaultButton:@"Ok" alternateButton:Nil otherButton:Nil
          informativeTextWithFormat:HCFRLocalizedString(@"The recorded calibration file (%@) could not be found. A default file is used instead.",
                                                      @"The recorded calibration file (%@) could not be found. A default file is used instead."), profilename] runModal];
    }
    else
    {
      [self loadCalibrationFile:filename];
    }
  }
  else
  {
//    NSLog (@"Loading manual calibration ...");
    [calibrationTypeMatrix selectCellAtRow:1 column:0];
  }
  
  [newDataStore setFilename:path];
  [self setDataStore:newDataStore];

  [[NSUserDefaults standardUserDefaults] setObject:path forKey:kHCFRLastDocumentFilepath];
  
  [unarchiver release];
  
  // on met à jour le menu "documents récents"
  [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:path]];
  
  return YES;
}  
-(BOOL) saveToPath:(NSString*)path
{
  @try
  {
    NSMutableData   *data = [[NSMutableData alloc] init];
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initForWritingWithMutableData:data];
    
    // le capteur et ses options
//    NSLog (@"saving sensor ID : %@", [[[configuration sensor] class] sensorId]);
    [archiver encodeObject:[[[configuration sensor] class] sensorId] forKey:@"sensorId"];
    NSDictionary  *sensorPreferences = [[configuration sensor] getPreferencesDictionary];
    if (sensorPreferences != Nil)
      [archiver encodeObject:sensorPreferences forKey:@"sensorPreferences"];

    // le générateur et ses options
//    NSLog (@"saving generator ID : %@", [[[configuration generator] class] generatorId]);
    [archiver encodeObject:[[[configuration generator] class] generatorId] forKey:@"generatorId"];
    NSDictionary  *generatorPreferences = [[configuration generator] getPreferencesDictionary];
    if (generatorPreferences != Nil)
      [archiver encodeObject:generatorPreferences forKey:@"generatorPreferences"];

    
    
    // le fichier de calibration utilisé (si il y en a un)
    if ([[configuration sensor] currentCalibrationFile] != Nil)
    {
//      NSLog (@"saving use calib file : YES : %@", [[[configuration sensor] currentCalibrationFile] lastPathComponent]);
      [archiver encodeBool:YES forKey:@"useCalibrationFile"];
      [archiver encodeObject:[[[configuration sensor] currentCalibrationFile] lastPathComponent] forKey:@"calibrationFileName"];
    }
    else
    {
//      NSLog (@"saving use calib file : NO");
      [archiver encodeBool:NO forKey:@"useCalibrationFile"];      
    }
    
    [archiver encodeObject:dataStore forKey:@"dataStore"];
    [archiver finishEncoding];
    [archiver release];
      
    BOOL result = [data writeToFile:path atomically:YES];
    if (!result)
      [[NSException exceptionWithName:@"SavingException" reason:@"File could not be written" userInfo:nil] raise];
    
    [dataStore setFilename:path];
    [[NSUserDefaults standardUserDefaults] setObject:path forKey:kHCFRLastDocumentFilepath];
  }
  @catch (NSException *exception)
  {
    [[NSAlert alertWithMessageText:HCFRLocalizedString(@"Data could not be saved", @"Data could not be saved")
                     defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                   alternateButton:nil
                       otherButton:nil
         informativeTextWithFormat:[exception reason]] runModal];
    return NO;
  }

  // on met à jour le menu documents récents
  [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:path]];
  
  return YES;
}
-(void) setDataStore:(HCFRDataStore*)newDataStore
{
  NSAssert (newDataStore != nil, @"HCFRMainController : cannot set nil dataStore");
  
  if (dataStore != nil)
  {
    // si un dataStore est actuellement ouvert et qu'il est dirty, on propose de le sauvegarder
    if ([dataStore dirty])
    {
      NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Do you want to save the current data ?",
                                                                         @"Do you want to save the current data ?")
                                       defaultButton:HCFRLocalizedString(@"Yes", @"Yes")
                                     alternateButton:HCFRLocalizedString(@"No", @"No")
                                         otherButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                           informativeTextWithFormat:@""];
      int result = [alert runModal];
      
      if (result == NSAlertDefaultReturn)
      {
        [self save:self];
        // Si le data store est encore dirty, c'est que l'utilisateur à annulé.
        if ([dataStore dirty])
          return;
      }
      else if (result == NSAlertOtherReturn)
        return;
    }
    
    [dataStore removeObserver:self forKeyPath:@"currentSerie"];
    [dataStore removeObserver:self forKeyPath:@"referenceSerie"];
    [dataStore removeObserver:self forKeyPath:@"colorReference"];
    [dataStore removeObserver:self forKeyPath:@"gamma"];
    [dataStore release];
  }
  
  dataStore = [newDataStore retain];
  [colorStandardPopup selectItemWithTag:[[dataStore colorReference] colorStandard]];
  [gammaTextField setFloatValue:[dataStore gamma]];
  [dataStore addObserver:self forKeyPath:@"colorReference" options:0 context:nil];
  [dataStore addObserver:self forKeyPath:@"gamma" options:0 context:nil];
  [dataStore addObserver:self forKeyPath:@"currentSerie" options:0 context:nil];
  [dataStore addObserver:self forKeyPath:@"referenceSerie" options:0 context:nil];
  
  // on simule un changement de gamma et de reference colorimétrique
  // pour les propager
  // on change la couleur de ref avant tout,
  // car certains graphs ont besoin d'une colorReference pour tracer.
  [self observeValueForKeyPath:@"colorReference" ofObject:dataStore change:[NSDictionary dictionary] context:nil];
  [self observeValueForKeyPath:@"gamma" ofObject:dataStore change:[NSDictionary dictionary] context:nil];
  
  // puis on fournis le dataStore au controler de la table des séries
  [seriesTableController setDataStore:dataStore];
  
  // et on signale un changement de série de reference et de serie courante
  [self observeValueForKeyPath:@"currentSerie" ofObject:dataStore change:[NSDictionary dictionary] context:nil];
  [self observeValueForKeyPath:@"referenceSerie" ofObject:dataStore change:[NSDictionary dictionary] context:nil];
}

#pragma mark tabView delegate
- (void)tabView:(NSTabView *)tabView didSelectTabViewItem: (NSTabViewItem *)tabViewItem
{
  // si c'est le tab du graph, on met à jour l'inspecteur de graph
  if (tabView == graphsTabView)
  {
    int     position = [tabView indexOfTabViewItem:tabViewItem];
    NSView  *viewToDisplay = noGraphOptionsView;
    int			titleBarHeight = [graphOptionsPanel frame].size.height - [[graphOptionsPanel contentView] frame].size.height;
    
    HCFRGraphController *graphController = [[pluginsController graphicPlugins] objectAtIndex:position];
    
    NSRect oldFrame = [graphOptionsPanel frame];
    
    // la vue noOptions est utilisée a plusieurs endroits, elle a pu être redimentionnée par un autre utilisateur
    // donc, on remet la taille qu'on souhaite
    [noGraphOptionsView setFrameSize:NSMakeSize(148, 83)];
    
    if ([graphController hasAnOptionsView])
      viewToDisplay = [graphController optionsView];
    
    NSRect newFrame = [graphOptionsPanel frame];
    newFrame.size = [viewToDisplay frame].size;
    newFrame.size.height += titleBarHeight;
    newFrame.origin.y -= (newFrame.size.height - oldFrame.size.height);
    [graphOptionsPanel setContentView:viewToDisplay];
    [graphOptionsPanel setFrame:newFrame display:YES animate:YES];
  }
}
#pragma mark KVO listener
- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
  if (object == dataStore)
  {
    if ([keyPath isEqualToString:@"currentSerie"])
    {
      // on signale à tout le monde que la série à changée
      
      // le calibration engine
//      [calibrationEngine setDataSerie:[dataStore currentSerie]];
      
      // les différents graphiques
      NSEnumerator *pluginsEnumerator = [[pluginsController graphicPlugins] objectEnumerator];
      HCFRGraphController *currentGraph;
      while (currentGraph = [pluginsEnumerator nextObject])
      {
        [currentGraph setCurrentSerie:[dataStore currentSerie]];
      }
      
      // la table de données
      [dataTableController setDataSerie:[dataStore currentSerie]];
    }
    else if ([keyPath isEqualToString:@"referenceSerie"])
    {
      // les différents graphiques
      NSEnumerator *pluginsEnumerator = [[pluginsController graphicPlugins] objectEnumerator];
      HCFRGraphController *currentGraph;
      while (currentGraph = [pluginsEnumerator nextObject])
      {
        [currentGraph setReferenceSerie:[dataStore referenceSerie]];
      }
    }
    else if ([keyPath isEqualToString:@"colorReference"])
    {
      // les différents graphiques
      NSEnumerator *pluginsEnumerator = [[pluginsController graphicPlugins] objectEnumerator];
      HCFRGraphController *currentGraph;
      while (currentGraph = [pluginsEnumerator nextObject])
      {
        [currentGraph setColorReference:[dataStore colorReference]];
      }
      
      // le générateur
      [[configuration generator] setColorReference:[dataStore colorReference]];
      
      // la table de données
      [dataTableController setColorReference:[dataStore colorReference]];
    }
    else if ([keyPath isEqualToString:@"gamma"])
    {
      // les différents graphiques
      NSEnumerator *pluginsEnumerator = [[pluginsController graphicPlugins] objectEnumerator];
      HCFRGraphController *currentGraph;
      while (currentGraph = [pluginsEnumerator nextObject])
      {
        [currentGraph setGamma:[dataStore gamma]];
      }
    }
  }
}

#pragma mark delegué du calibration engine
-(void) engineStartMeasures:(HCFRAbstractEngine*)startingEngine
{
  // on utilise objectForKey pour savoir si une valeur à été entrée ou non :
  // si non, on récupère nil.
  // boolForKey aurait retourné NO par defaut, sans préciser si c'est une valeur par défaut.
  // Si il n'y a pas de valeur, on en met une (YES pour l'occasion)
  if ([[NSUserDefaults standardUserDefaults] objectForKey:kHCFRDarkenUnusedScreens] == nil)
    [[NSUserDefaults standardUserDefaults] setBool:YES forKey:kHCFRDarkenUnusedScreens];
  NSLog (@"1");
  HCFRScreensManager *screensManager = [HCFRScreensManager sharedManager];
  [screensManager setCalibratedScreen:[startingEngine getCalibratedScreen]];
  if ([[NSUserDefaults standardUserDefaults] boolForKey:kHCFRDarkenUnusedScreens])
  {
    NSLog (@"2");
    if (calibrationEngine != nil && [calibrationEngine getFullScreenControlView] != nil)
      [screensManager setControlView:[calibrationEngine getFullScreenControlView]];
    [screensManager darken];
  }
  else
  {
    [screensManager ensureWindowVisibility:mainWindow];
  }
  
  [statusTextField setStringValue:HCFRLocalizedString(@"Measuring ...",
                                                    @"Measuring ...")];
  [statusProgressIndicator setHidden:NO];
  [statusProgressIndicator setUsesThreadedAnimation:YES];
  [statusProgressIndicator startAnimation:self];
  
  // on n'a pas besoin de demander à la toolbar de revalider,
  // car en cas de clic sur un menu ou sur un de ces items, ça se fait automatiquement.
}
-(void) engineMeasuresEnded:(HCFRAbstractEngine*)endingEngine
{
  [[HCFRScreensManager sharedManager] enlight];

  [statusTextField setStringValue:HCFRLocalizedString(@"Idle",
                                                    @"Idle")];
  [statusProgressIndicator setHidden:YES];
  [statusProgressIndicator stopAnimation:self];

  // on demande a la toolbar de revalider ses items
  [mainToolbar validateVisibleItems];
  
  //TODO : supprimer la var de class calibrationEngine
  // et utiliser celui reçus en paramètre en échange.
  // A voir : il faut créer un lock a prendre quand la calibration démarre
  [calibrationEngine autorelease];
  calibrationEngine = nil;
}

#pragma mark Gestion du splash
-(void) showSplash
{
  NSImage       *splash = [NSImage imageNamed:@"splash_shadowed.png"];
  NSImageView   *imageView = [[NSImageView alloc] init];
  NSRect        windowRect = NSMakeRect(0,0,0,0);
  
  windowRect.size = [splash size];
  [imageView setImage:splash];
  
  splashWindow = [[NSWindow alloc] initWithContentRect:windowRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
  [splashWindow setReleasedWhenClosed:NO];
  
  // transparente la fenêtre du splash !
  [splashWindow setOpaque:NO];
  [splashWindow setBackgroundColor:[NSColor colorWithDeviceWhite:1.0 alpha:0.0]];
  
  // et flotante en plus
  [splashWindow setLevel:NSFloatingWindowLevel];
  
  [splashWindow setContentView:imageView];
  [splashWindow center];
  [splashWindow makeKeyAndOrderFront:self];
  [imageView release];
}
- (void)closeSplash
{
  if (splashWindow != nil)
  {
    [splashWindow close]; 
    [splashWindow release];
    splashWindow = nil;
  }
}

#pragma mark fonctions callback pour les open et save panels
- (void)importFileSheetDidEnd:(NSOpenPanel *)panel returnCode:(int)returnCode  contextInfo:(void*)contextInfo
{
  if (returnCode == NSOKButton)
  {
    HCFRDataSerie *newSerie = [[HCFRDataSerie alloc] init];
    @try
    {
      [newSerie setName:[[panel filename] lastPathComponent]];
      [newSerie importFile:[panel filename]];
      [dataStore addSerie:newSerie andSelectIt:NO];
    }
    @catch (NSException *e)
    {
      NSAlert * alert;
      if ([[e name] isEqualToString:kHCFRFileFormatException])
      {
        alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Import failed",
                                                                  @"Import failed")
                                defaultButton:@"Ok" alternateButton:Nil otherButton:Nil
                    informativeTextWithFormat:HCFRLocalizedString(@"Unknown file format.",
                                                                  @"Unknown file format.")];
      }
      else
      {
        alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Import failed",
                                                                  @"Import failed")
                                defaultButton:@"Ok" alternateButton:Nil otherButton:Nil
                    informativeTextWithFormat:HCFRLocalizedString(@"The import of the file failed.\nYou can find more informations on why it failed in the console.",
                                                                  @"The import of the file failed.\nYou can find more informations on why it failed in the console.")];
      }

      [alert  beginSheetModalForWindow:mainWindow
                         modalDelegate:nil
                        didEndSelector:nil
                           contextInfo:nil];
      [newSerie release];
    }
    
  }
}
@end
