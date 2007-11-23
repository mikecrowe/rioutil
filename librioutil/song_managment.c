/**
 *   (c) 2001-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.4 song_managment.c
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

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include <errno.h>

#include "rio_internal.h"

/* the guts of any upload */
static int do_upload (rios_t *rio, u_int8_t memory_unit, int addpipe, info_page_t info) {
  int error;
  int file_slot = first_free_file_rio (rio, memory_unit);
  file_list *tmp, *tmp2;

  /* check if there is sufficient space for the file */
  if (FREE_SPACE(memory_unit) < (info.data->size - info.skip)/1024) {
    free (info.data);
    
    return ERIOFULL;
  }
    
  if ((error = init_upload_rio(rio, memory_unit)) != URIO_SUCCESS) {
    rio_log (rio, error, "init_upload_rio error\n");
    abort_transfer_rio(rio);
    return error;
  }
  
  if ((error = bulk_upload_rio(rio, info, addpipe)) != URIO_SUCCESS) {
    rio_log (rio, error, "bulk_upload_rio error\n");
    abort_transfer_rio(rio);
    return error;
  }
  
  close (addpipe);

  if ((error = complete_upload_rio(rio, memory_unit, info))!= URIO_SUCCESS) {
    rio_log (rio, error, "complete_upload_rio error\n");
    abort_transfer_rio(rio);
    return error;
  }

  for (tmp = rio->info.memory[memory_unit].files ; tmp ; tmp = tmp->next)
    if (tmp->num > file_slot) {
      tmp->num++;
    }

  /* rioutil keeps track of the rio's memory state */
  update_free_intrn_rio(rio, memory_unit);

  return URIO_SUCCESS;
}

/*
  add_file_rio:
    Add an downloadable file to the rio.

  PreCondition:
      - An initiated rio instance.
      - A memory unit.
      - A filename.
      - Amount to skip at the beginning.

  PostCondition:
      - URIO_SUCCESS if the file uploads.
      - < 0 if an error occurs.
*/
int add_file_rio (rios_t *rio, u_int8_t memory_unit, char *file_name,
		  int skip) {
    info_page_t info;
    int error, addpipe;

    if (!rio)
	return ENOINST;

    if (return_generation_rio (rio) == 4 && return_version_rio (rio) < 2.0)
      return EUNSUPP;

   if (memory_unit >= rio->info.total_memory_units)
     return -1;

    rio_log (rio, 0, "add_file_rio: copying file to rio.\n");

    if (memory_unit >= rio->info.total_memory_units)
      return -1;

    if (try_lock_rio (rio) != 0)
      return ERIOBUSY;

    if (strstr(file_name, ".lst") == NULL && strstr (file_name, ".m3u") == NULL) {
      if ((error = downloadable_info(&info, file_name)) != 0)
	UNLOCK(error);
    } else {
      // Future -- function ( checkplaylist() )
      if ((error = playlist_info(&info, file_name)) != 0)
	return error;
    }

    info.skip = skip;

    if ((addpipe = open(file_name, O_RDONLY)) == -1)
      return -1;

    /* i moved the major functionality of both add_file and add_song down a layer */
    if ((error = do_upload (rio, memory_unit, addpipe, info)) != URIO_SUCCESS) {
      free (info.data);

      close (addpipe);

      UNLOCK(error);
    }

    close (addpipe);

    free (info.data);

    rio_log (rio, 0, "add_file_rio: copy complete.\n");

    UNLOCK(URIO_SUCCESS);
}

#if !defined (PATH_MAX)
#define PATH_MAX 255
#endif

int create_playlist_rio (rios_t *rio, char *name, int songs[], int memory_units[],
		      int nsongs) {
  info_page_t info;
  int error, addpipe, i, tmpi;
  char filename[PATH_MAX];
  char tmpc;
  file_list *tmp;
  FILE *fh;

  if (!rio)
    return ENOINST;
  
  /* Current implementation only works for S-Series and newer. For
     older, upload a .lst file. */
  if (return_generation_rio (rio) < 4)
    return EUNSUPP;
  
  if (try_lock_rio (rio) != 0)
    return ERIOBUSY;

  rio_log (rio, 0, "create_playlist_rio: creating a new playlist %s.\n", name);

  /* Create a temporary file */
  snprintf (filename, PATH_MAX, "/tmp/rioutil_%s.%08x", name, time (NULL));
  fh = fopen (filename, "w");
  if (fh == 0)
    UNLOCK(errno);
  fprintf (fh, "FIDLST%c%c%c", 1, 0, 0);
#if BYTE_ORDER == BIG_ENDIAN
  tmpi = bswap_32 (nsongs);
#else
  tmpi = nsongs;
#endif
  fwrite (&tmpi, 1, 3, fh);

  for (i = 0 ; i < nsongs ; i++) {
    rio_log (rio, 0, "Adding for song %i to playlist %s...\n", songs[i], name);
    for (tmp = rio->info.memory[memory_units[i]].files ; tmp ; tmp = tmp->next)
      if (tmp->num == songs[i])
        break;

    if (tmp == NULL)
      continue;

#if BYTE_ORDER == BIG_ENDIAN
    tmpi = bswap_32 (tmp->rio_num);
#else
    tmpi = tmp->rio_num;
#endif
    fwrite (&tmpi, 1, 3, fh);
    fwrite (tmp->sflags, 3, 1, fh);
  }

  fclose (fh);

  new_playlist_info (&info, filename, name);
  
  if ((addpipe = open(filename, O_RDONLY)) == -1)
    return -1;
  
  /* i moved the major functionality of both add_file and add_song down a layer */
  if ((error = do_upload (rio, 0, addpipe, info)) != URIO_SUCCESS) {
    free (info.data);
    
    close (addpipe);

    if (fork() == 0)
      execl ("/bin/rm", "rm", "-f", filename);
    
    UNLOCK(error);
  }
  
  close (addpipe);
  
  if (fork() == 0)
    execl ("/bin/rm", "rm", "-f", filename);
    
  free (info.data);
  
  rio_log (rio, 0, "add_file_rio: copy complete.\n");
  
  UNLOCK(URIO_SUCCESS);
}

/*
  add_song_rio:
    Upload a music file to the rio.

  PreCondition:
      - An initiated rio instance.
      - A memory unit.
      - A filename.
    Optional:
      - Artist.
      - Title.
      - Album.

  PostCondition:
      - URIO_SUCCESS if the file was uploaded.
      - < 0 if an error occured.
*/
int add_song_rio (rios_t *rio, u_int8_t memory_unit, char *file_name,
                  char *artist, char *title, char *album) {
  info_page_t song_info;
  int error;
  int i, addpipe;
  char *tmp;
  
  if (!rio)
    return ENOINST;
  
  if (memory_unit >= rio->info.total_memory_units)
    return -1;
  
  tmp = file_name + strlen(file_name) - 3;
  
  /* check for file types by extension (wma doesnt work right now) */
  if (strspn(tmp, "mMpP3") == 3)
    error = mp3_info(&song_info, file_name);
  else if (strspn(tmp, "wWmMaA") == 3) {
    rio_log (rio, -1, "WMA uploading not implimented");
    return -1;
    error = wma_info(&song_info, file_name);
  } else
    return add_file_rio (rio, memory_unit, file_name, 0);
  
  /* just in case one of the info funcs failed */
  if (error < 0) {
    rio_log (rio, error, "Error getting song info.\n");
    
    return -1;
  }

  if (try_lock_rio (rio) != 0)
    return ERIOBUSY;

  rio_log (rio, 0, "Adding a song...\n");

  /* copy any user-suplied data*/
  if (artist)
    sprintf(song_info.data->artist, artist, 63);
  
  if (title)
    sprintf(song_info.data->title, title, 63);
  
  if (album)
    sprintf(song_info.data->album, album, 63);

  /* upload the song */
  if ((addpipe = open(file_name, O_RDONLY)) == -1)
    return -1;

  if ((error = do_upload (rio, memory_unit, addpipe, song_info)) != URIO_SUCCESS) {
    free(song_info.data);
    
    close (addpipe);

    UNLOCK(error);
  }
  
  close (addpipe);


  free(song_info.data);
  
  UNLOCK(URIO_SUCCESS);
}

int upload_from_pipe_rio (rios_t *rio, u_int8_t memory_unit, int addpipe, char *name, char *artist,
			  char *album, char *title, int mp3, int bitrate, int samplerate) {
  info_page_t song_info;
  int error;
 
  if (!rio)
    return ENOINST;
 
  if (name == NULL || addpipe < 0)
    return -1;
 
  if (memory_unit >= rio->info.total_memory_units)
    return -1;

  if (try_lock_rio (rio) != 0)
    return ERIOBUSY;

  if ((song_info.data = (rio_file_t *) calloc (sizeof(rio_file_t), 1)) == NULL) {
    perror ("upload_from_pipe:");

    UNLOCK(errno);
  }

  rio_log (rio, 0, "Adding from pipe %i...\n", addpipe);

  /* copy any user-suplied data*/
  sprintf(song_info.data->name, name, 63);

  if (artist)
    sprintf(song_info.data->artist, artist, 63);
  
  if (title)
    sprintf(song_info.data->title, title, 63);
  
  if (album)
    sprintf(song_info.data->album, album, 63);

  if (mp3) {
    song_info.data->bit_rate = bitrate;
    song_info.data->sample_rate = samplerate;

    song_info.data->mod_date = time(NULL);
    song_info.data->bits = 0x10000b11;
    song_info.data->type = TYPE_MP3;
    song_info.data->foo4 = 0x00020000;
  }

  if ((error = do_upload (rio, memory_unit, addpipe, song_info)) != URIO_SUCCESS) {
    free(song_info.data);

    UNLOCK(error);
  }

  free(song_info.data);
  
  UNLOCK(URIO_SUCCESS);
}

/*
  init_upload_rio:
    Send the write command and read ready and first SRIODATA.

    ** Change 7-27-2001 **
    Backpacks should work now!
*/
int init_upload_rio (rios_t *rio, u_int8_t memory_unit) {
  int ret;

  rio_log (rio, 0, "init_upload_rio: entering\n");

  wake_rio(rio);

  /* someone finally got a rio with a backpack :), see the
     Contributions file */
  if ((ret = send_command_rio(rio, RIO_WRITE, memory_unit, 0)) != URIO_SUCCESS)
    return ret;
  
  read_block_rio(rio, NULL, 64);
  
  if (strstr(rio->buffer, "SRIORDY") == NULL)
    return ERIORDY;
  
  if ((int)rio->cmd_buffer[0])
    read_block_rio(rio, NULL, 64);
  else
    return -1;
  
  if (strstr(rio->buffer, "SRIODATA") == NULL)
    return ERIORDY;
  
  rio_log (rio, 0, "init_upload_rio: finished\n");

  return URIO_SUCCESS;
}

/*
  bulk_upload_rio:
    function writes a file to the rio in blocks.
*/
int bulk_upload_rio(rios_t *rio, info_page_t info, int addpipe) {
  unsigned char file_buffer[RIO_FTS];

  long int copied = 0, amount;
  
  int ret;
  
  rio_log (rio, 0, "bulk_upload_rio: entering\n");
  rio_log (rio, 0, "Skipping %08x bytes of input\n", info.skip);
  lseek(addpipe, info.skip, SEEK_SET);
  memset (file_buffer, 0, RIO_FTS);

  while (amount = read (addpipe, file_buffer, RIO_FTS)) {
    /* if we dont know the size we dont know how close we are to finishing */
    if (info.data->size && rio->progress != NULL)
      rio->progress(copied, info.data->size, rio->progress_ptr);

    if ((ret = write_block_rio(rio, file_buffer, RIO_FTS, "CRIODATA")) != URIO_SUCCESS)
      return ret;
    
    memset (file_buffer, 0, RIO_FTS);
    copied += amount;
  }

  rio_log (rio, 0, "Read in %08x bytes from file. File size is %08x\n", copied, info.data->size);

  if (info.data->size == -1) {
    info.data->size = copied;

    /* the time value should also be set, but there is no good way to do so */
  }
  
  if (rio->progress != NULL)
    rio->progress(1, 1, rio->progress_ptr);

  rio_log (rio, 0, "bulk_upload_rio: finished\n");

  return URIO_SUCCESS;
}

/*
  complete_upload_rio:
    function uploads the final info page to tell the rio the transfer is complete

  * This function cannot be caller before start_upload_rio.
*/
int complete_upload_rio (rios_t *rio, u_int8_t memory_unit, info_page_t info) {
  int ret;
  
  rio_log (rio, 0, "complete_upload_rio: entering\n");

  /* Kelly: 08-23-03
     TODO: Set some the data in the RIOT's portion of the
     data structure.  Exclude other players until it
     can be confirmed that they use it or that it
     doesn't matter.
     
     All the variables ending in 2 may be
     specific to the RIOT.
     
     The size2 and possibly riot_file_no _MUST_ be
     set for the RIOT to successfully accept the data
     file. 
  */
  if (return_type_rio(rio) == RIORIOT) {
    info.data->size2 = info.data->size;
    info.data->riot_file_no = info.data->file_no;
    info.data->time2 = info.data->time;
    info.data->demarc = 0x20;
    info.data->file_prefix = 0x2d203130;
    
    strncpy(info.data->name2, info.data->name, 27);
    strncpy(info.data->title2, info.data->title, 48);
    strncpy(info.data->artist2, info.data->artist, 48);
    strncpy(info.data->album2, info.data->album, 48);
  }

  file_to_me (info.data);
  
  /* upload the info page */
  rio_log (rio, 0, "  writing file header\n");
  
  write_block_rio (rio, (unsigned char *)info.data, sizeof (rio_file_t), "CRIOINFO");
  
  /* file/song upload is now complete */

  rio_log (rio, 0, "  song upload complete\n");
  
  /* 0x60 command is send by both iTunes and windows softare */
  if ((ret = send_command_rio(rio, 0x60, 0, 0)) != URIO_SUCCESS)
    return ret;
  
  rio_log (rio, 0, "complete_upload_rio: finished\n");

  return URIO_SUCCESS;
}

/*
  delete_file_rio:

  delete a file off the rio. if the file has two info pages then delete
  both.

  PreCondition:
      - An initiated rio instance.
      - A memory unit.
      - A file on the unit.

  PostCondition:
      - URIO_SUCCESS if the file was delete successfully.
      - < 0 if some error occured.
*/
int delete_file_rio (rios_t *rio, u_int8_t memory_unit, u_int32_t fileno) {
    file_list *tmp, *tmp2;
    rio_file_t file;
    int ret;

    if (try_lock_rio (rio) != 0)
      return ERIOBUSY;

    if ((ret = wake_rio(rio)) != URIO_SUCCESS)
      UNLOCK(ret);

    /* hopefully this list is up to date */
    for (tmp = rio->info.memory[memory_unit].files ; tmp ; tmp = tmp->next)
      if (tmp->num == fileno)
	break;
    
    /* not really an error if the file doesnt exist */
    if (!tmp)
	UNLOCK(-1);
    
    /*
      The rio will automatically reassign file numbers to any numbered
      larger than the file being deleted. Update inum member so subsequent calls
      to delete remove the correct file.
    */
    for (tmp2 = tmp->next ; tmp2 ; tmp2 = tmp2->next)
      tmp2->inum--;

    if (get_file_info_rio(rio, &file, memory_unit, tmp->inum) != URIO_SUCCESS)
      UNLOCK(-1);
    
    if ((ret = send_command_rio(rio, RIO_DELET, memory_unit, 0)) != URIO_SUCCESS)
      UNLOCK(-1);

    if (strstr(rio->buffer, "SRIODELS"))
      UNLOCK(EDELETE);

    if ((int)rio->cmd_buffer[0] != 0) {
      if ((ret = read_block_rio(rio, NULL, 64)) != URIO_SUCCESS)
	UNLOCK(ret);
    } else
      UNLOCK(EDELETE);
    
    /* correct the endianness of data */
    file_to_me(&file);

    if ((ret = write_block_rio(rio, (unsigned char *)&file, RIO_MTS, NULL)) != URIO_SUCCESS)
      UNLOCK(ret);

    if (strstr(rio->buffer, "SRIODELD") == NULL)
      UNLOCK(EDELETE);

    /* adjust memory values */
    rio->info.memory[memory_unit].num_files -= 1;
    rio->info.memory[memory_unit].total_time -= tmp->time;

    if (rio->info.memory[memory_unit].num_files == 0)
      rio->info.memory[memory_unit].files = NULL;
    else {
      /* delete the item from the list */
      if (tmp->prev)
	tmp->prev->next = tmp->next;
      
      if (tmp->next)
	tmp->next->prev = tmp->prev;
      
      if (tmp == rio->info.memory[memory_unit].files)
	rio->info.memory[memory_unit].files = tmp->next;
    }
    
    free(tmp);

    update_free_intrn_rio (rio, memory_unit);
    
    UNLOCK(URIO_SUCCESS);
}

