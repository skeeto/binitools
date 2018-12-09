#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#define PROGRAM_VERSION "2.4"

static void
usage(FILE *f)
{
    fprintf(f, "usage: " PROGRAM_NAME " [-o path] [<INI|INI]\n");
    fprintf(f, "  -h       print this message\n");
    fprintf(f, "  -o path  output to a file (default: standard output)\n");
    fprintf(f, "  -V       print version information\n");
}

static void
version(void)
{
    printf(PROGRAM_NAME " " PROGRAM_VERSION "\n");
}

static void
fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, PROGRAM_NAME ": ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(EXIT_FAILURE);
}

static void *
xmalloc(size_t z)
{
    void *p = malloc(z);
    if (!p)
        fatal("out of memory");
    return p;
}

static void *
xreallocarray(void *p, size_t n, size_t m)
{
    if (n && m > (size_t)-1 / n)
        fatal("out of memory");
    p = realloc(p, n * m);
    if (!p)
        fatal("out of memory");
    return p;
}

/* Read an entire stream into a buffer. */
static void *
slurp(FILE *f, unsigned long *len)
{
    char *p;
    unsigned long cap = 4096;

    *len = 0;
    p = xmalloc(cap);
    for (;;) {
        size_t in = fread(p + *len, 1, cap - *len, f);
        *len += (unsigned long)in;
        if (in == 0 && ferror(f)) {
            fatal("error reading input");
        } else if (in < cap - *len) {
            return p;
        }
        p = xreallocarray(p, cap, 2);
        cap *= 2;
    }
}

#endif
