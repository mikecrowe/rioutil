/**
 *   (c) 2001-2006 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.2 main.c
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

#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#include <unistd.h>
#include <getopt.h>

#include <signal.h>

#include <sys/stat.h>
#include <dirent.h>

#include <errno.h>

#if defined HAVE_LIBGEN_H
#include <libgen.h>
#endif

#include "rio.h"
#include "main.h"

/* a simple version of basename that returns a pointer into x where the basename
   begines or NULL if x has a trailing slash. */
char *basename_simple (char *x){
  int i;
 
  for (i = strlen(x) - 1 ; x[i] != '/'; i--);

  return ((i == strlen(x) - 1) ? NULL : &x[i+1]);
}

#define MAX_DEPTH_RIO 3
#define TOTAL_MARKS  20
#define PROMPT "% "

#define max(a, b) ((a > b) ? a : b)

static rios_t *current_rio;
static int is_a_tty;
static int last_nummarks;

static void usage (void);
static void print_version (void);

static void progress_no_tty(int x, int X, void *ptr);
static void new_printfiles(rios_t *rio, int mflag, int mem_unit);
void print_info(rios_t *rio, int mflag, int mem_unit);
void enter_shell(rios_t *rio, int mflag, int mem_unit);
int create_playlist (rios_t *rio, int argc, char *argv[]);
int overwrite_file (rios_t *rio, int mem_unit, int argc, char *argv[]);


static struct upload_stack upstack = {NULL, NULL};

static void upstack_push (int mem_unit, char *title, char *artist, char *album, char *filename, int recursive_depth);
static void upstack_push_top (int mem_unit, char *title, char *artist, char *album, char *filename, int recursive_depth);
static struct _song *upstack_pop (void);
static void free__song (struct _song *);


/* signal handler */

static void aborttransfer (int sigraised) {
  current_rio->abort = 1;
}

/******************/


int main (int argc, char *argv[]) {
  int c, option_index;

  int aflag = 0, dflag = 0, uflag = 0, nflag = 0;
  int lflag = 0, iflag = 0, fflag = 0, cflag = 0;
  int jflag = 0, Oflag = 0, elvl = 0, bflag = 0, mflag = 0, gflag = 0;
  int pipeu = 0;
  int recovery = 0;

  char *uopt = NULL, *dopt = NULL, *copt = NULL;
  char *title = NULL, *artist = NULL, *album = NULL, *name = NULL;

  unsigned int mem_unit = 0;
  long int dev = 0;

  int i, ret = 0;
  rios_t rio;

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
    {"playlist",0, 0, 'j'},
    {"nocolor", 0, 0, 'k'},
    {"list",    0, 0, 'l'},
    {"memory",  1, 0, 'm'},
    {"name",    1, 0, 'n'},
    {"device",  1, 0, 'o'},
    {"overwrite",0,0, 'O'},
    {"pipe",    0, 0, 'p'}, 
    {"album" ,  1, 0, 'r'},
    {"artist",  1, 0, 's'},
    {"title" ,  1, 0, 't'},
    {"update",  1, 0, 'u'},
    {"version", 0, 0, 'v'},
    {"recovery",0, 0, 'z'}
  };
      
  /*
    find out if rioutil is running on a tty
    if you do not want coloring, replace this line with:
    is_a_tty = 0
  */
  is_a_tty = isatty(1);

  while((c = getopt_long(argc, argv, "W;a:bgld:ec:u:s:t:r:m:p:o:n:fh?ivgzjkO",
			 long_options, &option_index)) != -1){
    switch(c){
    case 'a':
      upstack_push (mem_unit, title, artist, album, optarg, 0);
      aflag = 1;

      break;
    case 'b':
      bflag = 1;
      
      break;
    case 'c':
      cflag = 1;
      copt = optarg;

      break;
    case 'd':
      dflag = 1;
      dopt = optarg;

      break;
    case 'e':
      elvl++;
      
      break;
    case 'f':
      fflag = 1;

      break;
    case 'g':
      gflag = 1;
      
      break;
    case 'i':
      iflag = 1;

      break;
    case 'j':
      jflag = 1;

      break;
    case 'k':
      is_a_tty = 0;
      break;
    case 'l':
      lflag = 1;

      break;
    case 'm':
      mflag = 1;
      mem_unit = atoi(optarg);

      break;
    case 'n':
      nflag = 1;
      name = optarg;
      
      break;
    case 'o':
      dev = strtol(optarg, NULL, 0);

      break;
    case 'p':
      pipeu = 1;

      break;
    case 'r':
      album = optarg;

      break;
    case 's':
      artist = optarg;

      break;
    case 't':
      title = optarg;

      break;
    case 'u':
      uflag = 1;
      uopt = optarg;

      break;
    case 'v':
      print_version ();

      return 0;
    case 'O':
      Oflag = 1;

      break;
    case 'z':
      recovery = 1;

      break;
    case 'h':
    case '?':
    default:
      if (c != 'h' && c != '?')
	printf ("Unrecognized option -%c.\n\n", c);

      usage ();
    }
  }

  if (bflag) {
    aflag = 1;

    for (i = optind ; i < argc ; i++)
      upstack_push (mem_unit, title, artist, album, argv[i], 0);
  }

  /* print usage and exit if no commands are specified */
  if (!gflag && !aflag && !dflag && !uflag && !fflag && !iflag && !lflag &&
      !nflag && !cflag && !pipeu && !jflag && !Oflag)
      usage();

  /* recovery mode is meant to work only with the format and upgrade commands */
  if (recovery && (!fflag || !uflag) && (dflag || aflag || iflag || lflag ||
					 nflag || gflag || pipeu || cflag ||
					 Oflag)) {
    fprintf (stderr, "Recovery mode (-z) must be used only with -f or -u.\n");
    exit(1);
  }

  if (jflag && (gflag || aflag || dflag || uflag || fflag || iflag || lflag ||
		nflag || cflag || pipeu)) {
    fprintf (stderr, "Playlist creation mode cannot take any additional flags.\n");
    exit (1);
  } else if (Oflag && (gflag || aflag || dflag || uflag || fflag || iflag ||
		       lflag || nflag || cflag || pipeu || jflag)) {
    fprintf (stderr, "File overwrite cannot be used with any other options.\n");
    exit (1);
  }

  
  if (!recovery)
    printf ("Attempting to open Rio and retrieve song list.... ");
  else
    printf ("Attempting to open Rio for recovery.... ");

  fflush (stdout);

  ret = open_rio (&rio, dev, elvl, (recovery) ? 0 : 1);

  current_rio = &rio;

  if (ret != URIO_SUCCESS) {
      fprintf (stderr, "failed!\n");
      fprintf (stderr, "Reason: %s.\n", strerror (-ret));
      fprintf (stderr, "librioutil tried to use method: %s\n", return_conn_method_rio ());
      
      exit (EXIT_FAILURE);
  }
  
  printf ("complete\n");

  /* set the progress bar function */
  set_progress_rio (&rio, ((is_a_tty) ? progress : progress_no_tty),
		    NULL);

  if (mflag) {
    if (mem_unit >= return_mem_units_rio (&rio)) {
      fprintf(stderr, "Invalid memory unit: %d\n", mem_unit);
      fprintf(stderr, "Max valid memory unit: %d\n",
	      (rio.info.total_memory_units-1));
      
      return 1;
    }
  }

  if (iflag) {
    print_info (&rio, mflag, mem_unit);
    
    if (lflag)
      printf("\n");
  }
  
  if (lflag)
    new_printfiles (&rio, mflag, mem_unit);

  if (jflag)
    ret = create_playlist (&rio, argc, argv);
  else if (Oflag)
    ret = overwrite_file (&rio, mem_unit, argc, argv);
  else if (cflag)
    ret = download_tracks (&rio, copt, mflag);
  else if (dflag)
    ret = delete_tracks (&rio, dopt, mem_unit);
  else if (gflag)
    enter_shell(&rio, mflag, mem_unit);
  else if (pipeu)
    ret = upload_from_pipe_rio (&rio, mem_unit, 0 /* stdin */, argv[optind+1], artist,
				album, title, atoi (argv[optind]), atoi (argv[optind+2]),
				atoi (argv[optind+3]));
  else if (fflag) {
    if ((ret = format_mem_rio (&rio, mem_unit)) == URIO_SUCCESS)
      printf(" Rio format command complete.\n");
    else
      printf(" Rio format command failed.\n");
  } else if (nflag){
    rio_info_t *info;
    if ((ret = get_info_rio (&rio, &info)) != URIO_SUCCESS)
      printf ("could not retrieve information from the rio.\n");

    if (ret == URIO_SUCCESS) {
      sprintf(info->name, name, 16);

      if ((ret = set_info_rio (&rio, info)) == URIO_SUCCESS)
        printf ("rename rio complete.\n");
      else
        printf ("rename rio failed.\n");

      free (info);
    }
  } else if (uflag) {
    printf ("Updating firmware, this will take about 30 seconds to complete.\n");

    if ((ret = firmware_upgrade_rio (&rio, uopt)) == URIO_SUCCESS)
      printf (" Rio update completed successfully.\n");
    else
      printf (" Rio update was not completed successfully.\n");
  } else if (aflag) {
    for (i = 0 ; i < return_mem_units_rio (&rio) ; i++)
      printf ("Free space on %s is %03.01f MiB.\n", rio.info.memory[i].name,
	     (float)return_free_mem_rio (&rio, i) / 1024.0);
    
    ret = add_tracks (&rio);
  }

  close_rio (&rio);
  
  return ret;
}

static void dir_add_songs (char *filename, int depth, int mem_unit) {
  struct stat statinfo;
  DIR *dir_fd;
  struct dirent *entry;
  char *path_temp = NULL;

  if (depth > MAX_DEPTH_RIO)
    return;

  dir_fd = opendir (filename);

  while ((entry = readdir (dir_fd)) != NULL) {
    if (path_temp) {
      free (path_temp);
      path_temp = NULL;
    }
      
    path_temp = calloc (strlen(filename) + strlen(entry->d_name) + 1, 1);
    sprintf (path_temp, "%s/%s", filename, entry->d_name);

    if (entry->d_name[0] == '.')
      continue;

    if (stat (path_temp, &statinfo) < 0)
      continue;
    
    if (S_ISDIR (statinfo.st_mode)) {
      dir_add_songs (path_temp, depth + 1, mem_unit);
      continue;
    }

    upstack_push_top (mem_unit, NULL, NULL, NULL, path_temp, depth + 1);
  }

  closedir (dir_fd);
}

int add_tracks (rios_t *rio){
  struct _song *p;
  struct stat statinfo;
  char *file_name;
  char display_name[32];
  int free_size, mem_units, file_namel;
  int ret, i;
  
  fprintf(stderr, "Setting up signal handler\n");
  signal (SIGINT, aborttransfer);
  signal (SIGKILL, aborttransfer);
  
  while ((p = upstack_pop()) != NULL) {
    ret = URIO_SUCCESS;

    if (stat(p->filename, &statinfo) < 0)
      printf("rioutil/src/main.c add_track: could not stat file %s (%s)\n", p->filename, strerror (errno));
    else if (S_ISDIR(statinfo.st_mode))
      /* add files from directory */
      dir_add_songs (p->filename, p->recursive_depth, p->mem_unit);
    else if (!S_ISREG(statinfo.st_mode))
      printf("rioutil/src/main.c add_track: %s is not a regular file!\n", p->filename);
    else {
      file_name = basename_simple (p->filename);
      file_namel = strlen (file_name);

      strncpy (display_name, file_name, 31);

      if (file_namel > 32)
	/* truncate long filenames */
	sprintf (&display_name[14], "...%s", &file_name[file_namel - 14]);

      printf("%32s [%03.1f MiB]: ", display_name, (double)statinfo.st_size / 1048576.0);
    
      /* mem_units will only ever be 1 or 2 */
      mem_units = return_mem_units_rio (rio);

      for (i = 0 ; i < mem_units ; i++) {
	free_size = return_free_mem_rio (rio, p->mem_unit) * 1024;

	if (free_size >= statinfo.st_size)
	  break;

	/* insufficient space on this memory unit, try another */
	p->mem_unit = (p->mem_unit + 1) % mem_units;
      }

      if (i == mem_units)
	ret = -ENOSPC;
      else
	ret = add_song_rio (rio, p->mem_unit, p->filename, p->artist, p->title, p->album);

      if (ret == URIO_SUCCESS) 
	printf(" Complete [memory %i]\n", p->mem_unit);
      else
	printf(" Incomplete: %s\n", strerror (-ret));
    }

    free__song (p);
    p = NULL;
  }
  
  return 0;
}

static int download_single_file (rios_t *rio, int file, int mem_unit) {
  int file_size;
  char *file_name;

  int ret;

  file_size = return_file_size_rio (rio, file, mem_unit);
  file_name = return_file_name_rio (rio, file, mem_unit);

  if (file_name != NULL) {
    printf ("%32s [%03.01f MiB]:", file_name, (float)file_size/1048576.0);
    free (file_name);
  } else {
    printf ("No file name associated with file number: %i. Aborting...\n", file);
    return -1;
  }

  if ((ret = download_file_rio (rio, mem_unit, file, NULL)) == URIO_SUCCESS)
    printf(" Download complete.\n");
  else
    printf(" Download failed. Reason: %s.\n", strerror (ret));

  return ret;
}

static int delete_single_file (rios_t *rio, int file, int mem_unit) {
  int ret;
  
  if ((ret = delete_file_rio (rio, mem_unit, file)) == URIO_SUCCESS)
    printf("File %i successfully deleted.\n", file);
  else
    printf("File %i could not be deleted.\n", file);

  return ret;
}

static int parse_input (rios_t *rio, char *copt, u_int32_t mem_unit, int (*fp)(rios_t *, int, int)) {
  int dtl;
  char *breaker;
  
  fprintf(stderr, "Setting up signal handler\n");
  signal (SIGINT, aborttransfer);
  signal (SIGKILL, aborttransfer);

  /*
    "a-b"
    
     runs fp with integers a through b
  */
  if ((breaker = strstr(copt, "-")) != NULL) {
    int low, high;
    
    low  = strtol (copt, NULL, 10);
    high = strtol (breaker + 1, NULL, 10);

    for (dtl = low ; dtl <= high ; dtl++)
      fp (rio, dtl, mem_unit);
    
  } else {
    /*
      "a b c d"
      
      runs fp with integers a, b, c, and d
    */

    char *startp, *endp;
    int file_number = -1;

    startp = copt;

    do {
      endp = startp;

      while ((*startp != '\0') && (endp == startp)) {
	file_number = strtol (startp, &endp, 10);

	if (endp == startp) {
	  /* recognize only strings that are made up of numbers and spaces */
	  if (*startp != ' ') 
	    *startp = '\0';
	  else
	    endp = startp = startp + 1;
	}
      }

      if (*startp != '\0') {
	fp (rio, file_number, mem_unit);
      
	startp = endp;
      }
    } while (*startp != '\0');
  }

  return URIO_SUCCESS;
}

int download_tracks (rios_t *rio, char *copt, u_int32_t mem_unit){
  return parse_input (rio, copt, mem_unit, download_single_file);
}

int delete_tracks (rios_t *rio, char *dopt, u_int32_t mem_unit) {
  return parse_input (rio, dopt, mem_unit, delete_single_file);
}

  
static int intwidth(int i) {
  int j = 1;

  while (i /= 10)
    j++;

  return ((j > 10) ? 10 : j);
}

static void new_printfiles(rios_t *rio, int mflag, int mem_unit) {
  flist_rio_t *tmpf;
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
  
  flist_rio_t **flst;
  
  if (mflag) {
    start_mem_unit = mem_unit;
    num_mem_units = 1;
  } else {
    start_mem_unit = 0;
    num_mem_units = return_mem_units_rio (rio);
  }
  
  flst = (flist_rio_t**) malloc (sizeof(flist_rio_t *) * num_mem_units);
  
  for (j = start_mem_unit ; j < (start_mem_unit + num_mem_units); ++j) {
    if (return_flist_rio (rio, j, RALL, &flst[j]) < 0) {
      printf ("Could not get a copy of the file list for memory unit %i\n", j);

      continue;
    }
    
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
  
  header_width = printf("%*s | %*s |  %*s   %*s:%2s %*s %s %s %s\n",
			id_width,
			"id",
			max_title_width, "Title",
			max_name_width, "Name",
			minutes_width, "mm", "ss",
			size_width, "Size", "Bitrate", "rio_num", "filn");
  
  for (j = 0; j < header_width; ++j)
    putchar('-');
  
  putchar('\n');
  
  for (j = start_mem_unit ; j < (start_mem_unit + num_mem_units); ++j) {
    if (is_a_tty)
      printf("[%im", 33 + j);

    printf("%s:\n", rio->info.memory[j].name);

    if (is_a_tty)
      printf("[m");

    
    for (tmpf = flst[j]; tmpf ; tmpf = tmpf->next) {
      printf("%*i | %*s |  %*s | %*i:%02i %*i %*i 0x%02x %i\n",
	     id_width, tmpf->num,
	     max_title_width, tmpf->title,
	     max_name_width, tmpf->name,
	     minutes_width, (tmpf->time / 60),
	     (tmpf->time % 60),
	     size_width, tmpf->size / 1024, 7, tmpf->bitrate, tmpf->rio_num, tmpf->inum);
    }
    
    free_flist_rio (flst[j]);
  }
  
  free (flst);
  
  printf ("\n");
}

static void print_version (void) {
  printf("%s %s\n", PACKAGE, VERSION);
  printf("Copyright (C) 2003-2006 Nathan Hjelm\n\n");
  
  printf("%s comes with NO WARRANTY.\n", PACKAGE);
  printf("You may redistribute copies of %s under the terms\n", PACKAGE);
  printf("of the GNU Lesser Public License.\n");
  printf("For more information about these issues\n");
  printf("see the file named COPYING in the %s distribution.\n", PACKAGE);
  
  exit (0);
}

static void usage (void) {
  printf("Usage: rioutil <OPTIONS>\n\n");
  printf("Interface with Diamond MM/Sonic Blue/DNNA MP3 players.\n");
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
  printf("  -k, --nocolor          supress ansi color\n");
  printf("  -m, --memory=<int>     memory unit to upload/download/delete/format to/from\n");
  printf("  -e, --debug            increase verbosity level.\n");
  printf("  -z, --recovery         use recovery mode. for use with players in \"upgrader\" mode\n");

  printf(" rioutil info: librioutil driver: %s\n", return_conn_method_rio ());
  printf("  -v, --version          print version\n");
  printf("  -?, --help             print this screen\n\n");

  exit (EXIT_FAILURE);
}

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

  printf("[m] [37;40m%3i[m%%", percent);

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

static struct stack_item *new_stack_item (int mem_unit, char *title, char *artist, char *album,
					  char *filename, int recursive_depth) {
  struct stack_item *p;

  if (filename == NULL) {
    fprintf (stderr, "main.c/new_stack_item: error! called with no path.\n");
    
    exit (EXIT_FAILURE);
  }

  p = (struct stack_item *) calloc (1, sizeof (struct stack_item));
  if (p == NULL) {
    perror ("main.c/new_stack_item: calloc failed");

    exit (EXIT_FAILURE);
  }

  p->data = (struct _song *) calloc (1, sizeof (struct _song));
  if (p->data == NULL) {
    perror ("main.c/new_stack_item: calloc failed");

    exit (EXIT_FAILURE);
  }

  p->data->mem_unit = mem_unit;
  p->data->title    = (title) ? strdup (title) : NULL;
  p->data->artist   = (artist) ? strdup (artist) : NULL;
  p->data->album    = (album) ? strdup (album) : NULL;
  p->data->filename = strdup (filename);
  p->data->recursive_depth = recursive_depth;

  return p;
}

/* upload stack routines */
static void upstack_push (int mem_unit, char *title, char *artist, char *album,
			  char *filename, int recursive_depth) {
  struct stack_item *p;
  
  p = new_stack_item (mem_unit, title, artist, album, filename, recursive_depth);
  p->next = NULL;

  if (upstack.tail != NULL) {
    upstack.tail->next = p;
    upstack.tail = p;
  } else
    upstack.tail = upstack.head = p;
}

static void upstack_push_top (int mem_unit, char *title, char *artist, char *album,
	       char *filename, int recursive_depth) {
  struct stack_item *p;
  
  p = new_stack_item (mem_unit, title, artist, album, filename, recursive_depth);
  p->next = upstack.head;

  if (upstack.head == NULL)
    upstack.tail = p;

  upstack.head = p;
}

static struct _song *upstack_pop(void) {
  struct stack_item *p;
  struct _song *dp;

  if (!upstack.head)
    return NULL;
  
  p = upstack.head;
  upstack.head = p->next;

  if (upstack.head == NULL)
    upstack.tail = NULL;

  dp = p->data;
  free (p);

  return dp;
}

static void free__song (struct _song *p) {
  if (!p)
    return;

  if (p->title)
    free (p->title);
  if (p->artist)
    free (p->artist);
  if (p->album)
    free (p->album);

  free (p->filename);
  free (p);
}




void print_info(rios_t *rio, int mflag, int mem_unit) {
  int i, j, ticks, ttime, type;
  int start_mem_unit, num_mem_units;
  int nfiles = 0;
  int ptt = 0, ptf = 0;

  float ptu, free_mem;
  float used = 0.;
  float size_div, total;
  rio_info_t *info;

  u_int8_t serial_number[16];

  type = return_type_rio (rio);

  get_info_rio (rio, &info);
  return_serial_number_rio (rio, serial_number);

  if (mflag) {
    start_mem_unit = mem_unit;
    num_mem_units = 1;
  } else {
    start_mem_unit = 0;
    num_mem_units = return_mem_units_rio (rio);
  }

  size_div = 1024.0;

  if (is_a_tty)
    printf("[37;40mName[m: %s\n", info->name);
  else
    printf("Name: %s\n", info->name);

  printf ("Serial Number: ");

  for (i = 0 ; i < 16 ; i++)
    printf ("%02x", serial_number[i]);

  printf ("\n");

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

  if (is_a_tty) {
    printf("\n[37:30mFirmware Version[m: %01.02f\n", info->firmware_version);

    printf("[37:30mMemory units[m: %i\n\n", return_mem_units_rio (rio));
  } else {
    printf("\nFirmware Version: %01.02f\n", info->firmware_version);

    printf("Memory units: %i\n\n", return_mem_units_rio (rio));
  }

  for (j = start_mem_unit ; j < (start_mem_unit + num_mem_units); j++) {
    printf("Memory unit %i: %s\n", j, rio->info.memory[j].name);
    
    total    = (float) return_total_mem_rio (rio, j) / size_div;
    used     = (float) return_used_mem_rio (rio, j)  / size_div;
    free_mem = (float) return_free_mem_rio (rio, j)  / size_div;
    nfiles   = return_num_files_rio (rio, j);
    ttime    = return_time_rio (rio, j);
      
    ticks = 50 * (used / total);
    
    if (is_a_tty)
      printf("[%im", 33 + j);

    printf("Free: %03.01f MiB (", free_mem);
    printf("%03.01f MiB Total)\n", total);
    printf("Used: %03.01f MiB in %i files\n", used, nfiles);
    
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

    if (is_a_tty)
      printf("[m\n");
  }

  printf("Player Space Used: %03.01f MiB in %i files\n", used, nfiles);
  printf("Total Player Time: %02i:%02i:%02i\n", (int)ptt / 3600, 
	 (int)(ptt % 3600) / 60, (int)ptt % 60);


  free (info);
}

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

/* this function was meant to test librioutil */
void enter_shell(rios_t *rio, int mflag, int mem_unit){
  int length = 255;
  char this_line[length];
  int memory = mem_unit;
  rio_info_t *info;
  
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
      if (get_info_rio (rio, &info) == URIO_SUCCESS)
	print_info(rio, mflag, memory);
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
    } else if ( (strstr(this_line, "delete") == this_line) ||
		(strstr(this_line, "remove") == this_line) ){
      char *tmp;
      
      for (tmp = this_line ; *tmp != ' ' ; tmp++);
      
      delete_tracks (rio, tmp + 1, memory);
    } else if (strstr(this_line, "download") == this_line){
      char *tmp;
      
      for (tmp = this_line ; *tmp != ' ' ; tmp++);
      
      download_tracks (rio, tmp + 1, memory);
    } else if (strstr(this_line, "list") == this_line){
      new_printfiles(rio, mflag, memory);
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
    fprintf (stderr, " Could not create playlist!\n");
  else
    fprintf (stderr, " Playlist creation successfull.\n");

  free (mems);
  free (songs);

  return 0;
}

int overwrite_file (rios_t *rio, int mem_unit, int argc, char *argv[]) {
  int song;
  int ret;

  sscanf (argv[optind], "%d", &song);
  printf ("Overwriting %s with contents of %s\n", argv[optind], argv[optind+1]);
  ret = overwrite_file_rio (rio, mem_unit, song, argv[optind+1]);

  if (ret < 0)
    printf (" Error %i\n", ret);
  else
    printf (" Complete\n");

  return ret;
}
