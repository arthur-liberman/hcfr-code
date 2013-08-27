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

#ifndef __CHC_FILE_H__
#define __CHC_FILE_H__

#include "Color.h"
#include <time.h>
#include <list>

using namespace std;

class CHCFile
{
private :
  list<CColor>    grayColors; // la liste contenant les mesures de niveau de gris
  list<CColor>    nearBlackColors; // la liste contenant les mesures de niveau de gris a proximité du noir
  list<CColor>    nearWhiteColors; // la liste contenant les mesures de niveau de gris a proximité du blanc
  list<CColor>    redSaturationColors; // la liste contenant les mesures de saturation rouge
  list<CColor>    greenSaturationColors; // la liste contenant les mesures de saturation vert
  list<CColor>    blueSaturationColors; // la liste contenant les mesures de saturation bleu
  list<CColor>    cyanSaturationColors; // la liste contenant les mesures de saturation cyan
  list<CColor>    magentaSaturationColors; // la liste contenant les mesures de saturation magenta
  list<CColor>    cc24SaturationColors; // la liste contenant les mesures de saturation magenta
  list<CColor>    yellowSaturationColors; // la liste contenant les mesures de saturation yellow
  list<CColor>    freeMeasuresColors; // la liste contenant les mesures libres
  list<CColor>    componnentsColors; // la liste contenant les composantes dans l'ordre suivant :
                                     // rouge, vert, bleu, cyan, magenta, jaune
  list<CColor>    fullscreenContrastColors; // la liste contenant les mesures de contrast dans l'ordre suivant :
                                             // ON OFF
  list<CColor>    ansiContrastColors; // la liste contenant les mesures de contrast dans l'ordre suivant :
                                             // ON OFF

  
public :
  CHCFile();
  ~CHCFile();
  
  // Attention, cette fonction est susceptible de lever une exception en cas de problème.
  void readFile (const char* path);
  
  list<CColor>& getGrayColors ();
  list<CColor>& getNearBlackColors ();
  list<CColor>& getNearWhiteColors ();
  list<CColor>& getRedSaturationColors ();
  list<CColor>& getGreenSaturationColors ();
  list<CColor>& getBlueSaturationColors ();
  list<CColor>& getCyanSaturationColors ();
  list<CColor>& getMagentaSaturationColors ();
  list<CColor>& getCC24SaturationColors ();
  list<CColor>& getYellowSaturationColors ();
  list<CColor>& getFreeMeasuresColors ();
  list<CColor>& getComponnentsColors ();
  list<CColor>& getFullscreenContrastColors ();
  list<CColor>& getANSIContrastColors ();
};

#endif