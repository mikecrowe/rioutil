/**
 *   (c) 2001-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.4.1 rio.h
 *
 *   header file for librioutil
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

#ifndef _RIO_H
#define _RIO_H

#include <sys/types.h>

/* errors */
#define URIO_SUCCESS 0

enum rios { RIO600, RIO800, PSAPLAY, RIO900, RIOS10, RIOS50,
	    RIOS35, RIOS30, RIOFUSE, RIOCHIBA, RIOCALI,
	    RIORIOT, RIOS11, UNKNOWN };

/* other defines */
#define MAX_MEM_UNITS   2   /* there are never more than 2 memory units */
#define MAX_RIO_FILES   3000 /* arbitrary */


/*
  Playlist structure:

  first entry is always blank.
  name is copied from rio_file->name
  title is 32 characters from rio_file->title
  unk[3] is an integer whose value is the entry number
      (stored in little endian format)

  Note:librioutil does not handle creation of playlists.
*/
typedef struct _rio_plst {
    struct song_list {
	u_int8_t  name[64];
	u_int32_t unk[8];
	u_int8_t  title[32];
    } playlist[128];
} rio_plst_t;

/*
  Use these for list_flags in function return_list_rio
*/
#define RMP3 0x01
#define RWMA 0x02
#define RWAV 0x04
#define RDOW 0x08
#define RSYS 0x10
#define RLST 0x20
#define RALL 0x3f

typedef struct _file_list {
  char artist[64];
  char title[64];
  char album[64];
  char name[64];
  
  int bitrate;
  int samplerate;
  int mod_date;
  int size;
  int time;
  
  /* pointer to start of file in rio's memory */
  int start;
  
  enum file_type {MP3 = 0, WMA, WAV, WAVE, OTHER} type;
  
  int num;
  int inum;
  
  struct _file_list *prev;
  struct _file_list *next;

  u_int8_t sflags[3];
  u_int32_t rio_num;

  char year[5];
  char genre[17];
} file_list;

typedef struct _mem_list {
    u_int32_t size;
    u_int32_t free;
    char name[32];

    file_list *files;

    u_int32_t total_time;
    u_int32_t num_files;
} mem_list;

typedef struct _rio_info {
  mem_list memory[MAX_MEM_UNITS];
  
  /*
    all of these values can be changed and set
  */
  unsigned char name[16];
  
  u_int8_t  light_state;
  u_int8_t  repeat_state;
  u_int8_t  eq_state;
  u_int8_t  bass;
  u_int8_t  treble;
  u_int8_t  sleep_time;
  u_int8_t  contrast;
  u_int8_t  playlist;
  u_int8_t  volume;
  u_int8_t  random_state;
  u_int8_t  the_filter_state;
  
  /*
    Can not be manipulated.
  */
  /* this is most likely only 1 or 2 */
  u_int8_t  total_memory_units;
  
  float version;
  u_int8_t serial_number[16];
} rio_info_t;

typedef struct _rios {
  /* void here to avoid the user needing to define WITH_USBDEVFS and such */
  void *dev;

  rio_info_t info;

  int debug;
  void *log;
  int abort;
  
  unsigned char cmd_buffer[16];
  unsigned char buffer    [64];
  
  void (*progress)(int x, int X, void *ptr);
  void *progress_ptr;

  /* make rioutil thread-safe */
  int lock;
} rios_t;

typedef rios_t rio_instance_t;

/*
  rio funtions:
*/
int     open_rio    (rios_t *rio, int number, int debug, int fill_structures);
void    close_rio   (rios_t *rio);

int     set_info_rio      (rios_t *rio, rio_info_t *info);
int     add_song_rio      (rios_t *rio, u_int8_t memory_unit, char *file_name,
			   char *artist, char *title, char *album);
int upload_from_pipe_rio  (rios_t *rio, u_int8_t memory_unit, int addpipe, char *name, char *artist,
			   char *album, char *title, int mp3, int bitrate, int samplerate);
int     delete_file_rio   (rios_t *rio, u_int8_t memory_unit, u_int32_t fileno);
int     format_mem_rio    (rios_t *rio, u_int8_t memory_unit);
int     update_rio        (rios_t *rio, char *file_name);
int     download_file_rio (rios_t *rio, u_int8_t memory_unit, u_int32_t fileno, char *fileName);
void    set_progress_rio  (rios_t *rio, void (*f)(int x, int X, void *ptr), void *ptr);

/* new functions 8-8-2001 */
int             update_info_rio      (rios_t *rio);
rio_info_t     *return_info_rio      (rios_t *rio);

/* new as of 01-20-2004 */
/* Works only with newer Rios (S-Series or newer) */
int create_playlist_rio (rios_t *rio, char *name, int songs[], int memory_units[], int nsongs);
int overwrite_file_rio (rios_t *rio, u_int8_t memory_unit, u_int32_t fileno, char *filename);
int return_serial_number_rio (rios_t *rio, u_int8_t serial_number[16]);

/* library info */
char          *return_conn_method_rio(void);

/*
  information on memory:

  memory_unit is an integer between 0 and MAX_MEM_UNITS - 1
*/
u_int8_t   return_mem_units_rio (rios_t *rio);
u_int32_t  return_free_mem_rio  (rios_t *rio, u_int8_t memory_unit);
u_int32_t  return_used_mem_rio  (rios_t *rio, u_int8_t memory_unit);
u_int32_t  return_total_mem_rio (rios_t *rio, u_int8_t memory_unit);
u_int32_t  return_num_files_rio (rios_t *rio, u_int8_t memory_unit);
u_int32_t  return_time_rio      (rios_t *rio, u_int8_t memory_unit);


file_list *return_list_rio      (rios_t *rio, u_int8_t memory_unit, u_int8_t list_flags);
void       free_file_list       (file_list *s);

char      *return_file_name_rio (rios_t *rio, u_int32_t song_id, u_int8_t memory_unit);
u_int32_t  return_file_size_rio (rios_t *rio, u_int32_t song_id, u_int8_t memory_unit);
int return_type_rio (rios_t *rio);

#endif /* _RIO_H */
