#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h> /* Only for uint32_t */
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "getopt.h"

static unsigned long
parse_u32(const unsigned char *p)
{
    return (unsigned long)p[0] <<  0 | 
           (unsigned long)p[1] <<  8 | 
           (unsigned long)p[2] << 16 | 
           (unsigned long)p[3] << 24;
}

static unsigned
parse_u16(const unsigned char *p)
{
    return (unsigned)p[0] <<  0 | 
           (unsigned)p[1] <<  8;
}

static long
conv_s32(unsigned long x)
{
    if (x & 0x80000000UL) /* Sign extend? */
        return x | ~0xffffffffUL;
    return x;
}

static float
conv_f32(unsigned long x)
{
    union {
        uint32_t i;
        float f;
    } conv;
    conv.i = x;
    return conv.f;
}

/* Print a string, potentially quoted/escaped.
 *
 * If S contains any character from SPECIAL, it will be quoted. If
 * SPECIAL is a null pointer, always printed it quoted.
 */
static void
print_special(const unsigned char *s, char *special, FILE *out)
{
    int simple = special && !strpbrk((char *)s, special);
    if (simple) {
        fputs((char *)s, out);
    } else {
        fputc('"', out);
        for (; *s; s++) {
            if (*s == '"')
                fputc('"', out);
            fputc(*s, out);
        }
        fputc('"', out);
    }
}

static void
print_section_name(const unsigned char *s, FILE *out)
{
    fputc('[', out);
    print_special(s, "\"[] \f\n\r\t\v", out);
    fputs("]\n", out);
}

static void
print_entry_name(const unsigned char *s, FILE *out)
{
    print_special(s, "\"=[] \f\n\r\t\v", out);
    fputs(" =", out);
}

static void
print_string(const unsigned char *s, FILE *out)
{
    long i;
    double f;
    char *end;

    /* Does it look like a float? Quote it. */
    errno = 0;
    f = strtod((char *)s, &end);
    if ((f != 0 || !errno) && *end == 0) {
        print_special(s, 0, out);
        return;
    }
    
    /* Does it look an integer? Quote it. */
    errno = 0;
    i = strtol((char *)s, &end, 10);
    if ((i != 0 || !errno) && *end == 0) {
        print_special(s, 0, out);
        return;
    }

    /* Print it as a string, maybe quoting it. */
    print_special(s, "\", \f\n\r\t\v", out);
}

/* Print the simplest form that parses identically with strtod().
 */
static void
print_minfloat(float f, FILE *out)
{
    int i;
    char buf[64];
    for (i = 1; i < 9; i++) {
        sprintf(buf, "%.*g", i, f);
        if (f == (float)strtod(buf, 0)) {
            fputs(buf, out);
            return;
        }
    }
    fprintf(out, "%.9g", f);
}

static void
usage(FILE *f)
{
    fprintf(f, "usage: unbini [-o path] [<BINI|BINI]\n");
    fprintf(f, "  -h       print this message\n");
    fprintf(f, "  -o path  output to a file (default: standard output)\n");
}

int
main(int argc, char **argv)
{
    int option;
    FILE *in = stdin;
    FILE *out = stdout;
    unsigned long len, textlen;
    unsigned long bini, vers, textoff;
    unsigned char *buf, *text, *p;

    while ((option = getopt(argc, argv, "ho:")) != -1) {
        switch (option) {
            case 'h':
                usage(stdout);
                exit(EXIT_SUCCESS);
            case 'o':
                out = fopen(optarg, "w");
                if (!out)
                    fatal("%s: %s", strerror(errno), optarg);
                break;
            default:
                usage(stderr);
                exit(EXIT_FAILURE);
        }
    }

    if (argv[optind]) {
        /* Open given filename */
        if (argv[optind + 1])
            fatal("too many input arguments");
        in = fopen(argv[optind], "rb");
        if (!in)
            fatal("%s: %s", strerror(errno), argv[optind]);
    } else {
        /* Use standard input */
        #ifdef _WIN32
        int _setmode(int, int);
        _setmode(_fileno(stdin), 0x8000);
        #endif
    }

    buf = slurp(in, &len);

    /* Validate the input */
    if (len < 12)
        fatal("input is too short: %lu bytes", len);
    bini    = parse_u32(buf + 0);
    vers    = parse_u32(buf + 4);
    textoff = parse_u32(buf + 8);
    if (bini != 0x494e4942UL)
        fatal("unknown input format (bad magic): 0x%08lx", bini);
    if (vers != 0x00000001UL)
        fatal("unknown input format (bad version): %lu", vers);
    if (textoff > len)
        fatal("unknown input format (bad text offset): %lu", textoff);
    if (textoff < len && buf[len - 1] != 0)
        fatal("invalid input (unterminated text segment)");

    /* Set up data pointers */
    p = buf + 12;
    text = buf + textoff;
    textlen = len - textoff;

    /* Parse each section */
    while (p < text - 3) {
        unsigned i;
        unsigned section_name = parse_u16(p + 0);
        unsigned nentry = parse_u16(p + 2);

        /* Print section name */
        if (section_name >= textlen)
            fatal("invalid section text offset, aborting");
        if (p > buf + 12)
            fputc('\n', out);
        print_section_name(text + section_name, out);

        /* Print each entry */
        p += 4;
        for (i = 0; i < nentry; i++) {
            int j, nvalue;
            unsigned name;

            /* is there enough room for this entry? */
            if (p > text - 3)
                fatal("truncated entry, aborting");
            
            /* parse entry struct */
            name = parse_u16(p);
            nvalue = p[2];
            p += 3;

            /* validate entry struct */
            if (name >= textlen)
                fatal("invalid entry text offset, aborting");
            if (nvalue * 5UL > (unsigned long)(text - p))
                fatal("truncated entry value, aborting");

            /* print each value */
            print_entry_name(text + name, out);
            for (j = 0; j < nvalue; j++) {
                int type = p[j * 5 + 0];
                unsigned long val = parse_u32(p + j * 5 + 1);

                fputs(j ? ", " : " ", out);
                switch (type) {
                    case 1:
                        fprintf(out, "%ld", conv_s32(val));
                        break;
                    case 2:
                        print_minfloat(conv_f32(val), out);
                        break;
                    case 3:
                        if (val >= textlen)
                            fatal("invalid value text offset, aborting");
                        print_string(text + val, out);
                        break;
                    default:
                        fatal("bad value type, %d", type);
                }
            }
            fputc('\n', out);

            p += nvalue * 5;
        }
    }

    /* Pointer *should* now be exactly at the text segment */
    if (p != text) {
        int c = (int)(text - p);
        fprintf(stderr, "warning: %d garbage byte%s before text segment\n",
                c, c == 1 ? "" : "s");
    }

    /* Clean up */
    if (fclose(out))
        fatal("%s", strerror(errno));
    if (in != stdin)
        fclose(in);
    free(buf);
    return 0;
}
