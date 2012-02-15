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
// Ce fichier contiend les exceptions qui seront utilisées dans les différents
// objets de la librairie
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//  JÈrÙme Duquennoy
/////////////////////////////////////////////////////////////////////////////

#ifndef __HCFRLIB_EXCEPTION_H__
#define __HCFRLIB_EXCEPTION_H__

class Exception
{
public:
  const char  *message;
  
  Exception(const char *message);
};

#endif