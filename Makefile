# Copyright (C) Christopher Wellons 2007
# 
# This Makefile is meant to be used with GNU make. Other versions of
# make may not handle this file correctly.

# Set the correct tools
LEX = flex
YACC = bison -y
CC = mingw32-gcc

CFLAGS = -O2 -g -W -Wall -DHAS_MMAP
LDFLAGS = 

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
.PHONY : clean test
clean :
	$(RM) $(solutions) $(OBJ)

test :
	find tests/ -name '*ini' -exec test.sh {} \;
