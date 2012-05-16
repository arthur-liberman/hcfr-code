//  ColorHCFR
//  HCFRKiSensor.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 15/09/07.
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

#import "HCFRKiSensor.h"
#import "HCFRKiDeviceStatusListener.h"

#pragma mark Les clé des préférences
#define kHCFRKiSensorFastMeasureMode      @"kiSensorFastMeasureMode"
#define kHCFRKiSensorSensorToUse          @"kiSensorSensorToUse"
#define kHCFRKiSensorInterleaveLevel      @"kiSensorInterleaveLevel"

@implementation HCFRKiSensor
-(HCFRKiSensor*)init
{
  [super init];

  NSDictionary *defaults = [NSDictionary dictionaryWithObjectsAndKeys:
                                          [NSNumber numberWithBool:NO], kHCFRKiSensorFastMeasureMode,
                                          [NSNumber numberWithChar:kHCFRKiSensorInterleaveNone], kHCFRKiSensorInterleaveLevel,
                                          [NSNumber numberWithChar:kHCFRKiSensorUseSensor1], kHCFRKiSensorSensorToUse,
                                          nil];
  [[NSUserDefaults standardUserDefaults] registerDefaults:defaults];
  
  // On charge le nib
  [NSBundle loadNibNamed:@"KiSensor" owner:self];
  
  // Pour les deux popups "sensor to use" et "interleave",
  // on n'utilise pas les bindings, qui sont peu pratique vu que les items des
  // popups sont complétés logicielement.

  // Pour le capteur à utiliser, on attend d'avoir un capteur connecté,
  // vu que ça dépendra de si la sonde à 1 ou 2 capteurs.
  [self updateSensorToUsePopupForHardwareVersion:@""];
  
  // on remplie le popup "average measures" en logiciel
  // pour profiter des constants et éviter les redondances
  [interleavedMeasuresPopup removeAllItems];
  NSMenuItem  *newItem;
  newItem = [[NSMenuItem alloc] init];
  [newItem setTitle:HCFRLocalizedString(@"No", @"No")];
  [newItem setTag:kHCFRKiSensorInterleaveNone];
  [[interleavedMeasuresPopup menu] addItem:newItem];
  [newItem release];
  // ---
  newItem = [[NSMenuItem alloc] init];
  [newItem setTitle:@"2"];
  [newItem setTag:kHCFRKiSensorInterleaveTwoLevels];
  [[interleavedMeasuresPopup menu] addItem:newItem];
  [newItem release];
  // ---
  newItem = [[NSMenuItem alloc] init];
  [newItem setTitle:@"4"];
  [newItem setTag:kHCFRKiSensorInterleaveFourLevels];
  [[interleavedMeasuresPopup menu] addItem:newItem];
  [newItem release];
  
  [interleavedMeasuresPopup selectItemWithTag:[[NSUserDefaults standardUserDefaults] integerForKey:kHCFRKiSensorInterleaveLevel]];
  
  // on recharge les options
  [self optionChanged:self];
  
  // et on récupère un accesseur au périphérique
  kiDevice = nil;
  
  return self;
}
-(void) dealloc
{
  [super dealloc];
}

-(void) refresh
{
  [softwareVersionTextField setStringValue:[kiDevice loadSoftwareSensorVersion]];
  NSString  *hardwareVersion = [kiDevice loadHardwareSensorVersion];
  [hardwareVersionTextField setStringValue:hardwareVersion];
  [self updateSensorToUsePopupForHardwareVersion:hardwareVersion];
}

-(void) updateSensorToUsePopupForHardwareVersion:(NSString*)hardwareVersion
{
  char defaultSensorToUse = (char)[[NSUserDefaults standardUserDefaults] integerForKey:kHCFRKiSensorSensorToUse];
  
  // on met à jour le popup des capteurs en fonction du nombre reel de capteurs
  [sensorToUsePopup removeAllItems];
  NSMenuItem  *newItem;
  // si on n'a que un capteur, on ne propose pas le choix
  if ([hardwareVersion hasSuffix:@"1"])
  {
    newItem = [[NSMenuItem alloc] init];
    [newItem setTitle:@"sensor 1"];
    [newItem setTag:kHCFRKiSensorUseSensor1];
    [[sensorToUsePopup menu] addItem:newItem];
    [newItem release];
    
    [sensorToUsePopup setEnabled:YES];
  }
  // si on a deux capteurs
  else if ([hardwareVersion hasSuffix:@"2"])
  {
    newItem = [[NSMenuItem alloc] init];
    [newItem setTitle:HCFRLocalizedString(@"sensor 1",@"sensor 1")];
    [newItem setTag:kHCFRKiSensorUseSensor1];
    [[sensorToUsePopup menu] addItem:newItem];
    [newItem release];
    //---
    newItem = [[NSMenuItem alloc] init];
    [newItem setTitle:HCFRLocalizedString(@"sensor 2",@"sensor 2")];
    [newItem setTag:kHCFRKiSensorUseSensor2];
    [[sensorToUsePopup menu] addItem:newItem];
    [newItem release];
    //---
    newItem = [[NSMenuItem alloc] init];
    [newItem setTitle:HCFRLocalizedString(@"both sensors",@"both sensors")];
    [newItem setTag:kHCFRKiSensorUseSensorBoth];
    [[sensorToUsePopup menu] addItem:newItem];
    [newItem release];
    
    [sensorToUsePopup setEnabled:YES];
  }
  // sinon
  else
  {
    newItem = [[NSMenuItem alloc] init];
    [newItem setTitle:@"..."];
    [newItem setTag:defaultSensorToUse];
    [[sensorToUsePopup menu] addItem:newItem];
    [newItem release];
    [sensorToUsePopup setEnabled:NO];
  }
  [sensorToUsePopup selectItemWithTag:defaultSensorToUse];
  [self optionChanged:sensorToUsePopup];
  NSLog (@"sensors (refresh) = %d", [[NSUserDefaults standardUserDefaults] integerForKey:kHCFRKiSensorSensorToUse]);
}
#pragma mark IBactions
-(IBAction) optionChanged:(id)sender
{
  if (sender == sensorToUsePopup)
  {
    [[NSUserDefaults standardUserDefaults] setInteger:[sensorToUsePopup selectedTag] forKey:kHCFRKiSensorSensorToUse];
    NSLog (@"sensors = %d", [[NSUserDefaults standardUserDefaults] integerForKey:kHCFRKiSensorSensorToUse]);
  }
  else if (sender == interleavedMeasuresPopup)
  {
    [[NSUserDefaults standardUserDefaults] setInteger:[interleavedMeasuresPopup selectedTag] forKey:kHCFRKiSensorInterleaveLevel];
    NSLog (@"interleave = %d", [[NSUserDefaults standardUserDefaults] integerForKey:kHCFRKiSensorInterleaveLevel]);
  }
}

#pragma mark fonction divers
+(NSString*) sensorName
{
  return HCFRLocalizedString(@"HCFR Sensor", @"HCFR Sensor");
}
+(NSString*) sensorId;
{
  return @"homecinema-fr.sensors.kiSensor";
}

#pragma mark sauvegarde/chargement des préférences
-(NSDictionary*) getPreferencesDictionary
{
  // on refait le même dictionnaire que pour les valeurs par defaut de NSUserDefaults
  // ainsi, on pourra utilise NSUserDefaults pour charger les prefs par la suite
  NSUserDefaults  *defaults = [NSUserDefaults standardUserDefaults];
  NSDictionary    *defaultsDict = [NSDictionary dictionaryWithObjectsAndKeys:
                                        [defaults objectForKey:kHCFRKiSensorFastMeasureMode], kHCFRKiSensorFastMeasureMode,
//                                        [defaults objectForKey:kHCFRKiSensorPerformeWhiteMeasure], kHCFRKiSensorPerformeWhiteMeasure,
//                                        [defaults objectForKey:kHCFRKiSensorPerformeRGBMeasure], kHCFRKiSensorPerformeRGBMeasure,
                                        [defaults objectForKey:kHCFRKiSensorSensorToUse], kHCFRKiSensorSensorToUse,
                                        nil];
    
  return defaultsDict;
}
-(void) setPreferencesWithDictionary:(NSDictionary*)preferences
{
  [[NSUserDefaults standardUserDefaults] registerDefaults:preferences];
  
  // pour la compatibilité avec les versions 1.0, qui utilisait la clé @"fastMeasure"
  if ([[NSUserDefaults standardUserDefaults] objectForKey:@"fastMeasure"] != nil)
  {
    NSString    *stringValue = [preferences objectForKey:@"fastMeasure"];
    BOOL        value = ([stringValue caseInsensitiveCompare:@"yes"] == NSOrderedSame);
    [[NSUserDefaults standardUserDefaults] setBool:value
                                            forKey:kHCFRKiSensorFastMeasureMode];

  }
}
#pragma mark Gestion du capteur
-(void) activate
{
  if (kiDevice != nil)
  {
    NSLog(@"KiSensor:activate : sensor is beeing activated twice. This should not be, please check your code.");
    return;
  }
  // on se connecte au capteur
  kiDevice = [[HCFRKiDevice sharedDevice] retain];
  [kiDevice addListener:self];
  [self refresh];
}
-(void) deactivate
{
  if (kiDevice != nil)
  {
    [kiDevice removeListener:self];
    [kiDevice release];
    kiDevice = nil;
  }
  else {
    NSLog(@"KiSensor:deactivate : sensor is not active. Double deactivation may indicate a bug.");
  }

}
-(BOOL) isAvailable
{
  return [kiDevice isReady];
}
-(BOOL) needsCalibration
{
  return YES;
}

#pragma mark Gestion du panneau de configuration
-(BOOL) hasASetupView
{
  return YES;
}
-(NSView*) setupView
{
  return setupView;
}

#pragma mark autre
-(HCFRColor*) currentMeasure
{
  if (kiDevice == nil)
    [[NSException exceptionWithName:kHCFRSensorFailureException
                             reason:HCFRLocalizedString(@"The sensor has not been activated.",@"The sensor has not been activated.")
                           userInfo:nil] raise];
  
  if (![kiDevice isReady])
    [[NSException exceptionWithName:kHCFRSensorFailureException
                             reason:HCFRLocalizedString(@"The sensor is unavailable.",@"The sensor is unavailable.")
                           userInfo:nil] raise];
  
  char      options = 0;
//  ssize_t   returnCode = 0;
  
  if (![[NSUserDefaults standardUserDefaults] boolForKey:kHCFRKiSensorFastMeasureMode])
    options = options | kHCFRKiSensorMeasureModeNormal;
  else
    options = options | kHCFRKiSensorMeasureModeFast;
  options = options | (char)[[NSUserDefaults standardUserDefaults] integerForKey:kHCFRKiSensorSensorToUse];
  options = options | (char)[[NSUserDefaults standardUserDefaults] integerForKey:kHCFRKiSensorInterleaveLevel];

  char            readBuffer[255];//="RGB_0:012004000091249250012007000091244022007248000091252197008171000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
  
  int returnCode = [kiDevice readRGBMeasureToBuffer:readBuffer bufferSize:255
                                     withOptions:options];

  // Problème : on n'arrive pas à obtenir de mesure ...
  // on lève une exception pour le signaler
  if (returnCode == -1)
    [[NSException exceptionWithName:kHCFRSensorFailureException
                             reason:HCFRLocalizedString(@"No response received from the sensor.",
                                                      @"No response received from the sensor.")
                           userInfo:nil] raise];
  
  // il reste à décoder cette lecture.
  unsigned int  colors[3] = {0, 0, 0};
  [self decodeKiAnswer:readBuffer toBuffer:colors];
  
  Matrix colorsMatrix(0.0, 3, 1);
  colorsMatrix[0][0] = (float)colors[0];
  colorsMatrix[1][0] = (float)colors[1];
  colorsMatrix[2][0] = (float)colors[2];
  
//  NSLog (@"HCFRKiSensor : read values : R=%f G=%f B=%f", colorsMatrix[0][0], colorsMatrix[1][0], colorsMatrix[2][0]);
  Matrix adjustedColorsMatrix([self getSensorToXYZMatrix] * colorsMatrix);
  HCFRColor *measure = [[HCFRColor alloc] initWithMatrix:adjustedColorsMatrix];
  return [measure autorelease];
}

// Le code de cette fonction est en très grande partie repris de la version PC.
// exemple de code de retour
// RGB_0:012 004 000 091 249 250 012 007 000 091 244 022 007 248 000 091 252 197 008 171 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000
//              |   durée R 1   | p R 1 |   durée G 1   | p G 1 |   durée B 1   | p B 1 |   durée W 1   | p W 1 |   durée R 2   | p R 2 |   durée G 2   | p G 2 |   durée B 2   | p B 2 |   durée W 2   | p W 2 |

-(void) decodeKiAnswer:(char*)answer toBuffer:(unsigned int[])RGB
{
  int sensorUsed = [[NSUserDefaults standardUserDefaults] integerForKey:kHCFRKiSensorSensorToUse];
	RGB[0]= RGB[1]= RGB[2]= 0;

	if ( strlen(answer) >= 156 )
	{
		int intRGB[48];
		int	nDiv, nMul;
		int nSensor;
		char dummyStr[4];
		
		sscanf(answer, "%4s%1d:%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d",
           dummyStr, & nSensor, & nDiv, & nMul,
           &intRGB[0],  &intRGB[1],  &intRGB[2],  &intRGB[3],  &intRGB[4],  &intRGB[5], 
           &intRGB[6],  &intRGB[7],  &intRGB[8],  &intRGB[9],  &intRGB[10], &intRGB[11], 
           &intRGB[12], &intRGB[13], &intRGB[14], &intRGB[15], &intRGB[16], &intRGB[17],
           &intRGB[18], &intRGB[19], &intRGB[20], &intRGB[21], &intRGB[22], &intRGB[23], 
           &intRGB[24], &intRGB[25], &intRGB[26], &intRGB[27], &intRGB[28], &intRGB[29], 
           &intRGB[30], &intRGB[31], &intRGB[32], &intRGB[33], &intRGB[34], &intRGB[35], 
           &intRGB[36], &intRGB[37], &intRGB[38], &intRGB[39], &intRGB[40], &intRGB[41], 
           &intRGB[42], &intRGB[43], &intRGB[44], &intRGB[45], &intRGB[46], &intRGB[47]);
		
		int color= 0;
		for(int hue= 0; hue <3; hue++) 
		{
      // la mesure pour une couleur est : (nb_periodes * multiplicateur)/(durée * diviseur)
      // si on utilise deux capteurs, on fait la moyenne de deux.
			if ( nSensor > 1 && (sensorUsed == kHCFRKiSensorUseSensorBoth) )
			{
        unsigned int numberOfPeriodesSensor1 = (( intRGB[color+4] << 8 ) + intRGB[color+5] ) * nMul;
        float durationSensor1 = (float)( ( intRGB[color+0] << 24 ) + ( intRGB[color+1] << 16 ) + ( intRGB[color+2] << 8 ) + intRGB[color+3] ) / nDiv;

        unsigned int numberOfPeriodesSensor2 = (( intRGB[color+24+4] << 8 ) + intRGB[color+24+5] ) * nMul;
        float durationSensor2 = (float)( ( intRGB[color+24+0] << 24 ) + ( intRGB[color+24+1] << 16 ) + ( intRGB[color+24+2] << 8 ) + intRGB[color+24+3] ) / nDiv;

				RGB[hue] = (int)( 1000000.0 * ((numberOfPeriodesSensor1 /	durationSensor1) + (numberOfPeriodesSensor2 /	durationSensor2)) / 2.0);
			}
			else
			{
        unsigned int numberOfPeriodesSensor1 = (( intRGB[color+4] << 8 ) + intRGB[color+5] ) * nMul;
        float durationSensor1 = (float)( ( intRGB[color+0] << 24 ) + ( intRGB[color+1] << 16 ) + ( intRGB[color+2] << 8 ) + intRGB[color+3] ) / nDiv;
				RGB[hue] = (int)( 1000000 * (numberOfPeriodesSensor1 /	durationSensor1));
			}
			color+= 6;
		}
	}
  
}

#pragma mark kiDeviceListener
-(void) kiSensorAvailabilityChanged
{
//  NSLog (@"ki sensor availability changed !");
  [self refresh];
}
@end