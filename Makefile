# Copyright (C) Christopher Wellons 2007
# 
# This Makefile is meant to be used with GNU make. Other versions of
# make may not handle this file correctly.

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
docs = README HACKING COPYING COPYRIGHT AUTHORS BUGS NEWS ChangeLog
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
	$(RM) $(solutions) $(OBJ)

# Place all .ini files to be used for testing in a test/
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
