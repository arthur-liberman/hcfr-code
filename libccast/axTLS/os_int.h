
 /* Ensure a consistent bit size */

/*************************************************************************
 Copyright 2014 Graeme W. Gill

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.

 *************************************************************************/

#ifndef HEADER_OS_INT_H
#define HEADER_OS_INT_H

#ifdef __cplusplus
extern "C" {
#endif

#if (__STDC_VERSION__ >= 199901L)	/* C99 */		\
 || defined(_STDINT_H_) || defined(_STDINT_H)		\
 || defined(_SYS_TYPES_H)							\
 || (defined(__APPLE__) && defined(__POWERPC__))

#include <stdint.h> 

#else  /* !__STDC_VERSION__ */
#ifdef _MSC_VER

typedef __int8 int8_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

#else  /* !_MSC_VER */

/* The following works on a lot of modern systems, including */
/* LLP64 and LP64 models, but won't work with ILP64 which needs int32 */

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef long long          int64_t;

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

#endif	/* !_MSC_VER */
#endif  /* !__STDC_VERSION__ */

#ifdef __cplusplus
}
#endif

#endif 
