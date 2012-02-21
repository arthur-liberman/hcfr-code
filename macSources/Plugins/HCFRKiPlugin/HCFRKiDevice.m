//  ColorHCFR
//  HCFRKiDevice.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 30/04/08.
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

#include <mach/mach.h>
#include <sys/ioctl.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#if defined(MAC_OS_X_VERSION_10_3) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_3)
#include <IOKit/serial/ioss.h>
#endif
#include <IOKit/IOBSD.h>
#import <IOKit/IOCFPlugIn.h>

#import "HCFRKiDevice.h"
#import "HCFRKiGeneratorIRProfile.h"

#define kHCFROldVendorID 0x04DB
#define kHCFROldProductID 0x005B
#define kHCFRVendorID 0x04D8
#define kHCFRProductID 0xFE17

const char kHCFRKiSensorUseSensor1 = 0x00; // pas vraiment utile, mais bon.
const char kHCFRKiSensorUseSensor2 = 0x04;
const char kHCFRKiSensorUseSensorBoth = 0x08;
const char kHCFRKiSensorMeasureModeNormal = 0x00; // pas vraiment utile, mais bon.
const char kHCFRKiSensorMeasureModeFast = 0x40; // on divise par deux le temps de mesure (et on réduit la précision)

const char kHCFRKiSensorLoadHardwareVersion = 0xFE; // A utiliser seul !
const char kHCFRKiSensorLoadSoftwareVersion = 0xFF; // A utiliser seul !

const char kHCFRKiSensorInterleaveNone = 0x00; // pas vraiment utile, mais bon.
const char kHCFRKiSensorInterleaveTwoLevels = 0x10;
const char kHCFRKiSensorInterleaveFourLevels = 0x20;

// quelques constantes privées
const char kHCFRKiSensorMeasureRGB = 0x01;
const char kHCFRKiSensorMeasureWhite = 0x02;

const char kHCFRKiSensorSendIRCode = 0x80;
const char kHCFRKiSensorReadIRCode = 0x81;

io_service_t findUSBDeviceForService(io_object_t media);
bool getVidAndPid(io_service_t device, int *vid, int *pid);
void deviceAdded(void * refCon, io_iterator_t iterator);
void deviceRemoved(void * refCon, io_iterator_t iterator);

@interface HCFRKiDevice (Private)
-(void) registerAsSerialServiceListener;
-(void) unregisterAsSerialServiceListener;

// les callbacks
-(void) deviceConnectedWithPath:(NSString*)deviceBSDPath;
-(void) deviceDisconnectedWithPath:(NSString*)deviceBSDPath;

-(BOOL) connectToPseudoFileWithPath:(NSString*)filePath;
@end

@implementation HCFRKiDevice

static HCFRKiDevice *sharedDevice = nil;

#pragma mark Fonctions de gestion du singleton
+ (HCFRKiDevice*)sharedDevice
{
  NSLog (@"kiDevice getSharedDevice");
  @synchronized(self) {
    if (sharedDevice == nil) {
      [[[self alloc] init] autorelease]; // On n'alloue pas sharedDevice ici : ce sera fait par allocWithZone
    }
  }
  return sharedDevice;
}
+ (id)allocWithZone:(NSZone *)zone
{
  NSLog (@"kiDevice alloc");
  @synchronized(self) {
    if (sharedDevice == nil) {
      sharedDevice = [super allocWithZone:zone];
      return sharedDevice;  // On alloue à l'instance partagée
    }
  }
  return nil; // et si d'autre allocations surviennent, on retourne nil.
}
- (id)copyWithZone:(NSZone *)zone
{
  return self;
}
/*- (id)retain
{
  return [super retain];
}*/
/*- (unsigned)retainCount
{
  return UINT_MAX;  // Cet objet ne peut pas être releasé
}*/
- (void)release
{
  @synchronized(self) {
    unsigned count = [self retainCount];
  
    // si count = 1, c'est la reference interne, elle ne doit pas être releasée depuis l'exterieur !!
    NSAssert (count > 1, @"HCFRKiDevice : trying to release internal reference -> forbidden");

    [super release];
  
    // Si count = 2, c'est que le dernier utilisateur release -> on detruit l'objet en supprimant la ref interne
    if (count == 2)
    {
      [super release];
      sharedDevice = nil;
    }
  }
}
/*- (id)autorelease
{
  return self;
}*/


#pragma mark Fonctions d'initialisation
- (HCFRKiDevice*) init
{
  NSLog (@"kiDevice init");
  
  self = [super init];
  
  listenersArray = [[NSMutableArray alloc] init];
  device_added_iterator = 0;
  device_removed_iterator = 0;
  notify_port = NULL;
  
  // on se place len listener sur les connexion et les déconnexion de periphériques
  [self registerAsSerialServiceListener];
  
  return self;
}
- (void) dealloc {
  NSLog (@"kiDevice release");

  [listenersArray release];
  [self unregisterAsSerialServiceListener];
  
  if (deviceFileAccessor != nil)  
    [deviceFileAccessor release];
  
  [super dealloc];
}

#pragma mark Fonctions de gestion de la connexion au périphérique
-(BOOL) connectToPseudoFileWithPath:(NSString*)filePath
{
  if (deviceFileAccessor != nil) // si on a un accesseur, on n'a pas besoin de se reconnecter
  {
    return YES;
  }

  // on récupère un device accessor pour le path choisi
  // on est obligé de caster le résultat du alloc
  // car alloc retourne un type id, et le compilo considère que initWithPath est celui d'une autre classe, qui prend comme arg une NSString.
  // Ca génère un warning. Or, dans les prefs, du projet, on considère les warnings comme des erreurs ...
  deviceFileAccessor = [((HCFRDeviceFileAccessor*)[HCFRDeviceFileAccessor alloc]) initWithPath:[filePath cStringUsingEncoding:NSUTF8StringEncoding]];

  if (deviceFileAccessor != nil)
    [self fireStatusChanged];
  
  return YES;
}

#pragma mark Fonctions utilitaires
-(BOOL) isReady
{
  return (deviceFileAccessor != nil);
}

#pragma mark gestion des listeners
-(void) addListener:(NSObject<HCFRKiDeviceStatusListener>*)newListener
{
  [listenersArray addObject:newListener];
}
-(void) removeListener:(NSObject<HCFRKiDeviceStatusListener>*)oldListener
{
  [listenersArray removeObject:oldListener];
}
-(void) fireStatusChanged
{
  NSEnumerator  *enumerator = [listenersArray objectEnumerator];
  NSObject<HCFRKiDeviceStatusListener> *currentListener;
 
  while (currentListener=(NSObject<HCFRKiDeviceStatusListener>*)[enumerator nextObject])
  {
    // et pour chaque listener, on appel entryAdded:forDataType
    [currentListener kiSensorAvailabilityChanged];
  }
}

#pragma mark Fonctions d'accès au capteur
-(NSString*) loadSoftwareSensorVersion
{
  if (![self isReady])
    return @"N/A";
  
  char  commande = kHCFRKiSensorLoadSoftwareVersion;
  char  readBuffer[30];
  
  int returnCode = [deviceFileAccessor writeCommand:&commande ofSize:1
                              andReadLineToBuffer:readBuffer ofSize:30];
  if (returnCode <= 0)
    return @"N/A";
  
  return [NSString stringWithCString:readBuffer];
}
-(NSString*) loadHardwareSensorVersion
{
  if (![self isReady])
    return @"N/A";
  
  char  commande = kHCFRKiSensorLoadHardwareVersion;
  char  readBuffer[30];
  
  int returnCode = [deviceFileAccessor writeCommand:&commande ofSize:1
                              andReadLineToBuffer:readBuffer ofSize:30];
  if (returnCode <= 0)
    return @"N/A";
  
  return [NSString stringWithCString:readBuffer];
}
/*!
    @function 
    @abstract   Lire une mesure
    @discussion Cette fonction envoi une demande de mesure au capteur avec les options spécifiées
 et retourne la réponse, ou nil si le capteur ne répond pas.
    @param      buffer Un buffer alloué qui reçevra la réponse du capteur
    @param      bufferSize La taille du buffer en octets
    @param      options Les options a utiliser
    @result     Le nombre de caractères lus, ou -1 si la lecture à échouée
*/
-(int)readRGBMeasureToBuffer:(char*)readBuffer bufferSize:(unsigned int)bufferSize withOptions:(char)options
{
  if (![self isReady])
    return -1;

  options = options | kHCFRKiSensorMeasureRGB;
  
  int returnCode = [deviceFileAccessor writeCommand:&options ofSize:1
                              andReadLineToBuffer:readBuffer ofSize:bufferSize];

  return returnCode;
}
/*!
    @function 
    @abstract   Ennvoi d'une command IR
    @discussion Cette fonction envoie la commande fournie en argument à la sonde, qui l'émettra en
infra rouge pour contrôler un appareil externe.
    @param      code Le code à envoyer en IR. Il s'agit d'un tableau de charactères terminé par un caractère '\0'.
    @result     -1 en cas d'echec, une valeur positive sinon.
*/
-(int)sendIRCode:(IRCodeBuffer)codeBuffer
{
  if (![self isReady])
    return -1;

  char *command;
  unsigned short commandSize = codeBuffer.bufferSize+1;
  char readBuffer[255];
  int returnCode;

  command = calloc(commandSize, 1);
  command[0] = kHCFRKiSensorSendIRCode;
  
  // on inverse les octets, la sonde est en little endian
  int i;
  for (i = 0; i < codeBuffer.bufferSize - 1; i = i+2)
  {
    command[i+1] = codeBuffer.buffer[i+1];
    command[i+2] = codeBuffer.buffer[i];
  }
  
  // on envoie la commande
  returnCode = [deviceFileAccessor writeCommand:command ofSize:(commandSize)
                      andReadBulkAnswerToBuffer:readBuffer ofSize:255 withTimeout:200];

//  printf ("status : 0x%02x   -   nb octets reçus :%d\n", (0x0000FF&readBuffer[0]), (0x000000FF&readBuffer[1]));
  // la sonde renvoie le code IR qu'elle a reçue -> on l'affiche
  // on réordonne les données qu'on a recus dans l'ordre suivant :
  // 2 1  4 3  6 5
/*  unsigned int index = 2;
  printf("echo code ID (%d char) :\n", returnCode-2);
  while ((index+1) < returnCode)
  {
    printf("%02x%02x ", (readBuffer [index+1]&0x000000FF), (readBuffer [index]&0x000000FF));
    
    index += 2;
  }
  printf ("\n");*/
  
  return returnCode;
}
/*!
    @function 
    @abstract   Lit un code infro rouge
    @discussion Cette fonction permet d'acquerir via la sonde le code infra rouge d'une télécomande.
    @param      readBuffer Le buffer déjà alloué dans lequel sera stoquée le code reçus
    @param      bufferSize La taille du buffer
    @result     Le nombre de caractères du code, ou -1 en cas d'echec.
*/
-(int)readIRCodeToBuffer:(char*)code bufferSize:(unsigned short)bufferSize
{
  if (![self isReady])
    return -1;

  char command = kHCFRKiSensorReadIRCode;
  char answer[255];
  
  bzero(answer, 255);
  
  int returnCode = [deviceFileAccessor writeCommand:&command ofSize:1
                          andReadBulkAnswerToBuffer:answer ofSize:255 withTimeout:200];
  
  if (returnCode == -1)
    return -1;
  
  // on réordonne les données qu'on a recus dans l'ordre suivant :
  // 2 1  4 3  6 5
  unsigned int index = 0;
  while ((index+1) < (bufferSize-1) && (index+1) < returnCode)
  {
    code [index] = answer [index+1];
    code [index+1] = answer [index];
    
    index += 2;
  }
  code[index] = '\0';
  
  return returnCode;
}

#pragma mark low level USB functions
-(void) registerAsSerialServiceListener
{
  mach_port_t             masterPort;				// requires <mach/mach.h>
  CFMutableDictionaryRef 	matchingDictionary;		// requires <IOKit/IOKitLib.h>
  kern_return_t           err;
  
//  NSLog (@"kiDevice : registerAsSerialServiceListener");

	// creation du master port pour le dialog avec le IOKit
	err = IOMasterPort(MACH_PORT_NULL, &masterPort);
  if (err)
  {
//    NSLog (@"HCFRKiDevice:registerAsUSBListener : could not create master port, err = %08x\n", err);
    return;
  }
  
	// creation du matching dictionary
  matchingDictionary = IOServiceMatching(kIOSerialBSDServiceValue);//kIOUSBDeviceClassName);	// requires <IOKit/usb/IOUSBLib.h>
  if (!matchingDictionary)
  {
//    NSLog (@"HCFRKiDevice:registerAsUSBListener : could not create matching dictionary\n");
    return;
	}
  
	// on demande a etre avertie quand un peripherique correspondant est connecte
	// c'est assynchrone, donc on met en place un callback et on avertie la boucle
	CFRunLoopSourceRef      run_loop_source;

	notify_port = IONotificationPortCreate(masterPort);
	run_loop_source = IONotificationPortGetRunLoopSource(notify_port);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), run_loop_source, kCFRunLoopDefaultMode);
  
	matchingDictionary=(CFMutableDictionaryRef) CFRetain(matchingDictionary); // la fonction addMatchingNotification consomme une référence
	err = IOServiceAddMatchingNotification(notify_port, kIOFirstMatchNotification, matchingDictionary, deviceAdded, self, &device_added_iterator);
	deviceAdded (self, device_added_iterator);
	if (err !=noErr) {
//    NSLog (@"HCFRKiDevice:registerAsUSBListener: Could not add matching notification");
    CFRelease(matchingDictionary); // release our reference
    return;
	}
  
	matchingDictionary=(CFMutableDictionaryRef) CFRetain(matchingDictionary); // la fonction addMatchingNotification consomme une référence
	err = IOServiceAddMatchingNotification(notify_port, kIOTerminatedNotification, matchingDictionary, deviceRemoved, self, &device_removed_iterator);
	deviceRemoved (NULL, device_removed_iterator);
	if (err !=noErr) {
//    NSLog (@"HCFRKiDevice:registerAsUSBListener: could not create matching notification");
    CFRelease(matchingDictionary); // release our reference
    return;
	}
  
  mach_port_deallocate(mach_task_self(), masterPort);
  CFRelease(matchingDictionary); // release our reference
}

-(void) unregisterAsSerialServiceListener
{
  if (device_added_iterator != 0)
  {
    IOObjectRelease(device_added_iterator);
    device_added_iterator = 0;
  }
  if (device_removed_iterator != 0)
  {
    IOObjectRelease(device_removed_iterator);
    device_removed_iterator = 0;
  }
  
  if (notify_port != NULL)
  {
    IONotificationPortDestroy(notify_port);
    notify_port = NULL;
  }
}

#pragma mark USB callbacks
-(void) deviceConnectedWithPath:(NSString*)deviceBSDPath
{
//  NSLog (@"device connectedWithPath : %@", deviceBSDPath);
  [self connectToPseudoFileWithPath:deviceBSDPath];
}
-(void) deviceDisconnectedWithPath:(NSString*)deviceBSDPath
{
  // si on est connecté à un fichier
  if (deviceFileAccessor != nil)
  {
    // et que c'est le peripherique associé à ce fichier qui à été enlevé,
    // alors on se déconnecte.
    if (strcmp ([deviceBSDPath cStringUsingEncoding:NSUnicodeStringEncoding], [deviceFileAccessor filePath]) == 0)
    {
      [deviceFileAccessor release];
      deviceFileAccessor = nil;
      [self fireStatusChanged];
    }
  }
}
@end

// recherche le periphérique USB associé à un serialService
io_service_t findUSBDeviceForService(io_object_t media)
{
	IOReturn status = kIOReturnSuccess;
	
	io_iterator_t		iterator = 0;
	io_service_t 		retService = 0;
	
	if (media == 0)
		return retService;
  
	status = IORegistryEntryCreateIterator(media,
                                         kIOServicePlane, (kIORegistryIterateParents |
                                                           kIORegistryIterateRecursively), &iterator); 
	if (iterator == 0) {
		status = kIOReturnError;
	}
	
	if (status == kIOReturnSuccess)
	{
		io_service_t service = IOIteratorNext(iterator);
		while (service)
		{
			io_name_t serviceName;
			kern_return_t kr =
        IORegistryEntryGetNameInPlane(service,
                                      kIOServicePlane, serviceName);
			if ((kr == 0) && (IOObjectConformsTo(service,
                                           "IOUSBDevice"))) {
				retService = service;
				break;
			}
			service = IOIteratorNext(iterator);
		}
	}
	return retService;
}

bool getVidAndPid(io_service_t device, int *vid, int *pid)
{
	bool success = false;
	
	CFNumberRef	cfVendorId = (CFNumberRef)IORegistryEntryCreateCFProperty(device,
                                                 CFSTR("idVendor"), kCFAllocatorDefault, 0);
	if (cfVendorId && (CFGetTypeID(cfVendorId) == CFNumberGetTypeID()))
	{
		Boolean result;
		result = CFNumberGetValue(cfVendorId, kCFNumberSInt32Type, vid);
		if (result)
		{
			CFNumberRef	cfProductId = (CFNumberRef)IORegistryEntryCreateCFProperty(device,
                                                   CFSTR("idProduct"), kCFAllocatorDefault, 0);
			if (cfProductId && (CFGetTypeID(cfProductId) == CFNumberGetTypeID()))
			{
				Boolean result;
				result = CFNumberGetValue(cfProductId, kCFNumberSInt32Type, pid);
				if (result)
				{
					success = true;
				}
			}
      if (cfProductId)
        CFRelease(cfProductId);
		}
	}
  if (cfVendorId)
    CFRelease(cfVendorId);
	return (success);
}

void deviceAdded(void * refCon, io_iterator_t iterator)
{
  io_object_t addedDevice;
  CFTypeRef devicePathAsCFString = nil;
  
//  NSLog (@"kiDevice : device added");
  
//  NSLog (@"deviceAdded called, running through devices");
	while (addedDevice = IOIteratorNext(iterator))
  {
//    NSLog (@"processing device ...");

    // on vérifie que le service série correspond bien à un périphérique USB
    io_service_t usbDevice;
    usbDevice = findUSBDeviceForService(addedDevice);
    if (usbDevice == 0)
    {
//      NSLog (@"Device is not a USB one -> skipping");
      continue;
    }
  
    // si c'est le cas, on essaye de récupérer son VID et son PID
    int deviceVid, devicePid;
    if (!getVidAndPid(usbDevice, &deviceVid, &devicePid))
    {
//      NSLog (@"Unable to get Vid and Pid for device -> skipping");
      continue;
    }
    
    // on vérifie que les VID/PID correspondent à ceux de la sonde HCFR
    if ( (deviceVid != kHCFROldVendorID || devicePid != kHCFROldProductID) &&
         (deviceVid != kHCFRVendorID || devicePid != kHCFRProductID) )
    {
//      NSLog (@"Device is not a HCFR sensor (VID/PID = %04X/%04X) -> skipping", deviceVid, devicePid);
      continue;
    }
    
    // maintenant, on est sûr que on travail bien avec une sonde HCFR
    // on concerve le path BSD du periphérique trouvé (si on en a pas déjà un), mais on itère pour vider
    //l'itérateur (necessaire au bon fonctionnement du callbackk
    // Si on trouve une deuxième sonde HCFR, on l'ignore.
    if (devicePathAsCFString == nil)
    {
      devicePathAsCFString = IORegistryEntryCreateCFProperty(addedDevice, CFSTR(kIOCalloutDeviceKey),
                                                                  kCFAllocatorDefault,0);    
      NSLog (@"HCFR sensor found at path %@", devicePathAsCFString);
    }
  }

  // si le path trouvé n'est pas null, alors on signale la connexion,
  // puis on libère la CFString.
  if (devicePathAsCFString != nil)
  {
    [(HCFRKiDevice*)refCon deviceConnectedWithPath:(NSString*)devicePathAsCFString];
    CFRelease(devicePathAsCFString);
  }
}

void deviceRemoved(void * refCon, io_iterator_t iterator)
{
  io_object_t   addedDevice;
  CFTypeRef     devicePathAsCFString = nil;
  
	while (addedDevice = IOIteratorNext(iterator))
  {
    // on vérifie que le service série correspond bien à un périphérique USB
    io_service_t usbDevice;
    usbDevice = findUSBDeviceForService(addedDevice);
    if (usbDevice == 0)
    {
      // NSLog (@"Device at path %@ is not a USB one -> skipping", deviceNameAsCFString);
      continue;
    }
    
    // si c'est le cas, on essaye de récupérer son VID et son PID
    int deviceVid, devicePid;
    if (!getVidAndPid(usbDevice, &deviceVid, &devicePid))
    {
      // NSLog (@"Unable to get Vid and Pid for device at path %@ -> skipping", deviceNameAsCFString);
      continue;
    }
    
    // on vérifie que les VID/PID correspondent à ceux de la sonde HCFR
    if ( (deviceVid != kHCFROldVendorID || devicePid != kHCFROldProductID) &&
         (deviceVid != kHCFRVendorID || devicePid != kHCFRProductID) )
    {
      // NSLog (@"Device at path %@ is not a HCFR sensor (VID/PID = %04X/%04X) -> skipping", deviceNameAsCFString, deviceVid, devicePid);
      continue;
    }
    
    // maintenant, on est sûr que on travail bien avec une sonde HCFR
    // on concerve le path BSD du periphérique trouvé si c'est le premier, mais on itère pour vider
    //l'itérateur (necessaire au bon fonctionnement du callbackk
    // On ignore les sondes HCFR suivantes si il  y en a plusieurs (cas fort peu probable).
    if (devicePathAsCFString != nil)
      devicePathAsCFString = IORegistryEntryCreateCFProperty(addedDevice, CFSTR(kIOCalloutDeviceKey),
                                                             kCFAllocatorDefault,0);    
  }
  
  // si on a trouvé un capteur déconnecté, on le signal à la classe gestionnaire.
  if (devicePathAsCFString != nil)
  {
//    NSLog (@"serial device disconnected : calling obj-c callback of %@", refCon);
    [(HCFRKiDevice*)refCon deviceDisconnectedWithPath:(NSString*)devicePathAsCFString];
    CFRelease(devicePathAsCFString);
  }
}