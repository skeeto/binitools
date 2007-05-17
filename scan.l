/* scan.l - Freelancer BINI format encoder
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
#define YY_NO_UNPUT
#include <stdlib.h>
#include <string.h>
#include "parse.h"
#include "convert.h"

/* handle strings*/
int hdl_str (char *yytext);

int line_num;			/* line counter */
%}

%%

 /* handle strings (non-numbers) */
\"([^\"\\]*((\\\")|(\\\\))*)*\"        return hdl_str(yytext);

 /* handle integers and floats */
-?[0-9]+\.[0-9]+            yylval.f   = strtod(yytext, NULL); return FLOAT;
-?[0-9]+                    yylval.i   = atoi(yytext); return INT;

 /* ignore comments */
#[^\n]*\n                   line_num++;

 /* line counter */
\n                          line_num++;

 /* leftover characters */
[\t\r ]+                    /* ignore whitespace */
.                           return (int) yytext[0];

%%

int hdl_str (char *yytext)
{
  yylval.str = add_str (yytext);
  return ID;
}
