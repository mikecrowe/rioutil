/**
 *   (c) 2001-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.4.4 song_managment.c
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
#include <sys/stat.h>
#include <errno.h>

#include "rio_internal.h"

static int do_upload (rios_t *rio, u_int8_t memory_unit, int addpipe, info_page_t info, int overwrite);
static int init_new_upload_rio (rios_t *rio, u_int8_t memory_unit);
static int init_overwrite_rio (rios_t *rio, u_int8_t memory_unit);
static int bulk_upload_rio (rios_t *rio, info_page_t info, int addpipe);
static int complete_upload_rio (rios_t *rio, u_int8_t memory_unit, info_page_t info);
static int add_file_rio (rios_t *rio, u_int8_t memory_unit, char *file_name, int skip);
static int delete_dummy_hdr (rios_t *rio, u_int8_t memory_unit, u_int32_t fileno);
static int upload_dummy_hdr (rios_t *rio, u_int8_t memory_unit, u_int32_t fileno);

/* the guts of any upload */
static int do_upload (rios_t *rio, u_int8_t memory_unit, int addpipe, info_page_t info, int overwrite) {
  int error;
  int file_slot = first_free_file_rio (rio, memory_unit);
  file_list *tmp, *tmp2;

  rio_log (rio, 0, "do_upload: entering\n");

  /* check if there is sufficient space for the file */
  if (overwrite == 0)
    if (FREE_SPACE(memory_unit) < (info.data->size - info.skip)/1024) {
      free (info.data);
      
      return -ENOSPC;
    }
    
  if (overwrite == 0) {
    if ((error = init_new_upload_rio(rio, memory_unit)) != URIO_SUCCESS) {
      rio_log (rio, error, "init_upload_rio error\n");
      abort_transfer_rio(rio);
      return error;
    }
  } else { 
    if ((error = init_overwrite_rio(rio, memory_unit)) != URIO_SUCCESS) {
      rio_log (rio, error, "init_upload_rio error\n");
      abort_transfer_rio(rio);
      return error;
    }
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

  rio_log (rio, 0, "do_upload: complete\n");

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
static int add_file_rio (rios_t *rio, u_int8_t memory_unit, char *file_name, int skip) {
  info_page_t info;
  int error, addpipe;
  
  if (!rio)
    return -EINVAL;
  
  if (return_generation_rio (rio) == 4 && return_version_rio (rio) < 2.0)
    return -EPERM;
  
  if (memory_unit >= rio->info.total_memory_units)
    return -1;
  
  rio_log (rio, 0, "add_file_rio: copying file to rio.\n");
  
  if (memory_unit >= rio->info.total_memory_units)
    return -1;
  
  if (try_lock_rio (rio) != 0)
    return -EBUSY;
  
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
  if ((error = do_upload (rio, memory_unit, addpipe, info, 0)) != URIO_SUCCESS) {
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
    return -EINVAL;
  
  /* Current implementation only works for S-Series and newer. For
     older, upload a .lst file. */
  if (return_generation_rio (rio) < 4)
    return -EPERM;
  
  if (try_lock_rio (rio) != 0)
    return -EBUSY;

  rio_log (rio, 0, "create_playlist_rio: creating a new playlist %s.\n", name);

  /* Create a temporary file */
  snprintf (filename, PATH_MAX, "/tmp/rioutil_%s.%08x", name, time (NULL));
  fh = fopen (filename, "w");
  if (fh == 0)
    UNLOCK(-errno);
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
  if ((error = do_upload (rio, 0, addpipe, info, 0)) != URIO_SUCCESS) {
    free (info.data);
    
    close (addpipe);

    /* make sure no malicious user has messed with this variable */
    if (strstr (filename, "/tmp/rioutil_") == filename)
      unlink (filename);
    
    UNLOCK(error);
  }
  
  close (addpipe);
  
  if (strstr (filename, "/tmp/rioutil_") == filename)
    unlink (filename);
    
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
    return -EINVAL;
  
  if (memory_unit >= rio->info.total_memory_units)
    return -1;

  rio_log (rio, 0, "add_song_rio: entering...\n");
  
  tmp = file_name + strlen(file_name) - 3;
  
  /* check for file types by extension */
  if (strspn(tmp, "mMpP3") == 3)
    error = mp3_info(&song_info, file_name);
  else
    return add_file_rio (rio, memory_unit, file_name, 0);
  
  /* just in case one of the info funcs failed */
  if (error != 0) {
    rio_log (rio, error, "Error getting song info.\n");
    
    return error;
  }

  if (try_lock_rio (rio) != 0)
    return -EBUSY;

  /* copy any user-suplied data*/
  if (artist)
    sprintf(song_info.data->artist, artist, 63);
  
  if (title)
    sprintf(song_info.data->title, title, 63);
  
  if (album)
    sprintf(song_info.data->album, album, 63);

  /* upload the song */
  addpipe = open(file_name, O_RDONLY);
  if (addpipe < 0)
    UNLOCK(-errno);

  rio_log (rio, 0, "add_song_rio: file opened and ready to send to rio.\n");

  if ((error = do_upload (rio, memory_unit, addpipe, song_info, 0)) != URIO_SUCCESS) {
    free(song_info.data);
    
    close (addpipe);

    UNLOCK(error);
  }
  
  close (addpipe);

  free(song_info.data);
  
  rio_log (rio, 0, "add_song_rio: complete\n");

  UNLOCK(URIO_SUCCESS);
}

int overwrite_file_rio (rios_t *rio, u_int8_t memory_unit, u_int32_t fileno, char *filename) {
  file_list *tmp;
  info_page_t song_info;  
  rio_file_t file;
  int ret;
  int addpipe;
  
  struct stat statinfo;

  if (try_lock_rio (rio) != 0)
    return -EBUSY;
  
  rio_log (rio, 0, "overwrite_file_rio: entering\n");

  if (stat (filename, &statinfo) < 0) {
    rio_log (rio, 0, "overwrite_file_rio: could not stat %s\n", filename);
    UNLOCK(-errno);
  }

  if ((ret = wake_rio(rio)) != URIO_SUCCESS)
    UNLOCK(ret);
  
  /* hopefully this list is up to date */
  for (tmp = rio->info.memory[memory_unit].files ; tmp ; tmp = tmp->next)
    if (tmp->num == fileno)
      break;
  
  /* not really an error if the file doesnt exist */
  if (!tmp) {
    rio_log (rio, 0, "overwrite_file_rio: file not found %i on %i\n", memory_unit, fileno);
    UNLOCK(-1);
  }

  if (get_file_info_rio(rio, &file, memory_unit, tmp->inum) != URIO_SUCCESS)
    UNLOCK(-1);

  file.size = statinfo.st_size;
  song_info.data = &file;
    
  if ((addpipe = open(filename, O_RDONLY)) == -1) {
    rio_log (rio, errno, "overwrite_file_rio: open failed\n");
    return -1;
  }
  
  /* i moved the major functionality of both add_file and add_song down a layer */
  if ((ret = do_upload (rio, 0, addpipe, song_info, 1)) != URIO_SUCCESS) {
    rio_log (rio, 0, "overwrite_file_rio: do_upload failed\n");
    close (addpipe);
    
    UNLOCK(ret);
  }
  
  close (addpipe);
  
  rio_log (rio, 0, "overwrite_file_rio: complete\n");
  
  UNLOCK(URIO_SUCCESS);
}

int upload_from_pipe_rio (rios_t *rio, u_int8_t memory_unit, int addpipe, char *name, char *artist,
			  char *album, char *title, int mp3, int bitrate, int samplerate) {
  info_page_t song_info;
  int error;
 
  if (!rio || name == NULL || addpipe < 0 ||
      memory_unit >= rio->info.total_memory_units)
    return -EINVAL;
 
  if (try_lock_rio (rio) != 0)
    return -EBUSY;

  if ((song_info.data = (rio_file_t *) calloc (sizeof(rio_file_t), 1)) == NULL) {
    perror ("upload_from_pipe:");

    UNLOCK(-errno);
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

  if ((error = do_upload (rio, memory_unit, addpipe, song_info, 0)) != URIO_SUCCESS) {
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
static int init_upload_rio (rios_t *rio, u_int8_t memory_unit, u_int8_t upload_command) {
  int ret;

  rio_log (rio, 0, "init_upload_rio: entering\n");

  if ((ret = wake_rio(rio)) != URIO_SUCCESS)
    return ret;

  /* someone finally got a rio with a backpack :), see the
     Contributions file */
  if ((ret = send_command_rio(rio, upload_command, memory_unit, 0)) != URIO_SUCCESS)
    return ret;
  
  read_block_rio(rio, NULL, 64);
  
  if (strstr(rio->buffer, "SRIORDY") == NULL)
    return -EBUSY;
  
  if ((int)rio->cmd_buffer[0])
    read_block_rio(rio, NULL, 64);
  else
    return -1;
  
  if (strstr(rio->buffer, "SRIODATA") == NULL)
    return -EBUSY;
  
  rio_log (rio, 0, "init_upload_rio: finished\n");

  return URIO_SUCCESS;
}

static int init_new_upload_rio (rios_t *rio, u_int8_t memory_unit) {
  return init_upload_rio (rio, memory_unit, RIO_WRITE);
}

static int init_overwrite_rio (rios_t *rio, u_int8_t memory_unit) {
  return init_upload_rio (rio, memory_unit, RIO_OVWRT);
}

/*
  bulk_upload_rio:
    function writes a file to the rio in blocks.
*/
static int bulk_upload_rio(rios_t *rio, info_page_t info, int addpipe) {
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
static int complete_upload_rio (rios_t *rio, u_int8_t memory_unit, info_page_t info) {
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
      return -EBUSY;

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
      UNLOCK(ret);

    if (strstr(rio->buffer, "SRIODELS"))
      UNLOCK(-EIO);

    if ((int)rio->cmd_buffer[0] != 0) {
      if ((ret = read_block_rio(rio, NULL, 64)) != URIO_SUCCESS)
	UNLOCK(ret);
    } else
      UNLOCK(-EIO);
    
    /* correct the endianness of data */
    file_to_me(&file);

    if ((ret = write_block_rio(rio, (unsigned char *)&file, RIO_MTS, NULL)) != URIO_SUCCESS)
      UNLOCK(ret);

    if (strstr(rio->buffer, "SRIODELD") == NULL)
      UNLOCK(-EIO);

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

static int delete_dummy_hdr (rios_t *rio, u_int8_t memory_unit, u_int32_t fileno) {
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
    return -EIO;

  /* check if the rio responded ok (with a 1) */
  if ((error = read_block_rio (rio, NULL, 64)) != URIO_SUCCESS)
    return error;

  /* correct the endianness of data */
  file_to_me(&file);

  if ((error = write_block_rio(rio, (unsigned char *)&file, RIO_MTS, NULL)) != URIO_SUCCESS)
    return error;

  if (strstr(rio->buffer, "SRIODELD") == NULL)
    return -EIO;

  return URIO_SUCCESS;
}

static int upload_dummy_hdr (rios_t *rio, u_int8_t memory_unit, u_int32_t fileno) {
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

  if ((error = init_new_upload_rio(rio, memory_unit)) != URIO_SUCCESS) {
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
    return -EBUSY;

  /* fetch the file's info */
  for (tmp = rio->info.memory[memory_unit].files ; tmp ; tmp = tmp->next)
    if (tmp->num == fileno)
      break;
  
  if (!tmp)
    UNLOCK(-ENOENT);
  
  if ((ret = get_file_info_rio(rio, &file, memory_unit, fileno)) != URIO_SUCCESS) {
    rio_log (rio, ret, "Error getting file info.\n");
    return -1;
  }

  /* Ignore this section on Riot and new flash players */
  if (type != RIORIOT && return_generation_rio (rio) != 5 &&
      (return_generation_rio (rio) != 4 && return_version_rio(rio) < 2.0)) {
    if (file.start == 0)
      UNLOCK(-EPERM);

    /* Older players can only download non-music files this will get
     around that limitation */
    if (player_generation == 3 && !(file.bits & 0x00000080)) {
      cr_dummy = fileno;
      fileno = upload_dummy_hdr (rio, memory_unit, fileno);
    }
  
    if ((ret = get_file_info_rio(rio, &file, memory_unit, fileno)) != URIO_SUCCESS) {
      rio_log (rio, ret, "Error getting file info.\n");
      UNLOCK(ret);
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
    
    if (size >= block_size)
      read_size = block_size;
    else
      read_size = size;
    
    read_block_rio (rio, downBuf, block_size);
    
    if (rio->progress)
      rio->progress(i, blocks, rio->progress_ptr);
    
    write(downfd, downBuf, read_size);
    
    size -= read_size;
    
    /* the rio seems to expect checksum */
    write_cksum_rio (rio, downBuf, block_size, "CRIODATA");
    
    if ((i != 0 && i % 4 == 0) || return_generation_rio(rio) >= 4) {
      read_block_rio(rio, NULL, 64);
      
      if (memcmp(rio->buffer, "SRIODONE", 8) == 0){
	if (rio->progress)
	  rio->progress(1, 1, rio->progress_ptr);
	
	free(downBuf);
	close(downfd);
	UNLOCK(URIO_SUCCESS);
      }
    }
    
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
