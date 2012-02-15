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
// Ce fichier contiend les routines necessaires ? la lecture et ?
// l'Ècriture d'un fichier de calibration ColorHCFR.
// Les donnÈes sont stoquÈes dans une structure spÈcifique HCFRCalibrationData.
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//  JÈrÙme Duquennoy
/////////////////////////////////////////////////////////////////////////////

#ifndef __CALIBRATION_FILE_H__
#define __CALIBRATION_FILE_H__

#include "Matrix.h"
#include <time.h>

typedef struct
{
  unsigned int  matrixVersion;
  unsigned int  sensorClassVersion;
  unsigned int  oneDeviceSensorClassVersion;
  unsigned int  calibrationInfoClassVersion;
  
  float         calibrationIRELevel;
  bool          doBlackCompensation;
  bool          verifyAdditivity;
  bool          showAdditivityResult;
  unsigned int  maxAdditifityError;

  time_t        calibrationDate;

  Matrix        sensorToXYZMatrix;
  Matrix        primariesChromacities;
  Matrix        whiteChromacity;
  
  char          *calibrationReferenceName;

  Matrix        measuresMatrix;
  Matrix        referencesMatrix;
  Matrix        whiteTestMatrix;
  Matrix        blackTestMatrix;
  
  char          *calibrationAuthor;
} HCFRCalibrationData;

HCFRCalibrationData readCalibrationFile (const char* filePath);

#endif