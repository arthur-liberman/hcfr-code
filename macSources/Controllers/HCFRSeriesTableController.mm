//  ColorHCFR
//  HCFRSeriesTableController.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 15/05/08.
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

#import "HCFRSeriesTableController.h"
#import <HCFRPlugins/HCFRDataSerie+io.h>
#import <HCFRPlugins/HCFRUtilities.h>

@interface HCFRSeriesTableController (Private)
-(void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context;
@end

@implementation HCFRSeriesTableController

- (HCFRSeriesTableController*)initWithDataStore:(HCFRDataStore*)newDataStore
{
  self = [super init];
  if (self != nil)
  {
    [self setDataStore:newDataStore];
  }
  return self;
}
- (void) awakeFromNib
{
  [seriesTableView registerForDraggedTypes:[NSArray arrayWithObject:NSFilenamesPboardType]];  
  [seriesTableView setDraggingSourceOperationMask:NSDragOperationCopy forLocal:NO];
}

- (void) dealloc {
  if (dataStore != nil)
  {
    [dataStore removeObserver:self forKeyPath:@"dataSeries"];
    [dataStore release];
  }

  [seriesTableView unregisterDraggedTypes];
  [super dealloc];
}

-(void) setDataStore:(HCFRDataStore*)newDataStore
{
  if (dataStore != nil)
  {
    [dataStore removeObserver:self forKeyPath:@"dataSeries"];
    [dataStore release];
  }
  dataStore = [newDataStore retain];
  [dataStore addObserver:self forKeyPath:@"dataSeries"
                 options:0
                 context:nil];
  [seriesTableView reloadData];
  // le changement de séléction provoque l'avertissement des listeners sur "currentSerie"
  
  int selectedIndex = [dataStore currentSerieIndex];
  if (selectedIndex == NSNotFound)
    [seriesTableView selectRowIndexes:[NSIndexSet indexSet] byExtendingSelection:NO];
  else
    [seriesTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:selectedIndex] byExtendingSelection:NO];
}

#pragma mark Fonctions pour les bindings
-(BOOL) currentSerieIsEditable
{
  return ([seriesTableView selectedRow] != -1);
}
-(void) setCurrentNotes:(NSAttributedString*) newNotes
{
  if ([dataStore currentSerie] != nil)
    [[dataStore currentSerie] setNotes:newNotes];
}
-(NSAttributedString*) currentNotes
{
  if ([dataStore currentSerie] != nil)
    return [[dataStore currentSerie] notes];

  return nil;
}

#pragma mark KVO listener
- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
  if (object == dataStore)
  {
    if ([keyPath isEqualToString:@"dataSeries"])
    {
      // on recharge la liste
      [seriesTableView reloadData];
    }
  }
}

#pragma mark Actions IB
-(IBAction) addNewSerie:(id)sender
{
  // on force la validation des champs text au cas ou un d'eux serait en cours d'edition
  // necessaire car selectionShouldChange n'est pas appelé dans ce cas ...
  [[seriesTableView window] makeFirstResponder:[seriesTableView window]];
  
  int newIndex = [dataStore addNewSerie:YES];
//  [seriesTableView reloadData]; remplacé par du KVO
  [seriesTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:newIndex] byExtendingSelection:NO];
  [seriesTableView editColumn:1 row:newIndex withEvent:nil select:YES];
}
-(IBAction) deleteSerie:(id)sender
{
  int newSelection = [dataStore removeSerieAtIndex:[seriesTableView selectedRow]];
//  [seriesTableView reloadData]; remplacé par du KVO
  int selectedIndex = [dataStore currentSerieIndex];
  if (selectedIndex == NSNotFound)
    [seriesTableView selectRowIndexes:[NSIndexSet indexSet] byExtendingSelection:NO];
  else
    [seriesTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:newSelection] byExtendingSelection:NO];
}
-(IBAction) toogleReferenceForCurrent:(id)sender
{
  // si on est sur la série de référence, on supprime la référence
  if ([dataStore currentSerie] == [dataStore referenceSerie])
    [dataStore setReferenceSerieIndex:-1];
  else
    [dataStore setReferenceSerieIndex:[seriesTableView selectedRow]];
  
  [seriesTableView reloadData];
}

#pragma mark fonctions utilitaires
-(HCFRDataSerie*) addNewSerieWithName:(NSString*)name
{
  int newIndex = [dataStore addNewSerie:NO];
  HCFRDataSerie *newSerie = [dataStore serieAtIndex:newIndex];
  [newSerie setName:name];
//  [seriesTableView reloadData];  remplacé par du KVO
  return newSerie;
}

#pragma mark Tableview data source
- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
  if (dataStore == nil)
    return 0;
  
  return [dataStore countSeries];
}
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
  HCFRDataSerie *serie = [dataStore serieAtIndex:rowIndex];

  if ([@"Icon" isEqualToString:[aTableColumn identifier]])
  {
    if (serie == [dataStore referenceSerie])
      return [NSImage imageNamed:@"reference.png"];
    else
      return nil;
  }
  else if ([@"Name" isEqualToString:[aTableColumn identifier]])
  {
    if (serie == nil)
      return @"Error";
    
    return [serie name];
  }
  
  return @"Error";
}
- (void)tableView:(NSTableView *)aTableView setObjectValue:(id)anObject forTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
  HCFRDataSerie *serie = [dataStore serieAtIndex:rowIndex];

  if ([@"Name" isEqualToString:[aTableColumn identifier]])
  {
    [serie setName:anObject];
  }
}
- (BOOL)tableView:(NSTableView *)tableView writeRowsWithIndexes:(NSIndexSet *)rowIndexes toPasteboard:(NSPasteboard*)pasteboard
{
  [pasteboard declareTypes:[NSArray arrayWithObjects:NSFilesPromisePboardType, nil] owner:self];
  // promet un fichier CSV
  [pasteboard setPropertyList:[NSArray arrayWithObject:@"csv"] forType:NSFilesPromisePboardType];
  
  return YES;
}
- (NSArray *)tableView:(NSTableView *)aTableView namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
  forDraggedRowsWithIndexes:(NSIndexSet *)rowIndexes
{
  unsigned int    bufSize = [rowIndexes count];
  unsigned int    *buf = new unsigned int[bufSize];
  NSMutableArray  *resultArray = [[NSMutableArray alloc] init];
  
  bufSize = [rowIndexes getIndexes:buf maxCount:bufSize inIndexRange:nil];
  
  for(int i = 0; i < bufSize; i++)
  {
    unsigned int  currentIndex = buf[i];
    HCFRDataSerie *exportedSerie = [dataStore serieAtIndex:currentIndex];
    NSString      *filename = [[NSString alloc] initWithFormat:@"%@.csv", [exportedSerie name]];

    @try
    {
      NSString  *filepath = [[NSString alloc] initWithFormat:@"%@/%@",[dropDestination path], filename];
      int       uniqueFileNumber = 0;
      
      // on vérifie que le fichier n'existe pas, sinon, on ajoute un numéro.
      while ([[NSFileManager defaultManager] fileExistsAtPath:filepath])
      {
        uniqueFileNumber ++;
        [filename release];
        filename = [[NSString alloc] initWithFormat:@"%@-%d.csv", [exportedSerie name], uniqueFileNumber];
        [filepath release];
        filepath = [[NSString alloc] initWithFormat:@"%@/%@",[dropDestination path], filename];
      }
      
      [exportedSerie saveDataToCSVFile:filepath];
      [filepath release];

      // on stock le nom du fichier qu'on a créé
      [resultArray addObject:filename];
      [filename release];
    }
    @catch (NSException *e)
    {
      NSLog (@"Exception while trying to save serie %@ to cvs : %@", [exportedSerie name], [e reason]);
      // on met un message d'alerte, vu que pour le moment, on ne peut avoir que un fichier exporté :
      NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Export failure",
                                                                         @"Export failure")
                                       defaultButton:@"Ok" alternateButton:nil otherButton:nil
                           informativeTextWithFormat:HCFRLocalizedString(@"Export of the serie %@ failed :\n%@",
                                                                         @"Export of the serie %@ failed :\n%@"), [exportedSerie name],[e reason]];
      [alert beginSheetModalForWindow:[seriesTableView window] modalDelegate:nil didEndSelector:nil contextInfo:nil];
      continue;
    }
  }
  
  delete buf;
  return [resultArray autorelease];
}
- (void)pasteboard:(NSPasteboard *)pboard provideDataForType:(NSString *)type {
  NSLog (@"provideDataForType : %@", type);
}

#pragma mark drag drop table view
-(NSDragOperation)tableView:(NSTableView*)tableView validateDrop:(id <NSDraggingInfo>)info proposedRow:(int)row proposedDropOperation:(NSTableViewDropOperation)op
{
  NSPasteboard  *pboard = [info draggingPasteboard];
  NSArray       *filenames = [pboard propertyListForType:NSFilenamesPboardType];
  int           filesCount = [filenames count];
  int           loopCounter;
  
  // si on a plus d'un fichier, on en peut pas dropper sur une ligne
  // Quoi qu'il en soit, les fichiers sont importés en fin de liste.
  if (filesCount > 1)
    [seriesTableView setDropRow:[dataStore countSeries] dropOperation:NSTableViewDropAbove];
  else if (op == NSTableViewDropAbove)
    [seriesTableView setDropRow:[dataStore countSeries] dropOperation:op];

  // on parcours les fichiers reçus.
  // Si on en a un qui est importable, on accepte le drop.
  for (loopCounter = 0; loopCounter < filesCount; loopCounter++)
  {
    NSString      *filename = [filenames objectAtIndex:loopCounter];
    
    if ([[HCFRDataSerie importableExtensions] containsObject:[filename pathExtension]])
      return NSDragOperationEvery;
  }
  
  // on ne peut importer aucun fichier, on refuse le drop.
  return NSDragOperationNone;    
}
- (BOOL)tableView:(NSTableView*)tv acceptDrop:(id <NSDraggingInfo>)info row:(int)row dropOperation:(NSTableViewDropOperation)op
{
  NSPasteboard  *pboard = [info draggingPasteboard];
  NSArray       *filenames = [pboard propertyListForType:NSFilenamesPboardType];
  int           filesCount = [filenames count];
  int           loopCounter;

  for (loopCounter = 0; loopCounter < filesCount; loopCounter++)
  {
    NSString      *filename = [filenames objectAtIndex:loopCounter];
    HCFRDataSerie *receivingSerie;
    
    if (op == NSTableViewDropOn)
      // on récupère la série sur laquelle on drop, et on import le fichier dedant
      receivingSerie = [[dataStore serieAtIndex:row] retain];
    else
    {
      // on crée une nouvelle série
      receivingSerie = [[HCFRDataSerie alloc] init];
      [receivingSerie setName:[filename lastPathComponent]];
    }
    
    // on import
    @try
    {
      [receivingSerie importFile:filename];
      if (op == NSTableViewDropAbove)
        // c'est une nouvelle série, on l'insère
        [dataStore addSerie:receivingSerie andSelectIt:NO];
    }
    @catch (NSException *e)
    {
      // on prendra des exceptions si dans les fichiers sélectionnés, certain ne sont pas du bon type.
      // c'est pas bien grâve.
      // on ne fait rien
    }
    @finally
    {
      [receivingSerie release];
    }
  }
  
  return YES;
}

#pragma mark Tableview delegate
- (BOOL)selectionShouldChangeInTableView:(NSTableView *)aTableView
{
  // on met la fenetre en first responder pour terminer valider le champ text édité (si il y en a un)
  [[seriesTableView window] makeFirstResponder:[seriesTableView window]];
  return YES;
}
- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
  NSLog (@"selection did change");
  [self willChangeValueForKey:@"currentSerieIsEditable"];
  [self willChangeValueForKey:@"currentNotes"];
  [dataStore setCurrentSerieIndex:[seriesTableView selectedRow]];
  [self didChangeValueForKey:@"currentNotes"];
  [self didChangeValueForKey:@"currentSerieIsEditable"];
}
- (BOOL)tableView:(NSTableView *)aTableView shouldEditTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
  // seul le nom est éditable.
  return [@"Name" isEqualToString:[aTableColumn identifier]];
}
@end
