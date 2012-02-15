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

#include "Endianness.h"
#include <algorithm> //required for std::swap

#if defined(__BIG_ENDIAN__)
void swapBytes(char * b, int n)
{
  register int i = 0;
  register int j = n-1;
  while (i<j)
  {
    std::swap(b[i], b[j]);
    i++, j--;
  }
}
unsigned short littleEndianUShortToHost (const unsigned short value)
{
  swapBytes ((char*)&value, sizeof(unsigned short));
  return value;
}
unsigned int littleEndianUIntToHost (const unsigned int value)
{
  swapBytes ((char*)&value, sizeof(unsigned int));
  return value;
}
float littleEndianFloatToHost (float value)//char *value)
{
  swapBytes ((char*)&value, sizeof(float));
  return value;//*((double*)value);
}
double littleEndianDoubleToHost (double value)//char *value)
{
  swapBytes ((char*)&value, sizeof(double));
  return value;//*((double*)value);
}
time_t littleEndianTime_TToHost (time_t value)
{
  swapBytes ((char*)&value, sizeof(time_t));
  return value;
}
uint32_t littleEndianUint32ToHost (const uint32_t value)
{
  swapBytes ((char*)&value, sizeof(uint32_t));
  return value;
}
uint32_t littleEndianUint16ToHost (const uint16_t value)
{
  swapBytes ((char*)&value, sizeof(uint32_t));
  return value;
}

uint16_t hostUint16TolittleEndian (const uint16_t value)
{
  swapBytes ((char*)&value, sizeof(uint16_t));
  return value;
}
uint32_t hostUint32TolittleEndian (const uint32_t value)
{
  swapBytes ((char*)&value, sizeof(uint32_t));
  return value;
}

#else
unsigned short littleEndianUShortToHost (const unsigned short value)
{ return value; }
unsigned int littleEndianUIntToHost (const unsigned int value)
{ return value; }
float littleEndianFloatToHost (float value)
{
  float result;
  
  // on copie les données, tout simplement.
  memcpy (&result, &value, 4);
  
  return result;
}
double littleEndianDoubleToHost (double value)
{
  double result;
  
  // on copie les données, tout simplement.
  memcpy (&result, &value, 8);
  
  return result;
}
time_t littleEndianTime_TToHost (time_t value)
{ return value;}
uint32_t littleEndianUint32ToHost (const uint32_t value)
{ return value; }
uint32_t littleEndianUint16ToHost (const uint16_t value)
{ return value; }
uint16_t hostUint16TolittleEndian (const uint16_t value)
{ return value; }
uint32_t hostUint32TolittleEndian (const uint32_t value)
{ return value; }
#endif