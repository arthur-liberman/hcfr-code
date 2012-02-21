//  ColorHCFR
//  HCFRKiGeneratorXMLParser.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/01/08.
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

#import "HCFRKiGeneratorIRProfilesRepository.h"

@interface HCFRKiGeneratorIRProfilesRepository (Private)
- (void)parser:(NSXMLParser *)parser didStartElement:(NSString *)elementName namespaceURI:(NSString *)namespaceURI qualifiedName:(NSString *)qName attributes:(NSDictionary *)attributeDict;
- (void)parser:(NSXMLParser *)parser foundCharacters:(NSString *)string;
- (void)parser:(NSXMLParser *)parser didEndElement:(NSString *)elementName namespaceURI:(NSString *)namespaceURI qualifiedName:(NSString *)qName;
@end

@implementation HCFRKiGeneratorIRProfilesRepository
static HCFRKiGeneratorIRProfilesRepository *sharedRepository = nil;
#pragma mark Fonctions de gestion du singleton
+ (HCFRKiGeneratorIRProfilesRepository*)sharedRepository
{
  @synchronized(self) {
    if (sharedRepository == nil) {
        [[[self alloc] init] autorelease]; // On n'alloue pas sharedDevice ici : ce sera fait par allocWithZone
    }
  }
  return sharedRepository;
}
+ (id)allocWithZone:(NSZone *)zone
{
  @synchronized(self) {
    if (sharedRepository == nil) {
      sharedRepository = [super allocWithZone:zone];
      return sharedRepository;  // On alloue à l'instance partagée
    }
  }
  return nil; // et si d'autre allocations surviennent, on retourne nil.
}
- (id)copyWithZone:(NSZone *)zone
{
  return self;
}
- (id)retain
{
  return self;
}
- (unsigned)retainCount
{
  return UINT_MAX;  // Cet objet ne peut pas être releasé
}
- (void)release
{
}
- (id)autorelease
{
  return self;
}

- (HCFRKiGeneratorIRProfilesRepository*) init {
  self = [super init];
  if (self != nil) {
    
    factoryProfilesArray = [[NSMutableArray alloc] init];
    customProfilesArray = [[NSMutableArray alloc] init];
    
    listenersArray = [[NSMutableArray alloc] init];
      
    currentProfile = nil;
    currentType = HCFRKiGeneratorXmlNoElement;
    
    // les profiles "d'usine"
    NSString *filePath = [[NSBundle bundleForClass:[self class]] pathForResource:@"irProfiles" ofType:@"xml"];
    if (filePath != nil)
    {
      currentlyLoadedProfilesArray = factoryProfilesArray;
      NSXMLParser *parser = [[NSXMLParser alloc] initWithContentsOfURL:[NSURL fileURLWithPath:filePath]];
      [parser setDelegate:self];
      [parser parse];
      [parser release];
      currentlyLoadedProfilesArray = nil;
    }
    
    // les profiles perso
    currentlyLoadedProfilesArray = customProfilesArray;

    NSString        *customProfilesFilePath = [[NSHomeDirectory() stringByAppendingString:@"/"]
                    stringByAppendingString:@"Library/Application Support/ColorHCFR/irProfiles.xml"];
    NSXMLParser *parser = [[NSXMLParser alloc] initWithContentsOfURL:[NSURL fileURLWithPath:customProfilesFilePath]];
    [parser setDelegate:self];
    [parser parse];
    [parser release];
    
    currentlyLoadedProfilesArray = nil;
  }
  return self;
}

- (void) dealloc {
  [factoryProfilesArray release];
  [customProfilesArray release];
  
  [super dealloc];
}
#pragma mark accesseurs
-(NSArray*) factoryProfilesArray{
  return factoryProfilesArray;
}

-(NSArray*) customProfilesArray{
  return customProfilesArray;
}

-(void) addCustomProfile:(HCFRKiGeneratorIRProfile*)newProfile
{
  [customProfilesArray addObject:newProfile];
}
-(void) removeCustomProfile:(HCFRKiGeneratorIRProfile*)profileToRemove;
{
  [customProfilesArray removeObject:profileToRemove];
}

#pragma mark Delegue du parser XML
-(void) parser:(NSXMLParser *)parser didStartElement:(NSString *)elementName
  namespaceURI:(NSString *)namespaceURI
 qualifiedName:(NSString *)qName
    attributes:(NSDictionary *)attributeDict
{
  if ([elementName isEqualToString:@"irProfiles"]) {
    // on vide le tableau
    [currentlyLoadedProfilesArray removeAllObjects];
  }
  else if ([elementName isEqualToString:@"irProfile"]) {
    currentProfile = [[HCFRKiGeneratorIRProfile init] alloc];
    [currentProfile setName:[attributeDict objectForKey:@"name"]];
  }
  else if ([elementName isEqualToString:@"description"]) {
    currentElementContent = [[NSMutableString alloc] init];
    currentType = HCFRKiGeneratorXmlElementDescription;
  }
  else if ([elementName isEqualToString:@"author"]) {
    currentElementContent = [[NSMutableString alloc] init];
    currentType = HCFRKiGeneratorXmlElementAuthor;
  }
  else if ([elementName isEqualToString:@"code"]) {
    currentElementContent = [[NSMutableString alloc] init];
    NSString *function = [attributeDict objectForKey:@"function"];
    if ([function isEqualToString:@"ok"])
      currentType = HCFRKiGeneratorXmlElementOkCode;
    else if ([function isEqualToString:@"nextChapter"])
      currentType = HCFRKiGeneratorXmlElementNextChapterArrowCode;
    else if ([function isEqualToString:@"rightArrow"])
      currentType = HCFRKiGeneratorXmlElementRightArrowCode;
    else if ([function isEqualToString:@"leftArrow"])
      currentType = HCFRKiGeneratorXmlElementLeftArrowCode;
    else if ([function isEqualToString:@"upArrow"])
      currentType = HCFRKiGeneratorXmlElementUpArrowCode;
    else if ([function isEqualToString:@"downArrow"])
      currentType = HCFRKiGeneratorXmlElementDownArrowCode;
    else
      currentType = HCFRKiGeneratorXmlNoElement;
  }
  else if ([elementName isEqualToString:@"delay"]) {
    currentElementContent = [[NSMutableString alloc] init];
    NSString *function = [attributeDict objectForKey:@"function"];
    if ([function isEqualToString:@"menuNavigation"])
      currentType = HCFRKiGeneratorXmlElementMenuNavigationDelay;
    else if ([function isEqualToString:@"menuValidation"])
      currentType = HCFRKiGeneratorXmlElementMenuValidationDelay;
    else if ([function isEqualToString:@"chapterNavigation"])
      currentType = HCFRKiGeneratorXmlElementChapterNavigationDelay;
    else
      currentType = HCFRKiGeneratorXmlNoElement;
  }
}
- (void)parser:(NSXMLParser *)parser foundCharacters:(NSString *)string
{
  if (currentElementContent != nil && string != nil)
  {
    [currentElementContent appendString:string];
  }
}

- (void)parser:(NSXMLParser *)parser didEndElement:(NSString *)elementName
  namespaceURI:(NSString *)namespaceURI
 qualifiedName:(NSString *)qName
{
  if ([elementName isEqualToString:@"irProfile"]) {
    [currentlyLoadedProfilesArray addObject:currentProfile];
  }
  else if ([elementName isEqualToString:@"description"]) {
    [currentProfile setDescription:currentElementContent];
    [currentElementContent release];
    currentElementContent = nil;
  }
  else if ([elementName isEqualToString:@"author"]) {
    [currentProfile setAuthor:currentElementContent];
    [currentElementContent release];
    currentElementContent = nil;
  }
  else if ([elementName isEqualToString:@"code"]) {
    if (currentType == HCFRKiGeneratorXmlElementOkCode)
      [currentProfile setCode:currentElementContent forType:IRProfileOkCode];
    else if (currentType == HCFRKiGeneratorXmlElementNextChapterArrowCode)
      [currentProfile setCode:currentElementContent forType:IRProfileNextChapterCode];
    else if (currentType == HCFRKiGeneratorXmlElementRightArrowCode)
      [currentProfile setCode:currentElementContent forType:IRProfileRightArrowCode];
    else if (currentType == HCFRKiGeneratorXmlElementDownArrowCode)
      [currentProfile setCode:currentElementContent forType:IRProfileDownArrowCode];
    
    [currentElementContent release];
    currentElementContent = nil;
  }
  else if ([elementName isEqualToString:@"delay"]) {
    if (currentType == HCFRKiGeneratorXmlElementMenuNavigationDelay)
      [currentProfile setDelay:[currentElementContent intValue] forType:IRProfileMenuNavigationDelay];
    else if (currentType == HCFRKiGeneratorXmlElementMenuValidationDelay)
      [currentProfile setDelay:[currentElementContent intValue] forType:IRProfileMenuValidationDelay];
    else if (currentType == HCFRKiGeneratorXmlElementChapterNavigationDelay)
      [currentProfile setDelay:[currentElementContent intValue] forType:IRProfileChapterNavigationDelay];
    
    [currentElementContent release];
    currentElementContent = nil;
  }
}

#pragma mark gestion des listeners
-(void) addListener:(NSObject<HCFRIrProfilesRepositoryListener>*)newListener
{
  [listenersArray addObject:newListener];
}
-(void) removeListener:(NSObject<HCFRIrProfilesRepositoryListener>*)oldListener
{
  [listenersArray removeObject:oldListener];
}
-(void) fireProfilesListChanged
{
  NSEnumerator  *enumerator = [listenersArray objectEnumerator];
  NSObject<HCFRIrProfilesRepositoryListener> *currentListener;
  
  while (currentListener=(NSObject<HCFRIrProfilesRepositoryListener>*)[enumerator nextObject])
  {
    // et pour chaque listener, on appel entryAdded:forDataType
    [currentListener irProfilesListChanged];
  }
}

#pragma mark sauvegarde
-(void) saveCustomProfiles
{
  // on envoi ça dans un fichier
  NSString        *directoryPath = [[NSHomeDirectory() stringByAppendingString:@"/"]
                      stringByAppendingString:@"Library/Application Support/ColorHCFR/"];
  NSString        *customProfilesFilePath = [directoryPath stringByAppendingString:@"irProfiles.xml"];
  
  // on vérifie que le dossier existe, on le crée au besoin
  if (![[NSFileManager defaultManager] fileExistsAtPath:directoryPath])
    [[NSFileManager defaultManager] createDirectoryAtPath:directoryPath attributes:nil];

  [self saveProfiles:customProfilesArray toPath:customProfilesFilePath];
}
-(BOOL) saveProfiles:(NSArray*)profilesArray toPath:(NSString*)destinationPath
{
  NSMutableString *customProfilesString = [[NSMutableString alloc] init];
  BOOL            atleastOneProfileExported = NO;
  
  // en-têtes
  [customProfilesString appendString:@"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE irProfiles PUBLIC \"-//homecinema-fr//IRProfiles XML//EN\" [\n<!ELEMENT irProfiles (profile+)>\n"];
  [customProfilesString appendString:@"<!ATTLIST irProfiles version CDATA #REQUIRED>\n<!ELEMENT irProfile (description, author, code+)>\n"];
  [customProfilesString appendString:@"<!ATTLIST irProfile name CDATA #REQUIRED>\n<!ELEMENT description (#PCDATA)>\n<!ELEMENT author (#PCDATA)>\n"];
  [customProfilesString appendString:@"<!ELEMENT code (#PCDATA)>\n<!ATTLIST code function (rightArrow | leftArrow | downArrow | upArrow | nextChapter | ok) #REQUIRED>\n"];
  [customProfilesString appendString:@"<!ELEMENT delay (#PCDATA)>\n<!ATTLIST code function (menuNavigation | menuValidation | chapterNavigation) #REQUIRED>\n]>\n"];
  [customProfilesString appendString:@"<irProfiles version=\"1\">\n"];
  
  // les profiles
  NSEnumerator  *profilesEnumerator = [profilesArray objectEnumerator];
  id            currentObject;
  while (currentObject = [profilesEnumerator nextObject])
  {
    if (![currentObject isKindOfClass:[HCFRKiGeneratorIRProfile class]])
      continue;
    
    HCFRKiGeneratorIRProfile  *exportedProfile = (HCFRKiGeneratorIRProfile*)currentObject;
    
    [customProfilesString appendFormat:@"<irProfile name=\"%@\">\n", [exportedProfile name]];
    [customProfilesString appendFormat:@"<description>%@</description>\n", [exportedProfile description]];
    [customProfilesString appendFormat:@"<author>%@</author>\n", [exportedProfile author]];
    [customProfilesString appendFormat:@"<code function='ok'>%@</code>\n", [exportedProfile codeAsStringForType:IRProfileOkCode]];
    [customProfilesString appendFormat:@"<code function='rightArrow'>%@</code>\n", [exportedProfile codeAsStringForType:IRProfileRightArrowCode]];
    [customProfilesString appendFormat:@"<code function='downArrow'>%@</code>\n", [exportedProfile codeAsStringForType:IRProfileDownArrowCode]];
    [customProfilesString appendFormat:@"<code function='nextChapter'>%@</code>\n", [exportedProfile codeAsStringForType:IRProfileNextChapterCode]];
    [customProfilesString appendFormat:@"<delay function='menuNavigation'>%d</code>\n", [exportedProfile delayForType:IRProfileMenuNavigationDelay]];
    [customProfilesString appendFormat:@"<delay function='menuValidation'>%d</code>\n", [exportedProfile delayForType:IRProfileMenuValidationDelay]];
    [customProfilesString appendFormat:@"<delay function='chapterNavigation'>%d</code>\n", [exportedProfile delayForType:IRProfileChapterNavigationDelay]];
    [customProfilesString appendString:@"</irProfile>\n"];
    atleastOneProfileExported = YES;
  }

  // on ferme la balise racine
  [customProfilesString appendString:@"</irProfiles>"];
  
  // aucun profile exporté, on n'écrit pas le fichier.
  if (!atleastOneProfileExported)
  {
    [customProfilesString release];
    return NO;
  }
  
  NSError *result;
  if (![customProfilesString writeToFile:destinationPath atomically:YES encoding:NSUTF8StringEncoding error:&result])
  {
    NSLog (@"saveCustomProfiles : failure : %@", [result description]);
    [customProfilesString release];
    return NO;
  }
  [customProfilesString release];
  return YES;
}

#pragma mark Import
-(BOOL) importXMLFile:(NSString*)filePath
{
  BOOL            result = YES;
  NSMutableArray  *importArray = [[NSMutableArray alloc] init];
  NSXMLParser     *parser;

  currentlyLoadedProfilesArray = importArray;
  
  @try
  {
    parser = [[NSXMLParser alloc] initWithContentsOfURL:[NSURL fileURLWithPath:filePath]];
    [parser setDelegate:self];
    [parser parse];
  }
  @catch (NSException *e)
  {
    NSLog (@"import IR profile from XML File failed : %@", [e reason]);
    result = NO;
  }
  @finally
  {
    [parser release];
  }
  
  currentlyLoadedProfilesArray = nil;

  if (result)
  {
    [customProfilesArray addObjectsFromArray:importArray];
    [self fireProfilesListChanged];
  }
  
  [importArray release];
  
  return result;
}
@end
