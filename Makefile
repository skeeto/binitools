.POSIX:
CC      = cc
CFLAGS  = -ansi -pedantic -Wall -Wextra -Os
LDFLAGS = -s
LDLIBS  =
EXE     =

all: bini$(EXE) unbini$(EXE)

bini$(EXE): bini.c common.h getopt.h trie.h
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ bini.c $(LDLIBS)

unbini$(EXE): unbini.c common.h getopt.h
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ unbini.c $(LDLIBS)

tests/fletcher64$(EXE): tests/fletcher64.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ tests/fletcher64.c $(LDLIBS)

check: bini$(EXE) unbini$(EXE) tests/fletcher64$(EXE)
	(cd tests && ./test.sh)

clean:
	rm -f bini$(EXE) unbini$(EXE) tests/fletcher64$(EXE)
