#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h> /* Only for uint32_t */
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "getopt.h"

static int
xisblank(int c)
{
    /* isblank() is not in C89 */
    return c == ' ' || c == '\t';
}

static unsigned long
murmurhash3(const void *key, unsigned long len, unsigned long seed)
{
    unsigned long mask32 = 0xffffffffUL;
    unsigned long hash = seed & mask32;
    unsigned long nblocks = (len & mask32) / 4;
    unsigned char *p = (unsigned char *)key;
    unsigned char *tail = (unsigned char *)key + nblocks * 4;
    unsigned long k1 = 0;
    unsigned long i;
    for (i = 0; i < nblocks; i++) {
        unsigned long k = p[i * 4 + 0] <<  0 |
                          p[i * 4 + 1] <<  8 |
                          p[i * 4 + 2] << 16 |
                          p[i * 4 + 3] << 24;
        k = (k * 0xcc9e2d51UL) & mask32;
        k = (k << 15) | (k >> 17);
        k = (k * 0x1b873593UL) & mask32;
        hash = hash ^ k;
        hash = ((hash << 13) | (hash >> 19)) * 5 + 0xe6546b64UL;
        hash &= mask32;
    }
    switch (len & 3UL) {
        case 3:
            k1 ^= tail[2] << 16;
            /* FALLTHROUGH */
        case 2:
            k1 ^= tail[1] << 8;
            /* FALLTHROUGH */
        case 1:
            k1 ^= tail[0];
            k1 = (k1 * 0xcc9e2d51UL) & mask32;
            k1 = (k1 << 15) | (k1 >> 17);
            k1 = (k1 * 0x1b873593UL) & mask32;
            hash ^= k1;
    }
    hash ^= len;
    hash ^= hash >> 16;
    hash *= 0x85ebca6bUL;
    hash ^= (hash & mask32) >> 13;
    hash *= 0xc2b2ae35UL;
    hash ^= (hash & mask32) >> 16;
    return hash & mask32;
}

/* String intern table */

struct string {
    char *s;
    unsigned offset;
};

struct strings {
    size_t cap;
    size_t count;
    struct string **strings;
};

static void
strings_init(struct strings *t, size_t cap)
{
    size_t i;
    t->cap = cap;
    t->count = 0;
    t->strings = xreallocarray(0, t->cap, sizeof(t->strings[0]));
    for (i = 0; i < cap; i++)
        t->strings[i] = 0;
}

static void
strings_insert(struct strings *t, struct string *s)
{
    unsigned long mask = (unsigned long)(t->cap - 1);
    unsigned long len = (unsigned long)strlen(s->s) + 1;
    unsigned long i = murmurhash3(s->s, len, 0) & mask;
    if (t->count * 2 > t->cap) {
        size_t j;
        struct strings grow[1];
        strings_init(grow, t->cap * 2);
        for (j = 0; j < t->cap; j++)
            if (t->strings[j])
                strings_insert(grow, t->strings[j]);
        free(t->strings);
        *t = *grow;
    }
    while (t->strings[i])
        i = (i + 1) & mask;
    t->strings[i] = s;
    t->count++;
}

static struct string *
strings_find(struct strings *t, char *s)
{
    unsigned long mask = (unsigned long)(t->cap - 1);
    unsigned long len = (unsigned long)strlen(s) + 1;
    unsigned long i = murmurhash3(s, len, 0) & mask;
    while (t->strings[i]) {
        if (!strcmp(t->strings[i]->s, s))
            return t->strings[i];
        i = (i + 1) & mask;
    }
    return 0;
}

static struct string *
strings_push(struct strings *t, char *s)
{
    struct string *ss;
    ss = strings_find(t, s);
    if (!ss) {
        ss = xmalloc(sizeof(*ss));
        ss->s = s;
        strings_insert(t, ss);
    }
    return ss;
}

static int
cmp(const void *a, const void *b)
{
    const struct string *sa = a;
    const struct string *sb = b;
    if (!sa->s && !sb->s)
        return 0;
    if (!sa->s)
        return 1;
    if (!sb->s)
        return -1;
    return strcmp(sa->s, sb->s);
}

/* Sort the string table and compute table offsets. */
static long
strings_finalize(struct strings *t)
{
    size_t i;
    long offset = 0;

    qsort(t->strings, t->cap, sizeof(*t->strings), cmp);

    for (i = 0; i < t->count; i++) {
        if (offset > 65535)
            fatal("too many strings");
        t->strings[i]->offset = offset;
        offset += (long)strlen(t->strings[i]->s) + 1;
    }

    return offset;
}

static void
strings_destroy(struct strings *t)
{
    size_t i;
    for (i = 0; i < t->count; i++)
        free(t->strings[i]);
    free(t->strings);
}

/* Parser stream */

struct parser {
    char *filename;
    long line;
    char *p;
    char *end;
};

static void
error(struct parser *p, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s:%ld: ", p->filename, p->line);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(EXIT_FAILURE);
}

static int
get(struct parser *p)
{
    int c = -1;
    if (p->p < p->end) {
        c = *(unsigned char *)(p->p);
        p->p++;
        if (c == '\n')
            p->line++;
    }
    return c;
}

static void
unget(struct parser *p)
{
    p->p--;
    if (*p->p == '\n')
        p->line--;
}

/* Advance the parser over all whitespace and comments.
 * Returns zero if EOF was reached.
 */
static int
skip_space(struct parser *p)
{
    for (;;) {
        int c;
        for (c = get(p); c != -1 && isspace(c); c = get(p))
            ;
        if (c == -1)
            return 0;
        if (c == ';') {
            for (c = get(p); c != -1 && c != '\n'; c = get(p))
                ;
            if (c == -1)
                return 0;
        } else {
            unget(p);
            return 1;
        }
    }
}

/* Skip only blank characters (space and tab).
 * Returns zero if EOF was reached.
 */
static int
skip_blank(struct parser *p)
{
    int c;
    for (c = get(p); c != -1 && xisblank(c); c = get(p))
        ;
    if (c != -1)
        unget(p);
    return c != -1;
}

/* Advance the parser to the end of the current quoted string.
 */
static void
parse_string(struct parser *p)
{
    int c;
    for (c = get(p); c != -1; c = get(p)) {
        if (c == '"') {
            c = get(p);
            if (c != '"') {
                unget(p);
                return;
            }
        }
    }
    error(p, "EOF in middle of string");
}

/* Advance the parser to the end of the current unquoted string.
 */
static void
parse_simple(struct parser *p, int term)
{
    int c;
    for (c = get(p); c != -1 && c != term && c != '\n' && c != ';'; c = get(p))
        ;
    if (c == term || c == '\n' || c == ';')
        unget(p);
}

/* Process a string, removing quotes and null-terminating it.
 */
static char *
escape_string(char *beg, char *end)
{
    if (*beg == '"') {
        char *s;
        for (s = beg; s < end; s++) {
            if (*s == '"') {
                memmove(s, s + 1, end - s);
                end--;
            }
        }
    } else {
        while (isspace(*beg))
            beg++;
        while (isspace(end[-1]))
            end--;
    }
    *end = 0;
    return beg;
}

/* BINI structs
 *
 * The layout here does *not* much resemble their layout in BINI files.
 *
 * The strings used by these structures point directly into the slurped
 * input buffer, and those strings are escaped in place. Some care must
 * be taken not to write the null terminator too soon.
 */

#define VALUE_INTEGER 1
#define VALUE_FLOAT   2
#define VALUE_STRING  3
struct value {
    struct value *next;
    union {
        long i;
        float f;
        struct string *s;
    } value;
    int type;
};

struct entry {
    struct entry *next;
    struct string *name;
    struct value *values;
};

struct section {
    struct section *next;
    struct string *name;
    struct entry *entries;
};

static struct value *
parse_value(struct parser *p, struct strings *strings, int *nextc)
{
    int c;
    char *beg, *end;
    struct value *value;

    value = xmalloc(sizeof(*value));
    value->next = 0;

    beg = p->p;
    c = get(p);
    if (c == '"') {
        /* Must be a quoted string */
        parse_string(p);
        end = p->p;
        *nextc = get(p);
        beg = escape_string(beg, end);
        value->value.s = strings_push(strings, beg);
        value->type = VALUE_STRING;
        return value;

    } else {
        long i;
        float f;

        /* Extract the token as if it were a simple string */
        parse_simple(p, ',');
        end = p->p;
        *nextc = get(p);
        beg = escape_string(beg, end);

        /* Is it an integer? */
        errno = 0;
        i = strtol(beg, &end, 10);
        if (!*end && (i || !errno)) {
            value->value.i = i;
            value->type = VALUE_INTEGER;
            return value;
        }

        /* Is it a float? */
        errno = 0;
        f = (float)strtod(beg, &end);
        if (!*end && (i || !errno)) {
            value->value.f = f;
            value->type = VALUE_FLOAT;
            return value;
        }

        /* Must just be a simple string */
        value->value.s = strings_push(strings, beg);
        value->type = VALUE_STRING;
        return value;
    }
}

static struct entry *
parse_entry(struct parser *p, struct strings *strings)
{
    int c;
    int nvalue = 0;
    char *beg, *end;
    struct value *value;
    struct value *tail = 0;
    struct entry *entry = 0;

    if (!skip_space(p))
        return 0;

    beg = p->p;
    c = get(p);

    if (c == '[') {
        /* found a section, stop */
        unget(p);
        return 0;
    }

    /* Parse the entry name */
    if (c == '"')
        parse_string(p);
    else
        parse_simple(p, '=');
    end = p->p;

    if (!skip_blank(p))
        error(p, "unexpected EOF in entry, expected '='");

    c = get(p);

    /* Entries must have a '=' */
    if (c != '=')
        error(p, "unexpected '%c', expected '='", c);

    /* With that resolved, escape the string which may clobber the '=' */
    beg = escape_string(beg, end);
    entry = xmalloc(sizeof(*entry));
    entry->next = 0;
    entry->name = strings_push(strings, beg);
    entry->values = 0;

    if (!skip_blank(p))
        return entry;

    /* Get the first value */
    c = get(p);
    if (c == ',')
        error(p, "unexpected ',', expected a value");
    unget(p);
    if (c == '\n' || c == ';')
        return entry; /* No more values possible */

    /* Comma was found, so get the rest of the values */
    for (;;) {
        value = parse_value(p, strings, &c);
        if (!tail) {
            entry->values = tail = value;
        } else {
            tail->next = value;
            tail = tail->next;
        }
        if (++nvalue > 255)
            error(p, "too many values in one entry");

        /* Check for more values */
        if (c == '\n')
            return entry;
        if (c == ';') {
            /* Can't unget the ';', so consume the comment */
            for (c = get(p); c != -1 && c != '\n'; c = get(p))
                ;
            return entry;
        }
        if (c != ',')
            error(p, "unexpected '%c', expected ','", c);

        /* Comma found, skip ahead to next value */
        if (!skip_blank(p))
            error(p, "unexpected EOF, expected a value");
    }
}

static struct section *
parse_section(struct parser *p, struct strings *strings)
{
    int c;
    char *beg, *end;
    long nentry = 0;
    struct entry *entry;
    struct entry *tail = 0;
    struct section *section = 0;

    if (!skip_space(p))
        return 0; /* EOF */

    /* All sections must start with a '[' */
    c = get(p);
    if (c != '[')
        error(p, "unexpected '%c', expected '['", c);

    /* Extract the section name */
    if (!skip_space(p))
        error(p, "unexpected end of file");
    beg = p->p;
    c = get(p);
    if (c == '"')
        parse_string(p);
    else
        parse_simple(p, ']');
    end = p->p;

    /* Find the closing ']' */
    if (!skip_space(p))
        error(p, "unexpected end of file");
    c = get(p);
    if (c != ']')
        error(p, "unexpected '%c', expected ']'", c);

    /* With ']' now consumed, escape the string */
    beg = escape_string(beg, end);
    section = xmalloc(sizeof(*section));
    section->next = 0;
    section->name = strings_push(strings, beg);
    section->entries = 0;

    /* Parse entries */
    while ((entry = parse_entry(p, strings))) {
        if (!tail) {
            section->entries = tail = entry;
        } else {
            tail->next = entry;
            tail = tail->next;
        }
        if (++nentry > 65535)
            error(p, "too many entries in one section");
    }

    return section;
}

static void
store_u32(unsigned long x, FILE *f)
{
    fputc(x >>  0, f);
    fputc(x >>  8, f);
    fputc(x >> 16, f);
    fputc(x >> 24, f);
}

static void
store_u16(unsigned x, FILE *f)
{
    fputc(x >> 0, f);
    fputc(x >> 8, f);
}

static unsigned long
conv_f32(float x)
{
    union {
        float f;
        uint32_t i;
    } conv;
    conv.f = x;
    return conv.i;
}

static void
usage(FILE *f)
{
    fprintf(f, "usage: bini [-o path] [<INI|INI]\n");
    fprintf(f, "  -h       print this message\n");
    fprintf(f, "  -o path  output to a file (default: standard output)\n");
}

int
main(int argc, char **argv)
{
    size_t i;
    int option;
    unsigned long inlen;
    char *inbuf;
    unsigned long outlen = 12;
    FILE *in = stdin;
    FILE *out = stdout;
    struct section head = {0};
    struct section *section;
    struct section *tail = &head;
    struct parser parser = {"stdin", 1, 0, 0};
    struct strings strings;

    while ((option = getopt(argc, argv, "ho:")) != -1) {
        switch (option) {
            case 'h':
                usage(stdout);
                exit(EXIT_SUCCESS);
            case 'o':
                out = fopen(optarg, "wb");
                if (!out)
                    fatal("%s: %s", strerror(errno), optarg);
                break;
            default:
                usage(stderr);
                exit(EXIT_FAILURE);
        }
    }

    /* Use argument rather than standard input */
    if (argv[optind]) {
        if (argv[optind + 1])
            fatal("too many input arguments");
        in = fopen(argv[optind], "r");
        if (!in)
            fatal("%s: %s", strerror(errno), argv[optind]);
        parser.filename = argv[optind];
    } else {
#ifdef _WIN32
        int _setmode(int, int);
        _setmode(_fileno(stdout), 0x8000);
#endif
    }

    strings_init(&strings, 64);

    /* Initialize the parser */
    parser.p = inbuf = slurp(in, &inlen);
    parser.end = inbuf + inlen;

    /* Sanity check */
    if (inlen >= 5 && !memcmp(inbuf, "BINI\x01", 5))
        fatal("input is a BINI file, use unbini instead: aborting");

    /* Parse the input into sections */
    for (;;) {
        tail->next = parse_section(&parser, &strings);
        if (!tail->next)
            break;
        tail = tail->next;
    }

    strings_finalize(&strings);

    /* Compute string table offset */
    for (section = head.next; section; section = section->next) {
        struct entry *entry;
        outlen += 4;
        for (entry = section->entries; entry; entry = entry->next) {
            struct value *value;
            outlen += 3;
            for (value = entry->values; value; value = value->next)
                outlen += 5;
        }
    }

    /* Write bini header */
    store_u32(0x494e4942UL, out);
    store_u32(0x00000001UL, out);
    store_u32(outlen, out);

    /* Write all structs */
    for (section = head.next; section; section = section->next) {
        unsigned nentry = 0;
        struct entry *entry;
        for (entry = section->entries; entry; entry = entry->next)
            nentry++;

        /* Write section */
        store_u16(section->name->offset, out);
        store_u16(nentry, out);

        for (entry = section->entries; entry; entry = entry->next) {
            int nvalue = 0;
            struct value *value;
            for (value = entry->values; value; value = value->next)
                nvalue++;

            /* Write entry */
            store_u16(entry->name->offset, out);
            fputc(nvalue, out);

            for (value = entry->values; value; value = value->next) {
                /* Write value */
                fputc(value->type, out);
                switch (value->type) {
                    case 1:
                        store_u32(value->value.i, out);
                        break;
                    case 2:
                        store_u32(conv_f32(value->value.f), out);
                        break;
                    case 3:
                        store_u32(value->value.s->offset, out);
                        break;
                }
            }
        }
    }

    /* Write string table */
    for (i = 0; i < strings.count; i++) {
        char *s = strings.strings[i]->s;
        size_t len = strlen(s) + 1;
        fwrite(s, len, 1, out);
    }


    /* Cleanup */
    section = head.next;
    while (section) {
        struct section *sdead = section;
        struct entry *entry = section->entries;
        while (entry) {
            struct entry *edead = entry;
            struct value *value = entry->values;
            while (value) {
                struct value *vdead = value;
                value = value->next;
                free(vdead);
            }
            entry = entry->next;
            free(edead);
        }
        section = section->next;
        free(sdead);
    }
    strings_destroy(&strings);
    free(inbuf);

    if (fclose(out))
        fatal("%s", strerror(errno));
    if (in != stdin)
        fclose(in);
    return 0;
}
