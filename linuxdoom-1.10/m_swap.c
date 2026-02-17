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
// DESCRIPTION:
//	Endianness handling, swapping 16bit and 32bit.
//
//-----------------------------------------------------------------------------

#ifndef __M_SWAP__
#define __M_SWAP__

// Endianness handling.
// WAD files are stored in little-endian format.
// On little-endian systems (x86/x64 Windows/Linux), NO swapping is needed.
// On big-endian systems (PowerPC, SPARC, etc.), swapping IS needed.

#ifdef __BIG_ENDIAN__

// Big-endian: need to swap bytes to read little-endian WAD data
unsigned short SwapSHORT(unsigned short x);
unsigned long  SwapLONG(unsigned long x);

#define SHORT(x)  ((short)SwapSHORT((unsigned short)(x)))
#define LONG(x)   ((int)SwapLONG((unsigned int)(x)))

#else

// Little-endian (x86/x64 Windows/Linux): WAD is already in native format.
// NO swapping needed - just cast directly.
#define SHORT(x)  ((short)(x))
#define LONG(x)   ((int)(x))

// These still need to be declared for m_swap.c to compile,
// but they won't be called on little-endian systems.
unsigned short SwapSHORT(unsigned short x);
unsigned long  SwapLONG(unsigned long x);

#endif // __BIG_ENDIAN__

#endif // __M_SWAP__