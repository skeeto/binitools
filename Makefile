.POSIX:
CC     = cc
CFLAGS = -ansi -pedantic -Wall -Wextra -O3

all: bini unbini

bini: bini.c common.h getopt.h trie.h
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ bini.c $(LDLIBS)

unbini: unbini.c common.h getopt.h
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ unbini.c $(LDLIBS)

tests/fletcher64: tests/fletcher64.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ tests/fletcher64.c $(LDLIBS)

check: bini unbini tests/fletcher64
	(cd tests && ./test.sh)

clean:
	rm -f bini bini.exe unbini unbini.exe tests/fletcher64
