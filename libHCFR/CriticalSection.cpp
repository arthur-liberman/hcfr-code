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

#include "CriticalSection.h"

CriticalSection::CriticalSection()
{
#ifdef LIBHCFR_HAS_WIN32_API
    InitializeCriticalSection(&m_critcalSection);
#elseif LIBHCFR_HAS_PTHREADS
    pthread_mutex_init(&m_matrixMutex, NULL);
#endif
}
CriticalSection::~CriticalSection()
{
#ifdef LIBHCFR_HAS_WIN32_API
    DeleteCriticalSection(&m_critcalSection);
#elseif LIBHCFR_HAS_PTHREADS
    pthread_mutex_destroy(&m_matrixMutex);
#endif
}
void CriticalSection::lock()
{
#ifdef LIBHCFR_HAS_WIN32_API
    EnterCriticalSection(&m_critcalSection);
#elseif LIBHCFR_HAS_PTHREADS
    pthread_mutex_lock(&m_matrixMutex);
#endif
}

void CriticalSection::unlock()
{
#ifdef LIBHCFR_HAS_WIN32_API
    LeaveCriticalSection(&m_critcalSection);
#elseif LIBHCFR_HAS_PTHREADS
    pthread_mutex_unlock (&m_matrixMutex);
#endif
}
