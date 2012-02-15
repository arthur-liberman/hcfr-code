//  ColorHCFR
//  HCFRGenerator.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 30/08/07.
//
//  $Rev: 96 $
//  $Date: 2008-08-19 12:35:23 +0100 (Tue, 19 Aug 2008) $
//  $Author: jerome $
// -----------------------------------------------------------------------------
// This program is free software; you can redistribute it and/or modify it under the terms of the GNU
// General Public License as published by the Free Software Foundation; either version 2 of the License,
// or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
// the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
// Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program; if not,
// write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
// -----------------------------------------------------------------------------

//-------------------------------------------------------
// Les clefs des valeurs dans le système de préférences
// Attention, certaines de ces cléfs sont utilisées pour le binding dans Interface builder.
// Si elle sont modifiées, il faut également les modifier dans IB.
//-------------------------------------------------------
#define kHCFRLastDocumentFilepath                   @"LastDocumentFilepath"
#define kHCFRNewMeasuresWithNonEmptySerieAction     @"newMeasuresWithNonEmptySerieAction"
#define kHCFRDarkenUnusedScreens                    @"DarkenUnusedScreens"

// kHCFRDefaultGammaValue n'est pas la valeur actuelle du gamma (elle est gérée par le dataStore)
// mais la valeur à mettre par défaut au lancement du logiciel
#define kHCFRDefaultGammaValue                      @"DefaultGammaValue"

// kHCFRMeasureDelayValue : la valeur du délai avant mesure
#define kHCFRMeasureDelayValue                      @"DelayBeforeMeasureValue"
#define kHCFRPauseBeforeContrastSteps               @"PauseBeforeContrastSteps"
#define kHCFRDefaultSensorId                        @"DefaultSensorId"
#define kHCFRDefaultGeneratorId                     @"DefaultGeneratorId"
#define kHCFRDefaultColorStandard                   @"DefaultColorStandard"
//#define kHCFRDefaultUseCalibrationFile              @"DefaultUseCalibrationFile"   // plus utilisé
//#define kHCFRDefaultCalibrationFile                 @"DefaultCalibrationFile"   // plus utilisé


// les valeurs de ce type correspondent aux tags de la matrice associée de la fenêtre de preference
typedef enum
{
  askTheUserAction = 0,
  createNewSerieAction = 1,
  overwriteCurrentSerieAction = 2
} NewMeasureAction;