/**
 *   (c) 2001-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.0.5 playlist.c 
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
  playlist_info:
*/
int playlist_info (info_page_t *newInfo, char *file_name) {
  rio_file_t *playlist_file;
  struct stat statinfo;
  char *tmp1, *tmp2;
  int fnum;
  
  if (stat(file_name, &statinfo) < 0) {
    newInfo->data = NULL;
    return -1;
  }
  
  playlist_file = (rio_file_t *)malloc(sizeof(rio_file_t));
  memset(playlist_file, 0, sizeof(rio_file_t));
  
  playlist_file->size = statinfo.st_size;
  
  /* filename of playlist should be only 14 characters + \0 */
  tmp1 = strdup (file_name);
  tmp2 = basename(tmp1);
  
  strncpy((char *)playlist_file->name , tmp2, 14);
  
  free (tmp1);

  sscanf(file_name, "Playlist%02d.lst", &fnum);
  
  sprintf((char *)playlist_file->title, "Playlist %02d%cst", fnum, 0);
  
  playlist_file->bits = 0x21000590; // playlist bits + file bits + download bit
  
  newInfo->skip = 0;
  newInfo->data = playlist_file;
  
  return URIO_SUCCESS;
}

/* Playlists for S-Series and newer. */
int new_playlist_info (info_page_t *newInfo, char *file_name, char *name) {
  rio_file_t *playlist_file;
  struct stat statinfo;
  char *tmp1, *tmp2;
  
  if (stat(file_name, &statinfo) < 0){
    newInfo->data = NULL;
    return -1;
  }
  
  playlist_file = (rio_file_t *)malloc(sizeof(rio_file_t));
  memset(playlist_file, 0, sizeof(rio_file_t));
  
  playlist_file->size = statinfo.st_size;
  
  strncpy((char *)playlist_file->name , name, 64);
  
  snprintf((char *)playlist_file->title, 64, "%s", name);

  playlist_file->bits = 0x11000110; // playlist bits + file bits + download bit
  playlist_file->type = TYPE_PLS;

  newInfo->skip = 0;
  newInfo->data = playlist_file;
  
  return URIO_SUCCESS;
}
