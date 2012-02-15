//  ColorHCFR
//  HCFRKiGeneratorIRProfile.m
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

#import "HCFRKiGeneratorIRProfile.h"

// une classe container pour pouvoir faire un NSDictionnary de IRCodeBuffers
@interface HCFRKiGeneratorIRProfile(Private)
-(IRCodeBuffer) hexStringToCodeBuffer:(NSString*)string;
-(NSString*) codeBufferToHexString:(IRCodeBuffer)codeBuffer;
@end

const IRCodeBuffer emptyIRCode = {NULL, 0};

@implementation HCFRKiGeneratorIRProfile
- (id) init {
  self = [super init];
  if (self != nil) {
    name=[HCFRLocalizedString(@"Unnamed profile", @"Unnamed profile") retain];
    description=@"";
    author=@"";
    
    // on initialise le tableau de codes
    int nbCodes = sizeof(codes)/sizeof(codes[0]);
    int i;
    for (i = 0; i < nbCodes; i ++)
    {
      codes[i] = emptyIRCode;
    }
  }
  return self;
}

- (void) dealloc {
  // on libère tous les codes du tableau
  int nbCodes = sizeof(codes)/sizeof(codes[0]);
  int i;
  for (i = 0; i < nbCodes; i ++)
  {
    if (codes[i].buffer != NULL)
      free(codes[i].buffer);
  }

  [super dealloc];
}

-(NSString*) name
{ return name; }
-(void) setName:(NSString*)newName
{
  if (name != nil)
    [name release];

  name = [newName retain];
}
-(NSString*) description
{ return description; }
-(void) setDescription:(NSString*)newDescription
{
  if (description != nil)
    [description release];
  
  description = [newDescription retain];
}
-(NSString*) author
{ return author; }
-(void) setAuthor:(NSString*)newAuthor
{
  if (author != nil)
    [author release];
  
  author = [newAuthor retain];
}

#pragma mark Accesseurs des codes IR
-(NSString*)codeAsStringForType:(IRProfileCodeTypes)type
{
  return [self codeBufferToHexString:codes[type]];
}
-(IRCodeBuffer)codeBufferForType:(IRProfileCodeTypes)type
{
  return codes[type];
}
-(void) setCode:(NSString*)newCode forType:(IRProfileCodeTypes)type
{
  if (codes[type].buffer != NULL)
  {
    free(codes[type].buffer);
    codes[type] = emptyIRCode;
  }
  
  codes[type] = [self hexStringToCodeBuffer:newCode];
}
-(void) setCodeBuffer:(char*)buffer codeSize:(unsigned int)codeSize forType:(IRProfileCodeTypes)type
{
  if (codes[type].buffer != NULL)
  {
    free(codes[type].buffer);
    codes[type] = emptyIRCode;
  }

  IRCodeBuffer newCodeBuffer;
  newCodeBuffer.buffer = malloc(codeSize);
  memcpy(newCodeBuffer.buffer,buffer,codeSize);
  newCodeBuffer.bufferSize = codeSize;
  
  codes[type] = newCodeBuffer;
}

#pragma mark accesseurs pour les delais
-(int) delayForType:(IRProfileDelayTypes)type
{
  return delays[type];
}
-(void) setDelay:(int)newDelay forType:(IRProfileDelayTypes)type
{
  delays[type] = newDelay;
}

#pragma mark Fonctions de transformation et de validation
-(IRCodeBuffer) hexStringToCodeBuffer:(NSString*)string
{
  // on se fait une copie de la chaine de char, dont on supprime les espaces
  NSMutableString *mutableString = [string mutableCopy];
  [mutableString replaceOccurrencesOfString:@" " withString:@"" options:0 range:NSMakeRange(0, [mutableString length])];
    
  int             index;
  int             nbOfChars = [mutableString length]/2;
  
  char  *cString = calloc ([mutableString length]+1, 1);
  [mutableString getCString:cString maxLength:[mutableString length]+1 encoding:NSASCIIStringEncoding];
  [mutableString release];

  char  *resultingCharArray = (char*)calloc(nbOfChars, 1);

  for (index = 0; index < nbOfChars; index ++)
  {
    int  value;
    char hexaValue[3];
    hexaValue[0] = cString[index*2];
    hexaValue[1] = cString[index*2+1];
    hexaValue[2] = '\0';
    
    // on lit deux charactères hexa, on récupère la valeur dans un int.
    int scanResult = sscanf(hexaValue, "%2x", &value);
    // puis on stock la valeur dans le buffer si on a lu une valeur correctement
    if (scanResult == 1)
      resultingCharArray[index]=value&0xFF;
  }
  
  free (cString);
  
  IRCodeBuffer newCodeBuffer;
  newCodeBuffer.buffer = resultingCharArray;
  newCodeBuffer.bufferSize = nbOfChars;
  
  return newCodeBuffer;
}
-(NSString*) codeBufferToHexString:(IRCodeBuffer)codeBuffer
{
  if (codeBuffer.buffer == NULL || codeBuffer.bufferSize == 0)
    return @"";
  
  int   index;
  NSMutableString *result = [[NSMutableString alloc] init];
  
  
  for (index = 0; index < codeBuffer.bufferSize; index ++)
  {
    [result appendFormat:@"%02X", 0x0000FF&codeBuffer.buffer[index]];
    if ((index&0x1))
      [result appendFormat:@" "];
  }

  return [result autorelease];
}
-(IRProfileCodeErrorType) validateIrCodeForType:(IRProfileCodeTypes)type
{
  IRCodeBuffer codeBuffer = codes[type];
  
  if (codeBuffer.bufferSize < 8)
    return IRProfileCodeHeaderIsIncomplete;
  
  UInt16          head, length1, length2;
  unsigned short  codeLength = codeBuffer.bufferSize - 8; // on enlève les 8 octets de header
  
  head = (codeBuffer.buffer[0]<<8) + codeBuffer.buffer[1];
  length1 = (codeBuffer.buffer[4]<<8) + codeBuffer.buffer[5];
  length2 = (codeBuffer.buffer[6]<<8) + codeBuffer.buffer[7];
  
  if ((head == 0x5000 || head == 0x6000))
  {
    if (codeLength != 4)
      return IRProfileCodeLengthDoNotConformeToHeaders;
  }
  else if (head == 0x0000)
  {
    // length1+length2 représente de nombre de mots de 16 bits.
    if (codeLength != (length1+length2)*4)
    {
      return IRProfileCodeLengthDoNotConformeToHeaders;
    }
        
    if (length1+length2 > 200)
      return IRProfileCodeTooLong;
  }
  else
    return IRProfileCodeHeaderIsInvalide;
  
  return IRProfileCodeIsValide;
}
-(BOOL) isUsable
{
  if ([self validateIrCodeForType:IRProfileOkCode] != IRProfileCodeIsValide)
    return NO;
  if ([self validateIrCodeForType:IRProfileRightArrowCode] != IRProfileCodeIsValide)
    return NO;
  if ([self validateIrCodeForType:IRProfileDownArrowCode] != IRProfileCodeIsValide)
    return NO;
  if ([self validateIrCodeForType:IRProfileNextChapterCode] != IRProfileCodeIsValide)
    return NO;
  
  return YES;
}
@end
