//  ColorHCFR
//  HCFRCIEChartView.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 04/09/07.
//
//  $Rev: 134 $
//  $Date: 2011-02-23 22:44:34 +0000 (Wed, 23 Feb 2011) $
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

#import <Cocoa/Cocoa.h>
#import <HCFRPlugins/HCFRColorReference.h>
#import <HCFRPlugins/HCFRColor.h>

/*!
  @class
  @abstract    Vue représentant le diagramme CIE
  @discussion  Cette vue calcule et affiche de diagramme CIE.
 Le calcul de l'image étant assez lourd, il est fait lors de l'instancaition, puis stockée dans une image.
 L'image est ensuite réutilisée lorsqu'il faut raffraichir la vue, ce qui est très rapide.
 Elle peut néanmoins être recalculée si les paramètres changent (coordonnées UV ou XY par exemple).
*/
@interface HCFRCIEChartView : NSView {
  NSBitmapImageRep    *xyBackgroundImageRep;
  NSBitmapImageRep    *uvBackgroundImageRep;
  HCFRColorReference  *reference;
  
  HCFRColor           *redComponent, *greenComponent, *blueComponent, *cyanComponent, *magentaComponent, *yellowComponent;
  
  bool                useUVCoordinates;
  bool                showBlackBodyCurve;
  bool                showGrid;

  NSLock              *backgroundCreationLock;
  NSProgressIndicator *progressIndicator;
  
  bool                abortBackgroundCalculation; // pour interrompre la thread de calcule de l'image de background
  
}
/*!
    @function 
    @abstract   Initialise la vue
    @discussion Cette initialiseur crée l'image du diagramme CIE avec la taille de la frame fournie, et en utilisant
 la couleur de référence fournie.
    @param      frame La frame de la nouvelle instance. L'image CIE sera calculée avec la même taille que cette frame.
*/
- (HCFRCIEChartView*)initWithFrame:(NSRect)frame colorReference:(HCFRColorReference*)reference useUVCoordinates:(bool)useUV;
/*!
 @function 
 @abstract   Crée une image représentant le diagramme CIE
 @discussion Cette fonction crée une instance de NSBitmapImageRep, un contenant le diagrame CIE
 en fonction des paramètres actuels.
 */
- (NSBitmapImageRep*)createBackgroundRepForCoordinates:(BOOL)drawForUVCoordinates;

  /*!
  @function 
   @abstract   Change l'espace colorimétrique.
   @discussion Change l'espace colorimétrique pour lequel le diagramme est tracé. L'appel à cette fonction
   provoquera le recalcul de l'image CIE.
   @param      newReference Le nouvel espace colorimétrique.
   */
-(void) setReference:(HCFRColorReference*)newReference;

#pragma mark accesseurs
-(void) setShowGrid:(bool)newValue;
-(void) setShowBlackBodyCurve:(bool)newValue;
/*!
  @function 
   @abstract   Change les coordonnées du graphique.
   @discussion Permet de choisir si on trace le diagramme en coordonnées UV ou XY.
   Un appel à cette fonction provoquera le recalcul de l'image CIE.
   @param      uvCoordinates Si ce paramètre est à YES, les coordonnées UV seront utilisées.
   Sinon, on utilisera les coordonnées XY.
   */
-(void) setCoordinates:(bool)uvCoordinates;
/*!
    @function 
    @abstract   Change la valeur de la composante rouge à afficher
    @discussion Cette fonction change la valeur de la composante rouge. Si la nouvelle valeur est nulle,
 la composante est effacée.
    @param      newRedComponent Une instance de HCFRColor représentant la composante rouge, ou nil pour effacer la composante actuelle.
*/
-(void) setRedComponent:(HCFRColor*)newRedComponent;
  /*!
  @function 
   @abstract   Change la valeur de la composante verte à afficher
   @discussion Cette fonction change la valeur de la composante verte. Si la nouvelle valeur est nulle,
   la composante est effacée.
   @param      newRedComponent Une instance de HCFRColor représentant la composante rouge, ou nil pour effacer la composante actuelle.
   */
-(void) setGreenComponent:(HCFRColor*)newGreenComponent;
  /*!
  @function 
   @abstract   Change la valeur de la composante bleue à afficher
   @discussion Cette fonction change la valeur de la composante bleue. Si la nouvelle valeur est nulle,
   la composante est effacée.
   @param      newRedComponent Une instance de HCFRColor représentant la composante bleue, ou nil pour effacer la composante actuelle.
   */
-(void) setBlueComponent:(HCFRColor*)newBlueComponent;
  /*!
  @function 
   @abstract   Change la valeur de la composante cyan à afficher
   @discussion Cette fonction change la valeur de la composante cyan. Si la nouvelle valeur est nulle,
   la composante est effacée.
   @param      newRedComponent Une instance de HCFRColor représentant la composante bleue, ou nil pour effacer la composante actuelle.
   */
-(void) setCyanComponent:(HCFRColor*)newCyanComponent;
  /*!
  @function 
   @abstract   Change la valeur de la composante jaune à afficher
   @discussion Cette fonction change la valeur de la composante jaune. Si la nouvelle valeur est nulle,
   la composante est effacée.
   @param      newRedComponent Une instance de HCFRColor représentant la composante bleue, ou nil pour effacer la composante actuelle.
   */
-(void) setMagentaComponent:(HCFRColor*)newMagentaComponent;
-(void) setYellowComponent:(HCFRColor*)newYellowComponent;
#pragma mark Les fonctions de traduction couleur <-> coordonnées
/*
 @function 
 @abstract   Retourne les coodronnées xy d'affichage d'une couleur
 @discussion Cette fonction prend en entrée une couleur, et retourne en sortie les coodronnées XY correpondantes dans le graph.
 @param      color La couleur à analyser.
 */
-(NSPoint) colorToCoordinates:(CColor)color;
/*
   @function 
   @abstract   Retourne les coodronnées xy d'affichage d'une couleur dont les coordonnées XY sont passées sous forme de point.
   @discussion Cette fonction prend en entrée un point represant la couleur (XY), et retourne en sortie un point contenant les coordonnées sur le graph.
   @param      color La couleur à analyser.
   */
-(NSPoint) colorPointToCIECoordienates:(NSPoint)xyPoint;
/*!
  @function 
   @abstract  Converti les coordonnées CIE en coordonnées pixels
   @discution Converti des coordonnées UV ou XY en coordonnées pixels.
   En mode xy, les maximums sont (0.8, 0.9)
   En mode uv, les maximums sont (0.7, 0.7)
   */
-(NSPoint) CIEToPixel:(NSPoint)inputPoint forFrame:(NSRect)frame forCoordinates:(BOOL)UVCoordinates;
/*!
  @function 
   @abstract  Converti les coordonnées pixels en coordonnées CIE XY
   @discution Converti des coordonnées pixels en coordonnées CIE XY.
   Si on est en mode UV, les coordonnées en pixels sont converties en UV, puis traduites en XY
   afin de pouvoir appliquer les matrices xy->rvb. Quoi qu'il en soit, les valeurs obtenues sont en coordonnées xy.
   */
-(NSPoint) pixelToCIE:(NSPoint)inputPoint forFrame:(NSRect)frame forCoordinates:(BOOL)UVCoordinates;
@end
