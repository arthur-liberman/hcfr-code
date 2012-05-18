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
// Ce fichier contiend les routines necessaires à la lecture
// de fichiers enregistrés par la version PC de colorHCFR.
// Seul la lecture des mesures est prévue. Les mesures sont retournées
// sour forme de list (std::list) d'objets CColor.
// Un accesseur est fournis pour chaque type de données
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//  Jérôme Duquennoy
/////////////////////////////////////////////////////////////////////////////

#include "CHCFile.h"
#include "PCFilesReaderUtilities.h"
#include "Exceptions.h"
#include "Endianness.h"

CHCFile::CHCFile ()
{
  
}
CHCFile::~CHCFile ()
{
  
}

void CHCFile::readFile (const char* path)
{
  ifstream file(path);
  
  if (!file.is_open())
  {
    cout << "LibHCFR : CHCFile::readFile : Could not open file" << endl;
    throw Exception("Unable to open file");
  }
  
  uint32_t header1;
	uint32_t header2;
	uint32_t version = 0;
    
  file.read((char*)&header1, 4);
  header1 = littleEndianUint32ToHost(header1);
  file.read((char*)&header2, 4);
  header2 = littleEndianUint32ToHost(header2);
  file.read((char*)&version, 4);
  version = littleEndianUint32ToHost(version);
  
  if( header1 != 0x4F4C4F43 || header2 != 0x46434852 )
  {
    cout << "LibHCFR : CHCFile::readFile : header not recognised" << endl;
    throw Exception("Invalid file format");
  }
  
  if(version > 3)
  {
    cout << "LibHCFR : CHCFile::readFile : unreadable format version : " << version << endl;
    throw Exception("Cannot read this file version (yet)");
  }
  
  // encore une version, mais ce n'est pas la même.
  file.read((char*)&version, 4);
  version = littleEndianUint32ToHost(version);
  if(version > 6)
  {
    cout << "LibHCFR : CHCFile::readCalibrationFile : unreadable measures format version : " << version << endl;
    throw Exception("Cannot read this file version (yet)");
  }
  
  // maintenant, les "vrai" données
  uint32_t  arraySize; // sera utilisé pour lire la taille des tableaux de Color
  uint32_t  loopIndex; // pour les boucles de lecture

  // les niveaux de gris
  file.read((char*)&arraySize, 4);
  arraySize = littleEndianUint32ToHost(arraySize);
  for (loopIndex = 0; loopIndex < arraySize; loopIndex ++)
  {
    CColor newColor (file);
    if (newColor.isValid())
      grayColors.push_back(newColor);
  }

  // les proximité du blanc et du noir, en version > 2 uniquement
  if (version > 2)
  {
    // proximité du noir
    file.read((char*)&arraySize, 4);
    arraySize = littleEndianUint32ToHost(arraySize);
    for (loopIndex = 0; loopIndex < arraySize; loopIndex ++)
    {
      CColor newColor (file);
      if (newColor.isValid())
        nearBlackColors.push_back(newColor);
    }

    // proximité du blanc
    file.read((char*)&arraySize, 4);
    arraySize = littleEndianUint32ToHost(arraySize);
    for (loopIndex = 0; loopIndex < arraySize; loopIndex ++)
    {
      CColor newColor (file);
      if (newColor.isValid())
        nearWhiteColors.push_back(newColor);
    }
  }
  
  // saturations à partir de la version 2
  if (version > 1)
  {
    // saturation rouge
    file.read((char*)&arraySize, 4);
    arraySize = littleEndianUint32ToHost(arraySize);
    for (loopIndex = 0; loopIndex < arraySize; loopIndex ++)
    {
      CColor newColor (file);
      if (newColor.isValid())
        redSaturationColors.push_back(newColor);
    }

    // saturation vert
    file.read((char*)&arraySize, 4);
    arraySize = littleEndianUint32ToHost(arraySize);
    for (loopIndex = 0; loopIndex < arraySize; loopIndex ++)
    {
      CColor newColor (file);
      if (newColor.isValid())
        greenSaturationColors.push_back(newColor);
    }

    // saturation bleu
    file.read((char*)&arraySize, 4);
    arraySize = littleEndianUint32ToHost(arraySize);
    for (loopIndex = 0; loopIndex < arraySize; loopIndex ++)
    {
      CColor newColor (file);
      if (newColor.isValid())
        blueSaturationColors.push_back(newColor);
    }

    // saturation jaune
    file.read((char*)&arraySize, 4);
    arraySize = littleEndianUint32ToHost(arraySize);
    for (loopIndex = 0; loopIndex < arraySize; loopIndex ++)
    {
      CColor newColor (file);
      if (newColor.isValid())
        yellowSaturationColors.push_back(newColor);
    }

    // saturation cyan
    file.read((char*)&arraySize, 4);
    arraySize = littleEndianUint32ToHost(arraySize);
    for (loopIndex = 0; loopIndex < arraySize; loopIndex ++)
    {
      CColor newColor (file);
      if (newColor.isValid())
        cyanSaturationColors.push_back(newColor);
    }

    // saturation magenta
    file.read((char*)&arraySize, 4);
    arraySize = littleEndianUint32ToHost(arraySize);
    for (loopIndex = 0; loopIndex < arraySize; loopIndex ++)
    {
      CColor newColor (file);
      if (newColor.isValid())
        magentaSaturationColors.push_back(newColor);
    }
  }
  
  // mesures libres
  file.read((char*)&arraySize, 4);
  arraySize = littleEndianUint32ToHost(arraySize);
  for (loopIndex = 0; loopIndex < arraySize; loopIndex ++)
  {
    CColor newColor (file);
    if (newColor.isValid())
      freeMeasuresColors.push_back(newColor);
  }
  
  // les composantes primaires et secondaires
  arraySize = 6;
  for (loopIndex = 0; loopIndex < arraySize; loopIndex ++)
  {
    CColor newColor (file);
    if (newColor.isValid())
      componnentsColors.push_back(newColor);
  }
  
  // le contrat plein écran
  arraySize = 2;
  for (loopIndex = 0; loopIndex < arraySize; loopIndex ++)
  {
    CColor newColor (file);
    if (newColor.isValid())
      fullscreenContrastColors.push_back(newColor);
  }

  // le contrat ANSI
  arraySize = 2;
  for (loopIndex = 0; loopIndex < arraySize; loopIndex ++)
  {
    CColor newColor (file);
    if (newColor.isValid())
      ansiContrastColors.push_back(newColor);
  }
  
  file.close();
}

list<CColor>& CHCFile::getGrayColors ()
{
  return grayColors;
}
list<CColor>& CHCFile::getNearBlackColors ()
{
  return nearBlackColors;
}
list<CColor>& CHCFile::getNearWhiteColors ()
{
  return nearWhiteColors;
}
list<CColor>& CHCFile::getRedSaturationColors ()
{
  return redSaturationColors;
}
list<CColor>& CHCFile::getGreenSaturationColors ()
{
  return greenSaturationColors;
}
list<CColor>& CHCFile::getBlueSaturationColors ()
{
  return blueSaturationColors;
}
list<CColor>& CHCFile::getCyanSaturationColors ()
{
  return cyanSaturationColors;
}
list<CColor>& CHCFile::getMagentaSaturationColors ()
{
  return magentaSaturationColors;
}
list<CColor>& CHCFile::getYellowSaturationColors ()
{
  return yellowSaturationColors;
}
list<CColor>& CHCFile::getFreeMeasuresColors ()
{
  return freeMeasuresColors;
}
list<CColor>& CHCFile::getComponnentsColors ()
{
  return componnentsColors;
}
list<CColor>& CHCFile::getFullscreenContrastColors ()
{
  return fullscreenContrastColors;
}
list<CColor>& CHCFile::getANSIContrastColors ()
{
  return ansiContrastColors;
}
