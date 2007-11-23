/**
 *   (c) 2001-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.4 mp3.c 
 *
 *   MPEG Layer 3 file parser for librioutil
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef linux
#include <byteswap.h>
#include <endian.h>
#elif defined(__FreeBSD__) || defined(__MacOSX__)
#include <machine/endian.h>
#endif

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif

#include "rio_internal.h"

#include "mp3tech.h"

/* Fills in the mp3_file structure and returns the file offset to the 
   first MP3 frame header.  Returns >0 on success; -1 on error.
   
   This routine is based in part on MP3Info 0.8.4.
   MP3Info 0.8.4 was created by Cedric Tefft <cedric@earthling.net> 
   and Ricardo Cerqueira <rmc@rccn.net>
*/
static int get_mp3_header_info (char *file_name, rio_file_t *mp3_file) {
  int scantype=SCAN_QUICK,fullscan_vbr=1;
  mp3info mp3;
  
  memset (&mp3,0,sizeof(mp3info));
  mp3.filename = file_name;
  
  if ( !( mp3.file = fopen(file_name,"r") ) ) {
    fprintf(stderr,"Error opening MP3 file: %s\n",file_name);
    return -1;
  } 

  get_mp3_info(&mp3,scantype,fullscan_vbr);
  if(!mp3.header_isvalid) {
    fclose(mp3.file);
    fprintf(stderr,"%s is corrupt or is not a standard MP3 file.\n",mp3.filename);
    return -1;
  }
  
  mp3_file->time = mp3.seconds;
  mp3_file->sample_rate =  header_frequency(&mp3.header);
  
  /* Rio's bit_rate field must be multiplied by 128 hence the << 7 */
  if (mp3.vbr)
    mp3_file->bit_rate = (int)mp3.vbr_average << 7;
  else
    mp3_file->bit_rate = header_bitrate(&mp3.header) << 7; 

  fclose(mp3.file);
  return mp3.data_start;
}

/*
  mp3_info:
    Function takes in a file name (MP3) and returns a
  Info structure containing the amount of junk (in bytes)
  and a compete Rio header struct.
*/
int mp3_info (info_page_t *newInfo, char *file_name){
  rio_file_t *mp3_file;

  struct stat statinfo;
  int id3_version;
  int mp3_header_offset;

  if (stat(file_name, &statinfo) < 0) {
    fprintf(stderr,"mp3_info error: Could not stat file: %s\n",file_name);
    newInfo->data = NULL;
    return -1;
  }

  mp3_file = (rio_file_t *)calloc(1, sizeof(rio_file_t));

  mp3_file->size = statinfo.st_size;

  /* chop the file down to the last component,
     and remove the trailing .mp3 if it's there */
  {
    char *tmp1, *tmp2;
    int i;
  
    tmp1 = (char *)malloc(strlen(file_name) + 1);
    memset(tmp1, 0, strlen(file_name) +1);
  
    strncpy(tmp1, file_name, strlen(file_name));
    tmp2 = basename(tmp1);
  
    for (i = strlen(tmp2) ; i > 0 && tmp2[i] != '.' ; i--);  
  
    strncpy((char *)mp3_file->name , tmp2, 63);
    if (i > 0) tmp2[i] = '\0';
    strncpy((char *)mp3_file->title, tmp2, (strlen(tmp2) > 63) ? 63 : strlen(tmp2));
  
    free(tmp1);
  }
  
  if ((id3_version = get_id3_info(file_name, mp3_file)) < 0) {
    free(mp3_file);
    newInfo->data = NULL;
    return -1;
  }
  
  if ((mp3_header_offset = get_mp3_header_info(file_name,mp3_file)) < 0) {
    free(mp3_file);
    newInfo->data = NULL;
    return -1;
  }

  /* the file that will be uploaded is smaller if there is junk */
  if (mp3_header_offset > 0 && !(id3_version >= 2)) {
      mp3_file->size -= mp3_header_offset;
      newInfo->skip = mp3_header_offset;
  } else
    /* dont want to not copy the id3v2 tags */
    newInfo->skip = 0;

  /* it is an mp3 all right, finish up the INFO structure */
  mp3_file->mod_date = time(NULL);
  mp3_file->bits     = 0x10000b11;
  mp3_file->type     = TYPE_MP3;
  mp3_file->foo4     = 0x00020000;

  /* current possibility: info0 = folder */
  strncpy(mp3_file->info1, "Music", 5);

  newInfo->data = mp3_file;

  return URIO_SUCCESS;
}
