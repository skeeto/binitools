/* convert.h - Freelancer BINI format encoder
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

#ifndef CONVERT_H
#define CONVERT_H

struct section
{
  unsigned short str_offset;	/* string offset */
  unsigned short num_entry;	/* number of entries in section */
  struct entry *elist;		/* entry list */
  struct section *next;		/* next section */
};

struct entry
{
  unsigned short str_offset;	/* string offset */
  char num_val;			/* number of values in entry */
  struct value *vlist;		/* value list */
  struct entry *next;		/* next entry */
};

struct value
{
  char type;			/* value type */
  union
  {
    int i;			/* integer value */
    float f;			/* float value */
    int s;			/* string offset value */
  };
  struct value *next;		/* next value */
};

/* initialize converter */
void convert_init ();

/* for building the tables */
void add_sec (unsigned short name);
void add_entry (unsigned short name);
void add_val (char type, int i, float f, unsigned short s);

/* add string to the string table and return a BINI pointer */
unsigned short add_str (char *instr);

/* writes the tables to file */
void write_ini (char *filename);

/* write string table to stdout */
void print_string_table ();

/* Settings */
extern char *progname;		/* this program's name (for error messages) */
extern int verbose;		/* verbose mode */
extern int do_nothing;		/* do nothing mode */
extern int summarize;		/* summarize the contents of the file */

#endif
