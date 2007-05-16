# Makefile - Freelancer BINI tools
# Copyright (C) 2007 Christopher Wellons <ccw129@psu.edu>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
######################################################################
# 
# This Makefile is meant to be used with GNU make. Other versions of
# make may not handle this file correctly.
#
# Important targets are:
#
# all
#   builds both binaries: bini and unbini
#
# clean
#   cleans all build generated files
#
# test
#   tests the tools against all BINI files in a tests/ directory
#
# zip
#   creates zip of binaries for binary distribution
#
# tarball
#   creates source distribution
#
######################################################################

# Select a C compiler (such as mingw32)
CC = mingw32-gcc

# Set the correct yacc and lex tools
LEX = flex
YACC = bison -y

# Compile flags
CFLAGS = -O2 -g -W -Wall -DHAS_MMAP
LDFLAGS = 

# Dist files
sources = unbini.c convert.c convert.h pool.h pool.c parse.y scan.l Makefile
docs = README HACKING COPYING COPYRIGHT AUTHORS BUGS NEWS ChangeLog VERSION
distfiles = $(sources) $(docs)
binfiles = bini.exe unbini.exe README COPYING COPYRIGHT AUTHORS NEWS ChangeLog

# Dist name
version = `cat VERSION`
distname = binitools-$(version)

# Final solutions
solutions = unbini unbini.exe bini bini.exe

# Project objects
BINI_OBJ = parse.o scan.o convert.o pool.o
UNBINI_OBJ = unbini.o pool.o
OBJ = $(BINI_OBJ) $(UNBINI_OBJ)

all : bini unbini

# Project solutions
bini : $(BINI_OBJ)
	$(CC) $(LDFLAGS) $^ -o $@
unbini : $(UNBINI_OBJ)
	$(CC) $(LDFLAGS) $^ -o $@

# Create objects
parse.o : parse.y
scan.o : scan.l
convert.o : convert.c convert.h pool.h
pool.o : pool.c pool.h
unbini.o : unbini.c pool.h

# Clean targets
.PHONY : clean test tarball zip
clean :
	$(RM) $(solutions) $(OBJ) y.tab.h

# Place all BINI files to be used for testing in a tests/
# directory. This target will test the tools on each one.
test :
	find tests/ -name '*ini' -exec test.sh {} \;

# The following are used for building the source tarballs and zip
# binary distribution.
tarball : 
	mkdir $(distname)
	cp $(distfiles) $(distname)/
	$(RM) $(distname).tar.gz
	tar -zcvf $(distname).tar.gz $(distname)/
	$(RM) -r $(distname)

zip : all
	mkdir $(distname)-bin
	cp $(binfiles) $(distname)-bin/
	$(RM) $(distname)-bin.tar.gz
	zip -r $(distname)-bin.zip $(distname)-bin/
	$(RM) -r $(distname)-bin
