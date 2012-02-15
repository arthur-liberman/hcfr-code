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

#ifndef __PCFILESREADERUTILITIES_H__
#define __PCFILESREADERUTILITIES_H__

#include "matrix.h"
#include "Color.h"

// Lit une CString depuis le fichier de calibration fourni, et retourne le pointeur sur la chaine
// de charactère résultante.
// La chaine retournée est allouée dynamiquement. Il faudra la libérer !
char* readCString (ifstream &calibrationFile);

void writeCString (ofstream &file, const char *string);
#endif