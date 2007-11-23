/**
 *   (c) 2001-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.1.0 byteorder.c
 * 
 *   Functions to handle big-endian machines.   
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

#include "rio_internal.h"

/*
  Swap le rio_file to machine architecture
*/
void file_to_me (rio_file_t *data)
{
#if BYTE_ORDER == BIG_ENDIAN
    data->file_no     = bswap_32(data->file_no);
    data->start       = bswap_32(data->start);
    data->size        = bswap_32(data->size);
    data->time        = bswap_32(data->time);
    data->mod_date    = bswap_32(data->mod_date);
    data->bits        = bswap_32(data->bits);
    data->type        = bswap_32(data->type);
    data->foo3        = bswap_32(data->foo3);
    data->foo4        = bswap_32(data->foo4);
    data->sample_rate = bswap_32(data->sample_rate);
    data->bit_rate    = bswap_32(data->bit_rate);
#endif
}

/*
  Swap le rio_mem to machine architecture
*/
void mem_to_me (rio_mem_t *data)
{
#if BYTE_ORDER == BIG_ENDIAN
    data->size   = bswap_32(data->size);
    data->used   = bswap_32(data->used);
    data->free   = bswap_32(data->free);
    data->system = bswap_32(data->system);
#endif
}
