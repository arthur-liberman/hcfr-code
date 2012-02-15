//  ColorHCFR
//  HCFRGraphicView.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 08/08/08.
//
//  $Rev: 116 $
//  $Date: 2008-09-04 17:31:29 +0100 (Thu, 04 Sep 2008) $
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


#import <Cocoa/Cocoa.h>
#import <SM2DGraphView/SM2DGraphView.h>

/*!
    @class
    @abstract    Cette classe est une extension de NSView qui propose une fonction pour obtenir une représentation
 sous forme d'image.
*/
@interface SM2DGraphView (ImageAdditions)
-(NSImage*) imageRepresentation;
-(void) copyImageRepresentationToPastboard:(id) sender;
-(void) saveGraphToPath:(NSString*)filePath;
@end
