/**
 *   (c) 2001-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.0.3 downloadable.c 
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
  downloadable_info:
     Function takes in a file name and returns a
  complete Rio header struct that allows the file to
  be downloaded from the Rio.
*/
int downloadable_info (info_page_t *newInfo, char *file_name)
{
    rio_file_t *misc_file;
    struct stat statinfo;
    char *tmp1, *tmp2;

    if (stat(file_name, &statinfo) < 0){
	newInfo->data = NULL;
	return -1;
    }
    
    misc_file = (rio_file_t *)malloc(sizeof(rio_file_t));
    memset(misc_file, 0, sizeof(rio_file_t));
    misc_file->size = statinfo.st_size;
    
    misc_file->mod_date = time(NULL);
    
    tmp1 = (char *)malloc(strlen(file_name) + 1);
    memset(tmp1, 0, strlen(file_name) + 1);
    
    strncpy(tmp1, file_name, strlen(file_name));
    tmp2 = basename(tmp1);
    
    strncpy((char *)misc_file->name , tmp2, 63);
    
    newInfo->skip = 0;
    
    if (strstr(file_name,".bin") == 0) {
	strncpy((char *)misc_file->title, tmp2, 63);

	misc_file->bits     = 0x10000591;
    } else {
	/* probably a special file */
	misc_file->bits     = 0x20800590;
	misc_file->type     = 0x46455250;
	misc_file->mod_date = 0x00000000;

	strncpy((char *)misc_file->info1, "system", 6);
    }
    
    newInfo->data = misc_file;
    
    return URIO_SUCCESS;
}
