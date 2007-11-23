/**
 *   (c) 2001-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.0.6 bsd.c
 *   
 *   Functions that are possibly missing.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *   
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *   
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 **/

#include <string.h>
#include <sys/types.h>

#include "config.h"
#include "rio_internal.h"

#if defined (__FreeBSD__) || defined (__MacOSX__)

u_int32_t bswap_32(u_int32_t x){
  return ((x<<24) & 0xff000000) |
    ((x<<8 ) & 0x00ff0000) |
    ((x>>8 ) & 0x0000ff00) |
    ((x>>24) & 0x000000ff);
}

#endif

#if !defined(HAVE_BASENAME)

char *basename(char *x){
  static char buffer[PATH_MAX];
  static int buffer_pos = 0;
  int i;
  /* there is no / in x */
  if (strstr(x, "/") == NULL)
    return x;
 
  for (i = strlen(x) - 1 ; x[i] != '/'; i--);

  if ((strlen(&x[i+1]) + 1) < (PATH_MAX - buffer_pos))
    buffer_pos = 0;

  memset (&buffer[buffer_pos], 0, strlen (&x[i+1]) + 1);
  memcpy (&buffer[buffer_pos], &x[i+1], strlen(&x[i+1]));

  return &buffer[buffer_pos];
}

#endif
