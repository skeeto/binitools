#                                               -*- Automake -*-
# Process this file with automake.

# Distributions files
bini_docs = BUGS HACKING
bini_tests = test.sh checkbuild.sh tests/simple.ini
EXTRA_DIST = $(bini_docs) $(bini_tests) parse.h

bin_PROGRAMS = bini unbini
unbini_SOURCES = unbini.c pool.c pool.h
bini_SOURCES = parse.y scan.l convert.c convert.h pool.c pool.h

# The test suite
TESTS = checkbuild.sh
