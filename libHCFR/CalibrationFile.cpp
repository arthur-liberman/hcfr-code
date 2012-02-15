/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2007 Association Homecinema Francophone.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.htm. If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////
// Ce fichier contiend les routines necessaires à la lecture et à
// l'écriture d'un fichier de calibration ColorHCFR.
// Les données sont stoquées dans une structure spécifique HCFRCalibrationData.
//
// !!!!!!! Le code dans ce fichier considère que un short est sur 16 bits,
// !!!!!!! un entier ou un float sur 32 bits et un double sur 64 bits.
// !!!!!!! C'est dégeulasse, il faudrait utiliser des des types de taille
// !!!!!!! définis pour éviter les problèmes inter-plateforme.
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//  Jérôme Duquennoy
/////////////////////////////////////////////////////////////////////////////

#include "endianness.h"
#include "PCFilesReaderUtilities.h"
#include "CalibrationFile.h"
#include "Exceptions.h"
#include <assert.h>
#include <iostream>

using namespace std;

HCFRCalibrationData readCalibrationFile (const char* filePath)
{
  // variables utilisées lors de la lecture
  unsigned int readIntValue; // pour lire les entiers, et les boolean (qui sont
                             // également stockés comme des entiers sur 4 octets)

  HCFRCalibrationData   result;
  ifstream              calibrationFile(filePath);
  
//  cout << "LibHCFR->readCalibrationFile : reading from file " << filePath << endl;

  if (!calibrationFile.is_open())
  {
    cout << "LibHCFR->readCalibrationFile : Could not open file" << endl;
    throw Exception("Unable to open calibration file."); // lever une exception
  }
  
  // la matrice de conversion
//  result.sensorToXYZMatrix = Matrix(0.0,3,3);
//  readDoubleMatrix(result.sensorToXYZMatrix, 3, 3, calibrationFile);
  result.sensorToXYZMatrix = Matrix(calibrationFile);
  cout << endl << "sensor to xyz :" << endl;
  result.sensorToXYZMatrix.Display();
  cout << endl;

  // on saute la version de c_Sensor
  calibrationFile.seekg(4, ios::cur);

  // la date du profile
  calibrationFile.read((char*)&(result.calibrationDate), 4);
  result.calibrationDate = littleEndianTime_TToHost(result.calibrationDate);

  // version de la classe cOneSensorDevice
  calibrationFile.read((char*)&(result.oneDeviceSensorClassVersion), 4);
  result.oneDeviceSensorClassVersion = littleEndianUIntToHost(result.oneDeviceSensorClassVersion);
  
  // IRE Level
  calibrationFile.read((char*)&(result.calibrationIRELevel), 4);
  result.calibrationIRELevel = littleEndianFloatToHost(result.calibrationIRELevel);
  
  // compensate black
  calibrationFile.read((char*)&(readIntValue), 4);
  result.doBlackCompensation = ((readIntValue >> 24) == 1);

  // verify additivity
  calibrationFile.read((char*)&(readIntValue), 4);
  result.verifyAdditivity = ((readIntValue >> 24) == 1);

  // show additivity
  calibrationFile.read((char*)&(readIntValue), 4);
  result.showAdditivityResult = ((readIntValue >> 24) == 1);
  
  // maxAdditifityError
  calibrationFile.read((char*)&(readIntValue), 4);
  result.maxAdditifityError = littleEndianUIntToHost(readIntValue);
  
  // la matrice primariesChromacities
//  result.primariesChromacities = Matrix(0.0,3,3);
//  readDoubleMatrix(result.primariesChromacities, 3, 3, calibrationFile);
  result.primariesChromacities = Matrix(calibrationFile);;
  cout << endl << "primaries chromacities :" << endl;
  result.primariesChromacities.Display();
  
  // la matrice whiteChromacity
//  result.whiteChromacity = Matrix(0.0,3,1);
//  readDoubleMatrix(result.whiteChromacity, 3, 1, calibrationFile);
  result.whiteChromacity = Matrix(calibrationFile);;
  cout << endl << "white chromacities :" << endl;
  result.whiteChromacity.Display();
    
  // calibrationReferenceName
  result.calibrationReferenceName = readCString (calibrationFile);
  
  if ((result.oneDeviceSensorClassVersion == 2) || (result.oneDeviceSensorClassVersion == 4))
  {
    // on saute la version de CCalibrationInfo
    calibrationFile.seekg(4, ios::cur);
    
    // on saute le numéro de version de la classe Matrix et la taille de la matrice
    calibrationFile.seekg(4*3, ios::cur);
    // la matrice des mesures
//    result.measuresMatrix = Matrix(0.0,3,3);
//    readDoubleMatrix(result.measuresMatrix, 3, 3, calibrationFile);
    result.measuresMatrix = Matrix(calibrationFile);
    cout << endl << "measures :" << endl;
    result.measuresMatrix.Display();

    // on saute le numéro de version de la classe Matrix et la taille de la matrice
    calibrationFile.seekg(4*3, ios::cur);
    // la matrice des mesures
//    result.referencesMatrix = Matrix(0.0,3,3);
//    readDoubleMatrix(result.referencesMatrix, 3, 3, calibrationFile);
    result.referencesMatrix = Matrix(calibrationFile);
//    cout << endl << "reference :" << endl;
//    result.referencesMatrix.Display();

    // on saute le numéro de version de la classe Matrix et la taille de la matrice
    calibrationFile.seekg(4*3, ios::cur);
    // la matrice des mesures
//    result.whiteTestMatrix = Matrix(0.0,3,1);
//    readDoubleMatrix(result.whiteTestMatrix, 3, 1, calibrationFile);
    result.whiteTestMatrix = Matrix(calibrationFile);
//    cout << endl << "white test :" << endl;
//    result.whiteTestMatrix.Display();

    // on saute le numéro de version de la classe Matrix et la taille de la matrice
    calibrationFile.seekg(4*3, ios::cur);
    // la matrice des mesures
//    result.blackTestMatrix = Matrix(0.0,3,1);
//    readDoubleMatrix(result.blackTestMatrix, 3, 1, calibrationFile);
    result.blackTestMatrix = Matrix(calibrationFile);
//    cout << endl << "black test :" << endl;
//    result.blackTestMatrix.Display();
  }
  
  // On s'arrete la, le nom du capteur n'est pas lu pour le moment.
  return result;
}

