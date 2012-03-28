/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2008 Association Homecinema Francophone.  All rights reserved.
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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

#if !defined(CRASHDUMP_INCLUDED)
#define CRASHDUMP_INCLUDED

#pragma once

#include <crtdbg.h>

/// CrashDump
/// Create and instance of this object to enable crash dumps on the current thread
class CrashDump
{
public:
    CrashDump();
    ~CrashDump();
private:
    HINSTANCE m_DbgHelpInstance;
    _se_translator_function m_OldTranslator;
};

#endif