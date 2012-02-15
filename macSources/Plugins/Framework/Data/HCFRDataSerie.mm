//  ColorHCFR
//  HCFRDataSerie.mm
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 13/05/08.
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

#import "HCFRConstants.h"
#import <HCFRPlugins/HCFRDataSerie.h>
#import <HCFRPlugins/HCFRDataStoreEntry.h>
#import <HCFRPlugins/HCFRUtilities.h>
#import <libHCFR/Color.h>

@interface HCFRDataSerie (Private)
#pragma mark fonctions utilitaires
/*!
@function 
 @abstract   FONCTION 0 USAGE INTERNE UNIQUEMENT
 @discussion Cette fonction, a usage interne (mais objective-c ne sais pas faire des fonctions privées ...)
 permet d'obtenir l'exclusivité sur le tableau de valeurs pour un type de données.
 @param      dataType Le type de donnée pour lequel on veut l'exclusivité
 */
-(void) lockForType:(HCFRDataType)dataType;
  /*!
  @function 
   @abstract   FONCTION 0 USAGE INTERNE UNIQUEMENT
   @discussion Cette fonction, a usage interne (mais objective-c ne sais pas faire des fonctions privées ...)
   permet de relacher une exclusibité sur le tableau de valeurs pour un type de données.
   @param      dataType Le type de donnée pour lequel on veut l'exclusivité
   */
-(void) unlockForType:(HCFRDataType)dataType;

  /*!
  @function 
   @abstract   Notifie un ajout de données à tous les listenenrs
   @discussion Signale à tous les listeners enregistrés qu'une valeur à été ajoutée pour le type dataType.
   @param      newEntry  L'entrée ajoutée
   @param      dataType  Le type de la donnée ajoutée
   */
-(void) fireEntryAdded:(HCFRDataStoreEntry*)newEntry forType:(HCFRDataType)dataType;
  /*!
  @function 
   @abstract   Notifie un changement de série aux listeners
   @discussion Signale à tous les listeners enregistrés qu'une nouvelle série commence. Les données sont toutes
   invalidées.
   @param      newEntry  L'entrée ajoutée
   @param      dataType  Le type de la donnée ajoutée
   */
-(void) fireSerieChangedForType:(HCFRDataType)dataType;
@end

const unsigned int kMaxContinuousMeasures = 50;

@implementation HCFRDataSerie
#pragma mark Constructeur et destructeur
-(HCFRDataSerie*) init
{
  name = [[NSString alloc] initWithString:HCFRLocalizedString(@"New serie", @"New serie")];
  notes = [[NSAttributedString alloc] init];
  
  dirty = NO;
  
  // Initialisation des types des donnés pour le dataStore et les listeners
  // on crée les dictionnaires de stockage
  dataStore = [[NSMutableDictionary alloc] initWithCapacity:8];
  [dataStore setObject:[NSMutableArray arrayWithCapacity:10] forKey:[NSNumber numberWithInt:kLuminanceDataType]];
  [dataStore setObject:[NSMutableArray arrayWithCapacity:10] forKey:[NSNumber numberWithInt:kFullScreenContrastDataType]];
  [dataStore setObject:[NSMutableArray arrayWithCapacity:10] forKey:[NSNumber numberWithInt:kANSIContrastDataType]];
  [dataStore setObject:[NSMutableArray arrayWithCapacity:10] forKey:[NSNumber numberWithInt:kRedSaturationDataType]];
  [dataStore setObject:[NSMutableArray arrayWithCapacity:10] forKey:[NSNumber numberWithInt:kGreenSaturationDataType]];
  [dataStore setObject:[NSMutableArray arrayWithCapacity:10] forKey:[NSNumber numberWithInt:kBlueSaturationDataType]];
  [dataStore setObject:[NSMutableArray arrayWithCapacity:6] forKey:[NSNumber numberWithInt:kComponentsDataType]];
  [dataStore setObject:[NSMutableArray arrayWithCapacity:25] forKey:[NSNumber numberWithInt:kContinuousDataType]];
  
  // on crée les dictionnaires de listener
  listeners = [[NSMutableDictionary alloc] initWithCapacity:8];
  [listeners setObject:[NSMutableArray arrayWithCapacity:3] forKey:[NSNumber numberWithInt:kLuminanceDataType]];
  [listeners setObject:[NSMutableArray arrayWithCapacity:3] forKey:[NSNumber numberWithInt:kFullScreenContrastDataType]];
  [listeners setObject:[NSMutableArray arrayWithCapacity:3] forKey:[NSNumber numberWithInt:kANSIContrastDataType]];
  [listeners setObject:[NSMutableArray arrayWithCapacity:3] forKey:[NSNumber numberWithInt:kRedSaturationDataType]];
  [listeners setObject:[NSMutableArray arrayWithCapacity:3] forKey:[NSNumber numberWithInt:kGreenSaturationDataType]];
  [listeners setObject:[NSMutableArray arrayWithCapacity:3] forKey:[NSNumber numberWithInt:kBlueSaturationDataType]];
  [listeners setObject:[NSMutableArray arrayWithCapacity:3] forKey:[NSNumber numberWithInt:kComponentsDataType]];
  [listeners setObject:[NSMutableArray arrayWithCapacity:3] forKey:[NSNumber numberWithInt:kContinuousDataType]];
  
  // et on crée les dictionnaires de lock pour les types qui ont des dictionnaires de stockage.
  NSLock *currentLock;
  locksArray = [[NSMutableDictionary alloc] initWithCapacity:6];
  currentLock = [[NSLock alloc] init];
  [locksArray setObject:currentLock forKey:[NSNumber numberWithInt:kLuminanceDataType]];
  [currentLock release];
  currentLock = [[NSLock alloc] init];
  [locksArray setObject:currentLock forKey:[NSNumber numberWithInt:kFullScreenContrastDataType]];
  [currentLock release];
  currentLock = [[NSLock alloc] init];
  [locksArray setObject:currentLock forKey:[NSNumber numberWithInt:kANSIContrastDataType]];
  [currentLock release];
  currentLock = [[NSLock alloc] init];
  [locksArray setObject:currentLock forKey:[NSNumber numberWithInt:kRedSaturationDataType]];
  [currentLock release];
  currentLock = [[NSLock alloc] init];
  [locksArray setObject:currentLock forKey:[NSNumber numberWithInt:kGreenSaturationDataType]];
  [currentLock release];
  currentLock = [[NSLock alloc] init];
  [locksArray setObject:currentLock forKey:[NSNumber numberWithInt:kBlueSaturationDataType]];
  [currentLock release];
  currentLock = [[NSLock alloc] init];
  [locksArray setObject:currentLock forKey:[NSNumber numberWithInt:kComponentsDataType]];
  [currentLock release];
  currentLock = [[NSLock alloc] init];
  [locksArray setObject:currentLock forKey:[NSNumber numberWithInt:kContinuousDataType]];
  [currentLock release];
  
  return self;
}

-(void) dealloc
{
  [dataStore release];
  [listeners release];
  [locksArray release];
//  [colorReference release];
  [name release];
  [notes release];
  
  [super dealloc];
}

-(NSString*) name
{
  return name;
}
-(void) setName:(NSString*)newName
{
  NSAssert (newName != nil, @"HCFRDataSerie : invalide nil name in setName");
  
  [name release];
  name = [newName retain];
  
  [self setDirty:YES];
}

#pragma mark Gestion des mesures
-(void) addMeasure:(HCFRColor*)value withReferenceNumber:(NSNumber*)referenceNumber forType:(HCFRDataType)dataType
{
  [self lockForType:dataType];
  
  NSMutableArray    *currentArray = (NSMutableArray*)[dataStore objectForKey:[NSNumber numberWithInt:dataType]];
  
  // On construit la nouvelle valeur, on l'insère dans le tableau associé au type, puis on la détruit.
  // Le tableau l'ayant retenu (retain), elle ne sera détruite que quand le tableau la liberera
  HCFRDataStoreEntry  *newEntry = [[HCFRDataStoreEntry alloc] initWithMeasure:value
                                                              referenceNumber:referenceNumber];
  
  // On ajoute l'objet en le triant.
  // Normalement, on n'aura pas de très grand tableaux.
  // Alors pour le moment, on se permet d'avoir un algo de tri en O(n) (ce qui est très médiocre !)
  // TODO : ce serait bien d'amméliorer ça !
  int objectsCount = [currentArray count];
  int insertIndex = 0;
  NSComparisonResult comparisonResult;
  for (insertIndex = 0; insertIndex<objectsCount; insertIndex++)
  {
    comparisonResult = [(HCFRDataStoreEntry*)[currentArray objectAtIndex:insertIndex] compare:newEntry];
    if (comparisonResult != NSOrderedAscending)
      break;
  }
  // si la valeur existe déjà, on la remplace
  if ((objectsCount > 0) && (comparisonResult == NSOrderedSame))
    [currentArray replaceObjectAtIndex:insertIndex withObject:newEntry];
  else // sinon on l'ajoute
    [currentArray insertObject:newEntry atIndex:insertIndex];
  
  
  // on libère le verrou du tablea AVANT DE PREVENIR LES LISTENERS parce que :
  // 1 - on n'a plus besoin du verrou, on a fini nos modifs
  // 2 - il y a de fortes chances pour que le listener demande le tableau, ce qui pourrait nous mettre
  // dans une situation de deadlock ... a éviter.
  [self unlockForType:dataType];
  
  // enfin, on averti tous les listeners pour le type actuel
  [self fireEntryAdded:newEntry forType:dataType];
  
  [newEntry release];
  
  [self setDirty:YES];
}
-(void) appendContinuousMeasure:(HCFRColor*)value
{
  [self lockForType:kContinuousDataType];
  
  NSMutableArray    *continuousMeasuresArray = (NSMutableArray*)[dataStore objectForKey:[NSNumber numberWithInt:kContinuousDataType]];

  HCFRDataStoreEntry  *lastEntry = [continuousMeasuresArray lastObject];
  int lastEntryReference = [[lastEntry referenceNumber] intValue];

  HCFRDataStoreEntry  *newEntry = [[HCFRDataStoreEntry alloc] initWithMeasure:value
                                                              referenceNumber:[NSNumber numberWithInt:lastEntryReference+1]];
  
  [continuousMeasuresArray addObject:newEntry];
  
  // si on a atteind la quantité limite de mesures, on supprime la première
  if ([continuousMeasuresArray count] > kMaxContinuousMeasures)
    [continuousMeasuresArray removeObjectAtIndex:0];
  
  // on libère le vérrou du tablea AVANT DE PREVENIR LES LISTENERS parce que :
  // 1 - on n'a plus besoin du verrou, on a fini nos modifs
  // 2 - il y a de fortes chances pour que le listener demande le tableau, ce qui pourrait nous mettre
  // dans une situation de deadlock ... a éviter.
  [self unlockForType:kContinuousDataType];

  // enfin, on averti tous les listeners pour le type actuel
  [self fireEntryAdded:newEntry forType:kContinuousDataType];

  [newEntry release];

  [self setDirty:YES];
}

-(void) clearForDataType:(HCFRDataType)dataType
{
  [self lockForType:dataType];  
  NSMutableArray    *currentArray = (NSMutableArray*)[dataStore objectForKey:[NSNumber numberWithInt:dataType]];
  
  // Si le type de donnée n'existe pas, on lève une exception
  NSAssert1 (currentArray!=nil, @"HCFRDataStore->beginNewSerieForDataType:forType : dataType %i non disponible", dataType);
  
  // on vide le tableau
  [currentArray removeAllObjects];
  
  // on libère le vérrou du tablea AVANT DE PREVENIR LES LISTENERS car :
  // 1 - on n'a plus besoin du verrou, on a fini nos modifs
  // 2 - il y a de fortes chances pour que le listener demande le tableau, ce qui pourrait nous mettre
  // dans une situation de deadlock .. a éviter.
  [self unlockForType:dataType];
  
  // enfin, on averti tous les listeners pour le type actuel
  [self fireSerieChangedForType:dataType];
  
  [self setDirty:YES];
}

-(void) clearStore
{
  NSEnumerator  *enumerator = [dataStore keyEnumerator];
  NSNumber      *currentKey;
  
  while (currentKey = [enumerator nextObject])
  {
    // On ne verrouille pas le tableau : beginNewSerie s'en occupera.
    [self clearForDataType:(HCFRDataType)[currentKey intValue]];
  }
  
  [self setDirty:YES];
}

#pragma mark accès aux mesures
-(int) count
{
  int result = 0;
  NSEnumerator  *dataTypesEnumerator = [dataStore keyEnumerator];
  NSNumber      *currentDataType;
  
  while (currentDataType = [dataTypesEnumerator nextObject])
  {
    NSArray       *arrayForType = [dataStore objectForKey:currentDataType];
    
    result += [arrayForType count];
  }
  
  return result;
}
-(int) countForType:(HCFRDataType)dataType
{
  NSMutableArray    *currentArray = (NSMutableArray*)[dataStore objectForKey:[NSNumber numberWithInt:dataType]];
  NSAssert1 (currentArray!=nil, @"HCFRDataStore->countForType:forType : dataType %i non disponible", dataType);
  
  int result = [currentArray count];
  
  return result;
}
-(HCFRDataStoreEntry*) entryAtIndex:(int)index forType:(HCFRDataType)dataType
{
  [self lockForType:dataType];
  
  NSMutableArray    *currentArray = (NSMutableArray*)[dataStore objectForKey:[NSNumber numberWithInt:dataType]];
  NSAssert1 (currentArray!=nil, @"HCFRDataStore->entryAtIndex:forType : dataType %i non disponible", dataType);
  
  // assertion supprimée car la protection existe déja au niveau du NSArray
  //  NSAssert2 ([currentArray count] > index, @"HCFRDataStore->entryAtIndex:forType : ligne %i pour dataType %i non disponible", index, dataType);
  
  if (([currentArray count]-1) < index)
  {
    NSLog (@"DataSerie : no value found for type %d and index %d", dataType, index);
    [self unlockForType:dataType];
    return Nil;
  }
  
  HCFRDataStoreEntry  *entry = [currentArray objectAtIndex:index];
  
  [self unlockForType:dataType];
  
  return entry;
}
-(HCFRDataStoreEntry*) lastEntryForType:(HCFRDataType)dataType
{
  [self lockForType:dataType];
  
  NSMutableArray    *currentArray = (NSMutableArray*)[dataStore objectForKey:[NSNumber numberWithInt:dataType]];
  NSAssert1 (currentArray!=nil, @"HCFRDataStore->countForType:forType : dataType %i non disponible", dataType);
  
  HCFRDataStoreEntry  *entry = [currentArray lastObject];
  
  [self unlockForType:dataType];
  
  return entry;
}
-(HCFRDataStoreEntry*) entryWithReferenceValue:(int)reference forType:(HCFRDataType)dataType
{
  HCFRDataStoreEntry  *result = nil;
  
  [self lockForType:dataType];
  
  NSMutableArray    *currentArray = (NSMutableArray*)[dataStore objectForKey:[NSNumber numberWithInt:dataType]];
  NSAssert1 (currentArray!=nil, @"HCFRDataStore->countForType:forType : dataType %i non disponible", dataType);
  
  NSEnumerator      *entriesEnumerator = [currentArray objectEnumerator];
  
  HCFRDataStoreEntry  *currentEntry;
  while (currentEntry = [entriesEnumerator nextObject])
  {
    if ([[currentEntry referenceNumber] intValue] == reference)
    {
      result = currentEntry;
      break;
    }
  }
  
  [self unlockForType:dataType];
  return result;
}

#pragma mark Gestion des données aditionnelles
-(void) setNotes:(NSAttributedString*) newNotes
{
  if (notes != nil)
    [notes release];
  notes = [newNotes retain];
  
  [self setDirty:YES];
}
-(NSAttributedString*) notes
{
  return notes;
}
-(void) setDirty:(BOOL)newDirtyStatus
{
  [self willChangeValueForKey:@"dirty"];
  dirty = newDirtyStatus;
  [self didChangeValueForKey:@"dirty"];
}
-(BOOL) dirty
{
  return dirty;
}

#pragma mark fonctions utilitaires
-(void) lockForType:(HCFRDataType)dataType
{
  // on prend le vérrou pour le type de donnée spécifié
  NSLock *currentLock = [locksArray objectForKey:[NSNumber numberWithInt:dataType]];
  
  // Si on n'a pas de lock, c'est que le type de donnée n'existe pas.
  NSAssert1 (currentLock!=nil, @"HCFRDataStore->lockForType : lock non disponible pour dataType %i", dataType);
  
  // on obtiend l'exclusivité sur ce tableau
  [currentLock lock];
}
-(void) unlockForType:(HCFRDataType)dataType
{
  // on prend le vérrou pour le type de donnée spécifié
  NSLock *currentLock = [locksArray objectForKey:[NSNumber numberWithInt:dataType]];
  
  // Si on n'a pas de lock, c'est que le type de donnée n'existe pas.
  NSAssert1 (currentLock!=nil, @"HCFRDataStore->lockForType : lock non disponible pour dataType %i", dataType);
  
  // on relache le verrou du tableau
  [currentLock unlock];
}

#pragma mark Gestion des listeners
-(void) addListener:(NSObject<HCFRDataSerieListener>*)newListener forType:(HCFRDataType)dataType
{
  NSMutableArray    *currentArray = (NSMutableArray*)[listeners objectForKey:[NSNumber numberWithInt:dataType]];
  
  // Si le type de donnée n'existe pas, on lève une exception
  NSAssert1 (currentArray!=nil, @"HCFRDataStore->addListener:forType : dataType %i non disponible", dataType);
  
  [currentArray addObject:newListener];
}
-(void) removeListener:(NSObject<HCFRDataSerieListener>*)oldListener forType:(HCFRDataType)dataType
{
  NSMutableArray    *currentArray = (NSMutableArray*)[listeners objectForKey:[NSNumber numberWithInt:dataType]];
  
  // Si le type de donnée n'existe pas, on lève une exception
  NSAssert1 (currentArray!=nil, @"HCFRDataStore->removeListener:forType : dataType %i non disponible", dataType);
  
  [currentArray removeObject:oldListener];
}
-(void) fireEntryAdded:(HCFRDataStoreEntry*)newEntry forType:(HCFRDataType)dataType
{
  NSMutableArray    *currentArray = (NSMutableArray*)[listeners objectForKey:[NSNumber numberWithInt:dataType]];
  
  // Si le type de donnée n'existe pas, on lêve une exception
  NSAssert1 (currentArray!=nil, @"HCFRDataStore->fireEntryAdded:forType : dataType %i non disponible", dataType);
  
  // on parcours les entrées du tableau
  NSEnumerator  *enumerator = [currentArray objectEnumerator];
  NSObject<HCFRDataSerieListener> *currentListener;
  
  while (currentListener=(NSObject<HCFRDataSerieListener>*)[enumerator nextObject])
  {
    // et pour chaque listener, on appel entryAdded:forDataType
    [currentListener entryAdded:newEntry toSerie:self forDataType:dataType];
  }
}
-(void) fireSerieChangedForType:(HCFRDataType)dataType
{
  NSMutableArray    *currentArray = (NSMutableArray*)[listeners objectForKey:[NSNumber numberWithInt:dataType]];
  
  // Si le type de donnée n'existe pas, on lève une exception
  NSAssert1 (currentArray!=nil, @"HCFRDataStore->fireSerieChangedForType : dataType %i non disponible", dataType);
  
  // on parcours les entrées du tableau
  NSEnumerator  *enumerator = [currentArray objectEnumerator];
  NSObject<HCFRDataSerieListener> *currentListener;
  
  while (currentListener=(NSObject<HCFRDataSerieListener>*)[enumerator nextObject])
  {
    // et pour chaque listener, on appel newSerieBegunForDataType:
    [currentListener serie:self changedForDataType:dataType];
  }
}
@end
