/**
 *   (c) 2001-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.4.1c rio.c
 *   
 *   c version of librioutil
 *   all sources are c style gnu (c-set-style in emacs)
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <stdlib.h>
#include <stdio.h>

#if defined (HAVE_LIBGEN_H)
#include <libgen.h>
#endif

#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#include "rio_internal.h"

#include "driver.h"

#include <stdarg.h>
void rio_log (rios_t *rio, int error, char *format, ...) {
  if ( (rio->debug > 0) && (rio->log != NULL) ) {
    va_list arg;
    
    va_start (arg, format);
    
    if (rio->log == NULL) return;
    
    if (error == 0) {
      vfprintf (rio->log, format, arg);
    } else {
      fprintf(rio->log, "Error %i: ", error);
      vfprintf (rio->log, format, arg);
    }
    
    va_end (arg);
  }
}

/*
  open_rio:
    Open rio.

  PostCondition:
      - An initiated rio instance.
      - NULL if an error occured.
*/
rios_t *open_rio (rios_t *rio, int number, int debug, int fill_structures) {
  int ret;

  if (rio == NULL)
    rio = (rios_t *) calloc (1, sizeof(rios_t));
  else
    memset(rio, 0, sizeof(rios_t));
  
  rio->debug       = debug;
  rio->log         = stderr;
  
  rio_log (rio, 0,
	   "open_rio: creating new rio instance. device: 0x%08x\n", number);
  
  if (debug) {
    rio_log (rio, 0, "open_rio: setting usb driver verbosity level to %i\n",
	     debug);

    usb_setdebug(debug);
  }

  rio->abort = 0;
  
  /* open the USB device (this calls the underlying driver) */
  if ((ret = usb_open_rio (rio, number)) != 0) {
    rio_log (rio, ret, "open_rio: could not open a Rio device\n");

    return NULL;
  }
  
  if (set_time_rio (rio) != URIO_SUCCESS && fill_structures != 0) {
    close_rio (rio);

    return (rio->dev = NULL);
  }
  
  unlock_rio (rio);

  if (fill_structures != 0)
    if (return_intrn_info_rio (rio) != URIO_SUCCESS) {
      close_rio (rio);
      
      return (rio->dev = NULL);
    }

  rio_log (rio, 0, "open_rio: new rio instance created.\n");

  return rio;
}

/*
  set_time_rio:
    Only sets the rio's time these days.
*/
int set_time_rio (rios_t *rio) {
  long int curr_time;
  struct timeval tv;
  struct timezone tz;
  struct tm *tmp;
  int ret;
  
  /*
   * the rio has no concept of timezones so we need to take
   * the local time into account when setting the time.
   * now using the (non)obselete gettimeofday function
   * i am not sure if this is the best way
   *
   */
  gettimeofday (&tv, &tz);
  tmp = localtime ((const time_t *)&(tv.tv_sec));

  rio_log (rio, 0, "Current time is: %s\n", asctime(tmp));

  curr_time = tv.tv_sec - 60 * tz.tz_minuteswest;
  
  if (tmp->tm_isdst != -1)
    curr_time += 3600 * tmp->tm_isdst;

  ret = send_command_rio (rio, 0x60, 0, 0);
  if (ret != URIO_SUCCESS)
    return ret;
  
  /* tell the rio what time it is, assuming your system clock is correct */
  ret = send_command_rio (rio, RIO_TIMES, curr_time >> 16, curr_time & 0xffff);
  if (ret != URIO_SUCCESS)
    return ret;
  
  return URIO_SUCCESS;
}

/*
  close_rio:
  Close connection with rio and free buffer.
*/
void close_rio (rios_t *rio) {
  int i;

  if (try_lock_rio (rio) != 0)
    return;
  
  rio_log (rio, 0, "close_rio: entering...\n");

  if (wake_rio(rio) != URIO_SUCCESS)
    return;
  
  /* this command seems to tell the rio we are done */
  send_command_rio(rio, 0x66, 0, 0);
  
  /* close connection */
  usb_close_rio (rio);
  
  /* release the memory used by this instance */
  free_info_rio (rio);

  unlock_rio (rio);
  
  rio_log (rio, 0, "close_rio: structure cleared.\n");
}

/*
  get_flist_riomc:
    Downloads file listing off of a flash media or smc based Rio (Rio600,
  Rio800, S-Series, etc.) and saves it as a linked list in head.
*/
int get_flist_riomc (rios_t *rio, u_int8_t memory_unit, int *total_time,
		     int *num_files, file_list **head) {
  int i, ret;
  rio_file_t file;
  
  file_list *flist, **tmp;
  file_list *prev = NULL;
  int first = 1;
  
  *total_time = 0;
  
  /*
    MAX_RIO_FILES is an arbitrary limit set since a Rio can get into a
    state where the file headers contain nothing but giberish resulting
    in the termination condition never being reached.

    This may not be a problem anymore, in which case a while(1) would be
    better here.
  */
  for (i = 0 ; i < MAX_RIO_FILES ; i++) {
    ret = get_file_info_rio(rio, &file, memory_unit, i);

    if (ret == ENOFILE)
      break; /* not an error */
    else if (ret != URIO_SUCCESS)
      return ret;

    if ( (flist = (file_list *) calloc (1, sizeof(file_list))) == NULL ) {
      rio_log (rio, errno, "As error occured allocating memory.\n");
      perror ("calloc");
      return errno;
    }

    flist->num  = i;
    flist->inum = i;
    flist->rio_num = file.file_no;
    strncpy(flist->artist, file.artist, 64);
    strncpy(flist->title, file.title, 64);
    strncpy(flist->album, file.album, 64);
    strncpy(flist->name, file.name, 64);
    
    flist->time = file.time;
    
    *total_time += file.time;
    
    flist->bitrate = file.bit_rate >> 7;
    flist->samplerate = file.sample_rate;
    flist->mod_date = file.mod_date;
    flist->size = file.size;
    flist->start = file.start;
    
    flist->prev = prev;
    
    if (file.type == TYPE_MP3)
      flist->type = MP3;
    else if (file.type == TYPE_WMA)
      flist->type = WMA;
    else if (file.type == TYPE_WAV)
      flist->type = WAV;
    else if (file.type == TYPE_WAVE)
      flist->type = WAVE;
    else
      flist->type = OTHER;

    if (return_generation_rio (rio) > 3)
      memcpy (flist->sflags, file.unk1, 3);
    
    if (first == 1) {
      first = 0;
      *head = flist;
    }    
		
    if (flist->prev)
      flist->prev->next = flist;
	
    prev = flist;
  }
    
  *num_files = i;
  
  return URIO_SUCCESS;
}

/* internal function to get around a bug in my dmca hacks */
int first_free_file_rio (rios_t *rio, u_int8_t memory_unit) {
  rio_file_t file;
  int i, error;
  int last_file = 0;

  /* loops through files until either file_no is 0 (no file) or
     file_no is larger than 1 + the previous file number */
  for (i = 0 ; ; i++) {
    if ((error = get_file_info_rio(rio, &file, memory_unit, i)) != URIO_SUCCESS)
      rio_log (rio, error, "first_free_file_rio: error getting file info.\n");
    
    if (file.file_no == (last_file + 1))
      last_file = file.file_no;
    else
      break;
  }

  return last_file;
}

int get_file_info_rio(rios_t *rio, rio_file_t *file,
		      u_int8_t memory_unit, u_int16_t file_no) {
  int ret;
  
  if (file == NULL)
    return -1;

  if ((ret = wake_rio(rio)) != URIO_SUCCESS)
    return ret;

  memset (file, 0, sizeof (rio_file_t));

  /* TODO -- Clean up code so it is easier to associate this with Riot
   * The RIOT doesn't need to do this to delete a file. */
  if (return_type_rio (rio) != RIORIOT) {
    /* send command to get file header */
    if ((ret = send_command_rio(rio, RIO_FILEI, memory_unit, file_no))
	!= URIO_SUCCESS)
      return ret;
    
    /* command was successful, read 2048 bytes of data from Rio */
    if ((ret = read_block_rio(rio, (unsigned char *)file, sizeof(rio_file_t)))
	!= URIO_SUCCESS)
      return ret;

    /* library handles endianness */
    file_to_me(file);
    
    /* no file exists with number 0, they are listed from 1 */
    if (file->file_no == 0)
      return ENOFILE;
  } else {
    /* for the RIOT to delete files (does this also work with downloads?) */
    file->riot_file_no = file_no;
  }

  return URIO_SUCCESS;
}

/*
  get_flist_riohd:
    Downloads the playlist from a Hard Drive based Rio player (Rio Riot)
   and saves it as a linked list in head.

   The num_files now counts the actual number of files and the
   block_count + i sets the 'filenumber' in the list.  Since the riot
   can have blank spaces in the file list it's important to display
   the correct file number; but count the actual files.
*/
int get_flist_riohd (rios_t *rio, u_int8_t memory_unit, int *total_time,
		     int *num_files, file_list **head) {
  int ret;
  u_int8_t *read_buffer;
  u_int32_t *iptr;

  u_int32_t hdr_size;

  file_list *flist, **tmp;
  file_list *prev = NULL;
  int first = 1;
  int block_count = 0;
  
  *total_time = 0;
  *num_files = 0;
  
  /* i dont think the Riot can have more than one memory unit */
  
  ret = send_command_rio (rio, RIO_RIOTF, 0, memory_unit);
  if (ret != URIO_SUCCESS) {
    rio_log (rio, ret, "Playlist read command sent, but no responce\n");
    return ret;
  }

  iptr = (u_int32_t *)read_buffer = (u_int8_t *) malloc (RIO_FTS);
  if (read_buffer == NULL) {
    rio_log (rio, errno, "Could not allocate read buffer\n");
    return errno;
  }
  
  read_block_rio (rio, read_buffer, 0x40);

  hdr_size = 0x100;

  while (1) {
    hd_file_t *hdf = (hd_file_t *)read_buffer;
    int i;

    /* send CRIODATA. Maybe this is used to get the checksum from the Rio */
    memset (rio->buffer, 0, 0x40);
    sprintf (rio->buffer, "CRIODATA");
    
    write_block_rio (rio, rio->buffer, 0x40, NULL);
    
    /* and now, the correct way to exit the loop */
    if (strstr (rio->buffer, "SRIODONE") != NULL) {
      free (read_buffer);
      return URIO_SUCCESS;
    }
    
    read_block_rio (rio, read_buffer, RIO_FTS);
    
    /* 0x40 is RIO_FTS/hdr_size */
    for (i = 0 ; i < 0x40 ; i++) {
      if (hdf->unk0 == 0) {  /* blank entry */
	hdf++;
	continue;
      }

      if ( (flist = (file_list *) calloc (1, sizeof(file_list))) == NULL ) {
	free (read_buffer);

	rio_log (rio, errno, "As error occured allocating memory.\n");
	perror ("calloc");
	return errno;
      }

      flist->num  = flist->inum = i + block_count;
      strncpy(flist->artist, hdf->artist, 48);
      strncpy(flist->title, hdf->title, 48);
      strncpy(flist->album, hdf->album, 48);
      strncpy(flist->name, hdf->file_name, 27);
      flist->size = hdf->size;
      flist->time = hdf->time;
     
      *total_time += flist->time;
      
      flist->prev = prev;
    
      flist->type = MP3;
    
      if (first == 1) {
	first = 0;
	*head = flist;
      }    
      
      if (flist->prev)
	flist->prev->next = flist;
      
      prev = flist;
      hdf++;
      *num_files += 1;

    }

    block_count += i;
  }

  free (read_buffer);
  return URIO_SUCCESS;
}

/*
  return_mem_list_rio:
  Return a two way linked list of rio_mems
*/
int return_mem_list_rio(rios_t *rio, mem_list *list) {
  int i, total_time, num_files, ret;
  rio_mem_t memory;
  file_list *file;
  int num_mem_units = MAX_MEM_UNITS;

  if (return_type_rio(rio) == RIORIOT) {
    memset(list, 0, sizeof(mem_list));
    num_mem_units = 1;
  } else
    memset(list, 0, sizeof(mem_list) * MAX_MEM_UNITS);
   
  for (i = 0 ; i < num_mem_units ; i++) {
    ret = get_memory_info_rio (rio, &memory, i);

    if (ret == ENOMEM)
      break; /* not an error */
    else if (ret != URIO_SUCCESS)
      return ret;
    
    strncpy(list[i].name, memory.name, 32);

    if (return_type_rio(rio) != RIORIOT)
      ret = get_flist_riomc(rio, i, &total_time, &num_files, &(list[i].files));
    else
      ret = get_flist_riohd(rio, i, &total_time, &num_files, &(list[i].files));

    if (ret == URIO_SUCCESS) {
      list[i].size       = memory.size;
      list[i].free       = memory.free;
      list[i].num_files  = num_files;
      list[i].total_time = total_time;

      rio_log (rio, 0, "Number of files: %i Total Time: %i\n\n",num_files,
	       (total_time/60)/60);
    } else
        return ret;
  }

  return URIO_SUCCESS;
}

int get_memory_info_rio(rios_t *rio, rio_mem_t *memory, u_int8_t memory_unit) {
  int ret;
  
  if (!rio)
    return -1;
  
  if ((ret = wake_rio(rio)) != URIO_SUCCESS)
    return ret;

  if (send_command_rio(rio, RIO_MEMRI, memory_unit, 0) != URIO_SUCCESS)
    return -1;

  /* command was successful, read 256 bytes from Rio */
  if ((ret = read_block_rio(rio, (unsigned char *)memory, 256)) != URIO_SUCCESS) 
      return ret;

  /* swap to big endian if needed */
  mem_to_me(memory);
  
  /* if requested memory unit is out of range Rio returns 256 bytes of 0's */
  if (memory->size == 0)
    return ENOMEM; /* not an error */

  return URIO_SUCCESS;
}

/* this should work better that before */
void update_free_intrn_rio (rios_t *rio, u_int8_t memory_unit) {
  rio_mem_t memory;

  get_memory_info_rio(rio, &memory, memory_unit);

  rio->info.memory[memory_unit].free = memory.free;
}

int return_type_rio(rios_t *rio) {
  return ((struct rioutil_usbdevice *)rio->dev)->entry->type;
}

int return_generation_rio (rios_t *rio) {
  int type = return_type_rio (rio);

  /*
    first generation : Rio300 (Unsupported)
    second generation: Rio500 (Unsupported)
    third generation : Rio600, Rio800, Rio900, psa[play, Riot
    fourth generation: S-Series
    fith generation  : Fuse, Chiba, Cali
                       Nitrus, Eigen, Karma (Unsupported)
  */

  if (type == RIO600 || type == RIO800 || type == RIO900 ||
      type == PSAPLAY || type == RIORIOT) 
    return 3;
  else if (type == RIOS10 || type == RIOS30 || type == RIOS35 ||
	   type == RIOS50 || type == RIOS11)
    return 4;
  else if (type == RIOFUSE || type == RIOCHIBA || type == RIOCALI)
    return 5;
  else
    return -1;
}

float return_version_rio (rios_t *rio) {
  return rio->info.version;
}

/*
  return_intrn_info_rio:
  BIG function that fills the rio_info structure.

  renamed from return_info_rio
*/
int return_intrn_info_rio(rios_t *rio) {
  rio_info_t *info = &rio->info;
  rio_prefs_t prefs;
  riot_prefs_t riot_prefs;
  
  unsigned char desc[256];
  unsigned char cmd;

  int ret;
  int i;
  
  if (try_lock_rio (rio) != 0)
    return ERIOBUSY;

  memset (info, 0, sizeof (rio_info_t));

  /* 
   * Send the initialize set of commands to the RIO
   */
  ret = send_command_rio(rio, 0x66, 0, 0);
  if (ret != URIO_SUCCESS) {
     rio_log (rio, ret, "return_info_rio: Error sending command\n");
    UNLOCK(ret);
  }

  ret = send_command_rio(rio, 0x65, 0, 0);
  if (ret != URIO_SUCCESS) {
    rio_log (rio, ret, "return_info_rio: Error sending command\n");
    UNLOCK(ret);
  }

  /*
    retrieve non-changable values
  */
  cmd = RIO_DESCP;
  if ((ret = send_command_rio(rio, cmd, 0, 0)) != 0) {
    rio_log (rio, ret, "return_info_rio: error sending command.\n");
    UNLOCK(ret);
  }

  ret = read_block_rio(rio, desc, 256);
  if (ret != URIO_SUCCESS) {
    rio_log (rio, ret, "return_info_rio: Error reading device info\n", cmd);

    UNLOCK(ret);
  }

  info->version = (desc[5] + (0.1) * (desc[4] >> 4)
		   + (0.01) * (desc[4] & 0xf));

  if ((ret = return_mem_list_rio(rio, rio->info.memory)) != URIO_SUCCESS) 
    return ret;
  /*
   * this is where we set which structure to use to fill the
   * prefs
   */

  
  /*
    retrieve changeable values
  */
  /* iTunes sends this set of commands before RIO_PREFR */
  cmd = RIO_PREFR;
  if ((ret = send_command_rio(rio, cmd, 0, 0)) == URIO_SUCCESS) {
    rio_log (rio, ret, "return_info_rio: Preference read command successful\n");

    if (return_type_rio (rio) != RIORIOT) { /* All but the RIOT */
      
      /* Read a block into the prefs structure */	    
      ret = read_block_rio(rio, (unsigned char *)&prefs, RIO_MTS);
      if (ret != URIO_SUCCESS) {
        rio_log (rio, ret, "return_info_rio: Error reading data after command 0x%x\n", cmd);
        UNLOCK(ret);
      }

      /* Copy the prefs into the info structure */
      memcpy(info->name, prefs.name, 17);
      info->volume           = prefs.volume;
      info->playlist         = prefs.playlist;
      info->contrast         = prefs.contrast - 1;
      info->sleep_time       = prefs.sleep_time % 5;
      info->treble           = prefs.treble;
      info->bass             = prefs.bass;
      info->eq_state         = prefs.eq_state % 8;
      info->repeat_state     = prefs.repeat_state % 4;
      info->light_state      = prefs.light_state % 6;
      info->random_state     = 0; /* RIOT Only */
      info->the_filter_state = 0; /* RIOT Only */

    } else { /* This is a RIOT */
      
      /* Read a block into the riot_prefs structure */
      ret = read_block_rio(rio, (unsigned char *)&riot_prefs, RIO_MTS);
      if (ret != URIO_SUCCESS) {
        rio_log (rio, ret, "return_info_rio: Error reading data from RIOT after command 0x%x\n",cmd);
	UNLOCK(ret);
      }

      /* Copy the riot_prefs into the info structure */
      memcpy(info->name, riot_prefs.name, 17);
      info->volume           = riot_prefs.volume;
      info->contrast         = riot_prefs.contrast - 1; /* do we really need the -1 */
      info->sleep_time       = riot_prefs.sleep_time;
      info->treble           = riot_prefs.treble;
      info->bass             = riot_prefs.bass;
      info->repeat_state     = riot_prefs.repeat_state % 4; /* Do we really need the mod 4? */
      info->light_state      = riot_prefs.light_state;
      info->random_state     = riot_prefs.random_state;
      info->the_filter_state = riot_prefs.the_filter_state;
      info->eq_state         = 0; /* Not on RIOT */
      info->playlist         = 0; /* Not on RIOT */
    }
  } else /* Failed the read */ 
      rio_log (rio, -1, "return_info_rio: Rio did not respond to Preference read command.\n");
     
  /*
    memory
  */
  for (i = 0 ; info->memory[i].size && i < MAX_MEM_UNITS ; i++)
    info->total_memory_units++;

  UNLOCK(URIO_SUCCESS);
}

static void sane_info_copy (rio_info_t *info, rio_prefs_t *prefs);

/*
  set_info_rio:
  Set preferences on rio.

  PreCondition:
  - An initiated rio instance (Rio S-Series does not support this command).
  - A pointer to a filled info structure.

  PostCondition:
  - URIO_SUCCESS if the preferences get set.
  - < 0 if an error occured.
*/
int set_info_rio(rios_t *rio, rio_info_t *info) {
  rio_prefs_t pref_buf;
  int error;
  unsigned char cmd;

  if (try_lock_rio (rio) != 0)
    return ERIOBUSY;

  /* noting to write */
  if (info == NULL)
    return -1;

  cmd = RIO_PREFR;
  if ((error = send_command_rio(rio, cmd, 0, 0)) != 0) {
    rio_log (rio, error, "set_info_rio: Error sending command\n");

    UNLOCK(EUNSUPP);
  }
  
  error = read_block_rio(rio, (unsigned char *)&pref_buf, RIO_MTS);
  if (error != URIO_SUCCESS) {
    rio_log (rio, error, "Error reading data after command 0x%x\n", cmd);
    
    UNLOCK(error);
  }
  
  sane_info_copy (info, &pref_buf);
  
  if (wake_rio(rio) != URIO_SUCCESS)
    UNLOCK(ENOINST);

  cmd = RIO_PREFS;
  if ((error = send_command_rio(rio, cmd, 0, 0)) != 0) {
    rio_log (rio, error, "set_info_rio: Error sending command\n");

    UNLOCK(-1);
  }

  error = read_block_rio(rio, NULL, 64);
  if (error != URIO_SUCCESS) {
    rio_log (rio, error, "set_info_rio: error reading data after command 0x%x\n", cmd);
    
    UNLOCK(error);
  }

  if ((error = write_block_rio(rio, (unsigned char *)&pref_buf, RIO_MTS, NULL)) != URIO_SUCCESS)
    rio_log (rio, error, "set_info_rio: error writing preferences\n");

  UNLOCK(error);
}

/*
  sane_info_copy:
  Make sure all values of info are sane and put them into prefs.
*/
static void sane_info_copy (rio_info_t *info, rio_prefs_t *prefs) {
  prefs->eq_state     = ((info->eq_state < 7)     ? info->eq_state     : 7);
  prefs->treble       = ((info->treble < 9)       ? info->treble       : 9);
  prefs->bass         = ((info->bass < 9)         ? info->bass         : 9);
  prefs->repeat_state = ((info->repeat_state < 2) ? info->repeat_state : 2);
  prefs->sleep_time   = ((info->sleep_time < 9)   ? info->sleep_time   : 9);
  prefs->light_state  = ((info->light_state < 5)  ? info->light_state  : 5);
  prefs->contrast     = ((info->contrast < 9)     ? info->contrast + 1 : 10);
  prefs->volume       = ((info->volume < 20)      ? info->volume       : 20);

  /* i don't think it would be a good idea to set this */
  /* prefs->playlist */

  if (strlen(info->name) > 0)
    strncpy(prefs->name, info->name, 16);
}

/*
  format_mem_rio:
  Format a memory unit.
*/
int format_mem_rio (rios_t *rio, u_int8_t memory_unit) {
  int ret;
  int pd;

  if ( (rio == NULL) || (rio->dev == NULL) )
    return ENOINST;

  if (try_lock_rio (rio) != 0)
    return ERIOBUSY;

  /* don't need to call wake_rio here */

  if (rio->progress)
    rio->progress (0, 100, rio->progress_ptr);

  if ((ret = send_command_rio(rio, RIO_FORMT, memory_unit, 0)) != URIO_SUCCESS)
    UNLOCK(ret);

  memset (rio->buffer, 0, 64);

  while (1) {
    if ((ret = read_block_rio(rio, NULL, 64)) != URIO_SUCCESS)
      UNLOCK(ret);

    /* newer players (Fuse, Chiba, Cali) return their progress */
    if (strstr(rio->buffer, "SRIOPR") != NULL) {
      sscanf (rio->buffer, "SRIOPR%02d", &pd);

      if (rio->progress)
	rio->progress (pd, 100, rio->progress_ptr);
    } else if (strstr(rio->buffer, "SRIOFMTD") != NULL) {
      break;
    } else
      UNLOCK(-1);
  }

  if (rio->progress)
    rio->progress (100, 100, rio->progress_ptr);

  UNLOCK(URIO_SUCCESS);
}

/*
  update_rio:

  Update the firmware on a Rio. Function supports all rioutil supported players.
*/
int update_rio (rios_t *rio, char *file_name) {
  unsigned char fileBuffer[0x4000];

  struct stat statinfo;
  int size, x;

  u_int32_t *intp;
  int blocks, blocksize;
  int updtf;

  int ret;
  unsigned char cmd;
  int pg;
  int player_generation = return_generation_rio (rio);

  if (try_lock_rio (rio) != 0)
    return ERIOBUSY;

  rio_log (rio, 0, "Updating firmware of generation %d rio...\n",
	   player_generation);
  
  rio_log (rio, 0, "Formatting internal memory\n");

  if ((ret = wake_rio(rio)) != URIO_SUCCESS)
    UNLOCK(ret);

  if (stat(file_name, &statinfo) < 0)
    UNLOCK(-1);

  size = statinfo.st_size;

  /* try to open the firmware file */
  if ((updtf = open(file_name, O_RDONLY)) < 0)
    UNLOCK(-1);

  /* it is not necessary to check the .lok file as the player will reject it if it is bad */

  rio_log (rio, 0, "Sending command...\n");

  if ((ret = send_command_rio(rio, RIO_UPDAT, 0x1, 0)) != URIO_SUCCESS)
    UNLOCK(ret);

  if ((ret = read_block_rio(rio, rio->buffer, 64)) != URIO_SUCCESS)
    UNLOCK(ret);
  
  rio_log (rio, 0, "Command sent... updating..\n");

  /* the rio wants the size of the firmware file */
  memset(rio->buffer, 0, 64);
  intp = (u_int32_t *)rio->buffer;

#if BYTE_ORDER == BIG_ENDIAN
  intp[0] = bswap_32(size);
#else
  intp[0] = size;
#endif

  if ((ret = write_block_rio(rio, rio->buffer, 64, NULL)) != URIO_SUCCESS)
    UNLOCK(ret);

  blocksize = 0x2000;
  blocks = size / blocksize;

  lseek(updtf, 0, SEEK_SET);
  
  /* erase */
  for (x = 0 ; x < blocks; x++) {
    /* read in a chunk of file */
    read(updtf, fileBuffer, blocksize);
    
    write_block_rio (rio, fileBuffer, blocksize, NULL);

    if (player_generation == 5) {
      if (strstr (rio->buffer, "SRIOPR") != NULL) {
	sscanf (rio->buffer, "SRIOPR%02d", &pg);
	
	if (rio->progress != NULL)
	  rio->progress (pg, 200, rio->progress_ptr);
      } else if (strstr (rio->buffer, "SRIODONE") != NULL) {
	if (rio->progress != NULL)
	  rio->progress (100, 100, rio->progress_ptr);

	close (updtf);
	return URIO_SUCCESS;
      }
    } else if (rio->buffer[1] == 2) {
      if (rio->progress != NULL)
	rio->progress (100, 100, rio->progress_ptr);

      close (updtf);
      return URIO_SUCCESS;
    } if (rio->progress != NULL)
	rio->progress (x/2, blocks, rio->progress_ptr);
  }

  lseek(updtf, 0, SEEK_SET);
    
  /* it takes a moment before the rio is ready to continue */
  usleep (1000);
  
  /* half-way mark on the progress bar */
  if (rio->progress != NULL)
    rio->progress (blocks/2, blocks, rio->progress_ptr);
    
  /* write firmware */
  for (x = 0 ; x < blocks ; x++) {
    /* read in a chunk of file */
    read (updtf, fileBuffer, blocksize);
    
    write_block_rio (rio, fileBuffer, blocksize, NULL);
    
    /* the rio expects the first block to be sent three times */
    if (x == 0) {
      write_block_rio (rio, fileBuffer, blocksize, NULL);
      write_block_rio (rio, fileBuffer, blocksize, NULL);
    }
  
    if (rio->progress != NULL)
      rio->progress (x/2 + blocks/2, blocks, rio->progress_ptr);
  }

  /* make sure the progress bar reaches 100% */
  if (rio->progress != NULL)
    rio->progress (blocks, blocks, rio->progress_ptr);

  close(updtf);

  UNLOCK(URIO_SUCCESS);

  return format_mem_rio (rio, 0);
}

int delete_dummy_hdr (rios_t *rio, u_int8_t memory_unit, u_int32_t fileno) {
  rio_file_t file;
  int error;

  rio_log (rio, 0, "Clearing dummy header...\n");

  if ((error = get_file_info_rio(rio, &file, memory_unit, fileno)) != URIO_SUCCESS)
    return error;

  file.size = 0;
  file.start = 0;
  file.time = 0;
  file.bits = 0;

  if ((error = send_command_rio (rio, RIO_DELET, memory_unit, 0)) != URIO_SUCCESS)
    return error;

  if (strstr(rio->buffer, "SRIODELS"))
    return EDELETE;

  /* check if the rio responded ok (with a 1) */
  if ((error = read_block_rio (rio, NULL, 64)) != URIO_SUCCESS)
    return error;

  /* correct the endianness of data */
  file_to_me(&file);

  if ((error = write_block_rio(rio, (unsigned char *)&file, RIO_MTS, NULL)) != URIO_SUCCESS)
    return error;

  if (strstr(rio->buffer, "SRIODELD") == NULL)
    return EDELETE;

  return URIO_SUCCESS;
}

int upload_dummy_hdr (rios_t *rio, u_int8_t memory_unit, u_int32_t fileno) {
  rio_file_t file;
  info_page_t info;
  int error;
  int file_num = first_free_file_rio (rio, memory_unit);

  rio_log (rio, 0, "uploading dummy header\n");

  if ((error = get_file_info_rio(rio, &file, memory_unit, fileno)) != URIO_SUCCESS) {
    rio_log (rio, error, "Error getting file info.\n");
    return -1;
  }

  if (file.bits & 0x00000080)
    return fileno;

  file.bits = 0x10000591;
  file.type = 0x00000000;
  file.file_no = 0;
  file.bit_rate = 0;
  file.sample_rate = 0;

  if ((error = init_upload_rio(rio, memory_unit)) != URIO_SUCCESS) {
    rio_log (rio, error, "upload_dummy_hdr(init_upload_rio) error\n");
    abort_transfer_rio(rio);
    return error;
  }

  info.data = &file;
  info.skip = 0;

  if ((error = complete_upload_rio(rio, memory_unit, info))!= URIO_SUCCESS) {
    rio_log (rio, error, "upload_dummy_hdr(complete_upload_rio) error\n");
    abort_transfer_rio(rio);
    return error;
  }

  return file_num;
}

/*
  download_file_rio:
  Function takes in the number of the file
  and attemt to download it from the Rio.
  
  Note: This only works with the following files:
  - Recorded WAVE files on the Rio 800
  - preferences.bin file
  - bookmarks.bin file
  - non-music files uploaded by rioutil
  - any file from an S-Series** or beyond player 
  
  ** curent S-Series firmware doesn't have support for this
    feature yet.

  -- Note --
  Any file on a third generation player that has the 0x80 bit
  set can be downloaded.
  
  It seems that, probably due to the riaa, diamond has
  made it so that wma and mp3 files CAN NOT
  be downloaded. mp3s that are downloaded will be deleted
  :(! Keep that in mind.

  All new players from Rio Audio support downloading of any
  file on the player!
*/
int download_file_rio (rios_t *rio, u_int8_t memory_unit,
		       u_int32_t fileno, char *fileName) {
  file_list *tmp;
  rio_file_t file;
  unsigned char *downBuf = (unsigned char *)malloc(RIO_FTS);
  
  int ret;

  int downfd;
  int size;
  int i, blocks;
  int *intp;
  
  int leftover;
  int cr_dummy = -1;
  int mode = S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP;
  int block_size;

  int type = return_type_rio (rio);
  int player_generation = return_generation_rio (rio);

  if (try_lock_rio (rio) != 0)
    return ERIOBUSY;

  /* build file list if needed */
  if (rio->info.memory[0].size == 0) 
    if ((ret = return_mem_list_rio(rio, rio->info.memory)) != URIO_SUCCESS)
      return ret;

  /* fetch the file's info */
  for (tmp = rio->info.memory[memory_unit].files ; tmp ; tmp = tmp->next)
    if (tmp->num == fileno)
      break;
  
  if (!tmp)
    UNLOCK(ENOFILE);
  
  if ((ret = get_file_info_rio(rio, &file, memory_unit, fileno)) != URIO_SUCCESS) {
    rio_log (rio, ret, "Error getting file info.\n");
    return -1;
  }

  /* Ignore this section on Riot and new flash players */
  if (type != RIORIOT && return_generation_rio (rio) != 5 &&
      (return_generation_rio (rio) != 4 && return_version_rio(rio) < 2.0)) {
    if (file.start == 0)
      UNLOCK(EUNSUPP);

    /* Older players can only download non-music files this will get
     around that limitation */
    if (player_generation == 3 && !(file.bits & 0x00000080)) {
      cr_dummy = fileno;
      fileno = upload_dummy_hdr (rio, memory_unit, fileno);
    }
  
    if ((ret = get_file_info_rio(rio, &file, memory_unit, fileno)) != URIO_SUCCESS) {
      rio_log (rio, ret, "Error getting file info.\n");
      UNLOCK(-1);
    }
  }

  size = tmp->size;
  
  /* send the send file command to the rio */
  if ((ret = send_command_rio(rio, RIO_READF, memory_unit, 0)) != URIO_SUCCESS)
    UNLOCK(ret);
  
  if ((ret = read_block_rio(rio, NULL, 64)) != URIO_SUCCESS)
    UNLOCK(ret);
    
  /* Write the info page to the rio */
  file_to_me(&file);
  
  write_block_rio(rio, (unsigned char *)&file, sizeof(rio_file_t), NULL);
  
  if (memcmp(rio->buffer, "SRIONOFL", 8) == 0)
    UNLOCK(-1);
  
  if (!fileName)
    /* create a file with identical name */
    downfd = creat((char *)&tmp->name, mode);
  else
    /* create a file with a user-specified name */
    downfd = creat((char *)fileName, mode);

  if (return_generation_rio (rio) >= 4)
    block_size = 0x4000;
  else
    block_size = 0x1000;

  blocks = size/block_size + ((size % block_size) ? 1 : 0);

  memset (downBuf, 0, block_size);
  
  for (i = 0 ; i < blocks ; i++) {
    int read_size;
    
    if (rio->abort) {
      abort_transfer_rio (rio);
      rio->abort = 0;
      
      if (rio->progress)
	rio->progress(1, 1, rio->progress_ptr);
      
      free(downBuf);
      close(downfd);
      UNLOCK(URIO_SUCCESS);
    }
    
    /* the rio seems to expect checksum */
    write_cksum_rio (rio, downBuf, block_size, "CRIODATA");
    
    if ((i+1)%4 == 0 || return_generation_rio(rio) >= 4) {
      read_block_rio(rio, NULL, 64);
      
      if (memcmp(rio->buffer, "SRIODONE", 8) == 0){
	if (rio->progress)
	  rio->progress(1, 1, rio->progress_ptr);
	
	free(downBuf);
	close(downfd);
	UNLOCK(URIO_SUCCESS);
      }
    }
    
    if (size >= block_size)
      read_size = block_size;
    else
      read_size = size;
    
    read_block_rio (rio, downBuf, block_size);
    
    if (rio->progress)
      rio->progress(i, blocks, rio->progress_ptr);
    
    write(downfd, downBuf, read_size);
    
    size -= read_size;
    
    if (rio->debug > 0)
      rio_log (rio, 0, "%08x bytes transferred, %08x to go\n", read_size, size);
  }
  
  write_cksum_rio (rio, downBuf, block_size, "CRIODATA");
  
  if (return_generation_rio(rio) < 4)
    read_block_rio(rio, NULL, 64);

  if (rio->progress)
    rio->progress(1, 1, rio->progress_ptr);
  
  close(downfd);
  
  /* 
     finish up a 16384 byte block (usually just zeros)
     I dont know exactly why it is needed
  */
  if (return_generation_rio (rio) < 4) {
    for ( ; i % 4 ; i++) {
      read_block_rio (rio, downBuf, block_size);
      
      write_cksum_rio (rio, downBuf, block_size, "CRIODATA");
    }
    
    read_block_rio(rio, NULL, 64);
  } 
 
  free(downBuf);
  
  send_command_rio(rio, 0x60, 0, 0);
  
  /* If a dummy header was uploaded, clear it */
  if (cr_dummy != -1) {
    delete_dummy_hdr (rio, memory_unit, fileno);

    /* if only this could be avoided without causing some pretty
       strange errors... */
    delete_file_rio (rio, memory_unit, cr_dummy);
  }
  
  UNLOCK(URIO_SUCCESS);
}

/*
  wake_rio:

  internal function which sends a common set of commands
*/
int wake_rio (rios_t *rio) {
  int *intp;
  int ret;
  
  if (!rio)
    return ENOINST;
  
  if ((ret = send_command_rio(rio, 0x66, 0, 0)) != URIO_SUCCESS)
    return ret;
  
  if ((ret = send_command_rio(rio, 0x65, 0, 0)) != URIO_SUCCESS)
    return ret;
  
  if ((ret = send_command_rio(rio, 0x60, 0, 0)) != URIO_SUCCESS)
    return ret;
  
  return URIO_SUCCESS;
}

// TODO -- Reorder all .c files to make more sense

/* frees the info ptr in rios_t structure */
void free_info_rio (rios_t *rio) {
  int i,j;
  file_list *tmp, *prev, *ntmp;
  
  for (i = 0 ; i < MAX_MEM_UNITS ; i++)
    for (tmp = rio->info.memory[i].files ; tmp ; tmp = ntmp) {
      ntmp = tmp->next;
      free(tmp);
    }
}

void free_file_list (file_list *s) {
  file_list *tmp, *prev, *ntmp;

  for (tmp = s ; tmp ; tmp = ntmp) {
    ntmp = tmp->next;
    free(tmp);
  }
}

/* New Functions -- Aug 8 2001 */
/*
  update_info_rio:

  funtion updates the info portion of the rio_instance structure
*/
int update_info_rio (rios_t *rio) {
  if (!rio)
    return ENOINST;

  free_info_rio (rio);
  
  return_intrn_info_rio (rio);
}


/*
  return_mem_units_rio:

  returns to total number of memory units an instance has.
  (usually 1 or 2)
*/
u_int8_t return_mem_units_rio (rios_t *rio) {
  if (rio == NULL)
    return ENOINST;
  
  return rio->info.total_memory_units;
}

/*
  return_free_mem_rio:

  returns the amount of free memory on a unit. In kB
*/
u_int32_t return_free_mem_rio (rios_t *rio, u_int8_t memory_unit) {
  if (rio == NULL)
    return ENOINST;
  
  if (memory_unit >= MAX_MEM_UNITS) {
    rio_log (rio, -2, "return_free_mem_rio: memory unit %02x out of range.\n",
	     memory_unit);
    return -2;
  }

  return FREE_SPACE(memory_unit);
}

/*
  return_used_mem_rio:

  returns the amount of memroy used on a unit.
*/
u_int32_t return_used_mem_rio (rios_t *rio, u_int8_t memory_unit) {
  if (rio == NULL)
    return ENOINST;

  if (memory_unit >= MAX_MEM_UNITS) {
    rio_log (rio, -2, "return_used_mem_rio: memory unit %02x out of range.\n",
	     memory_unit);
    return -2;
  }
  
  return (MEMORY_SIZE(memory_unit) - FREE_SPACE(memory_unit));
}

/*
  return_total_mem_rio:

  returns the total amount of memory a unit holds.
*/
u_int32_t return_total_mem_rio (rios_t *rio, u_int8_t memory_unit) {
  if (rio == NULL)
    return ENOINST;

  if (memory_unit >= MAX_MEM_UNITS) {
    rio_log (rio, -2, "return_total_mem_rio: memory unit %02x out of range.\n",
	     memory_unit);
    return -2;
  }
  
  return MEMORY_SIZE(memory_unit);
}

/*
  return_file_name_rio:

  returns the file name associated with a song_id/mem_id
*/
char *return_file_name_rio(rios_t *rio, u_int32_t song_id,
			   u_int8_t memory_unit) {
  file_list *tmp;
  char *ntmp;
  
  if (rio == NULL)
    return NULL;
  
  if (memory_unit >= MAX_MEM_UNITS) {
    rio_log (rio, -2, "return_file_name_rio: memory unit %02x out of range.\n",
	     memory_unit);
    return NULL;
  }
  
  /* find the file */
  for (tmp = rio->info.memory[memory_unit].files ; tmp ; tmp = tmp->next)
    if (tmp->num == song_id)
      break;
  
  if (tmp == NULL)
    return NULL;
  
  ntmp = (char *)calloc(strlen(tmp->name) + 1, 1);
  strncpy(ntmp, tmp->name, strlen(tmp->name));
  
  return ntmp;
}

u_int32_t return_file_size_rio(rios_t *rio, u_int32_t song_id,
			       u_int8_t memory_unit) {
  file_list *tmp;
    
  if (rio == NULL)
    return -1;
  
  if (memory_unit >= MAX_MEM_UNITS) {
    rio_log (rio, -2,
	     "return_file_size_rio: memory unit %02x out of range.\n",
	     memory_unit);
    return -2;
  }
  
  /* find the file */
  for (tmp = rio->info.memory[memory_unit].files ; tmp ; tmp = tmp->next)
    if (tmp->num == song_id)
      break;
  
  if (tmp == NULL)
    return -1;
  
  return tmp->size;
}

/*
  return_num_files_rio:

  returns the total number of files contained on a memory unit.
*/
u_int32_t return_num_files_rio (rios_t *rio, u_int8_t memory_unit) {
  if (rio == NULL)
    return ENOINST;
  
  if (memory_unit >= MAX_MEM_UNITS) {
    rio_log (rio, -2,
	     "return_num_files_rio: memory unit %02x out of range.\n",
	     memory_unit);
    return -2;
  }
  
  return rio->info.memory[memory_unit].num_files;
}

/*
  return_time_rio:

  returns the total time of tracks on a memory unit
*/
u_int32_t return_time_rio (rios_t *rio, u_int8_t memory_unit) {
  if (rio == NULL)
    return ENOINST;
  
  if (memory_unit >= MAX_MEM_UNITS) {
    rio_log (rio, -2, "return_time_rio: memory unit %02x out of range.\n",
	     memory_unit);
    return -2;
  }
  
  return rio->info.memory[memory_unit].total_time;
}

/*
  return_list_rio:

  returns a file_list contained on a memory unit.
*/
file_list *return_list_rio (rios_t *rio, u_int8_t memory_unit, u_int8_t list_flags) {
  file_list *tmp;
  file_list *bflist;
  file_list *prev = NULL;
  file_list *head = NULL;
  int first = 1, ret;
  
  if (rio == NULL)
    return NULL;

  if (memory_unit >= MAX_MEM_UNITS) {
    rio_log (rio, -2, "return_list_rio: memory unit %02x out of range.\n",
	     memory_unit);
    return NULL;
  }

  /* build file list if needed */
  if (rio->info.memory[0].size == 0) 
    if ((ret = return_mem_list_rio(rio, rio->info.memory)) != URIO_SUCCESS)
      return NULL;

  /* make a copy of the file list with only what we want in it */
  for (tmp = rio->info.memory[memory_unit].files ; tmp ; tmp = tmp->next) {
    if ( (list_flags == RALL) || ((list_flags & RMP3) && (tmp->type == MP3)) ||
	 ((list_flags & RWMA) && (tmp->type == WMA)) ||
	 ((list_flags & RWAV) && ((tmp->type == WAV)
				  || (tmp->type == WAVE))) ||
	 ((list_flags & RSYS) && (strstr(tmp->name, ".bin") != NULL)) ||
	 ((list_flags & RLST) && (strstr(tmp->name, ".lst") != NULL)) )
      {
	if ((bflist = malloc(sizeof(file_list))) == NULL) {
	  rio_log (rio, errno, "return_list_rio: Error in malloc\n");
	  return NULL;
	}

	*(bflist) = *(tmp);
	
	bflist->prev = prev;
	bflist->next = NULL;
	
	if (bflist->prev != NULL)
	  bflist->prev->next = bflist;
	
	if (first != 0) {
	  first = 0;
	  head = bflist;
	}
	
	prev = bflist;
      }
  }
  
  return head;
}

/*
  Other Info -- This does not return file list
*/
rio_info_t *return_info_rio (rios_t *rio) {
  rio_info_t *new_info;
  int i;
  
  if (rio == NULL)
    return NULL;

  if (rio->info.memory[0].size == 0)
    return_intrn_info_rio (rio);
  
  new_info = calloc(1, sizeof (rio_info_t));
  
  /* make a duplicate of rio's info */
  memcpy(new_info, &rio->info, sizeof(rio_info_t));

  for (i = 0 ; i < 2 ; i++)
    new_info->memory[i].files = NULL;
  
  return new_info;
}

/* hopefully these will be figured out soon */
int get_gid_rio (rios_t *rio, unsigned char gid[256]) {
  return -1; /* NOT IMPLEMENTED */
}

u_int32_t set_gid_rio (rios_t *rio, char *file_name) {
  return -1; /* NOT IMPLEMENTED */
}

/*
  set_progress_rio:

  set the function that librioutil calls while transfering.
*/
void set_progress_rio (rios_t *rio, void (*f)(int x, int X, void *ptr), void *ptr) {
  if (rio == NULL)
    return;
  
  rio->progress_ptr = ptr;
  
  rio->progress = f;
}

/*
  return_conn_method_rio:

  function to let any frontend know about the internals of lib.
*/
char *return_conn_method_rio (void) {
  return driver_method;
}

/* locking/unlocking routines */
int try_lock_rio (rios_t *rio) {
  if (rio->lock != 0)
    return -1;

  rio->lock = 1;

  return 0;
}

void unlock_rio (rios_t *rio) {
  rio->lock = 0;
}
