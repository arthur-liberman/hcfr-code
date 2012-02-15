//  ColorHCFR
//  HCFRKiDevice.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 30/04/08.
//
//  $Rev: 103 $
//  $Date: 2008-08-28 18:39:32 +0100 (Thu, 28 Aug 2008) $
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
#import "HCFRDeviceFileAccessor.h"
#import "HCFRKiDeviceStatusListener.h"

extern const char kHCFRKiSensorUseSensor1; // pas vraiment utile, mais bon.
extern const char kHCFRKiSensorUseSensor2;
extern const char kHCFRKiSensorUseSensorBoth;

extern const char kHCFRKiSensorInterleaveNone; // pas vraiment utile, mais bon.
extern const char kHCFRKiSensorInterleaveTwoLevels;
extern const char kHCFRKiSensorInterleaveFourLevels;

extern const char kHCFRKiSensorMeasureModeNormal; // pas vraiment utile, mais bon.
extern const char kHCFRKiSensorMeasureModeFast; // on divise par deux le temps de mesure (et on réduit la précision)

extern const char kHCFRKiSensorLoadHardwareVersion; // A utiliser seul !
extern const char kHCFRKiSensorLoadSoftwareVersion; // A utiliser seul !


/*!
    @typedef 
    @abstract   Ce type decrit un code IR.
    @discussion Il contiend un buffer contenant le code IR (qui n'est pas terminé par un '\0', et la taille du code
    @field      buffer Le code IR
    @field      bufferSize La taille en octets du code
*/
typedef struct
{
  char          *buffer;
  unsigned int  bufferSize;
} IRCodeBuffer;

/*!
    @class
    @abstract    Un accesseur pour la sonde HCFR
    @discussion  Cette classe permet d'accéder aux fonctionnalités de la sonde HCFR :
                  - lecture de mesure avec certaines options
                  - envoi de codes IR pour contrôler un appareil externe
                  - lire un code IR émis par une télécommande
 Cette classe est un singleton. On obtiend l'instance partagée en appelant "sharedDevice".
 Chaque utilisateur doit retenir l'instance tant qu'il en a besoin. Tant que l'instance est retenue,
 elle est listener pour la connection et la deconnection de périphériques USB/série, et si la sonde HCFR
 est connectée, le pseudo fichier associé est ouvert.
 Lorsque tous les utilisateurs ont releasé l'instance, elle est détruite. Le pseudo-fichier est donc fermé,
 et on n'est plus listener sur les connexion déconnexions des periphériques.
*/
@interface HCFRKiDevice : NSObject {
  HCFRDeviceFileAccessor  *deviceFileAccessor;
  NSMutableArray          *listenersArray;

  io_iterator_t           device_added_iterator;
  io_iterator_t           device_removed_iterator;
  IONotificationPortRef   notify_port;
}
+ (HCFRKiDevice*)sharedDevice;

-(BOOL) isReady;

-(NSString*) loadSoftwareSensorVersion;
-(NSString*) loadHardwareSensorVersion;
-(int)readRGBMeasureToBuffer:(char*)readBuffer bufferSize:(unsigned int)bufferSize withOptions:(char)options;
-(int)sendIRCode:(IRCodeBuffer)codeBuffer;
-(int)readIRCodeToBuffer:(char*)code bufferSize:(unsigned short)bufferSize;

#pragma mark Gestion des listeners
  /*!
  @function 
   @abstract   Ajoute un listener
   @discussion Ajoute l'objet newListener commme listener.
   L'objet sera retenu jusqu'a ce qu'il soit supprimé de la liste des listeners par un appel à la fonction
   removeLister:
   @param      newListener L'objet qui doit être ajouté comme listener
   */
-(void) addListener:(NSObject<HCFRKiDeviceStatusListener>*)newListener;
  /*!
  @function 
   @abstract   Supprime un listerner pour le type donné
   @discussion Supprime l'objet oldListener de la liste des listenenrs pour les données dy type dataType.
   l'objet sera libéré à cette occasion. Si l'objet n'a pas été enregistré au préalable, la fonction
   ne fait rien.
   @param      oldListener L'objet à supprimer de la liste des listeners
   @param      dataType Le type de données pour lequel l'objet doit être retiré
   */
-(void) removeListener:(NSObject<HCFRKiDeviceStatusListener>*)oldListener;

  /*!
  @function 
   @abstract   Notifie tous les listeners que la liste des profiles à changée
   */
-(void) fireStatusChanged;
@end
