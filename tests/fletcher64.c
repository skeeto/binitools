/* Compute Fletcher-64 checksum
 *
 * All bytes are processed in little-endian byte order regardless of the
 * host systems' native byte order.
 *
 * This is free and unencumbered software released into the public domain.
 */
#include <stdio.h>
#include <stdlib.h>

#define FLETCHER64_SIZE 8
#define FLETCHER64_INIT {0, 0}

struct fletcher64 {
    unsigned long lo;
    unsigned long hi;
};

/* Append a buffer of input to the checksum. All input buffers except
 * for the final one must have a length divisible by four.
 */
static void
fletcher64_append(struct fletcher64 *ctx, void *buf, size_t len)
{
    unsigned long block;
    unsigned long lo = ctx->lo;
    unsigned long hi = ctx->hi;
    const unsigned char *ptr = buf;
    const unsigned char *end = ptr + len / 4 * 4;

    /* Process each 32-bit block */
    for (; ptr < end; ptr += 4) {
        block = (unsigned long)ptr[0] <<  0 |
                (unsigned long)ptr[1] <<  8 |
                (unsigned long)ptr[2] << 16 |
                (unsigned long)ptr[3] << 24;
        lo += block;
        hi += lo;
    }

    /* Process any tail bytes */
    block = 0;
    switch (len % 4) {
        case 3: block |= (unsigned long)end[2] << 16; /* FALLTHROUGH */
        case 2: block |= (unsigned long)end[1] <<  8; /* FALLTHROUGH */
        case 1: block |= (unsigned long)end[0] <<  0;
                lo += block;
                hi += lo;
    }

    ctx->lo = lo;
    ctx->hi = hi;
}

static void
fletcher64_finish(struct fletcher64 *ctx, void *sum)
{
    unsigned long lo = ctx->lo;
    unsigned long hi = ctx->hi;
    unsigned char *p = sum;
    p[0] = lo >>  0;
    p[1] = lo >>  8;
    p[2] = lo >> 16;
    p[3] = lo >> 24;
    p[4] = hi >>  0;
    p[5] = hi >>  8;
    p[6] = hi >> 16;
    p[7] = hi >> 24;
}

int
main(void)
{
    int i;
    char result[17];
    unsigned char sum[FLETCHER64_SIZE];
    struct fletcher64 ctx = FLETCHER64_INIT;

#ifdef _WIN32
    {
        int _setmode(int, int);
        _setmode(_fileno(stdin), 0x8000);
    }
#endif

    for (;;) {
        static char buf[4096];
        size_t len = fread(buf, 1, sizeof(buf), stdin);
        if (!len) {
            if (ferror(stdin)) {
                fputs("fletcher64: input error\n", stderr);
                exit(EXIT_FAILURE);
            }
            break;
        }
        fletcher64_append(&ctx, buf, len);
        if (len < sizeof(buf))
            break;
    }

    fletcher64_finish(&ctx, sum);
    for (i = 0; i < 8; i++) {
        static const char hex[] = "0123456789abcdef";
        unsigned v = sum[i];
        result[i * 2 + 0] = hex[v >> 4];
        result[i * 2 + 1] = hex[v & 0x0f];
    }
    result[16] = 0;
    puts(result);

    if (fflush(stdout)) {
        fputs("fletcher64: output error\n", stderr);
        exit(EXIT_FAILURE);
    }
    return 0;
}
