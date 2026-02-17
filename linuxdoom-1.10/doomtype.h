// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	Simple basic typedefs, isolated here to make it easier
//	 separating modules.
//    
//-----------------------------------------------------------------------------


#ifndef __DOOMTYPE__
#define __DOOMTYPE__



#ifndef __BYTEBOOL__
#define __BYTEBOOL__
#ifndef __DOOM_BOOLEAN_DEFINED__
#define __DOOM_BOOLEAN_DEFINED__
#ifdef __cplusplus
typedef bool boolean;
#else
/* On Windows/MSVC, system headers (rpcndr.h/windows.h) commonly define
   `boolean` as an unsigned char. Define the same here to avoid
   redefinition errors regardless of include order. On other platforms
   keep the original enum-based boolean. */
#if defined(_WIN32) || defined(WIN32)
typedef unsigned char boolean;
#else
typedef enum { false, true } boolean;
#endif
/* Provide false/true macros for C code if not already available. */
#ifndef false
#define false 0
#endif
#ifndef true
#define true 1
#endif
#endif // __cplusplus
#endif // __DOOM_BOOLEAN_DEFINED__
typedef unsigned char byte;
#endif // __BYTEBOOL__

/* Integer limits: use OS header on Linux, else define here (e.g. Windows). */
#if defined(LINUX) && !defined(_WIN32)
#include <values.h>
#else
#ifndef MAXCHAR
#define MAXCHAR		((char)0x7f)
#endif
#ifndef MAXSHORT
#define MAXSHORT	((short)0x7fff)
#endif
#ifndef MAXINT
#define MAXINT		((int)0x7fffffff)
#endif
#ifndef MAXLONG
#define MAXLONG		((long)0x7fffffff)
#endif
#ifndef MINCHAR
#define MINCHAR		((char)0x80)
#endif
#ifndef MINSHORT
#define MINSHORT	((short)0x8000)
#endif
#ifndef MININT
#define MININT		((int)0x80000000)
#endif
#ifndef MINLONG
#define MINLONG		((long)0x80000000)
#endif
#endif

/* Map POSIX case-insensitive string functions to MSVC equivalents when
   building on Windows. This prevents unresolved externals for
   strcasecmp/strncasecmp. */
#if defined(_WIN32) || defined(WIN32)
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#endif



#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
