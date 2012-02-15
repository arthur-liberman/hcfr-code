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

#include "IHCFile.h"
#include "PCFilesReaderUtilities.h"
#include "Exceptions.h"
#include "Endianness.h"

IHCFile::IHCFile ()
{
  nextCode = NULL;
  okCode = NULL;
  downCode = NULL;
  rightCode = NULL;
  
  menuNavigationLatency = 0;
  menuValidationLatency = 0;
  chapterNavigationLatency = 0;
}
IHCFile::~IHCFile ()
{
  if (nextCode != NULL)
    delete nextCode;
  if (okCode != NULL)
    delete okCode;
  if (downCode != NULL)
    delete downCode;
  if (rightCode != NULL)
    delete rightCode;
}

void IHCFile::readFile (const char* path)
{
  ifstream file(path);
  
  if (!file.is_open())
  {
    cout << "LibHCFR : CHCFile::readFile : Could not open file" << endl;
    throw Exception("Unable to open file");
  }
  
	uint32_t version = 0;
    
  file.read((char*)&version, 4);
  version = littleEndianUint32ToHost(version);
  
  if(version > 1)
  {
    cout << "LibHCFR : IHCFile::readFile : unreadable format version : " << version << endl;
    throw Exception("Cannot read this file version (yet)");
  }
  
  // on saute le boolean sur 4 octets (!!!) qui indique si on est en read only
  file.seekg(4, ios::cur);
  
  // puis on lit les différents codes
  nextCode = readCString (file);
  okCode = readCString (file);
  downCode = readCString (file);  
  rightCode = readCString (file);
  
  file.read((char*)&menuNavigationLatency, 4);
  menuNavigationLatency = littleEndianUint32ToHost(menuNavigationLatency);
  
  file.read((char*)&menuValidationLatency, 4);
  menuValidationLatency = littleEndianUint32ToHost(menuValidationLatency);

  file.read((char*)&chapterNavigationLatency, 4);
  chapterNavigationLatency = littleEndianUint32ToHost(chapterNavigationLatency);

  file.close();
}

void IHCFile::writeFile (const char* path)
{
  ofstream file(path);
  
  if (!file.is_open())
  {
    cout << "LibHCFR : CHCFile::writeFile : Could not open file" << endl;
    throw Exception("Unable to open file");
  }
  
  // le numéro de version du fichier
  uint32_t intValue = 1;
  intValue = hostUint32TolittleEndian(intValue);
  file.write((char*)&intValue, sizeof(uint32_t));
  
  // le read only, toujours à false (0)
  intValue = 0;
  file.write((char*)&intValue, sizeof(uint32_t));
  
  // les codes
  if (nextCode != NULL)
    writeCString (file, nextCode);
  else
    writeCString (file, "");
  
  if (okCode != NULL)
  {
    writeCString (file, okCode);
  }
  else
    writeCString (file, "");
  
  if (downCode != NULL)
    writeCString (file, downCode);
  else
    writeCString (file, "");
  
  if (rightCode != NULL)
    writeCString (file, rightCode);
  else
    writeCString (file, "");
  
  // les délais
  uint32_t delayToWrite;
  delayToWrite = hostUint32TolittleEndian(menuNavigationLatency);
  file.write((char*)&delayToWrite, sizeof(uint32_t));  

  delayToWrite = hostUint32TolittleEndian(menuValidationLatency);
  file.write((char*)&delayToWrite, sizeof(uint32_t));  

  delayToWrite = hostUint32TolittleEndian(chapterNavigationLatency);
  file.write((char*)&delayToWrite, sizeof(uint32_t));  

  file.close();
}

void IHCFile::setNextCodeString (const char* code)
{
  uint32_t length = strlen(code);
  if (nextCode != NULL)
    delete (nextCode);
  nextCode = new char[length+1];
  strncpy (nextCode, code, length);
  nextCode[length] = '\0';
}
char* IHCFile::getNextCodeString ()
{
  return nextCode;
}
void IHCFile::setOkCodeString (const char* code)
{
  uint32_t length = strlen(code);
  if (okCode != NULL)
    delete (okCode);
  okCode = new char[length+1];
  strncpy (okCode, code, length);
  okCode[length] = '\0';
}
char* IHCFile::getOkCodeString ()
{
  return okCode;
}
void IHCFile::setDownCodeString (const char* code)
{
  uint32_t length = strlen(code);
  if (downCode != NULL)
    delete (downCode);
  downCode = new char[length+1];
  strncpy (downCode, code, length);
  downCode[length] = '\0';
}
char* IHCFile::getDownCodeString ()
{
  return downCode;
}
void IHCFile::setRightCodeString (const char* code)
{
  uint32_t length = strlen(code);
  if (rightCode != NULL)
    delete (rightCode);
  rightCode = new char[length+1];
  strncpy (rightCode, code, length);
  rightCode[length] = '\0';
}
char* IHCFile::getRightCodeString ()
{
  return rightCode;
}
void IHCFile::setMenuNavigationLatency (uint32_t latency)
{
  menuNavigationLatency = latency;
}
uint32_t IHCFile::getMenuNavigationLatency ()
{
  return menuNavigationLatency;
}
void IHCFile::setMenuValidationLatency (uint32_t latency)
{
  menuValidationLatency = latency;
}
uint32_t IHCFile::getMenuValidationLatency ()
{
  return menuValidationLatency;
}
void IHCFile::setChapterNavigationLatency (uint32_t latency)
{
  chapterNavigationLatency = latency;
}
uint32_t IHCFile::getChapterNavigationLatency ()
{
  return chapterNavigationLatency;
}