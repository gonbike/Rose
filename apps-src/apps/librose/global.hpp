/* $Id: global.hpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GLOBAL_HPP_INCLUDED
#define GLOBAL_HPP_INCLUDED

#ifdef _MSC_VER

// #undef snprintf
// #define snprintf _snprintf

// Disable warnig about source encoding not in current code page.
#pragma warning(disable: 4819)

// Disable warning about deprecated functions.
#pragma warning(disable: 4996)

//disable some MSVC warnings which are useless according to mordante
#pragma warning(disable: 4244)
#pragma warning(disable: 4345)
#pragma warning(disable: 4250)
#pragma warning(disable: 4355)
#pragma warning(disable: 4800)
#pragma warning(disable: 4351)

#endif

#ifdef _WIN32
// for memory lead detect begin
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
// for memory lead detect end
#endif

#define MAXLEN_APP				31
#define MAXLEN_TEXTDOMAIN		63	// MAXLEN_TEXTDOMAIN must >= MAXLEN_APP + 4. default textdomain is <app>-lib

// 1.mask must be power's 2, and not 0.
// 2.if val is 0, it will return 0. even if mask is any value.
// 3.if mask is power's 2, use posix_align_ceil, or use posix_align_ceil2.
#define posix_align_ceil(val, mask)     (((val) + (mask) - 1) & ~((mask) - 1))

int posix_align_ceil2(int dividend, int divisor);
#define posix_align_floor(dividend, divisor)	((dividend) - ((dividend) % (divisor)))

#endif
