//  ColorHCFR
//  HCFRFileAccessor.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 26/01/08.
//
//  $Rev: 55 $
//  $Date: 2008-06-09 18:47:46 +0100 (Mon, 09 Jun 2008) $
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


/*!
    @class
    @abstract    Fournis une interface d'accès à un pseudo fichier POSIX représenant un périphérique.
    @discussion  Cette classe permet d'accéder à un pseudo fichier
 représentant un périphérique en gérant un timeout sur la lecture, et un essai supplémentaire en cas d'erreur
 lors de l'écriture ou de la lecture.
*/
@interface HCFRDeviceFileAccessor : NSObject {
  NSLock  *fileAccessLock;
  char    *filePath;
  
  int     fileDescriptor;
}
-(HCFRDeviceFileAccessor*) initWithPath:(const char*)bsdPath;

-(bool) isReady;
-(const char*) filePath;

/*!
    @function 
    @abstract   Envoie une commande et lit une réponse finie sur le pseudo fichier
    @discussion Cette fonction envoie une commande à la sonde, et lit une réponse dont le dernier caractère
 est 0 (null) ou 10 (new line). Si aucun de ces caractères n'est reçus avant le timeout, un code d'erreur sera retourné. 
 La ligne lue sera terminée par un '\0'
 Si readbuffer est null, ou si readBufferSize est null, aucune réponse ne sera lue.
    @param      bufferToWrite Le buffer contenant les données à écrire
    @param      writeBufferSize La taille du buffer à écrire
    @param      readBuffer Le buffer dans lequel la réponse sera stockée.
    @param      readBufferSize la taille du buffer pour la réponse
    @result     -1 si une erreur est survenue, le nombre de caractères lus sinon
*/
-(int) writeCommand:(char*)bufferToWrite ofSize:(size_t)writeBufferSize
                          andReadLineToBuffer:(char*)readBuffer ofSize:(size_t)readBufferSize;
/*!
   @function 
   @abstract   Envoie une commande et lit une réponse non terminée sur le pseudo fichier
   @discussion Cette fonction envoie une commande à la sonde, et lit une réponse dont la fin n'est pas
 marquée. La réponse est lu tant qu'il y a des données. Après 200ms sans données, la fonction retourne
 ce qu'elle à lu.
   La réponse lue ne sera pas forcement terminée par un '\0', il s'agit d'un buffer mémoire, et non
 d'une chaine de caracteres.
   Si readbuffer est null, ou si readBufferSize est null, aucune réponse ne sera lue.
   @param      bufferToWrite Le buffer contenant les données à écrire
   @param      writeBufferSize La taille du buffer à écrire
   @param      readBuffer Le buffer dans lequel la réponse sera stockée
   @param      readBufferSize la taille du buffer pour la réponse
   @param      timeout le nombre de millisecondes pendant lequel on attend la réponse
   @result     -1 si une erreur est survenue, le nombre de caractères lus sinon
*/
-(int) writeCommand:(char*)bufferToWrite ofSize:(size_t)writeBufferSize
                      andReadBulkAnswerToBuffer:(char*)readBuffer ofSize:(size_t)readBufferSize
                                    withTimeout:(unsigned short)timeout;
-(NSString*)path;
@end
