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

#ifndef __IHC_FILE_H__
#define __IHC_FILE_H__

#include "Endianness.h"
#include "Color.h"
#include <time.h>
#include <list>

using namespace std;

class IHCFile
{
private :
  char  *nextCode;
  char  *okCode;
  char  *downCode;
  char  *rightCode;
  
  uint32_t   menuNavigationLatency;
  uint32_t   menuValidationLatency;
  uint32_t   chapterNavigationLatency;
  
public :
  IHCFile();
  ~IHCFile();
  
  // Attention, ces fonctions sont susceptible de lever une exception en cas de problème.
  void readFile (const char* path);
  void writeFile (const char* path);

  void  setNextCodeString (const char* code);
  char* getNextCodeString ();
  void  setOkCodeString (const char* code);
  char* getOkCodeString ();
  void  setDownCodeString (const char* code);
  char* getDownCodeString ();
  void  setRightCodeString (const char* code);
  char* getRightCodeString ();
  
  void      setMenuNavigationLatency (uint32_t latency);
  uint32_t  getMenuNavigationLatency ();
  void      setMenuValidationLatency (uint32_t latency);
  uint32_t  getMenuValidationLatency ();
  void      setChapterNavigationLatency (uint32_t latency);
  uint32_t  getChapterNavigationLatency ();
};

#endif