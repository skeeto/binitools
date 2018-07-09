#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

static void
fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "bini: ");
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
