/*
  Module       : utils.c
  Purpose      : Various utilities for qiv
  More         : see qiv README
  Policy       : GNU GPL
  Homepage     : http://qiv.spiegl.de/
  Original     : http://www.klografx.net/qiv/
*/

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <gdk/gdkx.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <tiffio.h>
#include <X11/extensions/dpms.h>
#ifdef HAVE_EXIF
#include <libexif/exif-loader.h>
#endif
#include "qiv.h"
#include "xmalloc.h"

#ifdef STAT_MACROS_BROKEN
#undef S_ISDIR
#endif

#if !defined(S_ISDIR) && defined(S_IFDIR)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

/* move current image to .qiv-trash */
int move2trash()
{
  char *ptr, *ptr2, *filename = image_names[image_idx];
  char trashfile[FILENAME_LEN], path_result[PATH_MAX];
  int i;

  if (readonly)
    return 0;

  if(!(ptr = strrchr(filename, '/'))) {   /* search rightmost slash */
    /* no slash in filename */
    strncpy(path_result, filename, PATH_MAX);
    /* move file to TRASH_DIR/filename */
    snprintf(trashfile, sizeof trashfile, "%s/%s", TRASH_DIR, path_result);
  } else {
    /* find full path to file */
    *ptr = 0;
    if(!realpath(filename,path_result)) {
      g_print("Error: realpath failure while moving file to trash\a\n");
      *ptr = '/';
      return 1;
    }
    /* move file to fullpath/TRASH_DIR/filename */
    snprintf(trashfile, sizeof trashfile, "%s/%s/%s", path_result, TRASH_DIR, ptr+1);

    strncat(path_result, "/", PATH_MAX - strlen(path_result) );
    strncat(path_result, ptr + 1, PATH_MAX - strlen(path_result) );
    *ptr = '/';
  }

#ifdef DEBUG
  g_print("*** trashfile: '%s'\n",trashfile);
#endif
  ptr = ptr2 = trashfile;
  if(trashfile[0] == '/') {  /* filename starts with a slash? */
    ptr += 1;
  }
  while((ptr = strchr(ptr,'/'))) {
    *ptr = '\0';
    if(access(ptr2,F_OK)) {
      if(mkdir(ptr2,0700)) {
        g_print("Error: Could not make directory '%s'\a\n",ptr2);
        return 1;
      }
    }
    *ptr = '/';
    ptr += 1;
  }

  unlink(trashfile); /* Just in case it already exists... */
  if(rename(filename,trashfile)) {
    g_print("Error: Could not rename '%s' to '%s'\a\n",filename,trashfile);
    return 1;
  } else {
    qiv_deletedfile *del;

    if (!deleted_files)
      deleted_files = (qiv_deletedfile*)xcalloc(MAX_DELETE,sizeof *deleted_files);

    del = &deleted_files[delete_idx++];
    if (delete_idx == MAX_DELETE)
      delete_idx = 0;
    if (del->filename)
	free(del->trashfile);
    del->filename = filename;
    del->trashfile = strdup(trashfile);
    del->pos = image_idx;

    --images;
    for(i=image_idx;i<images;++i) {
      image_names[i] = image_names[i+1];
    }

    /* If deleting the last file out of x */
    if(images == image_idx)
      image_idx = 0;

    /* If deleting the only file left */
    if(!images)
      exit(0);
  }
  return 0;
}

/* move current image to trash bin */
int move2trashbin()
{
  GFile  *del_file;
  GError *del_error=NULL;
  int    i;
  int    ret=0;

  del_file=g_file_new_for_path(image_names[image_idx]);

  if(g_file_trash(del_file, NULL, &del_error))
  {
    --images;
    for(i=image_idx;i<images;++i) {
      image_names[i] = image_names[i+1];
    }

    /* If deleting the last file out of x */
    if(images == image_idx)
      image_idx = 0;

    /* If deleting the only file left */
    if(!images)
      exit(0);
  }
  else
  {
    printf("%s\n", del_error->message);
    g_error_free(del_error);
    ret=1;
  }
  g_object_unref(del_file);
  return ret;
}

/* copy current image to SELECTDIR */
int copy2select()
{
  char *ptr, *filename = image_names[image_idx];
  char dstfile[FILENAME_LEN], dstfilebak[FILENAME_LEN], tmp[FILENAME_LEN], buf[BUFSIZ];
  int fdi, fdo, n, n2;

  /* try to create something; if select_dir doesn't exist, create one */
  snprintf(dstfile, sizeof dstfile, "%s/.qiv-select", select_dir);
  if((n = open(dstfile, O_CREAT, 0666)) == -1) {
    switch(errno) {
      case EEXIST:
        unlink(dstfile);
        break;
      case ENOTDIR:
      case ENOENT:
        if(mkdir(select_dir, 0777) == -1) {
          g_print("*** Error: Cannot create select_dir '%s': %s\a\n", select_dir, strerror(errno));
          return -1;
        }
        break;
      default:
        g_print("*** Error: Cannot open select_dir '%s': %s\a\n", select_dir, strerror(errno));
        return -1;
    }
  } else {
    close(n);
    unlink(dstfile);
  }

  if((ptr = strrchr(filename, '/')) != NULL) {   /* search rightmost slash */
    ptr++;
  } else {
    ptr = filename;
  }
  snprintf(dstfile, sizeof dstfile, "%s/%s", select_dir, ptr);

#ifdef DEBUG
  g_print("*** selectfile: '%s'\n",dstfile);
#endif
  ptr = dstfile;

  /* Just in case the file already exists... */
  /* unlink(dstfile); */
  snprintf(dstfilebak, sizeof dstfilebak, "%s", dstfile);
  while ((n2 = open(dstfilebak, O_RDONLY)) != -1) {
    close(n2);
    snprintf(tmp, sizeof dstfilebak, "%s.bak", dstfilebak);
    strncpy(dstfilebak, tmp, sizeof dstfilebak);
  }
  if ( strncmp(dstfile, dstfilebak, sizeof dstfile) != 0 ) {
#ifdef DEBUG
    g_print("*** renaming: '%s' to '%s'\n",dstfile,dstfilebak);
#endif
    rename(dstfile, dstfilebak);
  }

  fdi = open(filename, O_RDONLY);
  fdo = open(dstfile, O_CREAT | O_WRONLY, 0666);
  if(fdi == -1 || fdo == -1) {
    g_print("*** Error: Could not copy file: '%s'\a\n", strerror(errno));
  }
  while((n = read(fdi, buf, BUFSIZ)) > 0) write(fdo, buf, n);
  close(fdi);
  close(fdo);

  return 0;
}


/* move the last deleted image out of the delete list */
int undelete_image()
{
  int i;
  qiv_deletedfile *del;
  char *ptr;

  if (readonly)
    return 0;

  if (!deleted_files) {
    g_print("Error: nothing to undelete\a\n");
    return 1;
  }
  if (--delete_idx < 0)
    delete_idx = MAX_DELETE - 1;
  del = &deleted_files[delete_idx];
  if (!del->filename) {
    g_print("Error: nothing to undelete\a\n");
    return 1;
  }

  if (rename(del->trashfile,del->filename) < 0) {
    g_print("Error: undelete_image '%s' failed\a\n", del->filename);
    del->filename = NULL;
    free(del->trashfile);
    return 1;
  }

  /* unlink TRASH_DIR if empty */
  ptr = del->trashfile;
  /* the path without the filename is the TRASH_DIR */
  ptr = strrchr(ptr,'/');
  *ptr = '\0';
  if(rmdir(del->trashfile)) {
    /* we can't delete the TRASH_DIR because there are still files */
  }
  *ptr = '/';

  image_idx = del->pos;
  for(i=images;--i>=image_idx;) {
    image_names[i+1] = image_names[i];
  }
  images++;
  image_names[image_idx] = del->filename;
  del->filename = NULL;
  free(del->trashfile);

  return 0;
}

#define MAXOUTPUTBUFFER 16384
#define MAXLINES 100

/* run a command ... */
void run_command(qiv_image *q, char *n, char *filename, int *numlines, const char ***output)
{
  static char nr[100];
  static char *buffer = 0;
  static const char *lines[MAXLINES + 1];
  int pipe_stdout[2];
  int pid;
  char *newfilename;
  int i;
  struct stat before, after;

  stat(filename, &before);

  if (!buffer)
    buffer = xmalloc(MAXOUTPUTBUFFER + 1);

  *numlines = 0;
  *output = lines;

  snprintf(infotext, sizeof infotext, "Running: 'qiv-command %s %s'", n, filename);

  snprintf(nr, sizeof nr, "%s", n);

  /* Use some pipes for stdout and stderr */

  if (pipe(pipe_stdout) < 0) {
    perror("pipe");
    return;
  }

  pid = fork();

  if (pid == 0) {
    /* Child */
    dup2(pipe_stdout[1], 1);
    dup2(pipe_stdout[1], 2);
    close(pipe_stdout[1]);

    execlp("qiv-command", "qiv-command", nr, filename, NULL);
    perror("Error calling qiv-command");
    abort();
  }
  else if (pid > 0) {
    /* parent */
    int len = 0;
    char *p = buffer;

    gboolean finished = FALSE;
    close(pipe_stdout[1]);

    do {
      char *s = p, *q;
      finished = waitpid(pid, 0, WNOHANG) > 0;
      len = read(pipe_stdout[0], s, MAXOUTPUTBUFFER - (s - buffer));

      if (len < 0 || (finished && len == 0))
	break;

      s[len] = '\0';
      /* Process the buffer into lines */
      for (; *numlines < MAXLINES && p < s + len; ) {
	lines[(*numlines)++] = p;
	/* Find the end of the line */
	q = strchr(p, '\n');
	if (!q) break;
	*q = '\0';
	p = q + 1;
      }
    } while (len > 0);
    lines[(*numlines)] = 0;
    if (!finished)
      waitpid(pid, 0, 0);

    close(pipe_stdout[0]);
  }
  else {
    perror("fork");
    return;
  }

  /* Check for special keyword "NEWNAME=" in first line
   * indicating that the filename has changed */
  if ( lines[0] && strncmp(lines[0], "NEWNAME=", 8) == 0 ) {
    newfilename = strdup(lines[0]);
    newfilename += 8;
#ifdef DEBUG
    g_print("*** filename has changed from: '%s' to '%s'\n", image_names[image_idx], newfilename);
#endif

    image_names[image_idx] = strdup(newfilename);
    filename = strdup(newfilename);

    /* delete this line from the output */
    (*numlines)--;
    lines[0] = 0;
    for (i = 0; i < *numlines; i++) {
      lines[i] = lines[i+1];
    }
  }

  stat(filename, &after);

  /* If image modified reload, otherwise redraw */
  if (before.st_size == after.st_size &&
      before.st_ctime == after.st_ctime &&
      before.st_mtime == after.st_mtime)
    update_image(q, MIN_REDRAW);
  else
    qiv_load_image(q);
}


/*
   This routine jumps x images forward or backward or
   directly to image x
   Enter jf10\n ... jumps 10 images forward
   Enter jb5\n  ... jumps 5 images backward
   Enter jt15\n ... jumps to image 15
*/
void jump2image(char *cmd)
{
  int direction = 0;
  int x;

#ifdef DEBUG
    g_print("*** starting jump2image function: '%s'\n", cmd);
#endif

  if(cmd[0] == 'f' || cmd[0] == 'F')
    direction = 1;
  else if(cmd[0] == 'b' || cmd[0] == 'B')
    direction = -1;
  else if(!(cmd[0] == 't' || cmd[0] == 'T'))
    return;

  /* get number of images to jump or image to jump to */
  x = atoi(cmd+1);

  if (direction == 1) {
    if ((image_idx + x) > (images-1))
      image_idx = images-1;
    else
      image_idx += x;
  }
  else if (direction == -1) {
    if ((image_idx - x) < 0)
      image_idx = 0;
    else
      image_idx -= x;
  }
  else {
    if (x > images || x < 1)
      return;
    else
      image_idx = x-1;
  }

#ifdef DEBUG
    g_print("*** end of jump2image function\n");
#endif

}

void finish(int sig)
{
  gdk_pointer_ungrab(CurrentTime);
  gdk_keyboard_ungrab(CurrentTime);
  dpms_enable();
  exit(0);
}

/*
  Update selected image index image_idx
  Direction determines if the next or the previous
  image is selected.
*/
void next_image(int direction)
{
  static int last_modif = 1;	/* Delta of last change of index of image */

  if (!direction)
    direction = last_modif;
  else
    last_modif = direction;
  if (random_order)
    image_idx = get_random(random_replace, images, direction);
  else {
    image_idx = (image_idx + direction) % images;
    if (image_idx < 0)
      image_idx += images;
    else if (cycle && image_idx == 0)
      qiv_exit(0);
  }
}

int checked_atoi (const char *s)
{
    char *endptr;
    int num = strtol(s, &endptr, 0);

    if (endptr == s || *endptr != '\0') {
	g_print("Error: %s is not a valid number.\n", s);
	gdk_exit(1);
    }

    return num;
}

void usage(char *name, int exit_status)
{
    g_print("qiv (Quick Image Viewer) v%s\n"
	"Usage: qiv [options] files ...\n"
	"See 'man qiv' or type 'qiv --help' for options.\n",
        VERSION);

    gdk_exit(exit_status);
}

void show_help(char *name, int exit_status)
{
    int i;

    g_print("qiv (Quick Image Viewer) v%s\n"
	"Usage: qiv [options] files ...\n\n",
        VERSION);

    g_print(
          "General options:\n"
          "    --file, -F x           Read file names from text file x or stdin\n"
          "    --bg_color, -o x       Set root background color to x\n"
          "    --brightness, -b x     Set brightness to x (-32..32)\n"
          "    --browse, -B           Scan directory of file for browsing\n"
          "    --center, -e           Disable window centering\n"
          "    --contrast, -c x       Set contrast to x (-32..32)\n"
          "    --cycle, -C            do not cycle after last image\n"
          "    --display x            Open qiv window on display x\n"
          "    --do_grab, -a          Grab the pointer in windowed mode\n"
          "    --disable_grab, -G     Disable pointer/kbd grab in fullscreen mode\n"
          "    --fixed_width, -w x    Window with fixed width x\n"
          "    --fixed_zoom, -W x     Window with fixed zoom factor (percentage x)\n"
          "    --fullscreen, -f       Use fullscreen window on start-up\n"
          "    --gamma, -g x          Set gamma to x (-32..32)\n"
          "    --help, -h             This help screen\n"
          "    --ignore_path_sort, -P Sort filenames by the name only\n"
          "    --readonly, -R         Disable the deletion feature\n"
          "    --maxpect, -m          Zoom to screen size and preserve aspect ratio\n"
          "    --merged_case_sort, -M Sort filenames with AaBbCc... alpha order\n"
          "    --mtime_sort, -K       Sort files by their modification time\n"
          "    --no_filter, -n        Do not filter images by extension\n"
          "    --no_statusbar, -i     Disable statusbar\n"
          "    --statusbar, -I        Enable statusbar\n"
          "    --no_sort, -D          Do not apply any sorting to the list of files\n"
          "    --numeric_sort, -N     Sort filenames with numbers intuitively\n"
          "    --root, -x             Set centered desktop background and exit\n"
          "    --root_t, -y           Set tiled desktop background and exit\n"
          "    --root_s, -z           Set stretched desktop background and exit\n"
          "    --scale_down, -t       Shrink image(s) larger than the screen to fit\n"
          "    --trashbin             Use users trash bin instead of .qiv_trash when deleting\n"
          "                           (undelete key will not work in that case)\n"
          "    --transparency, -p     Enable transparency for transparent images\n"
          "    --watch, -T            Reload the image if it has changed on disk\n"
          "    --recursivedir, -u     Recursively include all files\n"
          "    --followlinks, -L      Follow symlinks to directories (requires --recursivedir)\n"
          "    --select_dir, -A x     Store the selected files in dir x (default is .qiv-select)\n"
#if GDK_PIXBUF_MINOR >= 12
          "    --autorotate, -l       Do NOT autorotate JPEGs according to EXIF rotation tag\n"
#endif
          "    --rotate, -q x         Rotate 90(x=1),180(x=2),270(x=3) degrees clockwise (11 & 13 for conditional)\n"
          "    --xineramascreen, -X x Use monitor x as preferred screen\n"
#ifdef SUPPORT_LCMS
          "    --source_profile, -Y x Use color profile file x as source profile for all images\n"
          "    --display_profile,-Z x Use color profile file x as display profile for all images\n"
#endif
          "    --vikeys               Enable movement with h/j/k/l, vi-style\n"
          "                           (HJKL will do what hjkl previously did)\n"
          "    --version, -v          Print version information and exit\n"
          "\n"
          "Slideshow options:\n"
	  "This can also be used for the desktop background (x/y/z)\n"
          "    --slide, -s            Start slideshow immediately\n"
          "    --random, -r           Random order\n"
          "    --shuffle, -S          Shuffled order\n"
          "    --delay, -d x          Wait x seconds between images [default=%d]\n"
          "\n"
          "Keys:\n", SLIDE_DELAY/1000);

    /* skip header and blank line */
    for (i=0; helpkeys[i]; i++)
        g_print("    %s\n", helpkeys[i]);

    g_print("\nValid image extensions:\nUse --no_filter/-n to disable");

    for (i=0; image_extensions[i]; i++)
	g_print("%s%s", (i%8) ? " " : "\n    ", image_extensions[i]);
    g_print("\n\n");

    g_print("Homepage: http://qiv.spiegl.de/\n"
	    "Please mail bug reports and comments to Andy Spiegl <qiv.andy@spiegl.de>\n");

    gdk_exit(exit_status);
}

/* returns a random number from the integers 0..num-1, either with
   replacement (replace=1) or without replacement (replace=0) */
int get_random(int replace, int num, int direction)
{
  static int index = -1;
  static int *rindices = NULL;  /* the array of random intgers */
  static int rsize;

  int n,m,p,q;

  if (!rindices)
    rindices = (int *) xmalloc((unsigned) max_rand_num*sizeof(int));
  if (rsize != num) {
    rsize = num;
    index = -1;
  }

  if (index < 0)         /* no more indices left in this cycle. Build a new */
    {		         /* array of random numbers, by not sorting on random keys */
      index = num-1;

      for (m=0;m<num;m++)
	{
	  rindices[m] = m; /* Make an array of growing numbers */
	}

      for (n=0;n<num;n++)   /* simple insertion sort, fine for num small */
	{
	  p=(int)(((float)rand()/RAND_MAX) * (num-n)) + n ; /* n <= p < num */
	  q=rindices[n];
	  rindices[n]=rindices[p]; /* Switch the two numbers to make random order */
	  rindices[p]=q;
	}
    }

  return rindices[index--];
}

/* Recursively gets all files from a directory if <recursive> is true,
 * else just reads directory */
int rreaddir(const char *dirname, int recursive)
{
  DIR *d;
  struct dirent *entry;
  char cdirname[FILENAME_LEN], name[FILENAME_LEN];
  struct stat sb;
  int before_count = images;

  strncpy(cdirname, dirname, sizeof cdirname);
  cdirname[FILENAME_LEN-1] = '\0';

  if (!(d = opendir(cdirname)))
    return -1;
  while ((entry = readdir(d)) != NULL) {
    if (strcmp(entry->d_name,".") == 0 ||
        strcmp(entry->d_name,"..") == 0 ||
        strcmp(entry->d_name,TRASH_DIR) == 0)
      continue;
    snprintf(name, sizeof name, "%s/%s", cdirname, entry->d_name);
    if ((followlinks ? stat(name, &sb) : lstat(name, &sb)) >= 0) {
      if (S_ISDIR(sb.st_mode)) {
        if (!recursive)
          continue;
        rreaddir(name,1);
      }
      else {
        if (images >= max_image_cnt) {
          max_image_cnt += 8192;
          if (!image_names)
            image_names = (char**)xmalloc(max_image_cnt * sizeof(char*));
          else
            image_names = (char**)xrealloc(image_names,max_image_cnt*sizeof(char*));
        }
        image_names[images++] = strdup(name);
      }
    }
  }
  closedir(d);
  return images - before_count;
}

/* Read image filenames from a file */
int rreadfile(const char *filename)
{
	FILE *fp;
	struct stat sb;
	int before_count = images;

         if (strcmp(filename,"-")) {
                 fp = fopen(filename, "r");
                 if(!fp) return -1;
         } else
                 fp = stdin;

	if (!images) {
		max_image_cnt = 8192;
		image_names = (char**)xmalloc(max_image_cnt * sizeof(char*));
	}

	while (1) {
		char line[ BUFSIZ ];
		size_t linelen;

		if (fgets(line, sizeof(line), fp) == NULL ) {
			if (ferror(fp))
				g_print("Error while reading %s: %s\n", filename, strerror(errno));
			fclose(fp);
			break;
		}
		linelen = strlen(line) -1;
		if (line[linelen] == '\n') line[linelen--]  = '\0';
		if (line[linelen] == '\r') line[linelen--]  = '\0';

		if (stat(line, &sb) >= 0 && S_ISDIR(sb.st_mode))
			rreaddir(line,1);
		else {
			if (images >= max_image_cnt) {
				max_image_cnt += 8192;
				image_names = (char**)xrealloc(image_names,max_image_cnt*sizeof(char*));
			}
			image_names[images++] = strdup(line);
		}
	}
    return images - before_count;
}

gboolean color_alloc(const char *name, GdkColor *color)
{
    gboolean result;

    result = gdk_color_parse(name, color);

    if (!result) {
        fprintf(stderr, "qiv: can't parse color '%s'\n", name);
        name = "black";
    }

    result = gdk_colormap_alloc_color(cmap, color, FALSE, TRUE);

    if (!result) {
        fprintf(stderr, "qiv: can't alloc color '%s'\n", name);
        color->pixel = 0;
    }

    return result;
}

void swap(int *a, int *b)
{
    int temp;
    temp = *a;
    *a = *b;
    *b = temp;
}

/* rounding a float to an int */
int myround( double a )
{
  return( (a-(int)a > 0.5) ? (int)a+1 : (int)a);
}

/* File watcher, an idle thread checking whether the loaded file has changed */

gboolean qiv_watch_file (gpointer data)
{
  struct stat statbuf;
  qiv_image *q=data;
  if(!watch_file)
  	return FALSE;

  stat(image_names[image_idx], &statbuf);

  if(current_mtime!=statbuf.st_mtime && statbuf.st_size){
          if(time(NULL)-statbuf.st_mtime > 0)
          {
              reload_image(q);
              check_size(q, FALSE);
              update_image(q, REDRAW);
          }
  }

  return TRUE;
}

int find_image(int images, char **image_names, char *name)
{
  int i;
  for (i=0; i<images; i++) {
    if (strcmp(name,image_names[i]) == 0)
      return i;
  }
  return 0;
}

#ifdef SUPPORT_LCMS
char *get_icc_profile(char *filename)
{
  FILE *infile;
  jpeg_saved_marker_ptr marker;
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  int j,i=0;
  cmsUInt32Number length=0;
  int jpg_ok;
  unsigned char pic_tst[4];

  char *icc_ptr=NULL;
  unsigned short *tag_length=NULL;
  unsigned char **tag_ptr=NULL;

  /* Jpeg ICC header:
   * Marker 0xffe2
   * "ICC_PROFILE\0"
   * Marker ID
   * No of total markers
   * data length
   * data
   */
  const char icc_string[]="ICC_PROFILE";
  int seq_max;

  /* Tiff ICC header:
   * Bytes
   * 0-1 TIFFTAG_ICCPROFILE 34675(0x8773)
   * 2-3 The field Type = 7
   * 4-7 the size of the embedded ICC profile
   * 8-11 Start of ICC profile in the tiff file
   */

  TIFF *tiff_image;

  if ((infile = fopen(filename, "rb")) == NULL)
  {
    fprintf(stderr, "can't open %s\n", filename);
    return NULL;
  }

  if (fread(pic_tst, 1, 4, infile) != 4)
  {
    fclose(infile);
    return NULL;
  }

  jpeg_prog=0;
  if(comment)
  {
    free(comment);
    comment=NULL;
  }

  /* Is pic a jpg? */
  if ( (pic_tst[0] == 0xff) && (pic_tst[1] == 0xd8) && (pic_tst[2] == 0xff) &&  ((pic_tst[3]& 0xf0) == 0xe0) )
  {
    rewind(infile);

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_save_markers(&cinfo, 0xE2, 0xFFFF);
    jpeg_save_markers(&cinfo, JPEG_COM, 0xFFFF);
    jpg_ok = jpeg_read_header(&cinfo, 0);
    if (jpg_ok == 0) {
      qiv_exit(2);
    }
    fclose(infile);
    jpeg_prog=cinfo.progressive_mode;

    for (marker = cinfo.marker_list; marker != NULL; marker = marker->next)
    {
      if(strncmp(icc_string, (const char *)marker->data, 11)==0)
      {
        if(i==0)
        {
          seq_max=marker->data[13];
          tag_length = calloc(seq_max, sizeof(short));
          tag_ptr    = calloc(seq_max, sizeof(char *));
        }
        // hmm, in theory both should be the same (tw)
        tag_length[marker->data[12]-1] = marker->data_length - 14;
//      tag_length[marker->data[12]-1] = (marker->data[16] << 8) +  marker->data[17];

        (tag_ptr[marker->data[12]-1])  = marker->data;
        (tag_ptr[marker->data[12]-1]) += 14;

        i++;
      }
      /* copy jpeg comment here to be printed out when exif data is displayed. */
      else if(marker->marker==JPEG_COM)
      {
        comment=calloc(1+marker->data_length,1);
        if(comment==NULL) return NULL;
        strncpy(comment, (char *) marker->data, marker->data_length);

        /* simulate perl's chomp */
        int len = strlen(comment);
        while (isspace(comment[len-1]))
        {
          comment[len-1] = 0;
          if(--len==0)
            break;
        }
      }
    }
    if(i==0)
    {
      /* No icc markers found */
      return NULL;
    }
    else
    {
      /* Sort the markers and copy them together */
      for(j=0; j< i; j++)
      {
        length+= tag_length[j];
      }
      icc_ptr = malloc(length+sizeof(length));
      if(icc_ptr==NULL) return NULL;
      *(unsigned int *)icc_ptr=length;
      length=0;
      for(j=0; j< i; j++)
      {
        memcpy(icc_ptr+length+sizeof(length), tag_ptr[j], tag_length[j]);
        length+= tag_length[j];
      }
      free(tag_ptr);
      free(tag_length);
    }
    jpeg_destroy_decompress(&cinfo);
    return icc_ptr;
  }
  /* is pic a tiff?*/
  else if((pic_tst[0] == pic_tst[1]) && ((pic_tst[0] == 0x49)||(pic_tst[0] == 0x4d)) && (pic_tst[2] == 0x2a) &&  (pic_tst[3] == 0x00))
  {
    uint16 count;
    unsigned char *data;

    fclose(infile);
    if((tiff_image = TIFFOpen(filename, "r")) == NULL){
      fprintf(stderr, "Could not open incoming image\n");
      return NULL;
    }
    if (TIFFGetField(tiff_image, TIFFTAG_ICCPROFILE, &count, &data))
    {
      length = count;
      icc_ptr = malloc(length+sizeof(length));
      *(unsigned int *)icc_ptr=length;
      memcpy(icc_ptr+sizeof(length), data, length);
    }
    TIFFClose(tiff_image);
    return icc_ptr;
  }

  fclose(infile);
  return NULL;
}
#endif

#ifdef HAVE_EXIF
char **get_exif_values(char *filename)
{
  ExifData *ed;
  ExifEntry *entry;
  int i,j;
  char *line;
  char **exif_lines=NULL;
  char buffer[256];

  /* displayed exif tags, when available */
  struct {
    ExifTag tag;
    ExifIfd ifd;
  } tags[] = {
    {EXIF_TAG_MAKE, EXIF_IFD_0},
    {EXIF_TAG_MODEL, EXIF_IFD_0},
    {EXIF_TAG_DOCUMENT_NAME, EXIF_IFD_0},
    {EXIF_TAG_PIXEL_X_DIMENSION , EXIF_IFD_EXIF},
    {EXIF_TAG_PIXEL_Y_DIMENSION , EXIF_IFD_EXIF},
    {EXIF_TAG_SOFTWARE, EXIF_IFD_0},
    {EXIF_TAG_DATE_TIME, EXIF_IFD_0},
    {EXIF_TAG_DATE_TIME_ORIGINAL, EXIF_IFD_EXIF},
    {EXIF_TAG_ARTIST, EXIF_IFD_0},
    {EXIF_TAG_ORIENTATION, EXIF_IFD_0},
    {EXIF_TAG_JPEG_PROC, EXIF_IFD_1},
    {EXIF_TAG_FLASH, EXIF_IFD_EXIF},
    {EXIF_TAG_EXPOSURE_TIME, EXIF_IFD_EXIF},
    {EXIF_TAG_ISO_SPEED_RATINGS, EXIF_IFD_EXIF},
    {EXIF_TAG_APERTURE_VALUE, EXIF_IFD_EXIF},
    {EXIF_TAG_FOCAL_LENGTH, EXIF_IFD_EXIF},
    {EXIF_TAG_FOCAL_LENGTH_IN_35MM_FILM, EXIF_IFD_EXIF},
    {EXIF_TAG_SHUTTER_SPEED_VALUE, EXIF_IFD_EXIF},
    {EXIF_TAG_SUBJECT_DISTANCE, EXIF_IFD_EXIF},
    {EXIF_TAG_METERING_MODE, EXIF_IFD_EXIF},
    {EXIF_TAG_EXPOSURE_MODE, EXIF_IFD_EXIF},
    {EXIF_TAG_SENSING_METHOD, EXIF_IFD_EXIF},
    {EXIF_TAG_WHITE_BALANCE, EXIF_IFD_EXIF},
    {EXIF_TAG_GAIN_CONTROL, EXIF_IFD_EXIF},
    {EXIF_TAG_COLOR_SPACE, EXIF_IFD_EXIF},
    {EXIF_TAG_CONTRAST, EXIF_IFD_EXIF},
    {EXIF_TAG_SATURATION, EXIF_IFD_EXIF},
    {EXIF_TAG_SHARPNESS, EXIF_IFD_EXIF},
    {EXIF_TAG_DIGITAL_ZOOM_RATIO, EXIF_IFD_EXIF}
  };

  ExifTag gps_tags[] = {
    EXIF_TAG_GPS_LATITUDE, EXIF_TAG_GPS_LATITUDE_REF,
    EXIF_TAG_GPS_LONGITUDE, EXIF_TAG_GPS_LONGITUDE_REF,
    EXIF_TAG_GPS_ALTITUDE, EXIF_TAG_GPS_ALTITUDE
  };

  j=0;
  ed = exif_data_new_from_file(filename);
  if(ed)
  {
    /* one too much to make sure the last one will allways be NULL
       size of tags + gpstags(3) + extra lines(3) + 1 */ 
    exif_lines=calloc(sizeof(tags)/sizeof(tags[0]),sizeof(char*)+3+3+1);
    for(i=0; i < sizeof(tags)/sizeof(tags[0]); i++)
    {
      entry=exif_content_get_entry( ed->ifd[tags[i].ifd], tags[i].tag);
      if(entry)
      {
        line=malloc(256); 
        exif_entry_get_value (entry, buffer, 255);
        snprintf(line, 255, "%-21s: %s\n", exif_tag_get_name_in_ifd(tags[i].tag,tags[i].ifd), buffer);
        exif_lines[j]=line;
        j++;
      }
    }
    for(i=0; i < 6; i+=2)
    {
      entry=exif_content_get_entry( ed->ifd[EXIF_IFD_GPS], gps_tags[i]);
      if(entry)
      {
        line=malloc(256); 
        exif_entry_get_value (entry, buffer, 255);
        if((entry=exif_content_get_entry( ed->ifd[EXIF_IFD_GPS], i+1)))
        {
          int len=strlen(buffer);
          if(len < 254)
          {
            buffer[len]=0x20;
            exif_entry_get_value(entry, buffer+len+1, 254-len);
          }
        }
        snprintf(line, 255, "%-21s: %s\n", exif_tag_get_name_in_ifd(gps_tags[i], EXIF_IFD_GPS), buffer);
        exif_lines[j]=line;
        j++;
      }
    }
  }
  if(j==0)
  {
    free(exif_lines);
    return NULL;
  }
  line=malloc(64);
  snprintf(line, 63, "%-21s: %i Bytes\n", "FileSize", (int)file_size);
  exif_lines[j++]=line;
  if(jpeg_prog)
  {
    line=malloc(64);
    snprintf(line, 63, "%-21s: %s\n", "JpegProcess", "Progressive");
    exif_lines[j++]=line;
  }
  if(comment)
  {
    line=malloc(256);
    snprintf(line, 255, "%-21s: %s\n", "Comment", comment);
    exif_lines[j++]=line;
  }

  return exif_lines;
}
#endif

/* The state of DPMS is checked at startup of qiv.
   If the display is DPMS capable and DPMS is activated, qiv disables
   it during a slideshow to prevent the screen from blanking. */
BOOL dpms_enabled = 0;

void dpms_check()
{
  int not_needed;
  Display * disp = GDK_DISPLAY();
  CARD16 power_level;

  if(DPMSQueryExtension(disp, &not_needed, &not_needed))
  {
    if(DPMSCapable(disp))
    {
      DPMSInfo(disp, &power_level, &dpms_enabled);
    }
  }
}

void dpms_enable()
{
  if(dpms_enabled)
  { 
    DPMSEnable(GDK_DISPLAY());
  }
}

void dpms_disable()
{
  if(dpms_enabled)
  { 
    DPMSDisable(GDK_DISPLAY());
  }
}
