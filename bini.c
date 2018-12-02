#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h> /* Only for uint32_t */
#include <stdlib.h>
#include <string.h>

#define PROGRAM_NAME "bini"

#include "trie.h"
#include "common.h"
#include "getopt.h"

static int
xisplainspace(int c)
{
    /* isspace() without newlines */
    return c == ' ' || c == '\f' || c == '\r' || c == '\t' || c == '\v';
}

static int
xisspace(int c)
{
    /* isspace() changes depending on the locale */
    return c == ' '  || c == '\f' || c == '\n' ||
           c == '\r' || c == '\t' || c == '\v';
}

static void
reverse(char *s)
{
    size_t i, len = strlen(s);
    for (i = 0; i < len / 2; i++) {
        int tmp = s[i];
        s[i] = s[len - i - 1];
        s[len - i - 1] = (char)tmp;
    }
}

/* String intern table */

struct string {
    char *s;
    struct string *parent;
    long offset;
};

static long
string_offset(struct string *s)
{
    long parent, offset;
    if (s->offset != -1) {
        offset = s->offset;
    } else {
        parent = string_offset(s->parent);
        offset = parent + (long)strlen(s->parent->s) - (long)strlen(s->s);
        if (offset > 65535)
            fatal("too many strings");
    }
    return offset;
}

static struct string *
strings_push(struct trie *t, char *str)
{
    struct string *s;
    reverse(str);
    s = trie_search(t, str);
    if (!s) {
        s = xmalloc(sizeof(*s));
        s->s = str;
        s->parent = 0;
        s->offset = -1;
        if (trie_insert(t, str, s))
            fatal("out of memory");
    }
    reverse(str);
    return s;
}

/* Child to be connected to next-visited string */
static struct string *child = 0;

static int
compute_offset(const char *key, void *data, void *arg, int nsiblings)
{
    struct string *s = data;
    long *offset = arg;
    if (child)
        child->parent = s;
    if (nsiblings) {
        /* Secondary string, connect it to the next visited node */
        child = s;
    } else {
        /* Primary string, append it to the table */
        if (*offset > 65535)
            fatal("too many strings");
        s->offset = *offset;
        *offset += (long)strlen(key) + 1;
        child = 0;
    }
    return 0;
}

/* Compute string table offsets. */
static long
strings_finalize(struct trie *t)
{
    long offset = 0;
    if (trie_visit(t, "", compute_offset, &offset))
        fatal("out of memory");
    return offset;
}

static int
free_visitor(const char *key, void *data, void *arg, int nsiblings)
{
    (void)key;
    (void)arg;
    (void)nsiblings;
    free(data);
    return 0;
}

static void
strings_free(struct trie *t)
{
    trie_visit(t, "", free_visitor, 0);
    trie_free(t);
}

static int
write_visit(const char *key, void *data, void *arg, int nsiblings)
{
    FILE *out = arg;
    struct string *s = data;
    size_t len = strlen(s->s) + 1;
    (void)key;
    if (!nsiblings)
        fwrite(s->s, len, 1, out);
    return 0;
}

static void
strings_write(struct trie *t, FILE *out)
{
    if (trie_visit(t, "", write_visit, out))
        fatal("out of memory");
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
        if (!c)
            error(p, "invalid NUL byte");
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
        for (c = get(p); c != -1 && xisspace(c); c = get(p))
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
    for (c = get(p); c != -1 && xisplainspace(c); c = get(p))
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
        beg++;
        for (s = beg; s < end; s++) {
            if (*s == '"') {
                memmove(s, s + 1, end - s);
                end--;
            }
        }
    } else {
        while (xisspace(*beg))
            beg++;
        while (xisspace(end[-1]))
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
    int nvalue;
};

struct section {
    struct section *next;
    struct string *name;
    struct entry *entries;
    long nentry;
    unsigned long size;
};

static struct value *
parse_value(struct parser *p, struct trie *strings, int *nextc)
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

    } else if (c == '\r' || c == '\n' || c == ',') {
        error(p, "missing/empty value");
        return 0;

    } else {
        long i;
        float f;

        /* Extract the token as if it were a simple string */
        parse_simple(p, ',');
        end = p->p;
        *nextc = get(p);
        beg = escape_string(beg, end);

        /* Negative zero? */
        if (end - beg == 2 && beg[0] == '-' && beg[1] == '0') {
            value->value.f = -0.0f;
            value->type = VALUE_FLOAT;
            return value;
        }

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
        if (!*end && (f || !errno)) {
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
parse_entry(struct parser *p, struct trie *strings)
{
    int c;
    char *beg, *end;
    struct value *value;
    struct value *tail = 0;
    struct entry *entry;

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
    entry->nvalue = 0;

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
        if (++entry->nvalue > 255)
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
parse_section(struct parser *p, struct trie *strings)
{
    int c;
    char *beg, *end;
    struct entry *entry;
    struct entry *tail = 0;
    struct section *section;

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
    section->nentry = 0;
    section->size = 4;

    /* Parse entries */
    while ((entry = parse_entry(p, strings))) {
        if (!tail) {
            section->entries = tail = entry;
        } else {
            tail->next = entry;
            tail = tail->next;
        }
        if (++section->nentry > 65535)
            error(p, "too many entries in one section");
        section->size += 3 + entry->nvalue * 5;
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

int
main(int argc, char **argv)
{
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
    struct trie *strings;

    while ((option = getopt(argc, argv, "ho:V")) != -1) {
        switch (option) {
            case 'h':
                usage(stdout);
                exit(EXIT_SUCCESS);
            case 'o':
                out = fopen(optarg, "wb");
                if (!out)
                    fatal("%s: %s", strerror(errno), optarg);
                break;
            case 'V':
                version();
                exit(EXIT_SUCCESS);
            default:
                usage(stderr);
                exit(EXIT_FAILURE);
        }
    }

    /* Use argument rather than standard input */
    if (argv[optind]) {
        if (argv[optind + 1])
            fatal("too many input arguments");
        in = fopen(argv[optind], "rb");
        if (!in)
            fatal("%s: %s", strerror(errno), argv[optind]);
        parser.filename = argv[optind];
    }

#ifdef _WIN32
    {
        int _setmode(int, int);
        if (out == stdout)
            _setmode(_fileno(stdout), 0x8000);
        if (in == stdin)
            _setmode(_fileno(stdin), 0x8000);
    }
#endif

    strings = trie_create();

    /* Initialize the parser */
    parser.p = inbuf = slurp(in, &inlen);
    parser.end = inbuf + inlen;

    /* Sanity check */
    if (inlen >= 5 && !memcmp(inbuf, "BINI\x01", 5))
        fatal("input is a BINI file, use unbini instead: aborting");

    /* Parse the input into sections */
    for (;;) {
        tail->next = parse_section(&parser, strings);
        if (!tail->next)
            break;
        tail = tail->next;
        outlen += tail->size;
    }

    strings_finalize(strings);

    /* Write bini header */
    store_u32(0x494e4942UL, out);
    store_u32(0x00000001UL, out);
    store_u32(outlen, out);

    /* Write all structs */
    for (section = head.next; section; section = section->next) {
        struct entry *entry;

        /* Write section */
        store_u16(string_offset(section->name), out);
        store_u16(section->nentry, out);

        for (entry = section->entries; entry; entry = entry->next) {
            struct value *value;

            /* Write entry */
            store_u16(string_offset(entry->name), out);
            fputc(entry->nvalue, out);

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
                        store_u32(string_offset(value->value.s), out);
                        break;
                }
            }
        }
    }

    /* Write string table */
    strings_write(strings, out);

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
    strings_free(strings);
    free(inbuf);

    if (fclose(out))
        fatal("%s", strerror(errno));
    if (in != stdin)
        fclose(in);
    return 0;
}
