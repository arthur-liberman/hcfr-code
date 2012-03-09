//  ColorHCFR
//  HCFRFileAccessor.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 26/01/08.
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

#import "HCFRDeviceFileAccessor.h"
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

#define kReadTimeoutKey @"KiDeviceFileAccessorReadTimeoutMiliseconds"

const unsigned short delayBetweenBits = 1; // le delai entre l'envoi de chaque bit. Sans pause, la sonde ki n'arrive pas à suivre le rythme

unsigned int readLine (int fileDescriptor, char* buff, int bufferSize, unsigned short timeout);
unsigned int readData (int fileDescriptor, char* buff, int bufferSize, unsigned short timeout);

@interface HCFRDeviceFileAccessor(Private)
-(void) openFile;
@end

@implementation HCFRDeviceFileAccessor
-(HCFRDeviceFileAccessor*) initWithPath:(const char*)bsdPath
{
  self = [super init];
  if (self != nil) {
    fileAccessLock = [[NSLock alloc] init];
    fileDescriptor = -1;

    filePath = malloc(strlen(bsdPath)+1);
    memcpy(filePath, bsdPath, strlen(bsdPath)+1);
    
    // on ouvre le fichier
    [self openFile];
  }
  return self;
}


- (void) dealloc {
  [fileAccessLock release];

  free (filePath);
  
  if (fileDescriptor != -1)
    close (fileDescriptor);
  
  [super dealloc];
}

-(void) openFile
{
  struct termios ttyParams;
  
  fileDescriptor = open (filePath, O_RDWR | O_NONBLOCK | O_NOCTTY);
  if (fileDescriptor != -1)
  {
    tcgetattr(fileDescriptor, &ttyParams);
    cfsetispeed(&ttyParams, B115200);
    cfsetospeed(&ttyParams, B115200);
    tcflush (fileDescriptor, TCIOFLUSH);
    tcsetattr(fileDescriptor, TCSANOW, &ttyParams);
  }
}

-(bool) isReady
{
  return (fileDescriptor != -1);
}

-(const char*) filePath
{
  return filePath;
}

-(int) writeCommand:(char*)bufferToWrite ofSize:(size_t)writeBufferSize
                          andReadLineToBuffer:(char*)readBuffer ofSize:(size_t)readBufferSize
{
  if (![self isReady])
    return -1;
  
  // avant d'envoyer la commande, on flush
  tcflush (fileDescriptor, TCIOFLUSH);
  
/*  int i;
  for (i = 0; i < writeBufferSize; i++)
  {
    printf ("%02x ", 0x0000FF & bufferToWrite[i]);
  }
  printf ("\n");*/
  
  // on boucle pour faire deux essais, en cas de problème sur la lecture et l'écriture.
  unsigned short  loopIndex = 0;
  ssize_t   returnCode = 0;
  
  do
  {
    NSLog (@"writing to sensor");
    // écriture de la commande dit par bit, sinon, la sonde perd le fil de l'histoire
    int i;
    for (i = 0; i < writeBufferSize; i++)
    {
      returnCode = write (fileDescriptor, bufferToWrite+i, 1);
      if (returnCode == -1)
      {
        // en cas d'erreur, on saute la lecture, on passe directe à la tentative suivante
        loopIndex ++;
        continue;
      }
      usleep(delayBetweenBits); // sans cette pause, la sonde Ki perd les pédales
    }

    // avant de lire la réponse, on flush
    tcflush (fileDescriptor, TCIOFLUSH);
    
    NSLog (@"reading from sensor");
    // on ne lit que si on a un buffer de lecture
    if (readBuffer != NULL && readBufferSize > 0)
    {
      // La mesure est une opération longue. on donne 30 secondes par defaut à la sonde pour répondre
      // (soit 30 000 milisecondes), mais cette valeur peut être modifiée par une préférence.
      int timeout = [[NSUserDefaults standardUserDefaults] integerForKey:kReadTimeoutKey];
      if (timeout == 0)
        timeout = 30000;
      returnCode = readLine(fileDescriptor, readBuffer, readBufferSize, timeout);
    }
    else
      returnCode = 0; // aucun caractère lu
    
    loopIndex ++;
    NSLog (@"loop %d, return code %ld", loopIndex, returnCode);
  }
  while ( (returnCode == -1) && (loopIndex < 2) );
  
  return returnCode;
}

-(int) writeCommand:(char*)bufferToWrite ofSize:(size_t)writeBufferSize
                      andReadBulkAnswerToBuffer:(char*)readBuffer ofSize:(size_t)readBufferSize
                                    withTimeout:(unsigned short)timeout
{
  if (![self isReady])
    return -1;
  
  // avant d'envoyer la commande, on flush
  tcflush (fileDescriptor, TCIOFLUSH);

  // on boucle pour faire deux essais, en cas de problème sur l'écriture.
  unsigned short  loopIndex = 0;
  ssize_t   returnCode = 0;
  do
  {
    // écriture de la commande bit par bit, sinon, la sonde ne recois que le premier char
    int i;
    for (i = 0; i < writeBufferSize; i++)
    {
      returnCode = write (fileDescriptor, bufferToWrite+i, 1);
      if (returnCode == -1)
      {
        // en cas d'erreur, on saute la lecture, on passe directe à la tentative suivante
        loopIndex ++;
        continue;
      }
      usleep(delayBetweenBits); // sans cette pause, la sonde Ki perd les pédales
    }

    // on ne lit que si on a un buffer de lecture
    if (readBuffer != NULL && readBufferSize > 0)
    {
      returnCode = readData(fileDescriptor, readBuffer, readBufferSize, timeout);
    }
    else
      returnCode = 0; // aucun caractère lu
    
    if (returnCode == -1)
    
    loopIndex ++;
  }
  while ( (returnCode == -1) && (loopIndex < 2) );
  
  return returnCode;
}

-(NSString*)path
{
  return [NSString stringWithCString:filePath encoding:NSUTF8StringEncoding];
}
@end

// Cette fonction lit une ligne dans le pseudo fichier, en gérant un timeout.
// Les caracteres de fin de ligne sont traités, et la chaine retournée est terminée par un '\0'
unsigned int readLine (int fileDescriptor, char* buff, int bufferSize, unsigned short timeout)
{
  unsigned int  readIndex = 0;
  char          readBuffer;
  
  if (bufferSize < 2)
    return 0;
  
  // on boucle pour lire tous les caractères de la chaîne.
  do {
    // on lit en mode non bloquant.
    // Donc, on boucle avec une petite pause sur le read, jusqu'a avoir un caractère,
    // ou jusqu'au timeout.
    const unsigned short maxRetries = timeout/10; // car on a une pause de 10ms par boucle.
    unsigned short       loopCounter = 0;
    ssize_t              returnCode;
    
    returnCode = read (fileDescriptor, &readBuffer, 1);
    while (returnCode == -1 && loopCounter < maxRetries)
    {
      // Si on arrive la, c'est que la lecture précedente à échouée.
      // On attend 10ms, et on ressaye.
      usleep(10*1000);
      returnCode = read (fileDescriptor, &readBuffer, 1);

      // si l'erreur de lecture est "bad file descriptor", c'est que la sonde à été déconnectée.
      // On n'attend pas, on retourne une erreur
      if (returnCode == -1 && errno == EBADF)
        return -1;
      
      NSLog (@"readline : returnCode = %ld, errno = %d", returnCode, errno);
      
      loopCounter ++;
    }

    // Si en arrivant ici returnCode vaux -1, c'est qu'on est dans une situation de timeout.
    if (returnCode == -1)
    {
      return -1;
    }
    
    // traitement du caractère lu.
    if ((readBuffer != 10) && (readBuffer != 13))
    {
      buff[readIndex] = readBuffer;
      readIndex ++;
    }
  }
  while ( (readBuffer != 0) && (readBuffer != 10) && (readIndex<(bufferSize-1)));
  
  // on clos la chaine de caractères
  buff[readIndex] = '\0';
  
  return readIndex;
}

// Cette fonction litdans le pseudo fichier tant qu'il y a des données et que le buffer n'est pas plein
// La chaine retournée ne finie pas forcément par un '\0' !
unsigned int readData (int fileDescriptor, char* buff, int bufferSize, unsigned short timeout)
{
  unsigned int  readIndex = 0;
  char          readBuffer;
  
  if (bufferSize < 1)
    return 0;
  
  // on boucle pour lire des caractères tant que on n'a pas d'erreur de lecture
  // qui indiquerait qu'on a lu toutes les données et que le buffer n'est pas plein
  ssize_t              returnCode;
  do {
    // on lit en mode non bloquant.
    // Donc, on boucle avec une petite pause sur le read, jusqu'a avoir un caractère,
    // ou jusqu'au timeout.
    const unsigned short maxRetries = timeout/10; // car on a une pause de 10ms par boucle.
    unsigned short       loopCounter = 0;
    
    returnCode = read (fileDescriptor, &readBuffer, 1);
    while (returnCode == -1 && loopCounter < maxRetries)
    {
      // Si on arrive la, c'est que la lecture précedente à échouée.
      // On attend 10ms, et on ressaye.
      usleep(10*1000);
      returnCode = read (fileDescriptor, &readBuffer, 1);

      // si l'erreur de lecture est "bad file descriptor", c'est que la sonde à été déconnectée.
      // On n'attend pas, on retourne une erreur
      if (returnCode == -1 && errno == EBADF)
        return -1;

      loopCounter ++;
    }

    // stockage caractère lu si on en a lu un
    if (returnCode == 1)
    {
      buff[readIndex] = readBuffer;
      readIndex ++;
    }
  }
  while ( (returnCode != -1) && (readIndex<bufferSize) );
  
  return readIndex;
}
