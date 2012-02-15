/*///////////////////////////////////////////////////////////////////////////
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
// d'index du repertoire en ligne des fichiers de calibration.
// Il s'agit d'un fichier flex. Flex le compilera pour gÈnerer un fichier
// .cpp, qui sera compilÈ par le compilateur c++.
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//  JÈrÙme Duquennoy
///////////////////////////////////////////////////////////////////////////*/
%{
#include "ProfilesRepositoryIndexFileReader.h"
#include <iostream>

time_t iso8601ToTime_t(const char *time_string);

ProfilesRepositoryIndexFileReader *currentObject;
%}

%option noyywrap

%%
[^\t\n]*  {currentObject->valueFound();}
\n        {currentObject->lineEnded();}
%%

ProfilesRepositoryIndexFileReader::ProfilesRepositoryIndexFileReader(const char* filePath)
{
  FILE  *fileToRead = fopen (filePath, "r");
  
  if (fileToRead == NULL)
  {
    cout << "LibHCFR->ProfilesIndexFile : Could not open file" << filePath << endl;
    throw Exception("Unable to open index file."); // lever une exception
  }

  currentObject = this;
  valueIndex = 0;
  rewind();
  
  yyin = fileToRead;
  
  yylex();
    
  fclose(fileToRead);
}

int ProfilesRepositoryIndexFileReader::size()
{
  return entries.size();
}

void ProfilesRepositoryIndexFileReader::rewind()
{
  readIndex = 0;
}

bool ProfilesRepositoryIndexFileReader::hasNext ()
{
  return !(readIndex >= entries.size());
}

ProfileFileInfos ProfilesRepositoryIndexFileReader::nextEntry()
{
  if (readIndex >= entries.size())
  {
    cout << "LibHCFR->ProfilesIndexFile : no more entries" << endl;
    throw Exception("requesting non existant value."); // lever une exception
  }

  readIndex ++;
  
  return entries[readIndex-1];
}

// Cette fonction est applÈe lorsqu'un sÈparateur est rencontrÈ
// Elle met la valeur du champ trouvÈ dans la structure en cours
// de crÈation, en fonction de la position de la valeur dans la ligne
void ProfilesRepositoryIndexFileReader::valueFound ()
{
  switch (valueIndex)
  {
    case 0:
      // on initialise l'entrÈe
      currentlyParsedEntry.fileName.assign(yytext, yyleng);
      currentlyParsedEntry.modificationDate = 0;
      currentlyParsedEntry.md5.clear();
      break;
    case 1:
      currentlyParsedEntry.modificationDate = iso8601ToTime_t(yytext);
      break;
    case 2:
      currentlyParsedEntry.md5.assign(yytext, yyleng);
      break;  
  }
  valueIndex++;
}

// Cette fonction est applÈe lorsqu'une fin de ligne est rencontrÈe.
// Elle met la structure parsÈe dans le vecteur contenant les rÈsultats.
void ProfilesRepositoryIndexFileReader::lineEnded ()
{
  // l'entrÈe n'est valide que si on a chargÈ exactement 3 valeurs
  if (valueIndex == 3)
  {
    entries.push_back(currentlyParsedEntry);
  }
    
  valueIndex = 0;
}

// converti une chaine de caractËres terminÈe par un \0
// en timestamp unix.
time_t iso8601ToTime_t(const char *time_string)
{
  tzset();

  struct tm timeStructure;
  memset(&timeStructure, 0, sizeof(struct tm));

  strptime(time_string, "%FT%T%z", &timeStructure);

  return mktime(&timeStructure);
}