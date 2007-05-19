/* unbini.c - unpack Freelancer "BINI" file
 * Copyright (C) 2007 Christopher Wellons <ccw129@psu.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>		/* fstat() */
#include "pool.h"		/* memory allocation pool */

#ifdef HAVE_CONFIG_H
#include "config.h"
char *version = PACKAGE_VERSION;
#else
char *version = "";
#endif

/* size of each chunk */
int header_size = 12;		/* size of the header */
int sec_size = 4;		/* size of a section */
int entry_size = 3;		/* size of an entry */
int val_size = 5;		/* size of a value */

/* getopt() variables */
extern int optind;
extern char *opt_arg;

/* file IO */
void bini_open (char *file);	/* open BINI file */
void *bini_read (size_t size);	/* read from BINI file */
char *bini_str_tab (size_t offset);
void bini_close ();		/* close BINI file */

/* Used when reading from stdin */
FILE *get_stdin ();

/* input file data */
struct stat inistat;		/* used to get the ini file information */
size_t filesize;		/* size of the ini file */
int bini_eof;			/* end of non-string-table */
FILE *inifile;			/* input file handle */
unsigned int read_pos;

/* output file info */
FILE *outfile = NULL;		/* output file */

/* header information */
uint8_t *bini;
uint32_t ver;
uint32_t str_offset = 0;

/* options */
int verbose = 0;		/* program verbosity */
int do_nothing = 0;		/* do not process file */
int print_str_tab = 0;		/* print string table */
int summarize = 0;		/* summarize file details */
char *arg_outfile = NULL;	/* selected output file */

/* handles special character expansion */
char *str_expand (char *str);

/* Unpacks the given file. It will use outfile if provided and append
 * to the given outfile if append is true. */
int unbini (char *inname, char *outname, int append);

void *xmalloc (size_t size);	/* malloc() wrapper */
pool_t *xmal_pool;		/* xmalloc memory pool */

/* program name */
char *progname;

/* program usage information */
void print_usage (int ret)
{
  printf ("Usage: %s [options] FILES\n\nOptions:\n\n", progname);
  printf ("\t-o file      Set output file\n");
  printf ("\t-t           Print string table\n");
  printf ("\t-s           Summarize file details\n");
  printf ("\t-n           Do not output a file\n");
  printf ("\t-v           Verbose mode\n");
  printf ("\t-c           Concatenate input files (requires -o option)\n");
  printf ("\t-q           Don't print message at startup\n");
  printf ("\t-h           Print this usage text\n");
  exit (ret);
}

int main (int argc, char **argv)
{
  progname = argv[0];

  int concat = 0;		/* concatenate files */
  int silent = 0;		/* silent mode */

  int c;			/* getopt() return value */
  while ((c = getopt (argc, argv, "o:tsnvcqh")) != -1)
    switch (c)
      {
      case 't':		/* output string table */
	print_str_tab = 1;
	break;
      case 's':		/* summarize file details */
	summarize = 1;
	break;
      case 'o':		/* set output file */
	arg_outfile = optarg;
	break;
      case 'n':		/* do nothing */
	do_nothing = 1;
	break;
      case 'v':		/* verbose mode */
	verbose = 1;
	break;
      case 'c':		/* concatenate */
	concat = 1;
	break;
      case 'q':		/* silent */
	silent = 1;
	break;
      case 'h':		/* print help */
	print_usage (EXIT_SUCCESS);
	break;
      case '?':		/* bad argument */
	print_usage (EXIT_FAILURE);
      }

  /* print startup message */
  if (!silent)
    {
      printf ("unbini, version %s.\n", version);
      printf ("Copyright (C) 2007 Chris Wellons\n");
      printf ("This is free software; see the source code ");
      printf ("for copying conditions.\n");
      printf ("There is ABSOLUTELY NO WARRANTY; not even for ");
      printf ("MERCHANTIBILITY or\nFITNESS FOR A PARTICULAR PURPOSE.\n\n");
    }

  if (verbose && do_nothing)
    printf ("Doing nothing.\n");

  /* check for missing filenames */
  if (argc - optind < 1)
    {
      fprintf (stderr, "%s: no input files\n", progname);
      print_usage (EXIT_FAILURE);
    }

  /* check for invalid combinations */
  if (argc - optind > 1)
    {
      if (arg_outfile != NULL && !concat)
	{
	  fprintf (stderr, "%s: -o option requires -c with multiple files \n",
		   progname);
	  print_usage (EXIT_FAILURE);
	}

      if (arg_outfile == NULL && concat)
	{
	  fprintf (stderr, "%s: -c option requires -o option\n", progname);
	  print_usage (EXIT_FAILURE);
	}
    }

  /* Unpack each file in order. First file never appends. */
  int i, sum = 0;
  for (i = optind; i < argc; i++)
    {
      /* create memory pool for next file */
      xmal_pool = create_pool (128);

      /* unpack the file */
      char *cur_file = argv[i];
      if (strcmp (cur_file, "-") == 0)
	{
	  /* Fetch file from stdin */
	  cur_file = NULL;
	  inifile = get_stdin ();
	}
      sum += unbini (cur_file, arg_outfile, concat * (i - optind));

      /* destroy the memory pool */
      free_pool (xmal_pool);
    }

  if (sum > 0)
    return EXIT_FAILURE;
  else
    return EXIT_SUCCESS;
}

int unbini (char *inname, char *outname, int append)
{
  /* create output filename if one wasn't selected */
  if (inname != NULL && outname == NULL && !do_nothing)
    {
      size_t size = strlen (inname) + 5;
      outname = (char *) xmalloc (size);
      snprintf (outname, size, "%s.txt", inname);
    }

  /* Open the input file */
  if (inname != NULL)
    bini_open (inname);

  /* extract the header information */
  bini = (uint8_t *) bini_read (4);
  ver = *((uint32_t *) bini_read (4));
  str_offset = *((uint32_t *) bini_read (4));

  /* check for valid header */
  if (memcmp (bini, "BINI", 4) || ver != 1)
    {
      if (inname == NULL)
	inname = "stdin";
      fprintf (stderr, "%s: %s is not a valid BINI file\n", progname, inname);
      bini_close ();
      return 1;
    }

  char *str_tab = bini_str_tab (str_offset);	/* string table */

  if (verbose)
    {
      printf ("String table size: %d\n", filesize - str_offset);
      printf ("String offset: %d\n", str_offset);
    }

  /* print string table if requested */
  if (print_str_tab)
    {
      char *cur_str;
      uint16_t offset = 0;
      while (offset < filesize - str_offset)
	{
	  cur_str = str_tab + offset;
	  printf ("%s\n", cur_str);
	  offset += (strlen (cur_str) + 1);
	}
    }

  /* open the output file */
  if (!do_nothing)
    {
      char *method = "w";
      if (append)
	method = "a";

      if (outname != NULL)
	{
	  outfile = fopen (outname, method);
	  if ((int) outfile == -1)
	    {
	      fprintf (stderr, "%s: failed to open output file %s: %s\n",
		       progname, outname, strerror (errno));
	      bini_close ();
	      return 1;
	    }
	}
      else
	outfile = stdout;
    }

  /* section data */
  char *sec_str = NULL;
  int sec_total = 0;		/* total number of sections */
  uint16_t num_entry;

  /* entry data */
  char *entry_str = NULL;
  int entry_total = 0;		/* total number of entries */
  uint8_t num_val;

  /* value data */
  int val_total = 0;		/* total number of values */
  int int_total = 0;		/* total number of integer values */
  int float_total = 0;		/* total number of float values */
  int string_total = 0;		/* total number of string values */

  /* if summary was not requested and do_nothing is set, stop here */
  if (!summarize && do_nothing)
    {
      bini_close ();
      return 0;
    }

  /* unpack the file */
  while (!bini_eof)
    {
      sec_total++;

      /* extract section information */
      sec_str = *((uint16_t *) bini_read (2)) + str_tab;
      sec_str = str_expand (sec_str);
      num_entry = *((uint16_t *) bini_read (2));

      entry_total += num_entry;
      if (verbose)
	printf ("Section: %s -> %d entries\n", sec_str, num_entry);

      if (!do_nothing)
	fprintf (outfile, "[\"%s\"]\n", sec_str);

      /* extract each section */
      uint16_t entry_count;
      for (entry_count = 0; entry_count < num_entry; entry_count++)
	{
	  /* extract entry information */
	  entry_str = (*(uint16_t *) bini_read (2)) + str_tab;
	  entry_str = str_expand (entry_str);
	  num_val = *(uint8_t *) bini_read (1);

	  val_total += num_val;
	  if (verbose)
	    printf ("Entry: %s -> %d values\n", entry_str, num_val);

	  /* empty entry */
	  if (!do_nothing)
	    {
	      if (num_val == 0)
		fprintf (outfile, "\"%s\"\n", entry_str);
	      else
		fprintf (outfile, "\"%s\" = ", entry_str);
	    }

	  uint8_t val_count;
	  char *str;		/* used to check string value */
	  uint8_t val_type;
	  for (val_count = 0; val_count < num_val; val_count++)
	    {
	      val_type = *((uint8_t *) bini_read (1));
	      if (verbose)
		printf ("Value: type %d\n", val_type);

	      /* determine value type */
	      switch (val_type)
		{
		case 1:	/* integer */
		  int_total++;
		  if (!do_nothing)
		    fprintf (outfile, "%d", *((int32_t *) bini_read (4)));
		  break;
		case 2:	/* float */
		  float_total++;
		  if (!do_nothing)
		    fprintf (outfile, "%f", *((float *) bini_read (4)));
		  break;
		case 3:	/* string */
		  string_total++;
		  str = *((uint32_t *) bini_read (4)) + str_tab;
		  str = str_expand (str);
		  if (!do_nothing)
		    fprintf (outfile, "\"%s\"", str);
		  break;
		}

	      /* comma or end of values */
	      if (!do_nothing)
		{
		  if (val_count != num_val - 1)
		    fprintf (outfile, ", ");
		  else
		    fprintf (outfile, "\n");
		}
	    }
	}
      if (!do_nothing)
	fprintf (outfile, "\n");
    }

  /* close the files */
  bini_close ();

  /* print summary */
  if (summarize)
    {
      printf ("Sections : %d\n", sec_total);
      printf ("Entries  : %d\n", entry_total);
      printf ("Values   : %d\n", val_total);
      printf ("  int    : %d\n", int_total);
      printf ("  float  : %d\n", float_total);
      printf ("  string : %d\n", string_total);
    }

  return 0;
}

char *str_expand (char *str)
{
  char *p;			/* hold last point of replacement */
  char *eq;			/* current point of replacement */
  int char_count = 0;

  /* count \ and ", then alloc a big enough string */
  p = str - 1;
  while ((p = strchr (p + 1, '\\')) != NULL)
    char_count++;
  p = str - 1;
  while ((p = strchr (p + 1, '"')) != NULL)
    char_count++;

  /* if special characters were found, get a bigger string */
  if (char_count > 0)
    {
      char *new_str = (char *) xmalloc (strlen (str) + char_count + 1);
      strcpy (new_str, str);
      str = new_str;
    }
  else
    return str;

  /* escape \ first */
  p = str;
  while ((eq = strchr (p, '\\')) != NULL)
    {
      memmove (eq + 1, eq, strlen (eq) + 1);
      *eq = '\\';
      p = eq + 2;
    }

  /* escape " now */
  p = str;
  while ((eq = strchr (p, '"')) != NULL)
    {
      memmove (eq + 1, eq, strlen (eq) + 1);
      *eq = '\\';
      p = eq + 2;
    }

  return str;
}

void *xmalloc (size_t size)
{
  void *out;
  out = pool_alloc (xmal_pool, size);
  if (out == NULL)
    {
      fprintf (stderr, "%s: malloc failed: %s\n", progname, strerror (errno));
      exit (EXIT_FAILURE);
    }
  return out;
}

/* file I/O open */
void bini_open (char *file)
{
  /* Open the input file. Note the "b" in the open mode. POSIX systems
     ignore it, but Windows needs it because it is silly like that. */
  inifile = fopen (file, "rb");
  if ((int) inifile == -1)
    {
      fprintf (stderr, "%s: failed to open input file %s: %s\n",
	       progname, file, strerror (errno));
      exit (EXIT_FAILURE);
    }
  stat (file, &inistat);
  filesize = inistat.st_size;

  read_pos = 0;
  bini_eof = 0;
}

/* file I/O read */
void *bini_read (size_t size)
{
  void *data;
  data = xmalloc (size);

  fread (data, size, 1, inifile);
  read_pos += size;

  /* check for end of file (hit string section) */
  bini_eof = (read_pos >= str_offset && str_offset != 0);
  bini_eof = bini_eof || read_pos >= filesize;

  return data;
}

/* file I/O string table fetch */
char *bini_str_tab (size_t offset)
{
  /* remember old position */
  long int pos = ftell (inifile);

  /* read in the entire string table */
  void *new_str_tab;
  new_str_tab = xmalloc (filesize - offset);
  fseek (inifile, offset, SEEK_SET);
  fread (new_str_tab, filesize - offset, 1, inifile);

  /* restore old position */
  fseek (inifile, pos, SEEK_SET);

  return new_str_tab;
}

/* file I/O close */
void bini_close ()
{
  fclose (inifile);
  if (!do_nothing && outfile != stdout)
    fclose (outfile);
}

FILE *get_stdin ()
{
  if (verbose)
    fprintf (stderr, "Writing stdin to temporary file.\n");

  FILE *out = tmpfile ();
  if (out == NULL)
    {
      fprintf (stderr, "%s: failed to create temporary file\n", progname);
      exit (EXIT_FAILURE);
    }

  uint8_t one_byte;
  size_t size = 0;
  while (!feof (stdin))
    {
      fread (&one_byte, 1, 1, stdin);
      fwrite (&one_byte, 1, 1, out);
      size++;
    }

  /* Initalize file data */
  fseek (out, 0, SEEK_SET);
  filesize = size;
  read_pos = 0;
  bini_eof = 0;

  return out;
}
