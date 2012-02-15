//  ColorHCFR
//  HCFRGraphController.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 26/02/09.
//
//  $Rev$
//  $Date$
//  $Author$
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


/*!
 @class
 @abstract    Classe de base pour les analyseurs de graphiques
 @discussion  Cette classe sera dérivée pour fournir pour un graphique spécifique une analyse
              permettant au logiciel de suggérer des actions à l'utilisateur.
 */
@interface HCFRGraphAnalyser {
}

/*!
 @function 
 @abstract   Résultats textuels de l'analyse
 @discussion Retourne un tableau contenant les réultats textuels de l'analyse. Si le graphique est vide,
             la fonction retournera un tableau vide.
 @result     Un tableau de NSStrings représentant le résultat de l'analyse
 */
-(NSArray*) analysisResults;
@end