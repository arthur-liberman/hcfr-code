/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2012 hcfr Project.  All rights reserved.
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
// John Adcock
/////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <stdexcept>
#include "CrashDump.h"
#include "DbgHelp.h"

namespace
{
    typedef BOOL (WINAPI * pfnMiniDumpWriteDump)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

    pfnMiniDumpWriteDump pMiniDumpWriteDump = 0;

    void SETranslate(unsigned int u, EXCEPTION_POINTERS* pExp)
    {
        fflush(stderr);
        if(pMiniDumpWriteDump)
        {
            BOOL bMiniDumpSuccessful;
            HANDLE hDumpFile;
            MINIDUMP_EXCEPTION_INFORMATION ExpParam;

            hDumpFile = CreateFile("minidump.dmp", GENERIC_READ|GENERIC_WRITE, 
                        FILE_SHARE_WRITE|FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

            ExpParam.ThreadId = GetCurrentThreadId();
            ExpParam.ExceptionPointers = pExp;
            ExpParam.ClientPointers = TRUE;

            bMiniDumpSuccessful = pMiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpWithDataSegs, &ExpParam, NULL, NULL);
        }
        throw std::runtime_error("SEH Exception Occured");
    }
}


CrashDump::CrashDump()
{
    m_DbgHelpInstance = LoadLibrary("DbgHelp.dll");
    if(m_DbgHelpInstance != 0 && pMiniDumpWriteDump == 0)
    {
        pMiniDumpWriteDump = (pfnMiniDumpWriteDump)GetProcAddress(m_DbgHelpInstance, "MiniDumpWriteDump");
    }
    m_OldTranslator = _set_se_translator(SETranslate);
}

CrashDump::~CrashDump()
{
    FreeLibrary(m_DbgHelpInstance);
    _set_se_translator(m_OldTranslator);
}
