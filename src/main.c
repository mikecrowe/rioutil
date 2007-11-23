/**
 *   (c) 2001-2004 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.4 main.c
 *
 *   Console based interface for Rios using librioutil
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

#if defined (HAVE_CONFIG_H)
#include "config.h"
#endif

#include "rio.h"
#include "main.h"

#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#include <unistd.h>
#include <getopt.h>

#include <signal.h>

#include <sys/stat.h>
#include <dirent.h>

#if defined HAVE_LIBGEN_H
#include <libgen.h>
#else

#if !defined(HAVE_BASENAME)
/* return a pointer to the start of the filename (w/out the path) */
static char *basename(char *p){
  char *tmp;
  
  if (!p)
    return NULL;
  
  /* look at the character (starting from the end of the string)
     and go backwards looking for first / */
  for(tmp = p + strlen(p) ; *tmp != '/' ; tmp--);
  
  return tmp + 1;
}
#endif /* HAVE_BASENAME */

#endif


#define MAX_DEPTH_RIO 3

rios_t *current_rio;
static int no_space = 0;
static int is_a_tty;
static int last_nummarks;

static void progress_no_tty(int x, int X, void *ptr);
static void new_printfiles(int mflag, int mem_unit);
void print_info(rio_info_t *info, int type, int mflag, int mem_unit);
void enter_shell(rios_t *rio, int mflag, int mem_unit);
int create_playlist (rios_t *rio, int argc, char *argv[]);

#define max(a, b) ((a > b) ? a : b)

void aborttransfer (int sigraised) {
  current_rio->abort = 1;
}

/* upload stack */

struct upload_stack {
  struct _info {
    int mem_unit;
    
    char *title;
    char *artist;
    char *album;
    
    char *filename;

    int recursive_depth;
  }info;
  
  struct upload_stack *next;
};

struct upload_stack *head;

void push (int mem_unit, char *title, char *artist, char *album, char *filename, int recursive_depth);
void push_top (int mem_unit, char *title, char *artist, char *album, char *filename, int recursive_depth);
struct upload_stack *pop (void);

/****************/

int main (int argc, char *argv[]) {
  int c, option_index, ret = 0;
  int aflag = 0, apush = 0, dflag = 0, uflag = 0, nflag = 0;
  int elvl = 0, bflag = 0;
  int mflag = 0, gflag = 0;
  char *aopt, *uopt = NULL, *dopt = NULL, *copt = NULL;
  char *title = NULL, *artist = NULL, *album = NULL, *name = NULL;

  int pipeu = 0;
  unsigned int mem_unit = 0;
  int compat_mode;
  long int dev = 0;
  char dev_string[64];

  int total, i, j;
  long int ticks, ttime;
  int lflag = 0, hflag = 0, iflag = 0, vflag = 0, fflag = 0, cflag = 0;
  int jflag = 0;
  rio_info_t     *info;

  int recovery = 0;

  struct stat statinfo;

  struct option long_options[] = {
    {"upload",  1, 0, 'a'},
    {"bulk",    0, 0, 'b'},
    {"download",1, 0, 'c'},
    {"delete",  1, 0, 'd'},
    {"debug",   0, 0, 'e'},
    {"format",  0, 0, 'f'},
    {"shell",   0, 0, 'g'},
    {"help",    0, 0, 'h'},
    {"info",    0, 0, 'i'},
    {"playlist", 0, 0, 'j'},
    {"update",  1, 0, 'u'},
    {"artist",  1, 0, 's'},
    {"title" ,  1, 0, 't'},
    {"album" ,  1, 0, 'r'},
    {"name",    1, 0, 'n'},
    {"memory",  1, 0, 'm'},
    {"device",  1, 0, 'o'},
    {"pipe",    0, 0, 'p'},
    {"list",    0, 0, 'l'},
    {"version", 0, 0, 'v'},
    {"recovery",0, 0, 'z'}
  };

  /* this doesnt do much of anything anymore */
  compat_mode = 0;

  sprintf(dev_string, "Device");
      
  /*
    find out if rioutil is running on a tty
    if you do not want coloring, replace this line with:
    is_a_tty = 0
  */
  is_a_tty = isatty(1);

  while((c = getopt_long(argc, argv, "W;a:bgld:ec:u:s:t:r:m:p:o:n:fh?ivgzj",
			 long_options, &option_index)) != -1){
    switch(c){
    case 'a':
      if (apush == (aflag - 1)) {
	if (aopt)
	  push(mem_unit, title, artist, album, aopt, 0);
	
	apush++;
	title = artist = album = NULL;
      }
      
      aflag++;
      aopt = optarg;
      
      break;
    case 'b':
      bflag = 1;
      
      break;
    case 'g':
      gflag = 1;
      
      break;
    case 'e':
      elvl++;
      
      break;
    case 'n':
      nflag = 1;
      name = optarg;
      
      break;
    case 'v':
      vflag = 1;
      
      break;
    case 'l':
      lflag = 1;

      break;
    case 'd':
      dflag = 1;
      dopt = optarg;

      break;
    case 'c':
      cflag = 1;
      copt = optarg;

      break;
    case 'u':
      uflag = 1;
      uopt = optarg;

      break;
    case 't':
      title = optarg;

      break;
    case 'r':
      album = optarg;

      break;
    case 'm':
      mflag = 1;
      mem_unit = atoi(optarg);

      break;
    case 'o':
      dev = strtol(optarg, NULL, 0);
      sprintf(dev_string, "Device %s", optarg);

      break;
    case 'p':
      pipeu = 1;

      break;
    case 's':
      artist = optarg;

      break;
    case 'f':
      fflag = 1;

      break;
    case 'h':
    case '?':
      hflag = 1;

      break;
    case 'i':
      iflag = 1;

      break;
    case 'j':
      jflag = 1;

      break;
    case 'z':
      recovery = 1;

      break;
    default:
      printf("Unrecognized option -%c.\n\n", c);
      usage();
    }
  }

  if (apush == (aflag - 1))
    push(mem_unit, title, artist, album, aopt, 0);
  
  if (vflag) {
    version();

    exit (1);
  }

  if (!gflag && !bflag && !aflag && !dflag && !uflag &&
      !fflag && !iflag && !lflag && !nflag && !cflag && 
      !pipeu && !jflag)
      usage();

  if (recovery && (!fflag || !uflag) && (dflag || aflag || iflag || lflag ||
					 nflag || gflag || pipeu || cflag)) {
    fprintf (stderr, "Recovery mode (-z) can only be used with -f and -u.\n");
    exit(1);
  }

  if (jflag && (gflag || bflag || aflag || dflag || uflag ||
		fflag || iflag || lflag || nflag || cflag ||
		pipeu)) {
    fprintf (stderr, "Playlist creation mode cannot take any additional flags.\n");
    exit (1);
  }
  
  if (!recovery)
    fprintf (stderr, "Attempting to open Rio and retrieve song list....");
  else
    fprintf (stderr, "Attempting to open Rio for recovery....");

  current_rio = open_rio (current_rio, dev, elvl, (recovery)?0:1);

  if (!current_rio || !current_rio->dev) {
      fprintf(stderr, "\n%s not found.\n", dev_string);
      fprintf(stderr, "library tried to use method: %s\n",
	      return_conn_method_rio ());
      
      return 1;
  }
  
  fprintf (stderr, "done\n");

  if (jflag)
    return create_playlist (current_rio, argc, argv);
      
  if (mflag) {
    if (mem_unit >= return_mem_units_rio (current_rio)) {
      fprintf(stderr, "Invalid memory unit: %d\n", mem_unit);
      fprintf(stderr, "Max valid memory unit: %d\n",
	      (current_rio->info.total_memory_units-1));
      
      return 1;
    }
  }

  if (fflag) {
    if (format_mem_rio (current_rio, mem_unit) == URIO_SUCCESS) {
        printf("Rio memory format complete.\n");
        close_rio (current_rio);
        return 0;
    }
    else {
        printf("Rio memory format did not complete.\n");
        close_rio (current_rio);
        return 1;
    }
  }
  
  /* set the progress bar function */
  set_progress_rio (current_rio, ((is_a_tty) ? progress : progress_no_tty),
		    NULL);

  if (gflag)
    enter_shell(current_rio, mflag, mem_unit);

  /*
    New download routine acts like new delete routine
  */
  if (cflag) {
      download_track(current_rio, copt, mflag);
      close_rio (current_rio);

      return 0;
  }

  if (pipeu) {
    upload_from_pipe_rio (current_rio, mem_unit, 0 /* stdin */, argv[optind+1], artist,
			  album, title, atoi (argv[optind]), atoi (argv[optind+2]),
			  atoi (argv[optind+3]));

    return 0;
  }
  
  if (nflag){
    info = return_info_rio (current_rio);

    sprintf(info->name, name, 16);

    if (set_info_rio (current_rio, info) == URIO_SUCCESS) {
        printf("rename rio complete.\n");
	free(info);
        close_rio (current_rio);

	return 0;
    } else {
        printf("rename rio did not complete.\n");
	free(info);
        close_rio (current_rio);

	return 1;
    }
  }

  if (iflag) {
    info = return_info_rio (current_rio);
    print_info(info, return_type_rio (current_rio), mflag, mem_unit);
    free(info);
    
    if (lflag)
      printf("\n");
  }
  
  if (lflag)
    new_printfiles(mflag, mem_unit);

  if (uflag){
    printf("Updating firmware, this should take about 30 seconds to complete.\n");
    if (update_rio (current_rio, uopt) == URIO_SUCCESS) {
        printf("Rio update completed successfully.\n");
        close_rio (current_rio);
        return 0;
    }
    else {
        printf("Rio update was not completed successfully.\n");
        close_rio (current_rio);
        return 1;
    }
  }

  /* new delete routine handles new syntax! */
  if (dflag){
      if (delete_track(current_rio, dopt, mem_unit) == URIO_SUCCESS) {
          printf("Rio delete track(s) completed successfully.\n");
          close_rio (current_rio);
          return 0;
      }
      else {
          printf("Rio delete track(s) did not completed successfully.\n");
          close_rio (current_rio);
          return 1;
      }
  }
  
  if (bflag) {
      for (i = optind - 1 ; i < argc ; i++) {
	  if (argv[i][0] != '-') {
	      push(mem_unit, title, artist, album, argv[i], 0);
	      aflag++;
	  }
      }
  }

  if (aflag) {
    for (i = 0 ; i < return_mem_units_rio (current_rio) ; i++)
      printf("Free space on %s is %03.01f MB.\n",
	     current_rio->info.memory[i].name,
	     (float)return_free_mem_rio (current_rio, i) / 1024.0);
    
    if (add_track(current_rio) == URIO_SUCCESS) {
      close_rio (current_rio);

      return 0;
    } else {
      close_rio (current_rio);

      return 1;
    }
  }
  
  close_rio (current_rio);
  
  return 0;
}

static void dir_add_songs (char *filename, int depth, int mem_unit) {
  DIR *dir_fd;
  struct dirent *entry;
  char *path_temp;

  if (depth > MAX_DEPTH_RIO)
    return;

  dir_fd = opendir (filename);

  while (entry = readdir (dir_fd)) {
    path_temp = calloc (strlen(filename) + strlen(entry->d_name) + 1, 1);
    sprintf (path_temp, "%s/%s", filename, entry->d_name);

    if (entry->d_name[0] != '.')
      push_top (mem_unit, NULL, NULL, NULL, path_temp, depth + 1);
  }

  closedir (dir_fd);
}

int add_track(rios_t *rio){
  struct upload_stack *p;
  struct stat statinfo;
  char *tmp;
  
  int ret;
  int free_size;
  
  fprintf(stderr, "Setting up signal handler\n");
  signal (SIGINT, aborttransfer);
  signal (SIGKILL, aborttransfer);
  
  while (p = pop()) {
    if (stat(p->info.filename, &statinfo) < 0) {
      printf("\nCould not stat %s!\n", p->info.filename);
      return ENOFILE;
    }
    
    /* add files in directory */
    if (S_ISDIR (statinfo.st_mode) ) {
      dir_add_songs (p->info.filename, p->info.recursive_depth, p->info.mem_unit);
      continue;
    }

    /* this macro is defined in sys/stat.h */
    if (!S_ISREG(statinfo.st_mode)) {
      printf("\n%s: Not a regular file!\n", p->info.filename);
      
      return ENOFILE;
    }
    
    tmp = (char *)basename(p->info.filename);
    if (strlen(tmp) <= 32)
      printf("%32s ", basename(p->info.filename));
    else {
      /* long filenames are truncated */
      tmp = (char *)malloc(32);
      strncpy(tmp, (char *) basename (p->info.filename), 14);
      strncpy(tmp + 14, "...", 3);
      strncpy(tmp + 17, p->info.filename +
	      (strlen(p->info.filename) - 14), 14);
      printf("%32s ", tmp);
      
      free(tmp);
    }
    
    printf("[%03.01f MB]: ", (double)statinfo.st_size / 1048576.0);
    
    free_size = return_free_mem_rio (rio, p->info.mem_unit);
    if ( (free_size < statinfo.st_size/1024) &&
	 (return_mem_units_rio (current_rio) > 1) ) {
      p->info.mem_unit = ((p->info.mem_unit == 0) ? 1 : 0);
      free_size = return_free_mem_rio (rio, p->info.mem_unit);
    }
    
    if (free_size < statinfo.st_size/1024) {
      printf(" Insufficient space\n");
      ret = ERIOFULL;
      break;
    }
    
    if ( (ret = add_song_rio (rio, p->info.mem_unit, p->info.filename, 
			      p->info.artist, p->info.title, p->info.album)) == URIO_SUCCESS )
      printf(" Complete [memory %i]\n", p->info.mem_unit);
    else if (ret == ERIOFULL) {
      printf(" Incomplete: Insufficient space\n");

      continue;
    } else if (ret == EUNSUPP) {
      printf(" Incomplete: Not supported\n");

      continue;
    } else {
      printf(" Incomplete: An error occurred\n");
      break;
    }	

    if (p->info.recursive_depth != 0)
      free (p->info.filename);
  }

  return ret;
}


int delete_track(rios_t *rio, char *dopt, u_int32_t mem_unit) {
  char *current, *next, *end;
  int track, track_end, track_count, i;
  
  current = dopt + strspn(dopt, " ,");
  next = NULL;
  end = dopt + strlen(dopt) - 1;

  /*
    first break it into comma (or space) seperated segments
  */

  while(current) {
    next = strpbrk(current, " ,");
    if(next) {
      *next = '\0';
      next++;
      next = next + strspn(next, " ,");
    }
    if(next > end)
      next = NULL;

    /*
      dash seperated pair
    */
    if(strstr(current, "-")) {
      track_count = sscanf(current, "%d - %d", &track, &track_end);
      if(track_count == 2) {
        for (i = track; i <= track_end; i++) {
          if (delete_file_rio (rio, mem_unit, i) == URIO_SUCCESS)
            printf("File %i deleted.\n", i);
          else {
            printf("File %i could not be deleted.\n", i);
            return EDELETE;
          }
        }
      }
    }
    /*
      single track
    */
    else {
      track_count = sscanf(current, "%d", &track);
      if(track_count == 1) {
        if (delete_file_rio (rio, mem_unit, track) == URIO_SUCCESS)
          printf("File %i deleted.\n", track);
        else {
          printf("File %i could not be deleted.\n", track);
          return EDELETE;
        }
      }
    }

    current = next;
  }
  return URIO_SUCCESS;
}

int download_track(rios_t *rio, char *copt, u_int32_t mem_unit){
  int dtl, ret;
  char *breaker;
  char *tmp;
  u_int32_t size;
  
  fprintf(stderr, "Setting up signal handler\n");
  signal (SIGINT, aborttransfer);
  signal (SIGKILL, aborttransfer);

  /*
    "a-b"
    
    downloads file a through file b
  */
  if (breaker = strstr(copt, "-")) {
    *breaker = 0;
    
    for (dtl = atoi(copt) ; dtl <= atoi(breaker + 1) ; dtl++) {
      if ((tmp = return_file_name_rio (rio, dtl, mem_unit)) != NULL) {
	printf("%32s [%03.01f MB]:", tmp,
	       (float)return_file_size_rio (rio, dtl, mem_unit)/1048576.0);
	free(tmp);
      }
      
      if ((ret = download_file_rio (rio, mem_unit, dtl, NULL)) == URIO_SUCCESS)
	printf(" Download complete.\n", dtl);
      else {
	printf("\nFile %i could not be downloaded.\n", atoi(copt));
	
	if (ret == EUNSUPP)
	  printf ("Player does not support downloading\n");
	return -1;
      }
    }
    
    /*
      "a b c d"
      
      downloads files a,b,c and d
    */
  } else if (breaker = strstr(copt, " ")) {
    do {
      *breaker = 0;
      
      if ((tmp = return_file_name_rio (rio, atoi(copt), mem_unit)) != NULL) {
	printf("%32s [%03.01f MB]:", tmp,
	       (float)return_file_size_rio (rio, atoi(copt), mem_unit)/1048576.0);
	free(tmp);
      }
      
      if (download_file_rio (rio, mem_unit, atoi(copt), NULL) == URIO_SUCCESS)
	printf(" Download complete.\n", atoi(copt));
      else {
	printf("File %i could not be downloaded.\n", atoi(copt));
	return -1;
      }
      
      copt = breaker + 1;
    } while (breaker = strstr(copt, " "));
    
    if ((tmp = return_file_name_rio (rio, atoi(copt), mem_unit)) != NULL) {
      printf("%32s [%03.01f MB]:", tmp,
	     (float)return_file_size_rio (rio, atoi(copt), mem_unit)/1048576.0);
      free(tmp);
    }
    
    if (download_file_rio (rio, mem_unit, atoi(copt), NULL) == URIO_SUCCESS)
      printf(" Download complete.\n", atoi(copt));
    else {
      printf("\nFile %i could not be downloaded.\n", atoi(copt));
      
      if (ret == EUNSUPP)
	printf ("Player does not support downloading\n");

      return -1;
    }
    
    /*
      download a single file
    */
  } else {
    if ((tmp = return_file_name_rio (rio, atoi(copt), mem_unit)) != NULL) {
      printf("%32s [%03.01f MB]:", tmp,
	     (float)return_file_size_rio (rio, atoi(copt), mem_unit)/1048576.0);
      free(tmp);
    }
    
    if (download_file_rio (rio, mem_unit, atoi(copt), NULL) == URIO_SUCCESS)
      printf(" Download complete.\n", atoi(copt));
    else {
      printf("\nFile %i could not be downloaded.\n", atoi(copt));
      
      if (ret == EUNSUPP)
	printf ("Player does not support downloading\n");

      return -1;
    }
  }
        
  return URIO_SUCCESS;
}
  
static int intwidth(int i) {
  int j = 1;

  while (i /= 10)
    j++;

  return ((j > 10) ? 10 : j);
}

static void new_printfiles(int mflag, int mem_unit) {
  file_list *tmpf;
  int j;
  int id_width;
  int size_width;
  int minutes_width;
  int header_width;
  int max_title_width = strlen("Title");
  int max_name_width = strlen("Name");
  int max_id = 0;
  int max_size = 0;
  int max_time = 0;
  int start_mem_unit, num_mem_units;
  
  file_list **flst;
  
  if (mflag) {
    start_mem_unit = mem_unit;
    num_mem_units = 1;
  } else {
    start_mem_unit = 0;
    num_mem_units = return_mem_units_rio (current_rio);
  }
  
  flst = (file_list**) malloc(sizeof(file_list *) * num_mem_units);
  
  for (j = start_mem_unit ; j < (start_mem_unit + num_mem_units); ++j) {
    flst[j] = return_list_rio (current_rio, j, RALL);
    
    for (tmpf = flst[j]; tmpf ; tmpf = tmpf->next) {
      max_title_width = max(max_title_width,strlen(tmpf->title));
      max_name_width = max(max_name_width,strlen(tmpf->name));
      max_id = max(max_id, tmpf->num);
      max_size = max(max_size,tmpf->size);
      max_time = max(max_time,tmpf->time);
    }
  }
  
  id_width = intwidth(max_id);
  id_width++;
  
  size_width = intwidth(max_size/1024);
  
  minutes_width = intwidth(max_time / 60);
  minutes_width++;
  
  header_width = printf("%*s | %*s |  %*s   %*s:%2s %*s %s\n",
			id_width,
			"id",
			max_title_width, "Title",
			max_name_width, "Name",
			minutes_width, "mm", "ss",
			size_width, "Size", "Bitrate");
  
  for (j = 0; j < header_width; ++j)
    putchar('-');
  
  putchar('\n');
  
  for (j = start_mem_unit ; j < (start_mem_unit + num_mem_units); ++j) {
    if (is_a_tty)
      printf("[%im", 33 + j);

    printf("%s:\n", current_rio->info.memory[j].name);

    if (is_a_tty)
      printf("[m");

    
    for (tmpf = flst[j]; tmpf ; tmpf = tmpf->next) {
      printf("%*i | %*s |  %*s | %*i:%02i %*i %*i\n",
	     id_width, tmpf->num,
	     max_title_width, tmpf->title,
	     max_name_width, tmpf->name,
	     minutes_width, (tmpf->time / 60),
	     (tmpf->time % 60),
	     size_width, tmpf->size / 1024, 7, tmpf->bitrate);
    }
    
    free_file_list (flst[j]);
  }
  
  free (flst);
  
  printf ("\n");
}

void usage(void){
  printf("Usage: rioutil <OPTIONS>\n\n");
  printf("Interface with Diamond MM MP3 players.\n");
  printf("An option is required, and if you use either the -t or\n");
  printf("the -s options you must use them with the -a option\n\n");

  printf(" uploading:\n");
  printf("  -a, --upload=<file>    upload an track\n");
  printf("  -b, --bulk=<filelist>  upload mutiple tracks\n");
  printf("  -u, --update=<file>    update with a new firmware\n\n");

  printf(" uploading from pipe:\n");
  printf("  -p, --pipe <mp3?(1 or 0)> <filename> <bitrate> <samplerate>\n\n");

  printf("  -s, --artist=<string>  artist. MAX:63 chars\n");
  printf("  -t, --title=<string>   title.  MAX:63 chars\n");
  printf("  -r, --album=<string>   album.  MAX:63 chars\n\n");


  printf(" other commands:\n");
  printf("  -j, --playlist <name> <list of mem_unit,song> create playlist (S-Series and newer)\n");
  printf("       i.e. rioutil -j fubar 0,0 1,0     (song 0 on mem_unit 0, song 0 on mem_unit 1)\n");
  printf("  -i, --info             rio info\n");
  printf("  -l, --list             list tracks\n");
  printf("  -f, --format           format rio memory (default is internal)\n");
  printf("  -n, --name=<string>    change the name. MAX:15 chars\n");
  printf("  -c, --download=<int>   download a track(s)\n");
  printf("  -d, --delete=<int>     delete a track(s)\n\n");

  printf(" options:\n");
#if !defined(__FreeBSD__) || !defined(__NetBSD__)
  printf("  -o, --device=<int>     minor number of rio (assigned by driver), /dev/usb/rio?\n");
#else
  printf("  -o, --device=<int>     minor number of rio (assigned by driver), /dev/urio?\n");
#endif
  printf("  -m, --memory=<int>     memory unit to upload/download/delete/format to/from\n");
  printf("  -e, --debug            increase verbosity level.\n");
  printf("  -z, --recovery         use recovery mode. for use with players in \"upgrader\" mode\n");

  printf(" rioutil info: librioutil driver: %s\n", return_conn_method_rio ());
  printf("  -v, --version          print version\n");
  printf("  -?, --help             print this screen\n\n");

  exit (0);
}

#define TOTAL_MARKS  20

void progress (int x, int X, void *ptr) {
  int nummarks = (x * TOTAL_MARKS) / X;
  int percent = (x * 100) / X;
  char m[] = "-\\|/";
  char HASH_MARK;
  int i;
  char HASH_BARRIER = '>';
  char NO_HASH      = ' ';

  if (percent != 100)
    HASH_MARK  = '-';
  else
    HASH_MARK  = '*';

  printf("%c [[34m", m[percent%4]);
  
  for (i = 0 ; i < TOTAL_MARKS ; i++){
    if (i < nummarks)
      putchar(HASH_MARK);
    else if (i == nummarks)
      putchar(HASH_BARRIER);
    else
      putchar(NO_HASH);
  }

  printf("[m] [37m%3i[m%%", percent);

  if (x != X)
    for (i = 0 ; i < (TOTAL_MARKS + 9) ; i++) putchar('\b');

  fflush(stdout);
}

static void progress_no_tty(int x, int X, void *ptr) {
  int nummarks = (x * TOTAL_MARKS) / X;

  if (nummarks > last_nummarks) {
    int i;
    for (i = nummarks - last_nummarks ; i > 0 ; --i)
      putchar('.');
    
    fflush(stdout);
    last_nummarks = nummarks;
  }
}

void push(int mem_unit, char *title, char *artist, char *album,
	  char *filename, int recursive_depth) {
  struct upload_stack **p;
  
  for ( p = &head ; (*p) ; p = &((*p)->next));
  
  *p = (struct upload_stack *)malloc(sizeof(struct upload_stack));
  
  (*p)->info.mem_unit = mem_unit;
  (*p)->info.title = title;
  (*p)->info.artist = artist;
  (*p)->info.album = album;
  (*p)->info.filename = filename;
  (*p)->info.recursive_depth = recursive_depth;

  (*p)->next = NULL;
}

void push_top (int mem_unit, char *title, char *artist, char *album,
	       char *filename, int recursive_depth) {
  struct upload_stack *p;
  
  p = (struct upload_stack *)malloc(sizeof(struct upload_stack));
  p->next = head;
  head = p;
  
  p->info.mem_unit = mem_unit;
  p->info.title = title;
  p->info.artist = artist;
  p->info.album = album;
  p->info.filename = filename;
  p->info.recursive_depth = recursive_depth;
}

struct upload_stack *pop(void) {
  struct upload_stack *p;
  
  if (!head)
    return NULL;
  
  p = head;
  head = head->next;
  return p;
}

void version(void){
  printf("%s %s\n", PACKAGE, VERSION);
  printf("Copyright (C) 2003 Nathan Hjelm\n\n");
  
  printf("%s comes with NO WARRANTY.\n", PACKAGE);
  printf("You may redistribute copies of %s under the terms\n", PACKAGE);
  printf("of the GNU Lesser Public License.\n");
  printf("For more information about these issues\n");
  printf("see the file named COPYING in the %s distribution.\n", PACKAGE);
  
  exit (0);
}

void print_info(rio_info_t *info, int type, int mflag, int mem_unit) {
  int i, j, ticks, ttime;
  int start_mem_unit, num_mem_units;
  int nfiles;
  int ptt = 0, ptf = 0;
  float ptu, used, free_mem;

  float size_div, total;

  if (mflag) {
    start_mem_unit = mem_unit;
    num_mem_units = 1;
  } else {
    start_mem_unit = 0;
    num_mem_units = return_mem_units_rio (current_rio);
  }

  size_div = 1024.0;
  
  printf("[37mName[m: %s\n", info->name);

  /* these values cannot be read/changed with the current S-Series firmware, so
     dont bother printing anything */
  if (type == RIO800 || type == RIO600 || type == RIORIOT ||
      type == PSAPLAY) {
    printf("Volume: %i\n", info->volume);
    printf("Repeat: %s\n", repeatStates[info->repeat_state]);
    if (type == RIORIOT) {
      printf("Random: %s\n", randomStates[info->random_state]);
      printf("'The' Filter: %s\n", theFilterStates[info->the_filter_state]);
    }
    printf("Sleep Time: %s\n",sleepStates[info->sleep_time]);
    printf("Bass: %i\n",-(info->bass - 6));
    printf("Treble: %i\n",-(info->treble - 6));
    
    if (type != RIORIOT) {
      printf("Equilizer: %s\n", equilizerStates[info->eq_state]);
      printf("Programmed to Playlist: %i\n", info->playlist);
    }
    
    if (type == RIO800 || type == RIORIOT)
      printf("Contrast: %i\n", info->contrast);

    printf("Backlight: %s\n", lightStates[info->light_state]);

  }
    
  printf("\n[37mFirmware Version[m: %01.02f\n", info->version);
  
  printf("[37mMemory units[m: %i\n\n", return_mem_units_rio (current_rio));
    
  for (j = start_mem_unit ; j < (start_mem_unit + num_mem_units); j++) {
    printf("Memory unit %i: %s\n", j, current_rio->info.memory[j].name);
    
    total    = (float) return_total_mem_rio (current_rio, j) / size_div;
    used     = (float) return_used_mem_rio (current_rio, j)  / size_div;
    free_mem = (float) return_free_mem_rio (current_rio, j)  / size_div;
    nfiles   = return_num_files_rio (current_rio, j);
    ttime    = return_time_rio (current_rio, j);
      
    ticks = 50 * (used / total);
    
    printf("[%im", 33 + j);
    printf("Free: %03.01f MB (", free_mem);
    printf("%03.01f MB Total)\n", total);
    printf("Used: %03.01f MB in %i files\n", used, nfiles);
    
    printf("[");
    for(i = 1; i <= 50 ; i++) {
      if (i <= ticks)
	putchar('#');
      else
	putchar(' ');
    }

    printf("] %03.01f %%\n\n", 100.0 * (used / total));
    
    printf("Total Time: %02i:%02i:%02i\n", (int)ttime / 3600, 
	   (int)(ttime % 3600) / 60, (int)ttime % 60);
    
    ptt += ttime;
    ptu += used;
    ptf += nfiles;
        
    printf("[m\n");
  }

  printf("Player Space Used: %03.01f MB in %i files\n", used, nfiles);
  printf("Total Player Time: %02i:%02i:%02i\n", (int)ptt / 3600, 
	 (int)(ptt % 3600) / 60, (int)ptt % 60);
}

#define PROMPT "% "

void print_commands(void) {
  printf("Shell more is experimental.\n");
  printf("Commands:\n");
  printf("exit, quit    : exit shell.\n");
  printf("list          : list files.\n");
  printf("format, erase : erase current memory.\n");
  printf("memory        : set memory unit.\n");
  printf("info          : rio info.\n");
  printf("delete        : delete file off current memory unit.\n");
  printf("download      : download a file off current memory unit.\n");
  printf("help, ?       : print this dialog.\n");
}

/* this is more for testing library than anything else */
void enter_shell(rios_t *rio, int mflag, int mem_unit){
    int length = 255;
    char this_line[length];
    int line_len;
    int memory = mem_unit;

    printf("Entering rioutil shell... (doesn't do much right now) ");

    while (1) {
	printf("\n%s", PROMPT);
	fgets(this_line, length, stdin);

	/* exit shell) */
	if ( (strstr(this_line, "exit") == this_line) ||
	     (strstr(this_line, "quit") == this_line) )
	    break;
	else if ( (strstr(this_line, "info") == this_line) ||
		  (strstr(this_line, "get info") == this_line) ){
	    rio_info_t *info = return_info_rio (rio);

	    print_info(info, return_type_rio(rio), mflag, memory);
	} else if ( (strstr(this_line, "format") == this_line) ||
		    (strstr(this_line, "erase") == this_line) ){
	    if (format_mem_rio (rio, memory) == URIO_SUCCESS)
		printf("Format complete");
	    else
		printf("Error!");
	} else if (strstr(this_line, "memory") == this_line){
	    memory = atoi(this_line+8);

	    if (memory >= rio->info.total_memory_units)
		memory = rio->info.total_memory_units - 1;
	    else if (memory < 0)
		memory = 0;

	    printf("Memory unit now %i", memory);
	} else if ( (strstr(this_line, "upload") == this_line) ||
		    (strstr(this_line, "add") == this_line) ){
	    char *tmp = this_line;

	    
	} else if ( (strstr(this_line, "delete") == this_line) ||
		    (strstr(this_line, "remove") == this_line) ){
	    char *tmp;

	    for (tmp = this_line ; *tmp != ' ' ; tmp++);

	    delete_track(rio, tmp + 1, memory);
	} else if (strstr(this_line, "download") == this_line){
	    char *tmp;

	    for (tmp = this_line ; *tmp != ' ' ; tmp++);

	    download_track(rio, tmp + 1, memory);
	} else if (strstr(this_line, "list") == this_line){
	    new_printfiles(mflag, memory);
	} else if ( (strstr(this_line, "help") == this_line) ||
		    (strstr(this_line, "?") == this_line) ) {
	    print_commands();
	} else {
	    printf("Unknown command: %s", this_line);
	}
    }

    close_rio (rio);

    exit(0);
}

int create_playlist (rios_t *rio, int argc, char *argv[]) {
  int nsongs = argc - optind - 1;
  int i, ret;
  int *mems, *songs;

  if (nsongs < 1) {
    fprintf (stderr, "No songs to add!\n");
    return 1;
  }

  fprintf (stderr, "Creating playlist %s on Rio.\n", argv[optind]);

  mems = calloc (sizeof (int), nsongs);
  songs = calloc (sizeof (int), nsongs);
  
  optind++;

  for (i = 0 ; i < nsongs ; i++) {
    sscanf (argv[optind+i], "%d,%d", &mems[i], &songs[i]);
  }

  ret = create_playlist_rio (rio, argv[optind-1], songs, mems, nsongs);

  if (ret < 0)
    fprintf (stderr, "Could not create playlist!\n");
  else
    fprintf (stderr, "Playlist creation successfull.\n");

  free (mems);
  free (songs);

  return 0;
}
