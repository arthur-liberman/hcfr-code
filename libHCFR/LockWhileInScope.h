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

#if !defined(LOCK_WHILE_IN_SCOPE_H_INCLUDED_)
#define LOCK_WHILE_IN_SCOPE_H_INCLUDED_

#include "CriticalSection.h"

/// CLockWhileInScope
/// RAII critical section locker to simplify
/// cross platform locking
class CLockWhileInScope
{
public:
    CLockWhileInScope(CriticalSection& sectionToLock) :
        m_section(sectionToLock)
    {
        m_section.lock();
    }
    ~CLockWhileInScope()
    {
        m_section.unlock();
    }
private:
    CriticalSection& m_section;
};

#endif
