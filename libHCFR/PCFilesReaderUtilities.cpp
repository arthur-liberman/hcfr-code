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

#include "PCFilesReaderUtilities.h"
#include "endianness.h"

char* readCString (ifstream &file)
{
  // la lecture d'une CString se fait en deux étapes :
  // - lire la taille de la chaine de char
  // - lire la chaine de char
  uint32_t  stringSize;
  
  unsigned char charBuffer;
  file.read((char*)&charBuffer, 1);
  if (charBuffer == 0xFF)
  {
    unsigned short shortBuffer;
    file.read((char*)&shortBuffer, 2);
    shortBuffer = littleEndianUShortToHost(shortBuffer);
    if (shortBuffer == 0xFFFF)
    {
      uint32_t intBuffer;
      file.read((char*)&intBuffer, 4);
      intBuffer = littleEndianUIntToHost(intBuffer);
      stringSize = intBuffer;
    }
    else
      stringSize = shortBuffer;
  }
  else
    stringSize = charBuffer;
  char  *string = new char[stringSize+1];
  file.read(string, stringSize);
  string[stringSize] = '\0';
  
  return string;
}

void writeCString (ofstream &file, const char *string)
{
  // on écrit la taille de la chaine
  uint32_t  stringSize = strlen(string);
  if (stringSize < 0xFF)
  {
    char actualSize = stringSize & 0xFF;
    file.write(&actualSize, 1);
  }
  else if (stringSize < 0xFFFF)
  {
    char marker = (char)0xFF;
    file.write(&marker, 1);
    uint16_t actualSize = stringSize & 0xFFFF;
    actualSize = hostUint16TolittleEndian(actualSize);
    file.write((char*)&actualSize, sizeof(uint16_t));
  }
  else
  {
    char marker[3] = {(char)0xFF, (char)0xFF, (char)0xFF};
    file.write(marker, 3);
    uint32_t actualSize = hostUint32TolittleEndian(stringSize);
    file.write((char*)&actualSize, sizeof(uint32_t));
  }
  
  // on écrit les données
  file.write(string, stringSize);
}
