//  ColorHCFR
//  HCFRColor.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 30/08/07.
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

#import <libHCFR/Color.h>
#import <HCFRPlugins/HCFRColorReference.h>
/*!
    @class
    @abstract    Représente une couleur.
    @discussion  Cette classe sert d'interface pour la classe CColor de la librairie libHCFR.
*/
@interface HCFRColor : NSObject <NSCoding> {
  @private
    CColor  *color;
}
-(HCFRColor*) initWithMatrix:(Matrix)newMatrix;
-(HCFRColor*) initWithRGBMatrix:(Matrix)newMatrix colorReference:(HCFRColorReference*)reference;
-(HCFRColor*) initWithColor:(CColor)newColor;
-(id) initWithCoder:(NSCoder*)coder;

-(double) X;
-(double) Y;
-(double) Z;
/*!
    @function 
    @abstract   Retourne la luminance
    @discussion Cette fonction retourne une mesure de la luminance pour la couleur.
 On peut choisir de préferer la mesure fournie par un luxmètre extern si elle est disponible
 ou utiliser la valeur fournie par la sonde.
    @param      preferLuxmeterMeasureIfAvailable Si cet argument est à true (ou YES), la mesure
 du luxmètre externe sera retournée si elle est disponible. Sinon,  la mesure de la sonde sera fournie.
    @result     La mesure de luminosité demandée.
*/
-(double) luminance:(bool)preferLuxmeterMeasureIfAvailable;

/*!
  @function 
   @abstract   Retourne l'instance CColor contenant les données XYZ
   @discussion Cette fonction retourne une instance de CColor, qui contiend les données au format
 XYZ (c'est a dire l'instance interne, sans modification)
   @result     Une variable CColor.
*/
-(CColor) XYZColor;
  /*!
  @function 
   @abstract   Retourn une instance de CColor contenant les données RGB.
   @discussion 
   @result     Une variable CColor. Ce devrait être une matrice.
*/
-(CColor) RGBColorWithColorReference:(HCFRColorReference*)reference;
/*!
    @function 
    @abstract   Retourn le resultat de GetxyYValue de CColor
    @discussion Cette fonction est juste un pont pour son equivalent de la classe CColor
 (TODO : decrire le fonctionnement de cette fameuse fonction)
*/
-(CColor) xyYColor;
/*!
  @function 
   @abstract   Retourn le resultat de getLuminance de CColor
   @discussion Cette fonction est juste un pont pour son equivalent de la classe CColor
   (TODO : decrire le fonctionnement de cette fameuse fonction)
*/
-(double) luminance;
/*!
  @function 
   @abstract   Retourn le resultat de getColorTemp de CColor
   @discussion Cette fonction est juste un pont pour son equivalent de la classe CColor
   (TODO : decrire le fonctionnement de cette fameuse fonction)
*/
-(double) temperatureWithColorReference:(HCFRColorReference*)reference;
/*!
    @function 
    @abstract   Affiche la couleur sur la sortie standard
    @discussion Affiche les trois composantes de la couleur sur la sortie standard
*/
-(void) display;

#pragma mark Fonctions utilitaires de génération de couleurs
/*!
    @function
    @abstract   G"nère un gris de luminausité donnée
    @discussion Cette fonction génère un objet NSColor représantant une couleur grise avec la luminausité
 demandée.
    @param      IRE La luminausité de la couleur à retourner, entre 0 (noir) et 100 (blanc)
    @result     Une instance autoreleased de NSColor
*/
+(NSColor*) greyNSColorWithIRE:(float)IRE;

/*!
 @function
 @abstract   G"nère un rouge de daturation donnée
 @discussion Cette fonction génère un objet NSColor représantant une couleur rouge avec la saturation
 demandée.
 @param      saturation La saturationé de la couleur à retourner, entre 0 (gris) et 100 (couleur pure)
 @result     Une instance autoreleased de NSColor
 */
+(NSColor*) redNSColorWithSaturation:(float)saturation forColorReference:(HCFRColorReference*)colorReference;

/*!
 @function
 @abstract   G"nère un vert de daturation donnée
 @discussion Cette fonction génère un objet NSColor représantant une couleur vert avec la saturation
 demandée.
 @param      saturation La saturationé de la couleur à retourner, entre 0 (gris) et 100 (couleur pure)
 @result     Une instance autoreleased de NSColor
 */
+(NSColor*) greenNSColorWithSaturation:(float)saturation forColorReference:(HCFRColorReference*)colorReference;

/*!
 @function
 @abstract   G"nère un bleu de daturation donnée
 @discussion Cette fonction génère un objet NSColor représantant une couleur bleu avec la saturation
 demandée.
 @param      saturation La saturationé de la couleur à retourner, entre 0 (gris) et 100 (couleur pure)
 @result     Une instance autoreleased de NSColor
 */
+(NSColor*) blueNSColorWithSaturation:(float)saturation forColorReference:(HCFRColorReference*)colorReference;
@end
