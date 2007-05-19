/* convert.c - Freelancer BINI format encoder
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include "pool.h"
#include "convert.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

int num_sec, num_entry, num_val;	/* counters */
int num_int, num_float, num_string;	/* value counters */
struct section *sec_list, *cur_sec;
struct entry *cur_entry;
struct value *cur_val;

pool_t *mpool;			/* allocation pool */

/* Settings (from header file) */
char *progname;
int verbose;
int do_nothing;
int summarize;

typedef struct str_node
{
  char *str;
  uint16_t ptr;
} str_node;

str_node **str_tab;		/* string table */
int str_cnt, str_max;		/* string counter and max */

/* expands special expressions in strings */
char *str_compact (char *);

/* strdup() using memory pool */
char *pool_strdup (char *str);

/* malloc() wrapper */
void *xmalloc (size_t size)
{
  void *out;
  out = pool_alloc (mpool, size);
  if (out == NULL)
    {
      fprintf (stderr, "%s: convert - malloc failed: %s\n",
	       progname, strerror (errno));
      exit (EXIT_FAILURE);
    }
  return out;
}

/* handles the string table */
uint16_t add_str (char *str)
{
  /* trim leading quote */
  str++;

  /* compact special expressions */
  str = str_compact (str);

  /* remove trailing quote */
  str[strlen (str) - 1] = 0;

  /* check for existing match */
  int i;
  for (i = 0; i < str_cnt; i++)
    {
      if (strcmp (str_tab[i]->str, str) == 0)
	{
	  /* restore trailing quote */
	  str[strlen (str)] = '"';
	  return str_tab[i]->ptr;
	}
    }

  /* restore trailing quote */
  str[strlen (str)] = '"';

  /* add new string */
  str_cnt++;
  if (str_cnt > str_max)
    {
      str_max *= 2;
      str_tab = (str_node **) realloc (str_tab, str_max * sizeof (str_node));
      if (str_tab == NULL)
	{
	  fprintf (stderr, "tobini: convert - realloc failed: %s\n",
		   strerror (errno));
	  exit (EXIT_FAILURE);
	}
    }

  str_tab[str_cnt - 1] = (str_node *) xmalloc (sizeof (str_node));

  str_tab[str_cnt - 1]->str = pool_strdup (str);

  /* chop off ending quote */
  str_tab[str_cnt - 1]->str[strlen (str) - 1] = 0;

  uint16_t ptr = 0;
  if (str_cnt > 1)
    {
      /* not first entry */
      ptr = strlen (str_tab[str_cnt - 2]->str) + 1
	+ str_tab[str_cnt - 2]->ptr;
    }
  str_tab[str_cnt - 1]->ptr = ptr;

  return ptr;
}

void convert_init ()
{
  /* prepare the memory pool */
  mpool = create_pool (2048);

  /* init some numbers */
  num_sec = 0;
  num_entry = 0;
  num_val = 0;
  num_int = 0;
  num_float = 0;
  num_string = 0;
  cur_sec = NULL;
  cur_entry = NULL;
  cur_val = NULL;

  /* string table */
  str_cnt = 0;
  str_max = 64;
  str_tab = (str_node **) malloc (sizeof (str_node *) * str_max);
  if (str_tab == NULL)
    {
      fprintf (stderr, "tobini: str_tab malloc failed: %s\n",
	       strerror (errno));
      exit (EXIT_FAILURE);
    }

  /* initial section */
  sec_list = (struct section *) xmalloc (sizeof (struct section));
  cur_sec = sec_list;
  cur_sec->num_entry = 0;
  cur_sec->next = NULL;

  /* initial entry */
  cur_sec->elist = (struct entry *) xmalloc (sizeof (struct entry));
  cur_entry = cur_sec->elist;
  cur_entry->num_val = 0;
  cur_entry->vlist = NULL;
  cur_entry->next = NULL;

}

void add_sec (uint16_t name)
{
  if (verbose)
    printf ("Section: %d\n", name);

  /* finalize section */
  cur_sec->str_offset = name;

  /* create new section */
  cur_sec->next = (struct section *) xmalloc (sizeof (struct section));
  cur_sec = cur_sec->next;
  cur_sec->elist = (struct entry *) xmalloc (sizeof (struct entry));
  cur_entry = cur_sec->elist;

  /* initialize entry */
  cur_entry->num_val = 0;
  cur_entry->vlist = NULL;
  cur_entry->next = NULL;

  num_sec++;

  /* store section data */
  cur_sec->num_entry = 0;
  cur_sec->next = NULL;
}

void add_entry (uint16_t name)
{
  if (verbose)
    printf ("Entry: %d\n", name);

  /* finalize entry */
  cur_entry->str_offset = name;

  cur_entry->next = (struct entry *) xmalloc (sizeof (struct entry));
  cur_entry = cur_entry->next;

  num_entry++;
  cur_sec->num_entry++;

  /* store entry data */
  cur_entry->num_val = 0;
  cur_entry->vlist = NULL;
  cur_entry->next = NULL;
}

void add_val (uint8_t type, int32_t i, float f, uint16_t s)
{
  if (verbose)
    printf ("Value: type %d -> %d %f %d\n", type, i, f, s);

  /* create new val */
  if (cur_entry->num_val == 0)
    {
      cur_entry->vlist = (struct value *) xmalloc (sizeof (struct value));
      cur_val = cur_entry->vlist;
    }
  else
    {
      cur_val->next = (struct value *) xmalloc (sizeof (struct value));
      cur_val = cur_val->next;
    }

  num_val++;
  cur_entry->num_val++;

  /* store value data */
  cur_val->type = type;
  switch (type)
    {
    case 1:			/* int */
      num_int++;
      cur_val->i = i;
      break;
    case 2:			/* float */
      num_float++;
      cur_val->f = f;
      break;
    case 3:			/* string */
      num_string++;
      cur_val->s = s;
      break;
    }
  cur_val->next = NULL;
}

void write_ini (char *filename)
{
  if (verbose)
    {
      if (filename != NULL)
	printf ("Writing %s ...\n", filename);
      else
	fprintf (stderr, "Writing to stdout ...\n");
    }

  FILE *outfile;
  if (filename != NULL)
    outfile = fopen (filename, "wb");
  else
    outfile = stdout;
  if (outfile == NULL)
    {
      fprintf (stderr, "bini: failed to open %s: %s\n",
	       filename, strerror (errno));
      exit (EXIT_FAILURE);
    }

  /* determine size of data section */
  uint32_t data_chunk = num_sec * 4 + num_entry * 3 + num_val * 5 + 12;

  /* write header (12 bytes) */
  uint32_t ver = 1;
  fwrite ("BINI", 4, 1, outfile);
  fwrite (&ver, 4, 1, outfile);
  fwrite (&data_chunk, 4, 1, outfile);

  /* write out the parse tree */
  int sec_count;
  cur_sec = sec_list;
  for (sec_count = 0; sec_count < num_sec; sec_count++)
    {
      /* write section data (4 bytes) */
      fwrite (&cur_sec->str_offset, 2, 1, outfile);
      fwrite (&cur_sec->num_entry, 2, 1, outfile);

      /* write out all the entries */
      int entry_count;
      entry_count = 0;
      cur_entry = cur_sec->elist;
      for (; entry_count < cur_sec->num_entry; entry_count++)
	{
	  /* write entry data (3 bytes) */
	  fwrite (&cur_entry->str_offset, 2, 1, outfile);
	  fwrite (&cur_entry->num_val, 1, 1, outfile);

	  /* write each value */
	  int val_count;
	  val_count = 0;
	  cur_val = cur_entry->vlist;
	  for (; val_count < cur_entry->num_val; val_count++)
	    {
	      /* write value data (5 bytes) */
	      fwrite (&cur_val->type, 1, 1, outfile);
	      fwrite (&cur_val->i, 4, 1, outfile);
	      cur_val = cur_val->next;
	    }

	  cur_entry = cur_entry->next;
	}
      cur_sec = cur_sec->next;
    }

  /* now write string section */
  int i;
  char *cur_str;
  for (i = 0; i < str_cnt; i++)
    {
      cur_str = str_tab[i]->str;
      fwrite (cur_str, strlen (cur_str) + 1, 1, outfile);
    }

  if (filename != NULL)
    fclose (outfile);

  /* print summary */
  if (summarize)
    {
      printf ("Sections : %d\n", num_sec);
      printf ("Entries  : %d\n", num_entry);
      printf ("Values   : %d\n", num_val);
      printf ("  int    : %d\n", num_int);
      printf ("  float  : %d\n", num_float);
      printf ("  string : %d\n", num_string);
    }

  /* free the parse tree and string table */
  free_pool (mpool);
  free (str_tab);

  if (verbose && filename != NULL)
    printf ("Done writing %s!\n", filename);
}

char *str_compact (char *str)
{
  char *p = str;		/* point of last replacement */
  char *eq;			/* search result */
  int dirty = 0;		/* indicates if string has been copied yet */

  while ((eq = strchr (p, '\\')) != NULL)
    {
      if (!dirty)
	{
	  /* copy the string so it can be modified */
	  str = pool_strdup (str);
	  eq = strchr (str, '\\');
	  dirty = 1;
	}

      /* replace \ with next character */
      *eq = *(eq + 1);
      memmove (eq + 1, eq + 2, strlen (eq) + 1);
      p = eq + 1;
    }

  return str;
}

char *pool_strdup (char *str)
{
  /* manual strdup() so memory pool is used */
  size_t len = strlen (str) + 1;
  char *new_str = (char *) xmalloc (len);
  memcpy (new_str, str, len);
  return new_str;
}

void print_string_table ()
{
  int i;
  for (i = 0; i < str_cnt; i++)
    printf ("%s\n", str_tab[i]->str);
}
