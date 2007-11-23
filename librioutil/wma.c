/**
 *   (c) 2001-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v0.5.9 wma.c (doesn't work, probably *never* will cause m$ sucks)
 *   
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *   
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *   
 *   You should have received a copy of the GNU Library Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 **/

#include <string.h>
#include <errno.h>

#include "config.h"

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif

#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "rio_internal.h"

/*
  wma_info:
    Function takes in a file name (WMA) and returns a
  structure with a completed Rio header struct.

  -- This is VERY imcomplete and does not work --
*/
int wma_info (info_page_t *newInfo, char *file_name)
{
    rio_file_t *wma_file;
    struct stat statinfo;
    char *tmp1, *tmp2;
    
    if (stat(file_name, &statinfo) < 0){
	newInfo->data = NULL;
	return -1;
    }
    
    wma_file = (rio_file_t *)malloc(sizeof(rio_file_t));
    memset(wma_file, 0, sizeof(rio_file_t));

    // It does want the size
    wma_file->size = statinfo.st_size;

    // No mod date on wma
    wma_file->mod_date = 0x00000000;

    tmp1 = (char *)malloc(strlen(file_name) + 1);
    memset(tmp1, 0, strlen(file_name) + 1);
  
    strncpy(tmp1, file_name, strlen(file_name));
    tmp2 = basename(tmp1);

    strncpy((char *)wma_file->info1, "\\Music", 7); // possibly folder
    strncpy((char *)wma_file->name , tmp2, (strlen(tmp2) < 64) ? strlen(tmp2) : 63);
    strncpy((char *)wma_file->title, tmp2, (strlen(tmp2) < 64) ? strlen(tmp2) : 63);
    //    strncpy((char *)wma_file->artist, tmp2, (strlen(tmp2) < 64) ? strlen(tmp2) : 63);
    //    strncpy((char *)wma_file->album, tmp2, (strlen(tmp2) < 64) ? strlen(tmp2) : 63);
    
    // Don't know about junk here yet
    newInfo->skip = 0;

    wma_file->bits = 0x10000b11;
    wma_file->type = TYPE_WMA;
    
    // But this may have somthing to do with it
    wma_file->foo3 = 0x21030000;
    //wma_file->foo3 = 0x00000321;
    wma_file->foobar[0] = 0x01;

    newInfo->data = wma_file;
    
    return URIO_SUCCESS;
}
