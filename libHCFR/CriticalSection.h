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

#if !defined(CRITICAL_SECTION_H_INCLUDED_)
#define CRITICAL_SECTION_H_INCLUDED_

#include "libHCFR_Config.h"

#ifdef LIBHCFR_HAS_WIN32_API
#   include <windows.h>
#else if LIBHCFR_HAS_PTHREADS
#   include <pthread.h>
#endif

/// CriticalSection
/// Class to provide a simple way of ensuring that
/// multiple threads do not access the same object at the same
/// time.  This class is expected to be used in conjunction
/// with the CLockWhileInScope class
class CriticalSection
{
public:
    CriticalSection();
    ~CriticalSection();
    void lock();
    void unlock();
private:
#   ifdef LIBHCFR_HAS_WIN32_API
        CRITICAL_SECTION m_critcalSection;
#   else if LIBHCFR_HAS_PTHREADS
        pthread_mutex_t m_matrixMutex;
#   endif
};



#endif // !defined(AFX_COLOR_H__BF14995B_5241_4AE0_89EF_21D3DD240DAD__INCLUDED_)
