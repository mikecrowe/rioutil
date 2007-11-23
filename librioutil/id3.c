/**
 *   (c) 2003-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v0.1.1 id3.c 
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "rio_internal.h"

#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>

#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include "genre.h"

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif

#define ID3_TITLE             0
#define ID3_TALT              1
#define ID3_ARTIST            2
#define ID3_ALBUM             3
#define ID3_TRACK             4
#define ID3_YEAR              5
#define ID3_GENRE             6
#define ID3_ENCODER           7
#define ID3_COMMENT           8

static int find_id3 (int version, int fd, char *tag_data, int *tag_datalen, int *major_version);
static void cleanup_id3 (char *tag_data, int tag_datalen, int version);

static int synchsafe_to_int (char *buf, int nbytes) {
  int id3v2_len = 0;
  int i;

  for (i = 0 ; i < nbytes ; i++) {
    id3v2_len <<= 7;
    id3v2_len += buf[i] & 0x7f;
  }

  return id3v2_len;
}

/*
  find_id3 takes in a file descriptor, a pointer to where the tag data is to be put,
  and a pointer to where the data length is to be put.

  find_id3 returns:
    0 for no id3 tags
    1 for id3v1 tag
    2 for id3v2 tag

  The file descriptor is reset to the start of the file on completion.
*/

static int find_id3 (int version, int fd, char *tag_data, int *tag_datalen,
		     int *major_version) {
    int head;
    char data[10];

    char id3v2_flags;
    int  id3v2_len;
    int  id3v2_extendedlen;

    read(fd, &head, 4);
    
#if BYTE_ORDER == LITTLE_ENDIAN
    head = bswap_32(head);
#endif


    if (version == 2) {
      /* version 2 */
      if ((head & 0xffffff00) == 0x49443300) {
	read (fd, data, 10);
	
	*major_version = head & 0xff;
	
	id3v2_flags = data[1];
	
	id3v2_len = synchsafe_to_int (&data[2], 4);
	
	/* the 6th bit of the flag field being set indicates that an
	   extended header is present */
	if (id3v2_flags & 0x40) {
	  /* Skip extended header */
	  id3v2_extendedlen = synchsafe_to_int (&data[6], 4);
	  
	  lseek(fd, 0xa + id3v2_extendedlen, SEEK_SET);
	  *tag_datalen = id3v2_len - id3v2_extendedlen;
	} else {
	  /* Skip standard header */
	  lseek(fd, 0xa, SEEK_SET);
	  *tag_datalen = id3v2_len;
	}
	
	return 2;
      }
    } else if (version == 1) {
      lseek(fd, 0, SEEK_SET);
      
      /* tag not at beginning? */
      if ((head & 0xffffff00) != 0x54414700) {
	/* maybe end */
	lseek(fd, -128, SEEK_END);
	read(fd, &head, 4);
	lseek(fd, -128, SEEK_END);
	
#if BYTE_ORDER == LITTLE_ENDIAN
	head = bswap_32(head);
#endif
      }
      
      /* version 1 */
      if ((head & 0xffffff00) == 0x54414700) {
	read(fd, tag_data, 128);
	
	return 1;
      }
    }
    
    /* no id3 found */
    return 0;
}

/*
  parse_id3
*/
static void one_pass_parse_id3 (int fd, char *tag_data, int tag_datalen,
			       int version, int id3v2_majorversion,
			       rio_file_t *mp3_file) {
  int data_type;
  int i, j;
  int field;
  char *slash;

  if (version == 2) {
    /* field tags associated with id3v2 with major version <= 2 */
    char *fields[]     = {"TT1", "TT2", "TP1", "TAL", "TRK", "TYE", "TCO",
			  "TEN", "COM", "TLE", "TKE", NULL};
    /* field tags associated with id3v2 with major version > 2 */
    char *fourfields[] = {"TIT1", "TIT2", "TPE1", "TALB", "TRCK", "TYER",
			  "TCON", "TENC", "COMM", "TLEN", "TIME", NULL};
    
    char *tag_temp;
    char *sizeloc;
    char genre_temp[4];

    for (i = 0 ; i < tag_datalen ; ) {
      int length = 0;
      int tag_found = 0;

      read (fd, tag_data, (id3v2_majorversion > 2) ? 10 : 6);

      if (id3v2_majorversion > 2) {
	if (strncmp (tag_data, "APIC", 4) == 0 || id3v2_majorversion == 4)
	  length = *((int *)&tag_data[4]);
	else
	  length = synchsafe_to_int (&tag_data[4], 4);

	for (field = 0 ; fourfields[field] != NULL ; field++)
	  if (strncmp(tag_data, fourfields[field], 4) == 0) {
	    tag_found = 1;
	    break;
	  }

	i += 10 + length;
      } else {
	if (strncmp (tag_data, "PIC", 3) == 0)
	  length = (tag_data[3] << 16) | (tag_data[4] << 8) | tag_data[5];
	else
	  length = synchsafe_to_int (&tag_data[3], 3);

	for (field = 0 ; fields[field] != NULL ; field++)
	  if (strncmp(tag_data, fields[field], 3) == 0) {
	    tag_found = 1;
	    break;
	  }

	i += 6 + length;
      }

      if (tag_found == 0 || length < 2) {
	lseek (fd, length, SEEK_CUR);

	continue;
      }

      memset (tag_data, 0, 128);
      read (fd, tag_data, (length < 128) ? length : 128);

      tag_temp = tag_data;
      
      if (length > 128)
	lseek (fd, length - 128, SEEK_CUR);

      for ( ; *tag_temp == '\0' && length ; tag_temp++, length--);

      length -= 1;

      switch (field) {
      case ID3_TITLE:
      case ID3_TALT:
	memset (mp3_file->title, 0, 63);
	strncpy (mp3_file->title, tag_temp, (length > 63) ? 63 : length);
	break;
      case ID3_ARTIST:
	memset (mp3_file->artist, 0, 63);
	strncpy (mp3_file->artist, tag_temp, (length > 63) ? 63 : length);
	break;
      case ID3_ALBUM:
	memset (mp3_file->album, 0, 63);
	strncpy (mp3_file->album, tag_temp, (length > 63) ? 63 : length);
	break;
      default:
	continue;
      }
    }
  } else if (version == 1) {
    for (field = 0 ; field <= ID3_COMMENT ; field++) {
      char *copy_from, *tmp, *dstp;
      
      i = 29;
      
      switch (field) {
      case ID3_TITLE:
	copy_from = &tag_data[3];
	dstp = mp3_file->title;
	break;
      case ID3_ARTIST:
	copy_from = &tag_data[33];
	dstp = mp3_file->artist;
	break;
      case ID3_ALBUM:
	copy_from = &tag_data[63];
	dstp = mp3_file->album;
	break;
      case ID3_COMMENT:
	copy_from = &tag_data[93];
	continue;
      case ID3_GENRE:
	if ((int)tag_data[127] >= genre_count ||
	    (signed char)tag_data[127] == -1)
	  continue;
	
	copy_from = genre_table[tag_data[127]];
	i = strlen (copy_from - 1);
	continue;
      default:
	continue;
      }
      
      if ((signed char) copy_from[0] == -1)
	continue;
      
      if (field != ID3_GENRE)
	for (tmp = copy_from + i ;(*tmp == ' ' || (signed char)(*tmp) == -1) &&
	       i >= 0; tmp--, i--)
	  *tmp = 0;
      else
	i = strlen(copy_from) - 1;
      
      if (i < 0)
	continue;
   
      i++;
      memset (dstp, 0, 63);
      strncpy (dstp, copy_from, i);
     }
  }
}

static void cleanup_id3 (char *tag_data, int tag_datalen, int version) {
  if (version && tag_data)
    free (tag_data);
}

int get_id3_info (char *file_name, rio_file_t *mp3_file) {
  int fd;
  int tag_datalen = 0;
  char tag_data[128];
  int version;
  int id3v2_majorversion;
  int has_v2 = 0;

  if ((fd = open (file_name, O_RDONLY)) < 0)
    return errno;
  
  /* ** NEW ** built-in id3tag reading -- id3v2, id3v1 */
  if ((version = find_id3(2, fd, tag_data, &tag_datalen,
			  &id3v2_majorversion)) != 0) {
    one_pass_parse_id3 (fd, tag_data, tag_datalen, version, id3v2_majorversion, mp3_file);
    has_v2 = 1;
  }

  if ((version = find_id3(1, fd, tag_data, &tag_datalen,
			  &id3v2_majorversion)) != 0)
    one_pass_parse_id3 (fd, tag_data, tag_datalen, version, id3v2_majorversion, mp3_file);
  
  if (strlen((char *)mp3_file->title) == 0) {
    char *tmp = basename(file_name);
    int i;
    
    for (i=strlen(tmp); (i != '.') && i > 0 ; i--);
    
    memcpy(mp3_file->title, tmp, ((strlen(tmp) - i) > 31) ? 31 :
	   (strlen(tmp) - i));
  }
  
  close(fd);

  if (has_v2)
    return 2;
  else
    return 1;
}
