//  ColorHCFR
//  HCFRSensorUpdater.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 29/07/08.
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

#import <IOKit/IOKitLib.h>
#import <IOKit/IOCFPlugIn.h>

#import "HCFRSensorUpdater.h"

#define kVendorID 0x04D8
#define kProductID 0x000B

// PIC USB commands in boot mode
typedef enum
{
  READ_VERSION    = 0x00,
  READ_FLASH      = 0x01,
  WRITE_FLASH     = 0x02,
  ERASE_FLASH     = 0x03,
  READ_EEDATA     = 0x04,
  WRITE_EEDATA    = 0x05,
  READ_CONFIG     = 0x06,
  WRITE_CONFIG    = 0x07,
  UPDATE_LED      = 0x32,
  RESET           = 0xFF
} PicUSBCommand;

typedef enum
{
  connectSensorStep,
  selectFileStep,
  updatingStep,
  SuccessStep,
  failureStep
} UpdaterStep;

// usb callback functions and utilities
/*!
    @function 
    @abstract   Callback appelé lors de la connexion d'un périphérique USB
*/
void HCFRSensorUpdaterDeviceAdded(void * refCon, io_iterator_t iterator);
/*!
  @function 
  @abstract   Callback appelé lors de la déconnexion d'un périphérique USB
*/
void HCFRSensorUpdaterDeviceRemoved(void * refCon, io_iterator_t iterator);

@interface HCFRSensorUpdater (private)
// mark Gestion USB
/*!
    @function 
    @abstract   Enregistre l'objet actuel pour reçevoir des notification lors de la connexion/déconnexion de périphériques USB
    @result     YES si l'enregistrement est OK, NO sinon
*/
-(BOOL) registerForUSBEvents;
/*!
    @function 
    @abstract   Supprime l'abonnement aux notifications lors de la connexion/Déconnexion de périphériques USB
*/
-(void) unregisterForUSBEvents;
/*!
    @function 
    @abstract   Action appelée par le callback lors de la connexion d'un périphérique
    @param      newDevice Le périphérique connecté
*/
-(void) deviceAdded:(IOUSBDeviceInterface **)newDevice;
/*!
    @function 
    @abstract   Ouverture de l'interface qui permettra d'envoyer des données au périphérique
    @result     YES si l'ouverture à pu se faire, NO sinon
*/
-(BOOL) openDeviceInterface;
/*!
    @function 
    @abstract   Fermeture de l'interface avec le périphérique courrant.
*/
-(void) closeDeviceInterface;
/*!
    @function 
    @abstract   Action appelée par le callback lors de la déconnexion d'un périphérique
    @discussion Cette fonction appelera automatiquement closeDeviceInterface avant de détruire de device. 
*/
-(void) deviceRemoved;

// Fonctions d'accès à la sonde
/*!
    @function 
    @abstract   Efface la mémoire de la sonde à l'adresse indiquée.
    @param      nbPages  Le nombre de pages de 64 bits à effacer
    @param      address  L'adresse de début de la page a effacer
*/
-(BOOL) erase:(int)nbPages rowsAt:(unsigned int)address;
/*!
    @function 
    @abstract   Ecrit les données fournis à l'adresse indiquée
    @discussion Si l'adresse n'est pas un multiple de 16, les octets entre le multiple de 16 inferieur et cette adresse
 reçevront 0xFF.
    @param      size La taille du buffer à écrire. Si la taille n'est pas multiple de 16, le buffer sera complété jusqu'au prochain
 multiple avec des 0xFF.
    @param      buffer Les données à envoyer
    @param      address L'adresse à laquelle les données doivent être envoyées. de préférence un multiple de 16.
    @result     YES en cas de succès, NO en cas d'echec.
*/
-(BOOL) write:(int)size bytesFrom:(byte*)buffer toAddress:(unsigned int)address;
/*!
    @function 
    @abstract   Enverra une commande de reset à la sonde, ce qui aura pour effet de la faire sortir du mode "load" et d'executer
 le firmware actuellement programmé.
 @result     YES en cas de succès, NO en cas d'echec.
*/
-(BOOL) resetDevice;

/*!
    @function 
    @abstract   Affiche l'étape souhaitée, en redimentionnant la fenêtre si nécessaire.
*/
-(void) displayStep:(UpdaterStep)stepIndex;

/*!
 @function 
 @abstract   Callback du filePanel
 */
-(void)filePanelDidEnd:(NSOpenPanel*)panel
            returnCode:(int)returnCode
           contextInfo:(void*)contextInfo;
@end

@implementation HCFRSensorUpdater
- (HCFRSensorUpdater*) init {
  self = [super init];
  if (self != nil) {  
    // on charge le nib
    [NSBundle loadNibNamed:@"KiSensorUpdaterTool" owner:self];
    
    [mainWindow center];
    
    device_added_iterator = 0;
    device_removed_iterator = 0;
    notify_port = NULL;
    device = NULL;
    interface = NULL;
    
    record = Nil;
    
    toolStarted = NO;
  }
  return self;
}

- (void) dealloc {
  if (record != Nil)
  {
    [record release];
    record = Nil;
  }
  [self unregisterForUSBEvents];
  [self deviceRemoved]; // pour fermer l'interface et le périphérique
  [super dealloc];
}

#pragma mark Implementation des fonctions de HCFRTool
+(NSString*) toolName {
  return HCFRLocalizedString(@"HCFR sensor updater", @"HCFR sensor updater");
}

-(void) startTool
{
  if (toolStarted)
    return;
  
  [step2Status setStringValue:@""];
  [filenameTextField setStringValue:@""];
  [updateButton setEnabled:NO];
  
  // on charge la première vue
  [self displayStep:connectSensorStep];

  [self registerForUSBEvents];

  // on affiche la fenêtre.
  [mainWindow makeKeyAndOrderFront:self];
  
  toolStarted = YES;
  updateDone = NO;
  updateRunning = NO;
}

#pragma mark Actions IB
- (IBAction)selectFile:(id)sender
{
  if (record != Nil)
    [record release];
  record = [[HCFRIntelHexRecord alloc] init];
  
  NSOpenPanel *openPanel = [NSOpenPanel openPanel];
  [openPanel setAllowsMultipleSelection:NO];
  
  [openPanel beginSheetForDirectory:nil
                           file:nil
                          types:[NSArray arrayWithObject:@"hex"]
                 modalForWindow:mainWindow
                  modalDelegate:self
                 didEndSelector:@selector(filePanelDidEnd:
                                          returnCode:
                                          contextInfo:)
                    contextInfo:nil];
}

-(void)filePanelDidEnd:(NSOpenPanel*)panel
            returnCode:(int)returnCode
           contextInfo:(void*)contextInfo {
  
  if (returnCode != NSOKButton)
    return;
  
  @try {
    [record loadFile:[panel filename]];
    [filenameTextField setStringValue:[[panel filename] lastPathComponent]];
    [step2Status setStringValue:@""];
    [updateButton setEnabled:YES];
  }
  @catch (NSException * e) {
    NSLog (@"Error reading Hex file : %@", [e reason]);
    if ([[e name] isEqualToString:HexRecordInvalidFormatException])
      [step2Status setStringValue:HCFRLocalizedString(@"Unreadable file : invlid format",@"Unreadable file : invlid format")];
    else if ([[e name] isEqualToString:HexRecordErroneousChecksumException])
      [step2Status setStringValue:HCFRLocalizedString(@"File is corrupted : checksum error",@"File is corrupted : checksum error")];
    else
      [step2Status setStringValue:HCFRLocalizedString(@"Could not read file (unknown error)",@"Could not read file (unknown error)")];
  }
}

- (IBAction)performUpdate:(id)sender
{
  NSAssert (device != NULL, @"Cannot update NULL device");
  NSAssert (record != Nil, @"Cannot update with Nil record");
  
  [self displayStep:updatingStep];
  [progressIndicator setMaxValue:[record pagesCount]];
  [progressIndicator setDoubleValue:0.0];

  [NSThread detachNewThreadSelector:@selector(performThreadedUpdate:) toTarget:self withObject:nil];
}

-(void) performThreadedUpdate:(id)object
{
  updateRunning = YES;
  BOOL error = NO;
  
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  // open the device interface
  if (![self openDeviceInterface])
  {
    [[NSAlert alertWithMessageText:HCFRLocalizedString(@"Unable to use the connected sensor", @"Unable to use the connected sensor")
                    defaultButton:@"Ok"
                  alternateButton:Nil
                      otherButton:Nil
        informativeTextWithFormat:HCFRLocalizedString(@"The configuration of the device failed.", @"The configuration of the device failed.")] runModal];

    error = YES;
  }
  else
  {
    NSEnumerator  *pagesEnumerator = [record pagesEnumerator];
    MemPage       *currentPage;
    int           counter = 0;
    while (currentPage = [pagesEnumerator nextObject])
    {
      counter ++;
      [self erase:1 rowsAt:[currentPage getBaseAddress]];
      if (![self write:64 bytesFrom:[currentPage getData] toAddress:[currentPage getBaseAddress]])
      {
        NSLog (@"Error writing to device");
        error = YES;
        break;
      }
      [progressIndicator setDoubleValue:counter];
    }
    updateDone = YES;
    
    // le reset provoquera un appel à la fonction "deviceRemoved", qui s'occupera de faire place nette
    [self resetDevice];
  }
  
  updateRunning = NO;

  if (error)
    [self displayStep:failureStep];
  else
    [self displayStep:SuccessStep];

  [pool release];
}

-(IBAction) visitForums:(id)sender
{
  [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://www.homecinema-fr.com/forum/viewforum.php?f=50"]];
}

#pragma mark Utilitaires d'interface
-(void) displayStep:(UpdaterStep)stepIndex
{
  NSView  *viewToDisplay = nil;

  switch (stepIndex)
  {
    case connectSensorStep :
      viewToDisplay = step1;
      break;
    case selectFileStep :
      viewToDisplay = step2;
      break;
    case updatingStep :
      viewToDisplay = step3;
      break;
    case SuccessStep :
      viewToDisplay = success;
      break;
    case failureStep :
      viewToDisplay = failure;
      break;
  }
  if (viewToDisplay != nil)
  {
    // on crée une vue vide pour la transition
    // sinon la vue affichée est redimentionnée, et on ne le souhaite pas.
    NSView *emptyView = [[NSView alloc] init];
    [mainWindow setContentView:emptyView];
    [emptyView release];
        
    NSRect baseFrame = [[mainWindow contentView] frame];
    NSRect targetFrame = [mainWindow frame];
    
    targetFrame.size.height = [viewToDisplay frame].size.height;
    targetFrame.size.width = [viewToDisplay frame].size.width;
    
    targetFrame = [mainWindow frameRectForContentRect:targetFrame];

    targetFrame.origin.x -= (targetFrame.size.width - baseFrame.size.width)/2.0;
    targetFrame.origin.y -= (targetFrame.size.height - baseFrame.size.height)/2.0;
    
      
    [mainWindow setFrame:targetFrame display:YES animate:YES];
    [mainWindow setContentView:viewToDisplay];
  }
}

#pragma mark Window delegate
- (BOOL)windowShouldClose:(id)window
{
  return !updateRunning;
}
- (void)windowWillClose:(NSNotification *)notification
{
  // on ne release pas l'outil :
  // le gestionnaire de plugin le conserve pour le ré-utiliser au besoin.
//  [self release];
  if (record != Nil)
  {
    [record release];
    record = Nil;
  }
  [self unregisterForUSBEvents];
  updateDone = YES; // BIDOUILLE DEGEULASSE !!!! juste pour éviter de changer la vue avant de fermer la fenêtre (cf la fonction deviceRemoved).
  [self deviceRemoved]; // pour fermer l'interface et le périphérique
  
  toolStarted = NO;
}

#pragma mark Gestion USB
// Evol rev 135 : on ne récupère le masterPort du IOKit, on utilise à place
// la constante kIOMasterPortDefault, le IOKit utilisera le master port général
-(BOOL) registerForUSBEvents
{
//  NSLog (@"updater : registering for USB events");

  // on ne peut s'enregistrer qu'une fois
  if (notify_port != NULL)
    return NO;

//  mach_port_t             masterPort;				// requires <mach/mach.h>
  // no more usefull : starting with mac os 10.2, kIOMasterPortDefault can be used instead, the iokit
  // manage to get the masterPort
  
  CFMutableDictionaryRef 	matchingDictionary;		// requires <IOKit/IOKitLib.h>

	kern_return_t		err;
	CFRunLoopSourceRef	run_loop_source;
  
	SInt32 idVendor = kVendorID;
	SInt32 idProduct = kProductID;
  
	// récupération du master port pour le dialog avec le IOKit
  // No longer needed since rev 135
//	err = IOMasterPort(MACH_PORT_NULL, &masterPort);
//  if (err)
//	{
//    NSLog (@"HCFRSensorUpdater : could not create master port, err = %08x\n", err);
//    return NO;
//  }
  
	// creation du matching dictionary
  matchingDictionary = IOServiceMatching(kIOUSBDeviceClassName);	// requires <IOKit/usb/IOUSBLib.h>
  if (!matchingDictionary)
	{
//    NSLog (@"HCFRSensorUpdater : could not create matching dictionary\n");
//    mach_port_deallocate(mach_task_self(), masterPort);
    return NO;
  }
	
	// on y rajoute l'id du vendeur
  CFNumberRef		numberRef;
  numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &idVendor);
  CFDictionaryAddValue(matchingDictionary, CFSTR(kUSBVendorID), numberRef);
  CFRelease(numberRef);
  numberRef = 0;
  
	// et l'id du produit.
  numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &idProduct);
  CFDictionaryAddValue(matchingDictionary, CFSTR(kUSBProductID), numberRef);
  CFRelease(numberRef);
  numberRef = 0;
  
	// on demande a etre avertie quand un peripherique correspondant est connecte
	// c'est assynchrone, donc on met en place un callbacke
	notify_port = IONotificationPortCreate(kIOMasterPortDefault); // on obtiend un port de notification
	run_loop_source = IONotificationPortGetRunLoopSource(notify_port); // on crée une source a partir de ce port
	CFRunLoopAddSource(CFRunLoopGetCurrent(), run_loop_source, kCFRunLoopDefaultMode); // et on ajoute la source à la runloop actuelle
  
  // chaque appel à IOServiceAddMatchingNotification consommera une référence
  // on n'a 2 appels, il nous faut donc deux référence.
  // Conclusion, on en rajoute une.
	matchingDictionary=(CFMutableDictionaryRef) CFRetain(matchingDictionary);
  
	err = IOServiceAddMatchingNotification(notify_port, kIOFirstMatchNotification, matchingDictionary, HCFRSensorUpdaterDeviceAdded, self, &device_added_iterator);
	HCFRSensorUpdaterDeviceAdded (self, device_added_iterator);
	if (err !=noErr) {
    NSLog (@"HCFRSensorUpdater: pblm lors de l'ajout de notification");
    return NO;
	}
  
	err = IOServiceAddMatchingNotification(notify_port, kIOTerminatedNotification, matchingDictionary, HCFRSensorUpdaterDeviceRemoved, self, &device_removed_iterator);
	HCFRSensorUpdaterDeviceRemoved (self, device_removed_iterator);
	if (err !=noErr) {
    NSLog (@"HCFRSensorUpdater: pblm lors de l'ajout de notification");
    return NO;
	}
	
//  mach_port_deallocate(mach_task_self(), masterPort);
	return YES;
}

-(void) unregisterForUSBEvents
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

-(void) deviceAdded:(IOUSBDeviceInterface **)newDevice
{
  // si on a déjà un device, on laisse tomber.
  if (device != NULL)
  {
    (*newDevice)->Release(newDevice);
    return;
  }
  
  device = newDevice;

  [self displayStep:selectFileStep];
}

-(BOOL) openDeviceInterface
{
  NSAssert (device != NULL, @"HCFRSensorUpdater : calling configureDevice with null device");
  
  kern_return_t	err;
  
	UInt8                           number_of_config;
	IOReturn                        result;
	IOUSBConfigurationDescriptorPtr	config_description;
  
  err = (*device)->USBDeviceOpen(device);
  if (err != kIOReturnSuccess)
	{
    NSLog (@"Unable to open device: %08x\n", err);
//    (*device)->Release(device);
//    device = NULL;
    return NO;
  }

	// on récupere le nombre de config possibles
	result = (*device)->GetNumberOfConfigurations(device, &number_of_config);
	if (result)
	{
		NSLog (@"could not get number of config for usb device");
    (*device)->USBDeviceClose(device);
		return NO;
	}
  
	result = (*device)->GetConfigurationDescriptorPtr (device, 0, &config_description);
	if (result)
	{
		NSLog (@"could not get config #0");
    (*device)->USBDeviceClose(device);
		return NO;
	}
  
	result = (*device)->SetConfiguration(device, config_description->bConfigurationValue);
	if (result)
	{
		NSLog (@"could not set number of config for usb device");
    (*device)->USBDeviceClose(device);
		return NO;
	}
  
  // on récupère l'interface d'interface
	IOReturn	kernResult;
	IOUSBFindInterfaceRequest	request;
	io_iterator_t	iterator;
	io_service_t	usbInterface;
	IOCFPlugInInterface	**plugInInterface = NULL;
	SInt32	score;
  
	request.bInterfaceClass = kIOUSBFindInterfaceDontCare;
	request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
	request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
	request.bAlternateSetting = kIOUSBFindInterfaceDontCare;
  
	(*device)->CreateInterfaceIterator(device, &request, &iterator);

  // on prend la première interface qu'on trouve.
	usbInterface = IOIteratorNext (iterator);
  
  IOCreatePlugInInterfaceForService(usbInterface, kIOUSBInterfaceUserClientTypeID,
                                                 kIOCFPlugInInterfaceID, &plugInInterface, &score);
  
  // une fois l'interface d'interface créée, on release le usbInterface et l'iterateur
  kernResult = IOObjectRelease(usbInterface);
  IOObjectRelease(iterator);
	if((kernResult != kIOReturnSuccess) || (!plugInInterface))
  {
    NSLog (@"unable to create plugin");
    (*device)->USBDeviceClose(device);
    return NO;
  }
  
  (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID),
                                              (LPVOID) &interface);
  
  (*plugInInterface)->Release(plugInInterface);
  
  kernResult = (*interface)->USBInterfaceOpen(interface);
	if((kernResult != kIOReturnSuccess))
  {
    NSLog (@"unable to open interface");
    return NO;
  }
  
	return YES;
}

-(void) closeDeviceInterface
{
  if (interface != NULL)
  {
    (*interface)->USBInterfaceClose(interface); 
    (*interface)->Release(interface);
    interface = NULL;
    (*device)->USBDeviceClose(device);
  }
}

-(void) deviceRemoved
{
  [self closeDeviceInterface];
  if (device != NULL)
  {
    (*device)->Release(device);
    device = NULL;
  }

  // si la mise à jour à été faite, la déconnexion est due au reset de la sonde
  // on ne retourne pas à la première étape, ue vue affichant le résultat de la programmation est mise en place.
  if (!updateDone)
    [self displayStep:connectSensorStep];
}

#pragma access functions
-(BOOL) erase:(int)nbPages rowsAt:(unsigned int)address
{
  NSAssert (interface != NULL, @"Sensor updater : erase called with NULL interface.");

	//Erase is done by row (64 bytes).  If the address is not a multiple 
	//of 64 bytes (low 6 bits of address are 0), then the erase fails.
	
	
	UInt8    command[5];
	IOReturn result;
	UInt8    data[1] = { 0};
	UInt32   bytesRead;
	int      numberOfLoops;
	int      loop;
		
	if ( (address & 0x3F) != 0)
		return false;
	
	//Although the protocol seems to allow for erasing more than one row at a time,
	//I have been unable to get the boot PIC to actually erase anything when using a length
	//other than 1.  It may well be a problem somewhere else in my code, but for now I will
	//erase one row at a time.
	//
	//Note - when attempting more than 1 row, I have been unable to detect any errors.  The
	//boot PIC seems to indicate that it is complete, but yet the erase didn't occur.  Maybe
	//a time delay is needed between erase commands?  Might as well just do them one row at a
	//time then.
	
	numberOfLoops = nbPages;
  
	for (loop = numberOfLoops;  loop > 0; loop--)
	{
		command[0] = ERASE_FLASH;
		command[1] = 1;					 //erase size is # of rows (64-bytes per row)
		command[2] = address & 0xFF;         //low byte of address
		command[3] = (address & 0xFF00) >>8; //high byte of address
		command[4] = (address & 0x00FF0000) >> 16; //upper bits of address
    
		address +=  64;
		
		result = (*interface)->WritePipe(interface, 1, command, 5);
		if (result != kIOReturnSuccess)
		{
			NSLog (@"ERASE_FLASH error, write to device failed\n");
      return NO;
		}
    
		//Device echos back command when done (1 byte), verify
		bytesRead = 1;
		data[0] = 0;
		result = (*interface)->ReadPipe(interface, 2, data, &bytesRead);
		if (result != kIOReturnSuccess)
		{
			NSLog (@"ERASE_FLASH error, could not read device response\n");
      return NO;
		}
    
		if (data[0] != ERASE_FLASH)
		{
			NSLog (@"ERASE_FLASH error, command not echoed\n");
      return NO;
		}
	}
	
	return YES;
}
-(BOOL) write:(int)size bytesFrom:(byte*)buffer toAddress:(unsigned int)address
{
  NSAssert (interface != NULL, @"Sensor updater : write called with NULL interface.");
  
    
  const unsigned short MAX_WRITE_SIZE = 16;
  const unsigned short BOOT_MSG_HEADER_SIZE = 5;
  
	UInt8    outMessage[MAX_WRITE_SIZE + BOOT_MSG_HEADER_SIZE];
	UInt8    index;
	IOReturn result;
	UInt8    data[1];
	UInt32   bytesRead;
	UInt32   numberOfLoops;
	UInt32   remainder;
	int      loop;
	UInt8    bytesToWrite;
	UInt32   sourceIndex;
	
	// The microchip bootloader appears to only accept addresses multiple of 16.
  // So, if the address is not, we prepend 0xFF
  UInt8 bytesToPrepend = address & 0xF;
  address = address - bytesToPrepend;
  
	//The Microchip bootloader version 1.0 will only write 16 bytes at a time.
	//The code below calculates the remainder, but the bootloader will fail to
	//write anything other than a unit of 16-bytes (seet boot.c in the Microchip
	//bootloader) so short writes must be padded out.
	numberOfLoops = (size+bytesToPrepend) / MAX_WRITE_SIZE;
	remainder     = (size+bytesToPrepend) - (numberOfLoops * MAX_WRITE_SIZE);
	
  sourceIndex = 0;
	
	for (loop = numberOfLoops; loop >=0; loop--)
	{
		
		//Write 16-bytes each pass.  On last pass, write any remaining bytes
		if (loop > 0)
			bytesToWrite = MAX_WRITE_SIZE;
		else
			bytesToWrite = remainder;
		    
		if (bytesToWrite == 0)
			break;
		
		//Set up the header - write command, size and dest address
		outMessage[0] = WRITE_FLASH;
		outMessage[1] = MAX_WRITE_SIZE;       
		outMessage[2] = address & 0xFF;
		outMessage[3] = (address & 0xFF00) >>8;
		outMessage[4] = (address & 0x00FF0000) >> 16;
    
    // if we must prepend bytes, do it
    for (index = 0; index < bytesToPrepend; index++)
    {
      outMessage[index+BOOT_MSG_HEADER_SIZE] = 0xFF;
    }
      
		//Copy the data into the message
		for(index = bytesToPrepend; index < bytesToWrite; index++)
		{
			outMessage[index+BOOT_MSG_HEADER_SIZE] = buffer[sourceIndex];
			sourceIndex++;
		}

//    NSLog (@"writing %d bytes to address : %02x%02x%02x, with %d bytes prepended", bytesToWrite, outMessage[4], outMessage[3], outMessage[2], bytesToPrepend);
//    if (bytesToWrite >= 8)
//      NSLog (@"first 8 bytes : %02x %02x %02x %02x %02x %02x %02x %02x", outMessage[BOOT_MSG_HEADER_SIZE],
//             outMessage[BOOT_MSG_HEADER_SIZE+1],
//             outMessage[BOOT_MSG_HEADER_SIZE+2],
//             outMessage[BOOT_MSG_HEADER_SIZE+3],
//             outMessage[BOOT_MSG_HEADER_SIZE+4],
//             outMessage[BOOT_MSG_HEADER_SIZE+5],
//             outMessage[BOOT_MSG_HEADER_SIZE+6],
//             outMessage[BOOT_MSG_HEADER_SIZE+7]);
    
    // bytes have been prepended, no more need to do it.
    bytesToPrepend = 0;
		
		//Since the bootloader expects 16 bytes, pad it out
		if (bytesToWrite < MAX_WRITE_SIZE)
		{
			for (index = bytesToWrite; index < MAX_WRITE_SIZE; index++)
			{
			  outMessage[index+BOOT_MSG_HEADER_SIZE] = 0xFF;	
			}
		}
		
		bytesToWrite = MAX_WRITE_SIZE;
    
		
		//Off it goes
		result = (*interface)->WritePipe(interface, 1, outMessage, BOOT_MSG_HEADER_SIZE+bytesToWrite);
    
		if (result != kIOReturnSuccess)
			return NO;
    
    
		//Device echos back command when done (1 byte), verify
		bytesRead = 1;
		data[0] = 0;
		result = (*interface)->ReadPipe(interface, 2, data, &bytesRead);
		if (result != kIOReturnSuccess)
		{
			NSLog (@"WRITE_FLASH error, failed to read result from device\n");
      return NO;
		}
    
		if (data[0] != WRITE_FLASH)
		{
			NSLog (@"WRITE_FLASH error, command not echoed back\n");
      return NO;
		}
		
		//Address of next 16-byte destination
		address += 16;
		
	}//End write loop
	
	
	//Success
	return YES;
}

-(BOOL) resetDevice
{
  NSAssert (interface != NULL, @"Sensor updater : reset called with NULL interface.");
  
  const unsigned short BOOT_MSG_HEADER_SIZE = 5;
  
	UInt8    outMessage[BOOT_MSG_HEADER_SIZE];
	IOReturn result;	
	
  //Set up the header - write command, size and dest address
  outMessage[0] = RESET;
  outMessage[1] = 0;
  outMessage[2] = 0;
  outMessage[3] = 0;
  outMessage[4] = 0;
  
  //Off it goes
  result = (*interface)->WritePipe(interface, 1, outMessage, BOOT_MSG_HEADER_SIZE);
  
  if (result != kIOReturnSuccess)
    return false;
  
  // pas la peine de chercher un echo : le périphérique à du rebooter ...
  
  return YES;
}
@end

void HCFRSensorUpdaterDeviceAdded(void * refCon, io_iterator_t iterator)
{
	io_service_t	usbDevice;
	kern_return_t	err;
	IOCFPlugInInterface         **plugInInterface = NULL;
  IOUSBDeviceInterface        **dev = NULL;
  HRESULT                     result;
  SInt32                      score;
  UInt16                      vendor;
  UInt16                      product;
  UInt16                      release;
	HCFRSensorUpdater           *updater = (HCFRSensorUpdater*)refCon;

  while (usbDevice = IOIteratorNext(iterator))
  {
    // on crée un plugin pour le device
    // ce plugin sera la connection entre l'appli et le kernel
		err = IOCreatePlugInInterfaceForService(usbDevice,
                                            kIOUSBDeviceUserClientTypeID,
                                            kIOCFPlugInInterfaceID,
                                            &plugInInterface, &score);
    // on n'a plus besoin du device
    err = IOObjectRelease(usbDevice);
    if ((kIOReturnSuccess != err) || !plugInInterface)
    {
      NSLog (@"Unable to create a plug-in (%08x)\n", err);
      continue;
    }
    
    // on crée maintenant une interface pour le plugin obtenu
    // ce plugin sera la connection entre l'appli et le kernel
    result = (*plugInInterface)->QueryInterface(plugInInterface,
                                                CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
                                                (LPVOID)&dev);
    //on n'a plus besoin du plugin
    IODestroyPlugInInterface(plugInInterface);
    if (result || !dev)
    {
      NSLog (@"Couldn't create a device interface (%08x)\n", (int) result);
      continue;
    }
    
    //Check these values for confirmation
    (*dev)->GetDeviceVendor(dev, &vendor);
    (*dev)->GetDeviceProduct(dev, &product);
    (*dev)->GetDeviceReleaseNumber(dev, &release);
    if ((vendor != kVendorID) || (product != kProductID))
    {
      (void) (*dev)->Release(dev);
      continue;
    }
		
    // On a notre interface.
    
    [updater deviceAdded:dev];
      
	 }
}

void HCFRSensorUpdaterDeviceRemoved(void * refCon, io_iterator_t iterator)
{
	HCFRSensorUpdater           *updater = (HCFRSensorUpdater*)refCon;
	kern_return_t               kr;
  io_service_t                object;
  
  while (object = IOIteratorNext(iterator))
	 {
    kr = IOObjectRelease(object);
    if (kr != kIOReturnSuccess)
    {
      NSLog (@"Couldn't release raw device object: %08x\n", kr);
      continue;
    }
    [updater deviceRemoved];
	 }
}
