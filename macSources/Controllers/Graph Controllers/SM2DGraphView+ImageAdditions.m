//  ColorHCFR
//  HCFRGraphicView.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 08/08/08.
//
//  $Rev: 135 $
//  $Date: 2011-02-27 10:48:51 +0000 (Sun, 27 Feb 2011) $
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

#import "SM2DGraphView+ImageAdditions.h"

// constantes privées -> non préfixées
#define kGraphExportSize @"GraphExportSize"
#define kGraphExportFormat @"GraphExportFormat"

@interface SM2DGraphView (Private)
/*!
    @function
    @abstract   Callback du savePanel
*/
- (void)savePanelDidEnd:(NSSavePanel *)savePanel returnCode:(int)returnCode  contextInfo:(void  *)contextInfo;
@end


@implementation SM2DGraphView (ImageAdditions)
-(NSImage*) imageRepresentation
{
  NSRect  originalRect = [self frame];
  NSRect  destRect = NSMakeRect(0.0,0.0,0.0,0.0);

  // on lit les préférences
  NSString *sizeString = [[NSUserDefaults standardUserDefaults] stringForKey:kGraphExportSize];
  if (sizeString != nil)
  {
    destRect.size = NSSizeFromString(sizeString);
    // si la lecture de la taille a échouée, on supprime l'entrée des prefs,
    // car NSSizeFromString est lent en cas d'echec.
    if (destRect.size.width == 0.0 || destRect.size.height == 0.0)
      [[NSUserDefaults standardUserDefaults] removeObjectForKey:kGraphExportSize];
  }
  
  // Si on n'a pas réussit à lire la taille (erreur de format ou preference innexistante)
  // on prend les valeurs par defaut (640*480)
  if (destRect.size.width == 0.0 || destRect.size.height == 0.0)
    destRect.size = NSMakeSize(640.0,480.0);

  NSImage *newImage = [[NSImage alloc] initWithSize:destRect.size];
  
  [self setFrame:destRect];
  [newImage lockFocus];
  [self drawRect:destRect];
  [newImage unlockFocus];
  [self setFrame:originalRect];

  return [newImage autorelease];
}
-(void) copyImageRepresentationToPastboard:(id)sender
{
  NSImage       *image = [self imageRepresentation];
  NSPasteboard  *pb = [NSPasteboard generalPasteboard];
  [pb declareTypes:[NSArray arrayWithObject:NSTIFFPboardType] owner:self];
  [pb setData:[image TIFFRepresentation] forType:NSTIFFPboardType];
}
-(void) saveAsImage:(id)sender
{
  NSSavePanel   *savePanel = [NSSavePanel savePanel];
  
  // on n'impose pas d'extension, on s'en servira pour savoir quel format utiliser.
  // Si il n'y en a pas, on prendra le format par defaut.
//  [savePanel setRequiredFileType:formatString];
  [savePanel beginSheetForDirectory:Nil
                               file:Nil
                     modalForWindow:[self window]
                      modalDelegate:self
                     didEndSelector:@selector(savePanelDidEnd:returnCode:contextInfo:)
                        contextInfo:Nil];
}
- (void)savePanelDidEnd:(NSSavePanel *)savePanel returnCode:(int)returnCode  contextInfo:(void  *)contextInfo
{
  if (returnCode != NSOKButton)
    return;
  
  [savePanel orderOut:self];
  [self saveGraphToPath:[savePanel filename]];
}
-(void) saveGraphToPath:(NSString*)filePath
{
  NSString          *localFilePath = filePath;
  NSData            *bitmapData;
  NSData            *imageData = [[self imageRepresentation]  TIFFRepresentation];
  NSBitmapImageRep  *imageRep = [NSBitmapImageRep imageRepWithData:imageData];
  
  //------------------------------------------------------------------
  // On commence par traiter le problème des extensions de fichier
  //------------------------------------------------------------------
  NSString  *formatExtension = [filePath pathExtension];
  // si on n'a pas d'extension, on prend la valeur des prefs
  if ([formatExtension length] == 0)
  {
    formatExtension = [[NSUserDefaults standardUserDefaults] stringForKey:kGraphExportFormat];
    // si on n'a toujours rien, on prend une valeur par defaut
    if (formatExtension == nil)
      formatExtension = @"png";
    
    // et on ajoute d'extension au fichier.
    localFilePath = [filePath stringByAppendingFormat:@".%@", formatExtension];
  }
  
  //------------------------------------------------------------------
  // Maintenant qu'on a l'extension, on en déduit le format du fichier
  //------------------------------------------------------------------
  NSBitmapImageFileType fileType;
  
  if ([formatExtension isEqualToString:@"png"])
    fileType = NSPNGFileType;
  else if ([formatExtension isEqualToString:@"jpg"])
    fileType = NSJPEGFileType;
  else if ([formatExtension isEqualToString:@"jp2"])
    fileType = NSJPEG2000FileType;
  else if ([formatExtension isEqualToString:@"gif"])
    fileType = NSGIFFileType;
  else if ([formatExtension isEqualToString:@"tiff"])
    fileType = NSTIFFFileType;
  else if ([formatExtension isEqualToString:@"bmp"])
    fileType = NSBMPFileType;
  else
  {
    // format inconnu -> on prend du png par defaut.
    fileType = NSPNGFileType;
    localFilePath = [filePath stringByAppendingString:@".png"];
  }
  
  bitmapData = [imageRep representationUsingType:fileType
                                      properties:nil];
  
  [bitmapData writeToFile:localFilePath atomically:YES];
}
-(NSMenu *) menuForEvent:(NSEvent *)theEvent
{
  NSMenu      *viewMenu = [[NSMenu alloc] init];

  // copier
  NSMenuItem  *menuItem = [[NSMenuItem alloc] initWithTitle:@"Copy"
                                                     action:@selector(copyImageRepresentationToPastboard:)
                                              keyEquivalent:@""];
  [menuItem setTarget:self];
  [viewMenu addItem:menuItem];
  [menuItem release];
  
  // enregistrer sous
  menuItem = [[NSMenuItem alloc] initWithTitle:@"Save as ..."
                                                     action:@selector(saveAsImage:)
                                              keyEquivalent:@""];
  [menuItem setTarget:self];
  [viewMenu addItem:menuItem];
  [menuItem release];
  
  
  return [viewMenu autorelease];
}

#pragma mark Drag & drop
- (void)mouseDragged:(NSEvent *)theEvent
{
  NSImage       *image = [self imageRepresentation];
  NSSize        dragImageSize = NSMakeSize(32,32*[image size].height/[image size].width);
  NSImage       *dragImage = [[NSImage alloc] initWithSize:dragImageSize];
  NSPasteboard  *pb = [NSPasteboard pasteboardWithName:NSDragPboard];

  [dragImage lockFocus];
  [image drawInRect:NSMakeRect(0, 0,dragImageSize.width,dragImageSize.height)
           fromRect:NSMakeRect(0, 0,[image size].width,[image size].height)
          operation:NSCompositeSourceOver
           fraction:0.5];
  [dragImage unlockFocus];
  
  [pb declareTypes:[NSArray arrayWithObjects:NSTIFFPboardType, NSFilesPromisePboardType, nil] owner:self];
  [pb setData:[image TIFFRepresentation] forType:NSTIFFPboardType];
  [pb setPropertyList:[NSArray arrayWithObject:@"ihc"] forType:NSFilesPromisePboardType];

  NSPoint   imagePosition = [self convertPoint:[theEvent locationInWindow] fromView:Nil];
  imagePosition.x -= dragImageSize.width/2.0;
  imagePosition.y -= dragImageSize.height/2.0;
    
  [self dragImage:dragImage
               at:imagePosition
           offset:NSMakeSize(0.0,0.0)
            event:theEvent
       pasteboard:pb
           source:self
        slideBack:YES];
  [dragImage release];
}

- (NSArray*)namesOfPromisedFilesDroppedAtDestination:(NSURL*) dropDestination
{
  // On ne met pas d'extension au nom du fichier, ce qui fait quesaveGraphToFile utilisera le format par defaut.
  NSString  *filename = [@"ColorHCFR-graph" retain];
  NSString  *filepath = [[NSString alloc] initWithFormat:@"%@/%@",[dropDestination path], filename];
  int       uniqueFileNumber = 0;
  
  // on vérifie que le fichier n'existe pas, sinon, on ajoute un numéro.
  while ([[NSFileManager defaultManager] fileExistsAtPath:filepath])
  {
    uniqueFileNumber ++;
    [filename release];
    filename = [[NSString alloc] initWithFormat:@"ColorHCFR-graph-%d", uniqueFileNumber];
    [filepath release];
    filepath = [[NSString alloc] initWithFormat:@"%@/%@",[dropDestination path], filename];
  }  

  [self saveGraphToPath:filepath];
  [filepath release];
  return [NSArray arrayWithObject:[filename autorelease]];
}  
@end
