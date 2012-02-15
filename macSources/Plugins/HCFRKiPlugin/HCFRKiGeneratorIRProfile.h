//  ColorHCFR
//  HCFRKiGeneratorIRProfile.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/01/08.
//
//  $Rev: 69 $
//  $Date: 2008-07-22 09:59:08 +0100 (Tue, 22 Jul 2008) $
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
#import <HCFRPlugins/HCFRUtilities.h>
#import "HCFRKiDevice.h"

typedef enum
{
  IRProfileOkCode = 0,
  IRProfileRightArrowCode = 1,
  IRProfileDownArrowCode = 2,
  IRProfileNextChapterCode = 3
} IRProfileCodeTypes;
typedef enum
{
  IRProfileMenuNavigationDelay = 0,
  IRProfileMenuValidationDelay = 1,
  IRProfileChapterNavigationDelay = 2
} IRProfileDelayTypes;
typedef enum
{
  IRProfileCodeHeaderIsIncomplete,
  IRProfileCodeHeaderIsInvalide,
  IRProfileCodeLengthDoNotConformeToHeaders,
  IRProfileCodeTooLong,
  IRProfileCodeIsValide
} IRProfileCodeErrorType;
/*!
    @class
    @abstract    Décrit un profile infra-rouge utilisé par le kiGenerator
    @discussion  Le profile est constitué de
 - un nom
 - un code IR pour la commande "chapitre suivant"
 - un code IR pour la commande "flèche bas"
 - un code IR pour la commande "flèche droite"
 - un code IR pour la commande "OK"
 - une description
*/
@interface HCFRKiGeneratorIRProfile : NSObject {
  NSString  *name;
  NSString  *description;
  NSString  *author;
  
  int               delays [3];
  IRCodeBuffer      codes [4];
}
-(NSString*) name;
-(void) setName:(NSString*)newName;
-(NSString*) description;
-(void) setDescription:(NSString*)newDescription;
-(NSString*) author;
-(void) setAuthor:(NSString*)newAuthor;

#pragma mark accesseurs pour les codes
-(NSString*)codeAsStringForType:(IRProfileCodeTypes)type;
-(IRCodeBuffer)codeBufferForType:(IRProfileCodeTypes)type;
-(void) setCode:(NSString*)newCode forType:(IRProfileCodeTypes)type;
-(void) setCodeBuffer:(char*)buffer codeSize:(unsigned int)codeSize forType:(IRProfileCodeTypes)type;

#pragma mark accesseurs pour les delais
-(int) delayForType:(IRProfileDelayTypes)type;
-(void) setDelay:(int)newDelay forType:(IRProfileDelayTypes)type;

-(IRProfileCodeErrorType) validateIrCodeForType:(IRProfileCodeTypes)type;
/*!
    @function 
    @abstract   Indique si le profile est utilisable ou non
    @discussion Cette fonction vérifie que les différents codes IR sont valides.
                Si tous les codes sont valides, le profile est déclaré utilisable.
    @result     YES si le profile est utilisable, NO sinon.
*/
-(BOOL) isUsable;
@end
