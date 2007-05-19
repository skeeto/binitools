/* parse.y - Freelancer BINI format encoder
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

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "convert.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
char *version = PACKAGE_VERSION;
#else
char *version = "";
#endif

/* lex declarations */
extern FILE *yyin;
int yylex ();
int yyerror (char *);
extern int line_num;

int yywrap ()
{
  return 1;
}

/* Creates an output filename based in input filename. */
char *get_outname (char *inname, char *argin);
int need_free;			/* indicates if outname was created
				   with malloc() */

/* backs up given file if it exists */
void backup_file (char *filename);

%}
%defines

%union {
  uint16_t str;
  int32_t i;
  float f;
}

%token <str> ID
%token <i> INT
%token <f> FLOAT

%%

sections:   section
          | sections section
;

section:   '[' ID entryblock { add_sec($2); }
;

entryblock :   ']' entries 
             | ']' /* empty*/
;

entries:   entry
         | entries entry
;

entry:   ID '=' values { add_entry($1); }
       | ID            { add_entry($1); }
;

values:   value
        | values ',' value
;

value:   INT   { add_val(1, $1, 0, 0); }
       | FLOAT { add_val(2, 0, $1, 0); }
       | ID    { add_val(3, 0,  0, $1); }
;

%%

void print_usage (int exit_stat)
{
  printf ("Usage: %s [options] FILES\n\nOptions:\n\n", progname);
  printf ("\t-o file      Set output file\n");
  printf ("\t-s           Summarize file details\n");
  printf ("\t-t           Print string table\n");
  printf ("\t-n           Do not output a file\n");
  printf ("\t-v           Verbose mode\n");
  printf ("\t-c           Concatenate input files\n");
  printf ("\t-q           Don't print message at startup\n");
  printf ("\t-h           Print this usage text\n");
  exit (exit_stat);
}

/* getopt() variables */
extern int optind;
extern char *opt_arg;

char *cur_file;			/* current input filename */

int main (int argc, char **argv)
{
  progname = argv[0];

  /* default settings */
  verbose = 0;
  summarize = 0;
  int print_str_tab = 0;
  do_nothing = 0;
  char *arg_outfile = NULL;
  int silent = 0;
  int concat = 0;

  int c;			/* getopt() return value */
  while ((c = getopt (argc, argv, "o:stnvcqh")) != -1)
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
      printf ("bini, version %s.\n", version);
      printf ("Copyright (C) 2007 Chris Wellons\n");
      printf ("This is free software; see the source code ");
      printf ("for copying conditions.\n");
      printf ("There is ABSOLUTELY NO WARRANTY; not even for ");
      printf ("MERCHANTIBILITY or\nFITNESS FOR A PARTICULAR PURPOSE.\n\n");
    }

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

  /* prepare the parse tree */
  convert_init ();

  char *outname;
  struct stat file_stat;
  int is_stdin = 0;
  int i;
  for (i = optind; i < argc; i++)
    {
      if (verbose)
	printf ("Parsing %s ...\n", argv[i]);

      /* open input file */
      line_num = 1;
      cur_file = argv[i];
      if (strcmp (cur_file, "-") == 0)
	{
	  yyin = stdin;
	  is_stdin = 1;
	}
      else
	{
	  yyin = fopen (argv[i], "r");
	  is_stdin = 0;

	  if (yyin == NULL)
	    {
	      fprintf (stderr, "%s: failed to open %s: %s\n",
		       progname, argv[i], strerror (errno));
	      break;
	    }

	  stat (argv[i], &file_stat);
	}
      if (is_stdin || file_stat.st_size > 1)	/* check for empty file */
	{
	  /* parse the file */
	  yyparse ();
	}

      if (!is_stdin)
	fclose (yyin);

      if (!concat && !do_nothing)
	{
	  /* print the string table (if requested) */
	  if (print_str_tab)
	    print_string_table ();

	  /* create output filename */
	  outname = get_outname (argv[i], arg_outfile);
	  backup_file (outname);

	  /* output the parse tree */
	  write_ini (outname);
	  if (need_free)
	    free (outname);

	  /* reset parse tree (for the next file) */
	  if (i < argc - 1)
	    convert_init ();
	}
    }

  /* If concatenating, then write out the parse tree now. */
  if (concat)
    write_ini (arg_outfile);

  return EXIT_SUCCESS;
}

int yyerror (char *s)
{
  printf ("%s:%s:%d: error: %s\n", progname, cur_file, line_num, s);
  return 0;
}

char *get_outname (char *inname, char *argin)
{
  /* If the filename ends with .txt, chop it off, if the filename does
   * not, then add .ini. */
  char *outname;
  need_free = 0;
  if (argin == NULL)
    {
      if (strcmp (inname, "-") == 0)
	return NULL;		/* use stdout */

      if (strcmp (".txt", inname + strlen (inname) - 4) != 0)
	{
	  /* add .ini */
	  size_t size = strlen (inname) + 5;
	  outname = (char *) malloc (size);
	  if (outname == NULL)
	    {
	      fprintf (stderr, "%s: outname malloc failed: %s\n",
		       progname, strerror (errno));
	      exit (EXIT_FAILURE);
	    }
	  snprintf (outname, size, "%s.ini", inname);
	  need_free = 1;
	}
      else
	{
	  /* remove .txt */
	  inname[strlen (inname) - 4] = 0;
	  outname = inname;
	}
    }
  else
    {
      if (strcmp (argin, "-") == 0)
	return NULL;		/* use stdout */

      outname = argin;
    }

  return outname;
}

void backup_file (char *filename)
{
  if (filename == NULL)
    return;			/* stdout is being used */

  /* check for file existance */
  if (access (filename, R_OK) != 0)
    return;

  size_t size = strlen (filename) + 5;

  char *bakname = (char *) malloc (size);
  if (bakname == NULL)
    {
      fprintf (stderr, "%s: bakname malloc failed: %s\n",
	       progname, strerror (errno));
      exit (EXIT_FAILURE);
    }
  snprintf (bakname, size, "%s.bak", filename);

  /* if backup already exists, do nothing */
  if (access (bakname, R_OK) == 0)
    {
      free (bakname);
      return;
    }

  /* move file to backup */
  int ret = rename (filename, bakname);

  if (verbose && ret == -1)
    printf ("Backup failed.\n");

  free (bakname);
}
