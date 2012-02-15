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
// Ce fichier contiend les routines necessaires ‡ la lecture du fichier
// d'index du repertoire en ligne des fichiers de calibration
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//  JÈrÙme Duquennoy
/////////////////////////////////////////////////////////////////////////////

#ifndef __PROFILES_REPOSITOTY_INDEX_FILE_READER_H__
#define __PROFILES_REPOSITOTY_INDEX_FILE_READER_H__

#include <time.h>
#include <string>
#include <vector>

#include "Exceptions.h"

using namespace std;

typedef struct
{
  string  fileName;
  time_t  modificationDate;
  string  md5;
} ProfileFileInfos;

class ProfilesRepositoryIndexFileReader
{
  int                           valueIndex;
  ProfileFileInfos              currentlyParsedEntry;
  vector<ProfileFileInfos>      entries;
  int                           readIndex;

  // les fonction pour le parser  
  public:
    ProfilesRepositoryIndexFileReader(const char* filePath);
    void valueFound ();
    void lineEnded ();

  // les fonctions pour le parcours des entrÈes

  // retourne le nombre d'Èlements disponibles
  int size();

  // Cette fonction remet le pointeur de lecture sur la premiËre entrÈe.
  void rewind();

  // indique si d'autre ÈlÈments sont dispo.
  // Si cette fonction retourne false, un appel
  // ‡ nextEntry lËvera une exception.  
  bool hasNext ();

  // Cette fonction retourne la structure CalibrationFileInfos
  // suivante, ou lËve une exception si il n'y ‡ plus d'entrÈes ‡ lire.
  ProfileFileInfos  nextEntry();
};

//vector<CalibrationFileInfos> readRepositoryIndexFile (const char* filePath);

#endif