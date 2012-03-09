//  ColorHCFR
//  HCFRIRProfilesManager.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 02/05/08.
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

#import <libHCFR/IHCFile.h>

#import "HCFRIRProfilesManager.h"
#import "HCFRKiGeneratorIRProfilesRepository.h"
#import "HCFRKiDevice.h"

#define kIRProfileManagerExportFormat   @"IRProfileManagerExportFormat"

const int               downArrowTag = 1;
const int               rightArrowTag = 2;
const int               nextChapterTag = 3;
const int               okKeyTag = 4;

// le tag 0 n'existe pas, on remplie la case avec un code quelconque dans le tableau de correspondance.
const IRProfileCodeTypes  tagToTypeArray[5] = {IRProfileDownArrowCode, IRProfileDownArrowCode, IRProfileRightArrowCode, IRProfileNextChapterCode, IRProfileOkCode};

@interface HCFRIRProfilesManager (Private)
-(void) setCurrentProfileIsEditable:(BOOL)editable;
// data source pour la outline view
- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item;
- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item;
- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item;
- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn*)tableColumn byItem:(id)item;
// delegate pour la outline view
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectItem:(id)item;
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldEditTableColumn:(NSTableColumn *)tableColumn item:(id)item;
- (void)outlineViewSelectionDidChange:(NSNotification *)notification;
  // delegate pour les textView
- (void)textDidEndEditing:(NSNotification *)aNotification;
// les fonctions d'import/export
-(void) addProfileAndSelectIt:(HCFRKiGeneratorIRProfile*)newProfile;
-(BOOL) importFiles:(NSArray*)filenames;
-(BOOL) importIHCFileAtPath:(NSString*)path;
-(NSString*) exportProfile:(HCFRKiGeneratorIRProfile*)profile asIHCFileToDirectory:(NSString*)destinationPath;
-(NSString*) exportProfile:(HCFRKiGeneratorIRProfile*)profile asXMLFileToDirectory:(NSString*)destinationPath;

- (void)importFileSheetDidEnd:(NSOpenPanel *)panel returnCode:(int)returnCode  contextInfo:(void*)contextInfo;
@end

@implementation HCFRIRProfilesManager
- (HCFRIRProfilesManager*) init {
  self = [super init];
  if (self != nil) {  
    factoryItem = [@"Factory" retain];
    customItem = [@"Custom" retain];
    
    [[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
                                                                  @"ihc", kIRProfileManagerExportFormat,
                                                                  nil]];
    
    // on charge le nib
    [NSBundle loadNibNamed:@"KiIRProfilesManagerTool" owner:self];
    [irCodeValidityTextField setStringValue:@""]; // on a laissé du text dans le nib pour pouvoir voir le champ
    
    [outlineView registerForDraggedTypes:[NSArray arrayWithObject:NSFilenamesPboardType]];
    [outlineView setDraggingSourceOperationMask:NSDragOperationCopy forLocal:NO];
  }
  return self;
}
- (void) dealloc {
  [factoryItem release];
  [customItem release];
  [super dealloc];
}

#pragma mark Implementation des fonctions de HCFRTool
+(NSString*) toolName {
  return HCFRLocalizedString(@"IR Profiles Manager", @"IR Profiles Manager");
}

-(void) startTool
{
  // on affiche la fenêtre.
  [mainWindow makeKeyAndOrderFront:self];
}

#pragma mark Bindings functions
// ces fonctions sont utisées par les bindings pour activer/desactiver les champs
-(BOOL) isCurrentProfileEditable
{
  HCFRKiGeneratorIRProfile  *selectedProfile = [outlineView itemAtRow:[outlineView selectedRow]];
  
  if (selectedProfile == nil)
    return NO;
  
  // Si c'est un profile perso, on peut l'éditer. Sinon, non.
  return [[[HCFRKiGeneratorIRProfilesRepository sharedRepository] customProfilesArray] containsObject:selectedProfile];
}
-(BOOL) isCurrentCodeTestable
{
  HCFRKiGeneratorIRProfile  *selectedProfile = [outlineView itemAtRow:[outlineView selectedRow]];
  
  return ([selectedProfile validateIrCodeForType:tagToTypeArray[[codesPopup selectedTag]]] == IRProfileCodeIsValide);
}

#pragma mark Utility functions
-(NSString*) getCodeFromProfile:(HCFRKiGeneratorIRProfile*)profile ForCodeTag:(int)tag
{
  return [profile codeAsStringForType:tagToTypeArray[tag]];
}
-(void) setCode:(NSString*)newCode ToProfile:(HCFRKiGeneratorIRProfile*)profile ForCodeTag:(int)tag
{
  [profile setCode:newCode forType:tagToTypeArray[tag]];
}
-(void) validateCurrentCode
{
  HCFRKiGeneratorIRProfile  *selectedProfile = [outlineView itemAtRow:[outlineView selectedRow]];
  IRProfileCodeErrorType    codeStatus = [selectedProfile validateIrCodeForType:tagToTypeArray[[codesPopup selectedTag]]];

  //et on affiche le status du code
  if (codeStatus == IRProfileCodeTooLong)
    [irCodeValidityTextField setStringValue:HCFRLocalizedString(@"Code is too long", @"Code is too long")];
  else if (codeStatus == IRProfileCodeHeaderIsInvalide)
    [irCodeValidityTextField setStringValue:HCFRLocalizedString(@"Code header is invalid", @"Code header is invalid")];
  else if (codeStatus == IRProfileCodeHeaderIsIncomplete)
    [irCodeValidityTextField setStringValue:HCFRLocalizedString(@"Code header is incomplete", @"Code header is incomplete")];
  else if (codeStatus == IRProfileCodeLengthDoNotConformeToHeaders)
    [irCodeValidityTextField setStringValue:HCFRLocalizedString(@"Code length not conform to header", @"Code length not conform to header")];
  else
    [irCodeValidityTextField setStringValue:@""];
}

#pragma mark OutlineView data source
- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
  if (item == nil) // on veut les fils de l'élement racine
  {
    if (index == 0)
      return factoryItem;
    else
      return customItem;
  }
  else if (item == factoryItem)
    return [[[HCFRKiGeneratorIRProfilesRepository sharedRepository] factoryProfilesArray] objectAtIndex:index];
  else if (item == customItem)
    return [[[HCFRKiGeneratorIRProfilesRepository sharedRepository] customProfilesArray] objectAtIndex:index];
  
  NSAssert (NO, @"HCFRIRProfilesManager : invalide idem in outlineView:child:ofItem:");
  return nil;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
  return (item == factoryItem) || (item == customItem);
}

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
  if (item == nil) // on veut les fils de l'élement racine
    return 2;
  else if (item == factoryItem)
    return [[[HCFRKiGeneratorIRProfilesRepository sharedRepository] factoryProfilesArray] count];
  else if (item == customItem)
    return [[[HCFRKiGeneratorIRProfilesRepository sharedRepository] customProfilesArray] count];
  else
    return 0;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn*)tableColumn byItem:(id)item
{
  if ([[tableColumn identifier] isEqualTo:@"name"])
  {
    if ([item isKindOfClass:[HCFRKiGeneratorIRProfile class]])
      return [(HCFRKiGeneratorIRProfile*)item name];
    else if ((item == factoryItem) || (item == customItem))
      return item;
    return @"Unknown item type !";
  }
  if ([[tableColumn identifier] isEqualTo:@"status"])
  {
      if ((item == factoryItem) || (item == customItem))
        return [[[NSImage alloc] init] autorelease]; // une image vide

      if (![item isKindOfClass:[HCFRKiGeneratorIRProfile class]])
        return @"Unknown item type !";
    
      NSString  *imagePath;
      if ([(HCFRKiGeneratorIRProfile*)item isUsable])
      {
        imagePath = [[NSBundle bundleForClass:[self class]] pathForImageResource:@"valid.png"];
      }
      else
      {
        imagePath = [[NSBundle bundleForClass:[self class]] pathForImageResource:@"error.png"];
      }
      return [[[NSImage alloc] initWithContentsOfFile:imagePath] autorelease];
  }
  return @"Unknown column";
}

- (void)outlineView:(NSOutlineView *)outlineView setObjectValue:(id)object
     forTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
  if ([item isKindOfClass:[HCFRKiGeneratorIRProfile class]])
  {
    [(HCFRKiGeneratorIRProfile*)item setName:object];
    [[HCFRKiGeneratorIRProfilesRepository sharedRepository] fireProfilesListChanged];
    [[HCFRKiGeneratorIRProfilesRepository sharedRepository] saveCustomProfiles];
  }
}

#pragma mark Outline view delegate
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectItem:(id)item
{
  return [item isKindOfClass:[HCFRKiGeneratorIRProfile class]];
}
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldEditTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
  // on ne peut éditer que les profiles perso ...
  // On ne ré-utilise pas la fonction isCurrentProfileEditable, parceque le profile edité n'est pas forcement
  // celui selectionné
  return [[[HCFRKiGeneratorIRProfilesRepository sharedRepository] customProfilesArray] containsObject:item];
}
- (void)outlineViewSelectionDidChange:(NSNotification *)notification
{
  // on signale que le status éditable vas changer pour tous les observateurs
  // Necessaire car les bouttons utilisent les bindings pour s'activer / se désactiver en fonction de la séléction.
  [self willChangeValueForKey:@"isCurrentProfileEditable"];
  [self willChangeValueForKey:@"isCurrentCodeTestable"];

  // on charge le profile séléctionné.
  HCFRKiGeneratorIRProfile  *selectedProfile = [outlineView itemAtRow:[outlineView selectedRow]];
  
  // on signale que le status éditable a changé pour tous les observateurs
  // avant de mettre les valeurs, pour éviter les modifications intempestives
  [self didChangeValueForKey:@"isCurrentProfileEditable"];

  if (selectedProfile == nil)
  {
    [authorTextField setStringValue:@""];
    [descriptionTextField setString:@""];
    [codeTextField setString:@""];
    [irCodeValidityTextField setStringValue:@""];
      
    [menuNavigationDelayTextField setIntValue:0];
    [menuSelectionDelayTextField setIntValue:0];
    [chapterNavigationDelayTextField setIntValue:0];
  }
  else
  {
    [authorTextField setStringValue:[selectedProfile author]];
    [descriptionTextField setString:[selectedProfile description]];
    [codeTextField setString:[self getCodeFromProfile:selectedProfile ForCodeTag:[codesPopup selectedTag]]];
    [irCodeValidityTextField setStringValue:@""];
      
    [menuNavigationDelayTextField setIntValue:[selectedProfile delayForType:IRProfileMenuNavigationDelay]];
    [menuSelectionDelayTextField setIntValue:[selectedProfile delayForType:IRProfileMenuValidationDelay]];
    [chapterNavigationDelayTextField setIntValue:[selectedProfile delayForType:IRProfileChapterNavigationDelay]];
    
    [self validateCurrentCode];
  }
  [self didChangeValueForKey:@"isCurrentCodeTestable"];
}

#pragma mark textView delegate
// quand le texte du code change, on met à jour le profile, on valide le code, mais on ne recharge pas le texte, et on ne sauve pas les profiles
- (void)textDidChange:(NSNotification *)aNotification
{
  if ([mainWindow firstResponder] != codeTextField)
    return;
  
  [self willChangeValueForKey:@"isCurrentCodeTestable"];

  HCFRKiGeneratorIRProfile  *selectedProfile = [outlineView itemAtRow:[outlineView selectedRow]];

  [self setCode:[[codeTextField textStorage] string] ToProfile:selectedProfile ForCodeTag:[codesPopup selectedTag]];
  [self validateCurrentCode];

  [self didChangeValueForKey:@"isCurrentCodeTestable"];
}
- (void)textDidEndEditing:(NSNotification *)aNotification
{
  // on ne sait pas quel champ à été édité -> on prend les deux ...
  // on charge le profile séléctionné.
  HCFRKiGeneratorIRProfile  *selectedProfile = [outlineView itemAtRow:[outlineView selectedRow]];
  
  // et on y met les nouvelles valeurs
  [selectedProfile setDescription:[[descriptionTextField textStorage] string]];
  [self setCode:[[codeTextField textStorage] string] ToProfile:selectedProfile ForCodeTag:[codesPopup selectedTag]];
  // on recharge le code, pour afficher le résutat du traitement de la chaîne de char
  [codeTextField setString:[self getCodeFromProfile:selectedProfile ForCodeTag:[codesPopup selectedTag]]];
  
  [[HCFRKiGeneratorIRProfilesRepository sharedRepository] saveCustomProfiles];
}

#pragma mark window delegate
- (void)windowWillClose:(NSNotification *)notification
{
  // on sort des champs texte pour valider un eventuel champ en cours d'édition
  [mainWindow makeFirstResponder:nil];
}

#pragma mark IBActions
-(IBAction) addProfile:(id)sender
{
  HCFRKiGeneratorIRProfile *newProfile = [[HCFRKiGeneratorIRProfile alloc] init];
  
  [[HCFRKiGeneratorIRProfilesRepository sharedRepository] addCustomProfile:newProfile];
  
  [outlineView reloadItem:customItem reloadChildren:YES];
  [outlineView expandItem:customItem];
  [outlineView selectRow:[outlineView rowForItem:newProfile] byExtendingSelection:NO];
  
  [newProfile release];
  [[HCFRKiGeneratorIRProfilesRepository sharedRepository] fireProfilesListChanged];
  [[HCFRKiGeneratorIRProfilesRepository sharedRepository] saveCustomProfiles];
  
  [self outlineViewSelectionDidChange:nil];
}

-(IBAction) deleteProfile:(id)sender
{
  HCFRKiGeneratorIRProfile  *selectedProfile = [outlineView itemAtRow:[outlineView selectedRow]];
  
  [[HCFRKiGeneratorIRProfilesRepository sharedRepository] removeCustomProfile:selectedProfile];

  [outlineView reloadItem:customItem reloadChildren:YES];

  [[HCFRKiGeneratorIRProfilesRepository sharedRepository] fireProfilesListChanged];
  [[HCFRKiGeneratorIRProfilesRepository sharedRepository] saveCustomProfiles];

  [self outlineViewSelectionDidChange:nil];
}

-(IBAction) importProfile:(id)sender
{
  NSOpenPanel *openPanel = [NSOpenPanel openPanel];
  
  [openPanel beginSheetForDirectory:nil
                               file:nil
                              types:[NSArray arrayWithObjects:@"ihc", @"IHC", nil]
                     modalForWindow:mainWindow
                      modalDelegate:self
                     didEndSelector:@selector(importFileSheetDidEnd:returnCode:contextInfo:)
                        contextInfo:nil];
}
-(IBAction) selectedCodeChanged:(id)sender
{
  HCFRKiGeneratorIRProfile  *selectedProfile = [outlineView itemAtRow:[outlineView selectedRow]];
  if (selectedProfile != nil)
  {
    [self willChangeValueForKey:@"isCurrentCodeTestable"];
    [codeTextField setString:[self getCodeFromProfile:selectedProfile ForCodeTag:[codesPopup selectedTag]]];
    [self validateCurrentCode];
    [self didChangeValueForKey:@"isCurrentCodeTestable"];
  }
}
-(IBAction) learnCode:(id)sender
{
  HCFRKiDevice              *device = [HCFRKiDevice sharedDevice];
  HCFRKiGeneratorIRProfile  *profile = [outlineView itemAtRow:[outlineView selectedRow]];
  
  if (profile == nil)
    return;
  
  int          tag = [codesPopup selectedTag];
  
  char         newIRCode[255];
  
  int result = [device readIRCodeToBuffer:newIRCode bufferSize:255];
  if (result == -1)
  {
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Unable to learn the IR code", @"Unable to learn the IR code")
                                     defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                                   alternateButton:nil
                                       otherButton:nil
                         informativeTextWithFormat:HCFRLocalizedString(@"Check that the HCFR sensor is connected.",
                                                                     @"Check that the HCFR sensor is connected.")];
    [alert beginSheetModalForWindow:mainWindow modalDelegate:nil didEndSelector:nil contextInfo:nil];
    return;
  }
  if (result == 0)
  {
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"No IR code received", @"No IR code received")
                                     defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                                   alternateButton:nil
                                       otherButton:nil
                         informativeTextWithFormat:HCFRLocalizedString(@"The remote controller must face the sensor to acquire the signal.",
                                                                     @"The remote controller must face the sensor to acquire the signal.")];
    [alert beginSheetModalForWindow:mainWindow modalDelegate:nil didEndSelector:nil contextInfo:nil];
    return;
  }

  [profile setCodeBuffer:newIRCode codeSize:result forType:tagToTypeArray[tag]];
  [codeTextField setString:[profile codeAsStringForType:tagToTypeArray[tag]]];
  
  // on signale le changement
  [mainWindow makeFirstResponder:codeTextField];
  [self textDidChange:nil];
}
-(IBAction) testCode:(id)sender
{
  HCFRKiDevice              *device = [HCFRKiDevice sharedDevice];
  HCFRKiGeneratorIRProfile  *profile = [outlineView itemAtRow:[outlineView selectedRow]];
  
  if (profile == nil)
    return;
  
  IRCodeBuffer codeBuffer;
  int          tag = [codesPopup selectedTag];
  
  if (tag == okKeyTag)
    codeBuffer = [profile codeBufferForType:IRProfileOkCode];
  else if (tag == rightArrowTag)
    codeBuffer = [profile codeBufferForType:IRProfileRightArrowCode];
  else if (tag == downArrowTag)
    codeBuffer = [profile codeBufferForType:IRProfileDownArrowCode];
  else if (tag == nextChapterTag)
    codeBuffer = [profile codeBufferForType:IRProfileNextChapterCode];

  int result = [device sendIRCode:codeBuffer];
  
  if (result == -1)
  {
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Unable to send the IR code", @"Unable to send the IR code")
                                     defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                                   alternateButton:nil
                                       otherButton:nil
                         informativeTextWithFormat:HCFRLocalizedString(@"Check that the HCFR sensor is connected.",
                                                                     @"Check that the HCFR sensor is connected.")];
    [alert beginSheetModalForWindow:mainWindow modalDelegate:nil didEndSelector:nil contextInfo:nil];
  }
}
-(IBAction) authorChanged:(id)sender
{
  // on charge le profile séléctionné.
  HCFRKiGeneratorIRProfile  *selectedProfile = [outlineView itemAtRow:[outlineView selectedRow]];
  
  // et on y met la nouvelle valeur
  [selectedProfile setAuthor:[authorTextField stringValue]];
  
  [[HCFRKiGeneratorIRProfilesRepository sharedRepository] saveCustomProfiles];
}
-(IBAction) delayChanged:(id)sender
{
  // on charge le profile séléctionné.
  HCFRKiGeneratorIRProfile  *selectedProfile = [outlineView itemAtRow:[outlineView selectedRow]];
  
  [selectedProfile setDelay:[menuNavigationDelayTextField intValue] forType:IRProfileMenuNavigationDelay];
  [selectedProfile setDelay:[menuSelectionDelayTextField intValue] forType:IRProfileMenuValidationDelay];
  [selectedProfile setDelay:[chapterNavigationDelayTextField intValue] forType:IRProfileChapterNavigationDelay];

  [[HCFRKiGeneratorIRProfilesRepository sharedRepository] saveCustomProfiles];
}

#pragma mark Fonction d'import / export
-(void) addProfileAndSelectIt:(HCFRKiGeneratorIRProfile*)newProfile
{
  [[HCFRKiGeneratorIRProfilesRepository sharedRepository] addCustomProfile:newProfile];
  
  [outlineView reloadItem:customItem reloadChildren:YES];
  [outlineView expandItem:customItem];
  [outlineView selectRow:[outlineView rowForItem:newProfile] byExtendingSelection:NO];
  
}
-(BOOL) importFiles:(NSArray*)filenames
{
  NSEnumerator  *itemsEnumerator = [filenames objectEnumerator];
  NSString      *currentFilename;
  
  BOOL          result = NO;
  
  while (currentFilename = [itemsEnumerator nextObject])
  {
    if ([currentFilename hasSuffix:@".ihc"] || [currentFilename hasSuffix:@".IHC"])
    {
      result = [self importIHCFileAtPath:currentFilename] || result;
    }
    else if ([currentFilename hasSuffix:@".xml"])
    {
      result = [[HCFRKiGeneratorIRProfilesRepository sharedRepository] importXMLFile:currentFilename] || result;
    }
  }    
  
  if (result)
  {
    [outlineView reloadItem:customItem reloadChildren:YES];
    [[HCFRKiGeneratorIRProfilesRepository sharedRepository] fireProfilesListChanged];
    [[HCFRKiGeneratorIRProfilesRepository sharedRepository] saveCustomProfiles];
  }
  
  return result;
}
-(BOOL) importIHCFileAtPath:(NSString*)path
{
  const char    *filePath = [path cStringUsingEncoding:NSUTF8StringEncoding];
  IHCFile fileReader;
  try
  {
    fileReader.readFile(filePath);
  }
  catch (...)
  {
    NSAlert  *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Import failed",
                                                                       @"Import failed")
                                     defaultButton:@"Ok" alternateButton:Nil otherButton:Nil
                         informativeTextWithFormat:HCFRLocalizedString(@"An error occured while reading file.",
                                                                       @"An error occured while reading file.")];
    [alert  beginSheetModalForWindow:mainWindow
                       modalDelegate:nil
                      didEndSelector:nil
                         contextInfo:nil];    
    return NO;
  }
  
  // on construit le profile à partir de ce qu'on à lu
  HCFRKiGeneratorIRProfile  *newProfile = [[HCFRKiGeneratorIRProfile alloc] init];
  [newProfile setName:[path lastPathComponent]];
  [newProfile setDescription:[NSString stringWithFormat:HCFRLocalizedString(@"This profile was imported from file %s",
                                                                            @"This profile was imported from file %s"), filePath]];
  [newProfile setCode:[NSString stringWithUTF8String:fileReader.getNextCodeString()] forType:IRProfileNextChapterCode];
  [newProfile setCode:[NSString stringWithUTF8String:fileReader.getOkCodeString()] forType:IRProfileOkCode];
  [newProfile setCode:[NSString stringWithUTF8String:fileReader.getDownCodeString()] forType:IRProfileDownArrowCode];
  [newProfile setCode:[NSString stringWithUTF8String:fileReader.getRightCodeString()] forType:IRProfileRightArrowCode];
  
  [newProfile setDelay:fileReader.getMenuNavigationLatency() forType:IRProfileMenuNavigationDelay];
  [newProfile setDelay:fileReader.getMenuValidationLatency() forType:IRProfileMenuValidationDelay];
  [newProfile setDelay:fileReader.getChapterNavigationLatency() forType:IRProfileChapterNavigationDelay];
  
  [self addProfileAndSelectIt:newProfile];
  
  [newProfile release];
  
  return YES;
}
-(NSString*) exportProfile:(HCFRKiGeneratorIRProfile*)profile asIHCFileToDirectory:(NSString*)destinationPath
{
  NSString  *fileName = [[NSString alloc] initWithFormat:@"%@.ihc",[profile name]];
  NSString  *filePath = [[NSString alloc] initWithFormat:@"%@/%@", destinationPath, fileName];
  int       uniqueFileNumber = 0;
  
  // on vérifie que le fichier n'existe pas, sinon, on ajoute un numéro.
  while ([[NSFileManager defaultManager] fileExistsAtPath:filePath])
  {
    uniqueFileNumber ++;
    [filePath release];
    [fileName release];
    fileName = [[NSString alloc] initWithFormat:@"%@-%d.ihc",[profile name], uniqueFileNumber];
    filePath = [[NSString alloc] initWithFormat:@"%@/%@", destinationPath, fileName];
  }
  
  try
  {
    IHCFile fileWriter;
    
    fileWriter.setOkCodeString([[profile codeAsStringForType:IRProfileOkCode] cStringUsingEncoding:NSUTF8StringEncoding]);
    fileWriter.setNextCodeString([[profile codeAsStringForType:IRProfileNextChapterCode] cStringUsingEncoding:NSUTF8StringEncoding]);
    fileWriter.setDownCodeString([[profile codeAsStringForType:IRProfileDownArrowCode] cStringUsingEncoding:NSUTF8StringEncoding]);
    fileWriter.setRightCodeString([[profile codeAsStringForType:IRProfileRightArrowCode] cStringUsingEncoding:NSUTF8StringEncoding]);
    
    fileWriter.setMenuValidationLatency([profile delayForType:IRProfileMenuValidationDelay]);
    fileWriter.setMenuNavigationLatency([profile delayForType:IRProfileMenuNavigationDelay]);
    fileWriter.setChapterNavigationLatency([profile delayForType:IRProfileChapterNavigationDelay]);
    
    fileWriter.writeFile([filePath cStringUsingEncoding:NSUTF8StringEncoding]);
  }
  catch (...)
  {
    NSAlert  *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Export failed",
                                                                        @"Export failed")
                                      defaultButton:@"Ok" alternateButton:Nil otherButton:Nil
                          informativeTextWithFormat:HCFRLocalizedString(@"The export of the profile %@ failed.",
                                                                        @"The export of the profile %@ failed."), [profile name]];
    [alert  beginSheetModalForWindow:mainWindow
                       modalDelegate:nil
                      didEndSelector:nil
                         contextInfo:nil];
    [filePath release];
    [fileName release];
    return nil;
  }
  [filePath release];
  return [fileName autorelease];
}
-(NSString*) exportProfile:(HCFRKiGeneratorIRProfile*)profile asXMLFileToDirectory:(NSString*)destinationPath
{
  NSString  *fileName = [[NSString alloc] initWithFormat:@"IRProfile-%@.xml",[profile name]];
  NSString  *filePath = [[NSString alloc] initWithFormat:@"%@/%@", destinationPath, fileName];
  int       uniqueFileNumber = 0;
  
  // on vérifie que le fichier n'existe pas, sinon, on ajoute un numéro.
  while ([[NSFileManager defaultManager] fileExistsAtPath:filePath])
  {
    uniqueFileNumber ++;
    [filePath release];
    [fileName release];
    fileName = [[NSString alloc] initWithFormat:@"IRProfile-%@-%d.xml",[profile name], uniqueFileNumber];
    filePath = [[NSString alloc] initWithFormat:@"%@/%@", destinationPath, fileName];
  }

  if (![[HCFRKiGeneratorIRProfilesRepository sharedRepository] saveProfiles:[NSArray arrayWithObject:profile]
                                                                                         toPath:filePath])
  {
    [filePath release];
    [fileName release];
    return nil;
  }
  
  [filePath release];
  return [fileName autorelease];  
}
#pragma mark fonctions callback pour les open et save panels
- (void)importFileSheetDidEnd:(NSOpenPanel *)panel returnCode:(int)returnCode  contextInfo:(void*)contextInfo
{
  if (returnCode == NSOKButton)
    [self importFiles:[panel filenames]];
}

#pragma mark Fonction de gestion du drag&drop
- (BOOL)outlineView:(NSOutlineView *)outlineView writeItems:(NSArray*)items toPasteboard:(NSPasteboard*)pasteboard
{
  [pasteboard declareTypes:[NSArray arrayWithObjects:NSFilesPromisePboardType, nil] owner:self];

  if ([[[NSUserDefaults standardUserDefaults] stringForKey:kIRProfileManagerExportFormat] isEqualToString:@"ihc"])
    // promet un fichier ihc
    [pasteboard setPropertyList:[NSArray arrayWithObject:@"ihc"] forType:NSFilesPromisePboardType];
  else
    // promet un fichier xml
    [pasteboard setPropertyList:[NSArray arrayWithObject:@"xml"] forType:NSFilesPromisePboardType];
  
  return YES;
}
- (NSArray*)outlineView:(NSOutlineView*) outlineView namesOfPromisedFilesDroppedAtDestination:(NSURL*) dropDestination
         forDraggedItems:(NSArray*) items
{
  NSString                  *destinationPath = [dropDestination path];
  NSEnumerator              *itemsEnumerator = [items objectEnumerator];
  id                        currentobject;
  NSMutableArray            *resultArray = [[NSMutableArray alloc] init];
  
  while (currentobject = [itemsEnumerator nextObject])
  {
    HCFRKiGeneratorIRProfile  *currentProfile;

    if (![currentobject isKindOfClass:[HCFRKiGeneratorIRProfile class]])
      continue;

    currentProfile = (HCFRKiGeneratorIRProfile*)currentobject;
    
    NSString *filename;
    
    if ([[[NSUserDefaults standardUserDefaults] stringForKey:kIRProfileManagerExportFormat] isEqualToString:@"ihc"])
      filename = [self exportProfile:currentProfile asIHCFileToDirectory:destinationPath];
    else
      filename = [self exportProfile:currentProfile asXMLFileToDirectory:destinationPath];

    if (filename != nil)
      [resultArray addObject:filename];
  }
  
  return [resultArray autorelease];
}  

- (unsigned int)outlineView:(NSOutlineView*)theOutlineView validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(int)index
{
  NSPasteboard  *pasteboard = [info draggingPasteboard];
  NSArray       *filenames = [pasteboard propertyListForType:NSFilenamesPboardType];
  
  NSEnumerator              *itemsEnumerator = [filenames objectEnumerator];
  NSString                  *currentFilename;
  
  while (currentFilename = [itemsEnumerator nextObject])
  {
    if ([currentFilename hasSuffix:@".ihc"] || [currentFilename hasSuffix:@".IHC"] || [currentFilename hasSuffix:@".xml"])
    {
      [theOutlineView setDropItem:customItem dropChildIndex:[[[HCFRKiGeneratorIRProfilesRepository sharedRepository] customProfilesArray] count]];
      return NSDragOperationCopy;
    }
  }    

  return NSDragOperationNone;
}

- (BOOL)outlineView:(NSOutlineView*)outView acceptDrop:(id <NSDraggingInfo>)info item:(id)item childIndex:(int)index
{
  NSPasteboard  *pasteboard = [info draggingPasteboard];
  NSArray       *filenames = [pasteboard propertyListForType:NSFilenamesPboardType];
  
    
  return [self importFiles:filenames];
}
@end
