//  ColorHCFR
//  HCFRProfiles.mm
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 30/12/07.
//
//  $Rev: 133 $
//  $Date: 2010-12-26 17:22:04 +0000 (Sun, 26 Dec 2010) $
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

#import "HCFRProfilesManager.h"
#import "Sparkle/NSFileManager+Authentication.h"

// kCalibrationFilesDirectoryName est le nom du dossier contenant les profiles
#define kCalibrationFilesDirectoryName @"CalibrationProfiles/"

#define kProfileUpdateServerURL @"ftp://www.homecinema-fr.com/fichiers_etalons/"
// kDedicatedTemporaryDirectory pointe sur un dossier qui sera utilisé temporairement
// pour télécharger les fichiers.
// Il doit être dédié, car il sera supprimé à la fin de la MAJ des profiles.
// Si il n'existe pas, il sera créé
// Le "/" final est nécessaire.
#define kDedicatedTemporaryDirectory @"/tmp/colorHCFR/"

@interface HCFRProfilesManager(Private)
/*!
    @function 
    @abstract   Charge les informations sur le profile (date, md5 ...)
    @discussion (description)
    @param      path Le chemin du fichier profile
    @result     (description)
*/
+(ProfileFileInfos) getProfileInformation:(NSString*)path;

-(void)download:(NSURLDownload *)download didFailWithError:(NSError *)error;
-(void)downloadDidFinish:(NSURLDownload *)download;

/*!
    @function 
    @abstract   Charge et traite le profile suivant
    @discussion Cette fonction vas chercher le profile suivant, et lance son téléchargement
    @result     YES si un téléchargement à été lancé, NO si il n'y a plus de fichier à télécharger
*/
-(BOOL) downloadNextProfile;
/*!
    @function 
    @abstract   Vérifie si le profile doit être mise à jour ou non
    @result     Yes si le profile doit être mis à jour, NO sinon.
*/
-(BOOL) checkIfProfileNeedsUpdate:(ProfileFileInfos*)profile;
  /*!
  @function 
   @abstract   Lance le téléchargement du fichier demandé à partir du serveur HCFR
   @param      remoteFileName Le nom du fichier distant
   @result     YES si le téléchargement à pu être lancé, NO en cas d'erreur.
   */
-(BOOL) startDownloadForFile:(NSString*)remoteFileName;
/*!
    @function 
    @abstract   Doit être appelé à la fin de la procédure de MAJ
    @discussion cette fonction affichera le rapport de MAJ, et releasera les objets qui en ont besoin.
*/
-(void) finishUpdate;
/*!
    @function 
    @abstract   Recherche un profile en fonction du nom de fichier dans la liste des path
    @param      fileName Le nom du fichier à chercher
    @result     le path si le profile est trouvé, nil si il n'existe pas
*/
-(NSString*) findProfileInArrayWithFileName:(NSString*)fileName;
/*!
    @function 
    @abstract   Change la vue de la fenettre de progression en redimentionant la fenêtre
    @param      newView La nouvelle vue à mettre en place
*/
-(void) setView:(NSView*)newView;
/*!
    @function 
    @abstract   Fournis une chaine de caractère de rapport
    @result     Le rapport
*/
-(NSString*) reportForAddedProfiles;
@end
#pragma mark -
@implementation HCFRProfilesManager
+(HCFRProfilesManager*) sharedinstance
{
  static HCFRProfilesManager *sharedInstance = nil;
  
  if (sharedInstance == nil)
    sharedInstance = [[HCFRProfilesManager alloc] init];
  
  return sharedInstance;
}

-(HCFRProfilesManager*) init
{
  [super init];
  
  [NSBundle loadNibNamed:@"ProfilesManager" owner:self];
  
  updateRunning = NO;
  profilesIndex = NULL;
  
  currentlyAvailableProfiles = nil;
  downloadedFileName = nil;
  
  addedProfiles = [[NSMutableArray alloc] init];
  
  return self;
}
-(void) dealloc
{
  if (currentlyAvailableProfiles != nil)
    [currentlyAvailableProfiles release];
  if (downloadedFileName != nil)
    [downloadedFileName release];
  
  [addedProfiles release];
    
  [super dealloc];
}

+(NSString*) getProfileWithName:(NSString*)profileName
{
  NSEnumerator *enumerator = [[HCFRProfilesManager getAvailableProfiles] objectEnumerator];
  NSString     *currentPath;
  
  while (currentPath = [enumerator nextObject])
  {
    if ([currentPath hasSuffix:profileName])
      return currentPath;
  }
  
  return nil;
}

+(NSArray*) getAvailableProfiles
{
  // Mise en place du dictionnaire de path

  // Pour le moment, on se simplifie la vie en ne gérant que le chemin appPath pour les profiles.
  // Pour ajouter le dossier système et le dossier utilisateur, il faut les ajouter au tableau
  // et gérer leur mise à jour.
	NSString        *appPath = [NSString stringWithFormat:@"%@/%@",[[NSBundle mainBundle] resourcePath],kCalibrationFilesDirectoryName];
//	NSString        *applicationSupportFolder = [@"Library/Application Support/ColorHCFR/" stringByAppendingString:calibrationFilesDirectoryName];
//	NSString        *userPath = [[NSHomeDirectory() stringByAppendingString:@"/"]
//                    stringByAppendingString:applicationSupportFolder];
//	NSString        *systemPath = [@"/" stringByAppendingString:applicationSupportFolder];
	NSArray         *paths = [NSArray arrayWithObject:appPath];
  
	NSEnumerator    *pathEnumerator = [paths objectEnumerator];
	NSString        *currentPath;
  
  // le tableau que l'on va remplire
  NSMutableArray       *resultArray = [NSMutableArray arrayWithCapacity:10];
  
  while (currentPath = [pathEnumerator nextObject])
  {
    NSEnumerator	*fileEnumerator = [[[NSFileManager defaultManager] directoryContentsAtPath:currentPath] objectEnumerator];
		NSString		*fileName;
    
		while ((fileName = [fileEnumerator nextObject]))
		{
      // on vérifie que le fichier à une extention ".thc"
			if (![[fileName pathExtension] isEqualToString:@"thc"])
        continue;
      
      [resultArray addObject:[currentPath stringByAppendingString:fileName]];
    }      
  }
  return resultArray;
}

-(void) updateProfiles
{
  // pour le moment, les MAJ ne sont faisable que par un admin
  NSString        *appPath = [NSString stringWithFormat:@"%@/%@",[[NSBundle mainBundle] resourcePath],kCalibrationFilesDirectoryName];

  if (![[NSFileManager defaultManager] isWritableFileAtPath:appPath])
  {
    [[NSAlert alertWithMessageText:HCFRLocalizedString(@"Updates can only be preformed by and administrator in this version.",
                                                     @"Updates can only be preformed by and administrator in this version.")
                    defaultButton:HCFRLocalizedString(@"Ok",@"Ok") alternateButton:nil otherButton:nil
         informativeTextWithFormat:HCFRLocalizedString(@"Please ask the personne who have installed the software.",
                                                     @"Please ask the personne who have installed the software.")] runModal];
    return;
  }
  
  if (currentlyAvailableProfiles != nil)
    [currentlyAvailableProfiles release];
  currentlyAvailableProfiles = [[HCFRProfilesManager getAvailableProfiles] retain];
  
  // une seule mise à jour à la fois
  if (updateRunning)
    return;
  
  // init des variables
  updateRunning = YES;
  updateState = 0;
  
  if (profilesIndex != NULL)
    delete profilesIndex;
  
  [addedProfiles removeAllObjects];

    
  // progress monitor
  [self setView:progressView];
  [progressWindow makeKeyAndOrderFront:self];
  [progressIndicator setDoubleValue:0.0];
  [progressIndicator setIndeterminate:YES];
  [progressTextField setStringValue:HCFRLocalizedString(@"Loading profiles list ...",
                                                      @"Loading profiles list ...")];
  
  // Le principe est le suivant :
  // On lance le téléchargement du fichier listant les profiles.
  // Le délégué sera averti lorsque le téléchargement sera terminé, traitera le fichier obtenu
  // et passera à l'étape suivante.

  // première étape donc, on charge le fichier listant les profiles
  [self startDownloadForFile:@"index"];  
}

#pragma mark Les actions IB
-(IBAction) closeWindow:(id)sender
{
  [progressWindow close];
}
@end

#pragma mark -
@implementation HCFRProfilesManager (Private)
+(ProfileFileInfos) getProfileInformation:(NSString*)path
{
  ProfileFileInfos    profileInfos;
  NSDictionary        *fileInfos = [[NSFileManager defaultManager] fileAttributesAtPath:path traverseLink:NO];
  NSDate              *date = [fileInfos objectForKey:NSFileModificationDate];
  
  // le nom
  profileInfos.fileName=[[path lastPathComponent] cStringUsingEncoding:NSUTF8StringEncoding];
  
  // on récupère la date
  profileInfos.modificationDate = [date timeIntervalSince1970];
    
  return profileInfos;
}

#pragma mark le delegue pour les téléchargements
-(void)download:(NSURLDownload *)download didFailWithError:(NSError *)error
{
  // release the connection
  [download release];
  
/*  NSLog(@"Download failed! Error - %@ %@",
        [error localizedDescription],
        [[error userInfo] objectForKey:NSErrorFailingURLStringKey]); */
  
  // selon l'etat
  switch (updateState)
  {
    case 0 :
      // on n'a pas pu obtenir le fichier listant les profils -> on laisse tomber.
      
      [messageTextField setStringValue:HCFRLocalizedString(@"Update failed :\nThe profiles list could not be loaded.",
                                                         @"Update failed :\nThe profiles list could not be loaded.")];
      [self setView:messageView];
      
      [self finishUpdate];
      break;
    case 1 :
      // le téléchargement d'un des profiles a echoué -> pas grave, on l'ignore.
      // on n'oublie pas d'avancer la barre de progression
      [progressIndicator setDoubleValue:[progressIndicator doubleValue]+100.0/profilesIndex->size()];
      
      if (![self downloadNextProfile])
      {
        // si il n'y en a pas, on a fini -> on affiche le rapport
        [reportTextField setStringValue:[self reportForAddedProfiles]];
        [self setView:reportView];
        [self finishUpdate];
      }
      break;
  }
}

-(void)downloadDidFinish:(NSURLDownload *)download
{
  // release the connection
  [download release];
  
  // selon l'etat
  switch (updateState)
  {
    case 0 :
//      NSLog(@"HCFRProfilesManager:update -> index downloaded, parsing it.");
      
      try
      {
        // on crée le indexFileReader en lui passant le fichier à lire
        profilesIndex = new ProfilesRepositoryIndexFileReader([[kDedicatedTemporaryDirectory stringByAppendingString:@"index"] cStringUsingEncoding:NSUTF8StringEncoding]);
        
        [progressIndicator setIndeterminate:NO];
        
        // puis on change l'état, et on lance le téléchargement du profile suivant.
        updateState ++;
        if (![self downloadNextProfile])
        {
          // Aucune MAJ dispo
          [messageTextField setStringValue:HCFRLocalizedString(@"Your profiles are up to date.",@"Your profiles are up to date.")];
          [self setView:messageView];
          [self finishUpdate];
        }
      }
      catch (Exception e)
      {
        NSLog(@"HCFRProfilesManager:update -> parsing failed : %s", e.message);
        [self finishUpdate];
      }

      break;
    case 1 :
      NSAssert (downloadedFileName!=nil, @"profilesManager:downloadDidFinish -> downloadedFileName is nil");
      [progressIndicator setDoubleValue:[progressIndicator doubleValue]+100.0/(2*profilesIndex->size())];
      
      // on a recus un profile, on doit l'installer
      // et l'ajouter au tableau des profiles mis à jour.
      NSString *installPath = [NSString stringWithFormat:@"%@/%@%@", [[NSBundle mainBundle] resourcePath], kCalibrationFilesDirectoryName, downloadedFileName];

      // on efface le fichier pour le remplacer par le nouveau
      if([[NSFileManager defaultManager] fileExistsAtPath:installPath])
      {
        int tag = 0;
        
        [[NSWorkspace sharedWorkspace] performFileOperation:NSWorkspaceRecycleOperation
                                                     source:[installPath stringByDeletingLastPathComponent]
                                                destination:@""
                                                      files:[NSArray arrayWithObject:[installPath lastPathComponent]]
                                                        tag:&tag];
      }
        
      if ([[NSFileManager defaultManager] movePath:[kDedicatedTemporaryDirectory stringByAppendingString:downloadedFileName]
                                            toPath:installPath handler:nil])
        [addedProfiles addObject:downloadedFileName];
      else
        NSLog (@"profilesManager:downloadDidFinish -> installation failed");
      
      [progressIndicator setDoubleValue:[progressIndicator doubleValue]+100.0/(2*profilesIndex->size())];
      // on charge ensuite le profile suivant
      if (![self downloadNextProfile])
      {
        // si il n'y en a pas, on a fini -> on affiche le rapport
        [reportTextField setStringValue:[self reportForAddedProfiles]];
        [self setView:reportView];
        [self finishUpdate];
      }
      break;
  }
}

#pragma mark les fonctions utilitaires
-(BOOL) downloadNextProfile
{
  try
  {
    // on boucle sur les profiles jusqu'a ce que un téléchargement soit lancé
    // ou qu'on ait fini de parcourir la liste
    while (profilesIndex->hasNext())
    {
      ProfileFileInfos currentProfileInfos = profilesIndex->nextEntry();
      const char  *cFileName = currentProfileInfos.fileName.c_str();
      
      if (downloadedFileName != nil)
        [downloadedFileName release];
      downloadedFileName = [[NSString stringWithCString:cFileName] retain];
      
      if ([self checkIfProfileNeedsUpdate:&currentProfileInfos])
      {
        [progressTextField setStringValue:[HCFRLocalizedString(@"Loading profile ",
                                                             @"Loading profile ") stringByAppendingString:downloadedFileName]];
        return [self startDownloadForFile:downloadedFileName];
      }
      else
      {
        // on avance l'indicateur
        [progressIndicator setDoubleValue:[progressIndicator doubleValue]+100.0/profilesIndex->size()];
        continue;
      }
    }
    // si on arrive là, c'est que on a fait le tour des profiles
    return NO;
  }
  catch (Exception e)
  {
    NSLog(@"HCFRProfilesManager:update -> next entry failed : %s", e.message);
    // on ne sais pas trop pourquoi on passe ici, mais à priori, on peut dire que c'est finie -> on retourne NO
    return NO;
  }
}  

-(BOOL) checkIfProfileNeedsUpdate:(ProfileFileInfos*)profile
{
  NSString    *profileName = [NSString stringWithCString:profile->fileName.c_str()];
  
  // on vérifie si le profile existe
  // Si il exite, on recupère le path complet
  NSString  *profilePath = [self findProfileInArrayWithFileName:profileName];
  
  
  if (profilePath != nil)
  {
    // on test si le fichier dont on dispose est plus ancien ou pas.
    ProfileFileInfos localProfile = [HCFRProfilesManager getProfileInformation:profilePath];

    return (localProfile.modificationDate < profile->modificationDate);
  }
  else // on n'a pas ce profile en stock -> on doit le télécharger.
  {
    return YES;
  }
}  

-(BOOL) startDownloadForFile:(NSString*)remoteFileName
{
//  NSLog (@"starting download for %@", remoteFileName);
  
  NSURL         *url = [NSURL URLWithString:[kProfileUpdateServerURL stringByAppendingString:remoteFileName]];
  NSURLRequest  *request = [NSURLRequest requestWithURL:url cachePolicy:NSURLRequestReloadIgnoringCacheData timeoutInterval:10];
  NSURLDownload *download = [[NSURLDownload alloc] initWithRequest:request delegate:self];
  BOOL          isDirectory = NO;
  
  // on vérifie que le dossier destination existe.
  if (![[NSFileManager defaultManager] fileExistsAtPath:kDedicatedTemporaryDirectory isDirectory:&isDirectory] || !isDirectory)
  {
    [[NSFileManager defaultManager] createDirectoryAtPath:kDedicatedTemporaryDirectory attributes:nil];
  }
  
  if (download) {
    // on fixe la destination
    [download setDestination:[kDedicatedTemporaryDirectory stringByAppendingString:remoteFileName] allowOverwrite:YES];
    return YES;
  }
  else {
    NSLog(@"Download failed (immediate error)");
    return NO;
  }
}

-(void) finishUpdate
{
//  NSLog (@"HCFRProfilesManager:update -> update finished");
  
  delete profilesIndex;
  profilesIndex = NULL;
  updateRunning = NO;
  
}

-(NSString*) findProfileInArrayWithFileName:(NSString*)fileName
{
  NSEnumerator  *enumerator = [currentlyAvailableProfiles objectEnumerator];
  NSString      *currentProfile;
  
  while (currentProfile = [enumerator nextObject])
  {
    if ([fileName isEqualToString:[currentProfile lastPathComponent]])
      return currentProfile;
  }
  return nil;
}

-(void) setView:(NSView*)newView
{
  NSRect  oldFrame = [progressWindow frame];
  int			titleBarHeight = [progressWindow frame].size.height - [[progressWindow contentView] frame].size.height;

  NSRect newFrame = [progressWindow frame];
  newFrame.size = [newView frame].size;
  newFrame.size.height += titleBarHeight;
  newFrame.origin.y -= (newFrame.size.height - oldFrame.size.height);
  [progressWindow setContentView:newView];
  [progressWindow setFrame:newFrame display:YES animate:YES];
}

-(NSString*) reportForAddedProfiles
{
  int       index = 0;
  int       nbProfiles = [addedProfiles count];
  
  // normalement, on n'arrivera pas là : ce cas est traité lors de la reception du fichier d'index.
  // Néanmoins, on y passera si tous les profiles à mettre à jour n'ont pas pu être téléchargé.
  if (nbProfiles == 0)
    return HCFRLocalizedString(@"Your profiles are up to date.",@"Your profiles are up to date.");
    
  NSMutableString  *result;

  result = [NSMutableString stringWithString:HCFRLocalizedString(@"Those profiles were updated :\n",
                                                               @"Those profiles were updated :\n")];
  for (index = 0; index < nbProfiles; index ++)
  {
    [result appendFormat:@"\t- %@\n", [addedProfiles objectAtIndex:index]];
  }

  return result;
}
@end