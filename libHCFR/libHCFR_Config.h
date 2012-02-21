/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2012 Hcfr Project.  All rights reserved.
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
//  Author(s):
//  John Adcock
/////////////////////////////////////////////////////////////////////////////

#ifndef __LIBHCFR_CONFIG_FILE_H__
#define __LIBHCFR_CONFIG_FILE_H__

// this is in place for now until I can update the mac project as well
#ifdef WIN32

#define LIBHCFR_HAS_WIN32_API
#define LIBHCFR_HAS_MFC
//#define LIBHCFR_HAS_PTHREADS

#else

//#define LIBHCFR_HAS_WIN32_API
//#define LIBHCFR_HAS_MFC
#define LIBHCFR_HAS_PTHREADS
#endif
#endif