/**
 *   (c) 2001-2006 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.0 rio.h
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
	    RIORIOT, RIOS11, RIONITRUS, UNKNOWN };

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

  int track_number;
} file_list;

typedef file_list flist_rio_t;

typedef struct _mem_list {
    u_int32_t size;
    u_int32_t free;
    char name[32];

    flist_rio_t *files;

    u_int32_t total_time;
    u_int32_t num_files;
} mem_list;

typedef mem_list mlist_rio_t;

typedef struct _rio_info {
  mem_list memory[MAX_MEM_UNITS];
  
  /* these values can be changed an sent to the rio */
  char name[16];
  
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
  
  /* these values can not be manipulated */
  u_int8_t  total_memory_units; /* 1 or 2 */
  
  float firmware_version;
  u_int8_t serial_number[16];
  u_int32_t caps;
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
int open_rio (rios_t *rio, int number, int debug, int fill_structures);
void close_rio (rios_t *rio);

int set_info_rio (rios_t *rio, rio_info_t *info);
int add_song_rio (rios_t *rio, u_int8_t memory_unit, char *file_name, char *artist, char *title, char *album);
int download_file_rio (rios_t *rio, u_int8_t memory_unit, u_int32_t fileno, char *fileName);
int upload_from_pipe_rio (rios_t *rio, u_int8_t memory_unit, int addpipe, char *name, char *artist,
			  char *album, char *title, int mp3, int bitrate, int samplerate);
int delete_file_rio (rios_t *rio, u_int8_t memory_unit, u_int32_t fileno);
int format_mem_rio (rios_t *rio, u_int8_t memory_unit);

/* upgrade the rio's firmware from a file */
int firmware_upgrade_rio (rios_t *rio, char *file_name);

/* update the rio structure's internal info structure */
int update_info_rio (rios_t *rio);
/* store a copy of the rio's internal info structure in info */
int get_info_rio (rios_t *rio, rio_info_t **info);

/* sets the progress callback function */
void set_progress_rio  (rios_t *rio, void (*f)(int x, int X, void *ptr), void *ptr);

/* These only work with S-Series or newer Rios */
int create_playlist_rio (rios_t *rio, char *name, int songs[], int memory_units[], int nsongs);
int overwrite_file_rio (rios_t *rio, u_int8_t memory_unit, u_int32_t fileno, char *filename);
int return_serial_number_rio (rios_t *rio, u_int8_t serial_number[16]);


/* Added to API 02-02-2005 */
/* Returns the file number that will be assigned to the next file uploaded. */
int first_free_file_rio (rios_t *rio, u_int8_t memory_unit);

/* library info */
char          *return_conn_method_rio(void);

/*
  retrieve information on the rio's memory units

  memory_unit is an integer between 0 and MAX_MEM_UNITS - 1
*/
int return_mem_units_rio (rios_t *rio);
int return_free_mem_rio (rios_t *rio, u_int8_t memory_unit);
int return_used_mem_rio (rios_t *rio, u_int8_t memory_unit);
int return_total_mem_rio (rios_t *rio, u_int8_t memory_unit);
int return_num_files_rio (rios_t *rio, u_int8_t memory_unit);
int return_time_rio (rios_t *rio, u_int8_t memory_unit);

/* store a copy of the rio's file list in flist */
int return_flist_rio (rios_t *rio, u_int8_t memory_unit, u_int8_t list_flags, flist_rio_t **flist);

void free_flist_rio (flist_rio_t *flist);

char *return_file_name_rio (rios_t *rio, u_int32_t song_id, u_int8_t memory_unit);
int return_file_size_rio (rios_t *rio, u_int32_t song_id, u_int8_t memory_unit);
int return_type_rio (rios_t *rio);

#endif /* _RIO_H */
