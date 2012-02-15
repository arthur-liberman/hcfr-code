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
//  Author(s):
//  Jérôme Duquennoy
/////////////////////////////////////////////////////////////////////////////

#ifndef __ENDIANNESS_H__
#define __ENDIANNESS_H__

#include <time.h>
#include <stdint.h>

void swapBytes(char * b, int n);

// pour l'import
unsigned short littleEndianUShortToHost (const unsigned short value);
unsigned int littleEndianUIntToHost (const unsigned int value);
float littleEndianFloatToHost (float value);
double littleEndianDoubleToHost (double value);
time_t littleEndianTime_TToHost (time_t value);
uint32_t littleEndianUint32ToHost (const uint32_t value);
uint32_t littleEndianUint16ToHost (const uint16_t value);

// pour l'export
uint16_t hostUint16TolittleEndian (const uint16_t value);
uint32_t hostUint32TolittleEndian (const uint32_t value);
#endif