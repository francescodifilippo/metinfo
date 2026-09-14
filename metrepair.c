/**
 * metrepair - offline .part.met verification and reconstruction tool
 *
 * Companion to metinfo. Given a "reference" source of per-block MD4
 * hashes (either a healthy .part.met file, or an ed2k link that embeds
 * a p=hash1:hash2:... part-hash parameter) and a .part data file on
 * disk, it re-hashes each block of the data file and compares it to
 * the reference, then can rebuild a valid .part.met with correct
 * Gap/Filename/Filesize/Transferred tags.
 *
 * This reproduces the core, purely local part of what tools like
 * MetMedic do to repair a .part.met lost to a crash: it never
 * contacts any eDonkey/eMule server or peer. You still need to obtain
 * the reference block hashes yourself (e.g. by re-adding the same
 * download in eMule/aMule once, which recreates a fresh, correctly
 * hashed but 0%-downloaded .part.met - or from an ed2k link that
 * includes a p= hash-set).
 *
 * See meta.md section 4.4 and README.md for background on the
 * .part.met tag format this relies on.
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <getopt.h>
#include <errno.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

#define PARTSIZE 9728000ULL /* bytes per block, per the .part.met format */

#define TAGTYPE_HASH16    0x01
#define TAGTYPE_STRING    0x02
#define TAGTYPE_UINT32    0x03
#define TAGTYPE_FLOAT32   0x04
#define TAGTYPE_BOOL      0x05
#define TAGTYPE_BOOLARRAY 0x06
#define TAGTYPE_BLOB      0x07
#define TAGTYPE_UINT16    0x08
#define TAGTYPE_UINT8     0x09
#define TAGTYPE_BSOB      0x0A
#define TAGTYPE_UINT64    0x0B
#define TAGTYPE_STR1      0x11
#define TAGTYPE_STR16     0x20

#define FT_FILENAME    1
#define FT_FILESIZE    2
#define FT_TRANSFERRED 8

#define PARTFILE_VERSION_14_0      224 /* 0xE0 */
#define PARTFILE_VERSION_14_1      225 /* 0xE1 */
#define PARTFILE_VERSION_LARGEFILE 226 /* 0xE2 */

/* ------------------------------------------------------------------ */
/* MD4 (RFC 1320 reference algorithm; not part of the standard libc)  */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned int state[4];
    unsigned int count[2]; /* message length in bits, low/high */
    unsigned char buffer[64];
} MD4_CTX;

static void md4Encode(unsigned char *output, const unsigned int *input, unsigned int len) {
    unsigned int i, j;
    for (i = 0, j = 0; j < len; i++, j += 4) {
        output[j]     = (unsigned char)(input[i] & 0xff);
        output[j + 1] = (unsigned char)((input[i] >> 8) & 0xff);
        output[j + 2] = (unsigned char)((input[i] >> 16) & 0xff);
        output[j + 3] = (unsigned char)((input[i] >> 24) & 0xff);
    }
}

static void md4Decode(unsigned int *output, const unsigned char *input, unsigned int len) {
    unsigned int i, j;
    for (i = 0, j = 0; j < len; i++, j += 4) {
        output[i] = ((unsigned int)input[j]) | (((unsigned int)input[j + 1]) << 8) |
                    (((unsigned int)input[j + 2]) << 16) | (((unsigned int)input[j + 3]) << 24);
    }
}

#define MD4_F(x, y, z) (((x) & (y)) | ((~(x)) & (z)))
#define MD4_G(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define MD4_H(x, y, z) ((x) ^ (y) ^ (z))
#define MD4_ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define MD4_FF(a, b, c, d, x, s) { (a) += MD4_F((b), (c), (d)) + (x); (a) = MD4_ROTL((a), (s)); }
#define MD4_GG(a, b, c, d, x, s) { (a) += MD4_G((b), (c), (d)) + (x) + 0x5a827999u; (a) = MD4_ROTL((a), (s)); }
#define MD4_HH(a, b, c, d, x, s) { (a) += MD4_H((b), (c), (d)) + (x) + 0x6ed9eba1u; (a) = MD4_ROTL((a), (s)); }

static void md4Transform(unsigned int state[4], const unsigned char block[64]) {
    unsigned int a = state[0], b = state[1], c = state[2], d = state[3], x[16];
    md4Decode(x, block, 64);

    /* Round 1 */
    MD4_FF(a, b, c, d, x[0], 3);  MD4_FF(d, a, b, c, x[1], 7);
    MD4_FF(c, d, a, b, x[2], 11); MD4_FF(b, c, d, a, x[3], 19);
    MD4_FF(a, b, c, d, x[4], 3);  MD4_FF(d, a, b, c, x[5], 7);
    MD4_FF(c, d, a, b, x[6], 11); MD4_FF(b, c, d, a, x[7], 19);
    MD4_FF(a, b, c, d, x[8], 3);  MD4_FF(d, a, b, c, x[9], 7);
    MD4_FF(c, d, a, b, x[10], 11); MD4_FF(b, c, d, a, x[11], 19);
    MD4_FF(a, b, c, d, x[12], 3);  MD4_FF(d, a, b, c, x[13], 7);
    MD4_FF(c, d, a, b, x[14], 11); MD4_FF(b, c, d, a, x[15], 19);

    /* Round 2 */
    MD4_GG(a, b, c, d, x[0], 3);  MD4_GG(d, a, b, c, x[4], 5);
    MD4_GG(c, d, a, b, x[8], 9);  MD4_GG(b, c, d, a, x[12], 13);
    MD4_GG(a, b, c, d, x[1], 3);  MD4_GG(d, a, b, c, x[5], 5);
    MD4_GG(c, d, a, b, x[9], 9);  MD4_GG(b, c, d, a, x[13], 13);
    MD4_GG(a, b, c, d, x[2], 3);  MD4_GG(d, a, b, c, x[6], 5);
    MD4_GG(c, d, a, b, x[10], 9); MD4_GG(b, c, d, a, x[14], 13);
    MD4_GG(a, b, c, d, x[3], 3);  MD4_GG(d, a, b, c, x[7], 5);
    MD4_GG(c, d, a, b, x[11], 9); MD4_GG(b, c, d, a, x[15], 13);

    /* Round 3 */
    MD4_HH(a, b, c, d, x[0], 3);  MD4_HH(d, a, b, c, x[8], 9);
    MD4_HH(c, d, a, b, x[4], 11); MD4_HH(b, c, d, a, x[12], 15);
    MD4_HH(a, b, c, d, x[2], 3);  MD4_HH(d, a, b, c, x[10], 9);
    MD4_HH(c, d, a, b, x[6], 11); MD4_HH(b, c, d, a, x[14], 15);
    MD4_HH(a, b, c, d, x[1], 3);  MD4_HH(d, a, b, c, x[9], 9);
    MD4_HH(c, d, a, b, x[5], 11); MD4_HH(b, c, d, a, x[13], 15);
    MD4_HH(a, b, c, d, x[3], 3);  MD4_HH(d, a, b, c, x[11], 9);
    MD4_HH(c, d, a, b, x[7], 11); MD4_HH(b, c, d, a, x[15], 15);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

static void md4Init(MD4_CTX *ctx) {
    ctx->count[0] = ctx->count[1] = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
}

static void md4Update(MD4_CTX *ctx, const unsigned char *input, unsigned int inputLen) {
    unsigned int i, index, partLen;

    index = (ctx->count[0] >> 3) & 0x3f;
    if ((ctx->count[0] += (inputLen << 3)) < (inputLen << 3)) {
        ctx->count[1]++;
    }
    ctx->count[1] += (inputLen >> 29);

    partLen = 64 - index;

    if (inputLen >= partLen) {
        memcpy(&ctx->buffer[index], input, partLen);
        md4Transform(ctx->state, ctx->buffer);
        for (i = partLen; i + 63 < inputLen; i += 64) {
            md4Transform(ctx->state, &input[i]);
        }
        index = 0;
    } else {
        i = 0;
    }

    memcpy(&ctx->buffer[index], &input[i], inputLen - i);
}

static const unsigned char MD4_PADDING[64] = { 0x80 /* rest is zero-initialized */ };

static void md4Final(unsigned char digest[16], MD4_CTX *ctx) {
    unsigned char bits[8];
    unsigned int index, padLen;

    md4Encode(bits, ctx->count, 8);

    index = (ctx->count[0] >> 3) & 0x3f;
    padLen = (index < 56) ? (56 - index) : (120 - index);
    md4Update(ctx, MD4_PADDING, padLen);
    md4Update(ctx, bits, 8);

    md4Encode(digest, ctx->state, 16);
    memset(ctx, 0, sizeof(*ctx));
}

static void md4Digest(const unsigned char *data, size_t len, unsigned char out[16]) {
    MD4_CTX ctx;
    md4Init(&ctx);
    md4Update(&ctx, data, (unsigned int)len);
    md4Final(out, &ctx);
}

/**
 * Verify the MD4 implementation against the RFC 1320 test vectors.
 * Correctness here is critical: this whole tool exists to compare
 * hashes, so a broken implementation must never run silently.
 */
static void md4SelfTest(void) {
    struct { const char *msg; const char *expectedHex; } vectors[] = {
        { "",    "31d6cfe0d16ae931b73c59d7e0c089c0" },
        { "abc", "a448017aaf21d8525fc10ae87aa6729d" },
        { "message digest", "d9130a8164549fe818874806e1c7014b" },
    };

    for (size_t v = 0; v < sizeof(vectors) / sizeof(vectors[0]); v++) {
        unsigned char digest[16];
        md4Digest((const unsigned char *)vectors[v].msg, strlen(vectors[v].msg), digest);

        char got[33];
        for (int i = 0; i < 16; i++) {
            sprintf(got + i * 2, "%02x", digest[i]);
        }
        got[32] = '\0';

        if (strcmp(got, vectors[v].expectedHex) != 0) {
            errx(EXIT_FAILURE,
                "Internal error: MD4 self-test failed for input \"%s\" (got %s, expected %s). "
                "Refusing to run with an unverified hash implementation.",
                vectors[v].msg, got, vectors[v].expectedHex);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Low-level file I/O helpers (same wire format as metinfo.c)         */
/* ------------------------------------------------------------------ */

static unsigned char readByte(int fd) {
    unsigned char byte;
    if (read(fd, &byte, 1) != 1) {
        err(EXIT_FAILURE, "Error reading file");
    }
    return byte;
}

static unsigned short readWord(int fd) {
    unsigned char b[2];
    if (read(fd, b, 2) != 2) {
        err(EXIT_FAILURE, "Error reading file");
    }
    return (unsigned short)(b[0] | (b[1] << 8));
}

static unsigned int readDWord(int fd) {
    unsigned char b[4];
    if (read(fd, b, 4) != 4) {
        err(EXIT_FAILURE, "Error reading file");
    }
    return (b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
}

static unsigned long long readQWord(int fd) {
    unsigned char b[8];
    if (read(fd, b, 8) != 8) {
        err(EXIT_FAILURE, "Error reading file");
    }
    unsigned long long v = 0;
    for (int i = 7; i >= 0; i--) {
        v = (v << 8) | b[i];
    }
    return v;
}

static void readBytes(int fd, unsigned char *buf, size_t len) {
    if (len == 0) return;
    ssize_t got = read(fd, buf, len);
    if (got < 0 || (size_t)got != len) {
        err(EXIT_FAILURE, "Error reading file");
    }
}

static char *readString(int fd, unsigned int len) {
    char *s = (char *)malloc(len + 1);
    if (s == NULL) {
        err(EXIT_FAILURE, "Memory allocation error");
    }
    readBytes(fd, (unsigned char *)s, len);
    s[len] = '\0';
    return s;
}

static void writeByte(int fd, unsigned char v) {
    if (write(fd, &v, 1) != 1) {
        err(EXIT_FAILURE, "Error writing output file");
    }
}

static void writeWord(int fd, unsigned short v) {
    unsigned char b[2] = { (unsigned char)(v & 0xff), (unsigned char)((v >> 8) & 0xff) };
    if (write(fd, b, 2) != 2) {
        err(EXIT_FAILURE, "Error writing output file");
    }
}

static void writeDWord(int fd, unsigned int v) {
    unsigned char b[4] = {
        (unsigned char)(v & 0xff), (unsigned char)((v >> 8) & 0xff),
        (unsigned char)((v >> 16) & 0xff), (unsigned char)((v >> 24) & 0xff)
    };
    if (write(fd, b, 4) != 4) {
        err(EXIT_FAILURE, "Error writing output file");
    }
}

static void writeQWord(int fd, unsigned long long v) {
    unsigned char b[8];
    for (int i = 0; i < 8; i++) {
        b[i] = (unsigned char)(v & 0xff);
        v >>= 8;
    }
    if (write(fd, b, 8) != 8) {
        err(EXIT_FAILURE, "Error writing output file");
    }
}

static void writeBytes(int fd, const unsigned char *buf, size_t len) {
    if (len == 0) return;
    ssize_t w = write(fd, buf, len);
    if (w < 0 || (size_t)w != len) {
        err(EXIT_FAILURE, "Error writing output file");
    }
}

/* ------------------------------------------------------------------ */
/* Reference info: id hash, per-block hashes, filename, filesize      */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned char idHash[16];
    unsigned int numBlocks;
    unsigned char (*blockHashes)[16];
    char *filename;
    unsigned long long filesize;
} RefInfo;

static void freeRefInfo(RefInfo *r) {
    free(r->blockHashes);
    free(r->filename);
}

/**
 * Read (and mostly discard) the tags of a .part.met file, capturing
 * only the FILENAME and FILESIZE special tags. Reused for every
 * .part.met version, since the tag encoding itself is identical; only
 * where the tag list sits in the file differs.
 */
static void readAndExtractTags(int fd, unsigned int numTags, char **outFilename, unsigned long long *outFilesize) {
    for (unsigned int i = 0; i < numTags; i++) {
        int type = readByte(fd);
        unsigned int nameLen = readWord(fd);
        unsigned char nameId = 0;
        char *name = NULL;
        if (nameLen == 1) {
            nameId = readByte(fd);
        } else {
            name = readString(fd, nameLen);
        }

        unsigned long long ival = 0;
        char *sval = NULL;

        switch (type) {
            case TAGTYPE_HASH16: {
                unsigned char h[16];
                readBytes(fd, h, 16);
                break;
            }
            case TAGTYPE_STRING: {
                unsigned int len = readWord(fd);
                sval = readString(fd, len);
                break;
            }
            case TAGTYPE_UINT8:
                ival = readByte(fd);
                break;
            case TAGTYPE_UINT16:
                ival = readWord(fd);
                break;
            case TAGTYPE_UINT32:
                ival = readDWord(fd);
                break;
            case TAGTYPE_UINT64:
                ival = readQWord(fd);
                break;
            case TAGTYPE_BOOL:
                ival = readByte(fd);
                break;
            case TAGTYPE_FLOAT32: {
                unsigned char b[4];
                readBytes(fd, b, 4);
                break;
            }
            case TAGTYPE_BOOLARRAY: {
                unsigned short bitLen = readWord(fd);
                if (lseek(fd, (bitLen / 8) + 1, SEEK_CUR) == (off_t)-1) {
                    err(EXIT_FAILURE, "Error seeking within reference file");
                }
                break;
            }
            case TAGTYPE_BLOB: {
                unsigned int blobLen = readDWord(fd);
                if (lseek(fd, blobLen, SEEK_CUR) == (off_t)-1) {
                    err(EXIT_FAILURE, "Error seeking within reference file");
                }
                break;
            }
            case TAGTYPE_BSOB: {
                unsigned char bsobLen = readByte(fd);
                if (lseek(fd, bsobLen, SEEK_CUR) == (off_t)-1) {
                    err(EXIT_FAILURE, "Error seeking within reference file");
                }
                break;
            }
            default:
                if (type >= TAGTYPE_STR1 && type <= TAGTYPE_STR16) {
                    int len = type - TAGTYPE_STR1 + 1;
                    sval = readString(fd, (unsigned int)len);
                } else {
                    errx(EXIT_FAILURE, "Unrecognized tag type 0x%02X while reading reference .part.met", type);
                }
        }

        if (nameLen == 1) {
            if (nameId == FT_FILENAME && sval != NULL) {
                free(*outFilename);
                *outFilename = sval;
                sval = NULL;
            } else if (nameId == FT_FILESIZE) {
                *outFilesize = ival;
            }
        }

        free(name);
        free(sval);
    }
}

static RefInfo readRefFromPartMet(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        err(EXIT_FAILURE, "Unable to open reference file %s", path);
    }

    RefInfo ref;
    memset(&ref, 0, sizeof(ref));

    unsigned char verByte = readByte(fd);

    if (verByte == PARTFILE_VERSION_14_0 || verByte == PARTFILE_VERSION_LARGEFILE) {
        readDWord(fd); /* date */
        readBytes(fd, ref.idHash, 16);
        ref.numBlocks = readWord(fd);
        ref.blockHashes = malloc((size_t)ref.numBlocks * 16);
        if (ref.blockHashes == NULL && ref.numBlocks > 0) {
            err(EXIT_FAILURE, "Memory allocation error");
        }
        for (unsigned int i = 0; i < ref.numBlocks; i++) {
            readBytes(fd, ref.blockHashes[i], 16);
        }
        unsigned int numTags = readDWord(fd);
        readAndExtractTags(fd, numTags, &ref.filename, &ref.filesize);

    } else if (verByte == PARTFILE_VERSION_14_1) {
        readByte(fd); /* unknown1 */
        readDWord(fd); /* date */
        readBytes(fd, ref.idHash, 16);
        unsigned int numTags = readDWord(fd);
        readAndExtractTags(fd, numTags, &ref.filename, &ref.filesize);

        unsigned char haveHashes = readByte(fd);
        if (haveHashes == 1) {
            unsigned int nblocks = (unsigned int)(ref.filesize / PARTSIZE);
            if (ref.filesize % PARTSIZE != 0) nblocks++;
            ref.numBlocks = nblocks;
            ref.blockHashes = malloc((size_t)nblocks * 16);
            if (ref.blockHashes == NULL && nblocks > 0) {
                err(EXIT_FAILURE, "Memory allocation error");
            }
            for (unsigned int i = 0; i < nblocks; i++) {
                readBytes(fd, ref.blockHashes[i], 16);
            }
        } else {
            close(fd);
            errx(EXIT_FAILURE,
                "Reference file %s is a v14.1 .part.met with no stored block hashes "
                "(HaveHashes=0); it cannot be used to verify blocks. Use a v14.0/0xE2 "
                "reference instead.", path);
        }
    } else {
        close(fd);
        errx(EXIT_FAILURE, "Unrecognized .part.met version in reference file %s: %d", path, verByte);
    }

    close(fd);

    if (ref.filename == NULL) {
        errx(EXIT_FAILURE, "Reference file %s has no FILENAME tag", path);
    }
    if (ref.filesize == 0) {
        errx(EXIT_FAILURE, "Reference file %s has no (non-zero) FILESIZE tag", path);
    }
    if (ref.numBlocks == 0 || ref.blockHashes == NULL) {
        errx(EXIT_FAILURE, "Reference file %s has no block hashes", path);
    }

    return ref;
}

/* ------------------------------------------------------------------ */
/* ed2k link parsing (ed2k://|file|NAME|SIZE|HASH|p=H1:H2:...|/)      */
/* ------------------------------------------------------------------ */

static int isEd2kLink(const char *s) {
    return strncasecmp(s, "ed2k://", 7) == 0;
}

/* strdup() is POSIX, not standard C99; provide our own to stay -std=c99 clean */
static char *ownStrdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (copy == NULL) {
        err(EXIT_FAILURE, "Memory allocation error");
    }
    memcpy(copy, s, len);
    return copy;
}

static int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hexToBytes(const char *hex, unsigned char *out, int outLen) {
    if ((int)strlen(hex) != outLen * 2) return -1;
    for (int i = 0; i < outLen; i++) {
        int hi = hexNibble(hex[i * 2]);
        int lo = hexNibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    return 0;
}

static char *urlDecode(const char *s) {
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (out == NULL) {
        err(EXIT_FAILURE, "Memory allocation error");
    }
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '%' && i + 2 < len && hexNibble(s[i + 1]) >= 0 && hexNibble(s[i + 2]) >= 0) {
            out[j++] = (char)((hexNibble(s[i + 1]) << 4) | hexNibble(s[i + 2]));
            i += 2;
        } else if (s[i] == '+') {
            out[j++] = ' ';
        } else {
            out[j++] = s[i];
        }
    }
    out[j] = '\0';
    return out;
}

static RefInfo parseEd2kLink(const char *link) {
    RefInfo ref;
    memset(&ref, 0, sizeof(ref));

    const char *prefix = "ed2k://";
    char *copy = ownStrdup(link);
    if (copy == NULL) {
        err(EXIT_FAILURE, "Memory allocation error");
    }

    char *fields[64];
    int nfields = 0;
    char *tok = strtok(copy + strlen(prefix), "|");
    while (tok != NULL && nfields < 64) {
        fields[nfields++] = tok;
        tok = strtok(NULL, "|");
    }

    if (nfields < 4 || strcasecmp(fields[0], "file") != 0) {
        free(copy);
        errx(EXIT_FAILURE, "Unsupported ed2k link (expected ed2k://|file|NAME|SIZE|HASH|...)");
    }

    ref.filename = urlDecode(fields[1]);
    ref.filesize = strtoull(fields[2], NULL, 10);
    if (hexToBytes(fields[3], ref.idHash, 16) != 0) {
        free(copy);
        errx(EXIT_FAILURE, "Invalid main hash in ed2k link");
    }

    for (int i = 4; i < nfields; i++) {
        if (strncasecmp(fields[i], "p=", 2) == 0) {
            char *hashesStr = ownStrdup(fields[i] + 2);
            if (hashesStr == NULL) {
                err(EXIT_FAILURE, "Memory allocation error");
            }

            unsigned int cap = 64, cnt = 0;
            unsigned char (*arr)[16] = malloc((size_t)cap * 16);
            if (arr == NULL) {
                err(EXIT_FAILURE, "Memory allocation error");
            }

            char *h = strtok(hashesStr, ":");
            while (h != NULL) {
                if (cnt >= cap) {
                    cap *= 2;
                    arr = realloc(arr, (size_t)cap * 16);
                    if (arr == NULL) {
                        err(EXIT_FAILURE, "Memory allocation error");
                    }
                }
                if (hexToBytes(h, arr[cnt], 16) != 0) {
                    errx(EXIT_FAILURE, "Invalid part hash in ed2k link: %s", h);
                }
                cnt++;
                h = strtok(NULL, ":");
            }
            free(hashesStr);

            ref.numBlocks = cnt;
            ref.blockHashes = arr;
            break;
        }
    }

    free(copy);

    if (ref.numBlocks == 0) {
        errx(EXIT_FAILURE,
            "The ed2k link has no p= part-hash parameter, so it only carries the whole-file "
            "hash, which is not enough to verify individual blocks. Use a link that includes "
            "p=hash1:hash2:..., or a .part.met reference instead.");
    }

    return ref;
}

/* ------------------------------------------------------------------ */
/* Block verification                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned long long start;
    unsigned long long end;
} GapRange;

typedef struct {
    unsigned int numBlocks;
    unsigned char *blockOk;
    unsigned long long verifiedBytes;
    GapRange *gaps;
    unsigned int numGaps;
} VerifyResult;

static void freeVerifyResult(VerifyResult *r) {
    free(r->blockOk);
    free(r->gaps);
}

static VerifyResult verifyBlocks(const RefInfo *ref, int dataFd, unsigned long long dataSize) {
    VerifyResult res;
    memset(&res, 0, sizeof(res));
    res.numBlocks = ref->numBlocks;
    res.blockOk = (unsigned char *)malloc(res.numBlocks);
    if (res.blockOk == NULL) {
        err(EXIT_FAILURE, "Memory allocation error");
    }

    unsigned char *buf = (unsigned char *)malloc((size_t)PARTSIZE);
    if (buf == NULL) {
        err(EXIT_FAILURE, "Memory allocation error");
    }

    for (unsigned int i = 0; i < ref->numBlocks; i++) {
        unsigned long long blockStart = (unsigned long long)i * PARTSIZE;
        unsigned long long blockEnd = blockStart + PARTSIZE;
        if (blockEnd > ref->filesize) blockEnd = ref->filesize;
        unsigned long long blockLen = blockEnd - blockStart;

        int ok = 0;
        if (blockStart < dataSize) {
            unsigned long long available = dataSize - blockStart;
            unsigned long long toRead = available < blockLen ? available : blockLen;

            if (lseek(dataFd, (off_t)blockStart, SEEK_SET) == (off_t)-1) {
                err(EXIT_FAILURE, "Error seeking in data file");
            }
            ssize_t got = read(dataFd, buf, (size_t)toRead);
            if (got < 0) {
                err(EXIT_FAILURE, "Error reading data file");
            }
            if ((unsigned long long)got == blockLen) {
                unsigned char digest[16];
                md4Digest(buf, (size_t)blockLen, digest);
                if (memcmp(digest, ref->blockHashes[i], 16) == 0) {
                    ok = 1;
                }
            }
        }

        res.blockOk[i] = (unsigned char)ok;
        if (ok) res.verifiedBytes += blockLen;
    }
    free(buf);

    GapRange *gaps = (GapRange *)malloc((size_t)res.numBlocks * sizeof(GapRange));
    if (gaps == NULL && res.numBlocks > 0) {
        err(EXIT_FAILURE, "Memory allocation error");
    }
    unsigned int ngaps = 0;
    unsigned int i = 0;
    while (i < res.numBlocks) {
        if (!res.blockOk[i]) {
            unsigned long long start = (unsigned long long)i * PARTSIZE;
            unsigned int j = i;
            while (j < res.numBlocks && !res.blockOk[j]) j++;
            unsigned long long end = (unsigned long long)j * PARTSIZE;
            if (end > ref->filesize) end = ref->filesize;
            gaps[ngaps].start = start;
            gaps[ngaps].end = end;
            ngaps++;
            i = j;
        } else {
            i++;
        }
    }
    res.gaps = gaps;
    res.numGaps = ngaps;

    return res;
}

/* ------------------------------------------------------------------ */
/* Reporting                                                          */
/* ------------------------------------------------------------------ */

static void printVerifyReport(const RefInfo *ref, const VerifyResult *res, int json_output) {
    double pct = ref->filesize ? (res->verifiedBytes * 100.0 / (double)ref->filesize) : 0.0;

    if (json_output) {
        printf("{\"filename\":\"%s\",\"filesize\":%llu,\"num_blocks\":%u,"
               "\"verified_bytes\":%llu,\"verified_percentage\":%.1f,\"gaps\":[",
               ref->filename, ref->filesize, res->numBlocks, res->verifiedBytes, pct);
        for (unsigned int i = 0; i < res->numGaps; i++) {
            printf("{\"start\":%llu,\"end\":%llu,\"size\":%llu}%s",
                   res->gaps[i].start, res->gaps[i].end, res->gaps[i].end - res->gaps[i].start,
                   (i + 1 < res->numGaps) ? "," : "");
        }
        printf("]}\n");
    } else {
        printf("File: %s\n", ref->filename);
        printf("Size: %llu bytes\n", ref->filesize);
        printf("Blocks: %u (block size %llu bytes)\n", res->numBlocks, PARTSIZE);
        printf("Verified: %llu bytes (%.1f%%)\n", res->verifiedBytes, pct);
        if (res->numGaps == 0) {
            printf("No gaps: the data file matches the reference block hashes completely.\n");
        } else {
            printf("Gaps (%u):\n", res->numGaps);
            for (unsigned int i = 0; i < res->numGaps; i++) {
                printf("  [%llu - %llu] (%llu bytes)\n",
                       res->gaps[i].start, res->gaps[i].end, res->gaps[i].end - res->gaps[i].start);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Rebuilding a valid .part.met                                       */
/* ------------------------------------------------------------------ */

static void writeStringTag(int fd, unsigned char nameId, const char *value) {
    writeByte(fd, TAGTYPE_STRING);
    writeWord(fd, 1);
    writeByte(fd, nameId);
    unsigned int len = (unsigned int)strlen(value);
    writeWord(fd, (unsigned short)len);
    writeBytes(fd, (const unsigned char *)value, len);
}

static void writeIntTag(int fd, unsigned char nameId, unsigned long long value) {
    if (value > 0xFFFFFFFFULL) {
        writeByte(fd, TAGTYPE_UINT64);
        writeWord(fd, 1);
        writeByte(fd, nameId);
        writeQWord(fd, value);
    } else {
        writeByte(fd, TAGTYPE_UINT32);
        writeWord(fd, 1);
        writeByte(fd, nameId);
        writeDWord(fd, (unsigned int)value);
    }
}

static void writeGapTag(int fd, unsigned char startOrEnd, unsigned int refNum, unsigned long long pos) {
    char refStr[16];
    snprintf(refStr, sizeof(refStr), "%u", refNum);
    unsigned int refLen = (unsigned int)strlen(refStr);
    unsigned int nameLen = 1 + refLen;

    if (pos > 0xFFFFFFFFULL) {
        writeByte(fd, TAGTYPE_UINT64);
        writeWord(fd, (unsigned short)nameLen);
        writeByte(fd, startOrEnd);
        writeBytes(fd, (const unsigned char *)refStr, refLen);
        writeQWord(fd, pos);
    } else {
        writeByte(fd, TAGTYPE_UINT32);
        writeWord(fd, (unsigned short)nameLen);
        writeByte(fd, startOrEnd);
        writeBytes(fd, (const unsigned char *)refStr, refLen);
        writeDWord(fd, (unsigned int)pos);
    }
}

static void rebuildPartMet(const char *outPath, int force, const RefInfo *ref, const VerifyResult *res) {
    int flags = O_WRONLY | O_CREAT | (force ? O_TRUNC : O_EXCL);
    int fd = open(outPath, flags, 0644);
    if (fd == -1) {
        if (errno == EEXIST) {
            errx(EXIT_FAILURE, "Output file %s already exists (use --force to overwrite)", outPath);
        }
        err(EXIT_FAILURE, "Unable to create output file %s", outPath);
    }

    unsigned char version = (ref->filesize > 0xFFFFFFFFULL) ? PARTFILE_VERSION_LARGEFILE : PARTFILE_VERSION_14_0;
    writeByte(fd, version);
    writeDWord(fd, (unsigned int)time(NULL));
    writeBytes(fd, ref->idHash, 16);
    writeWord(fd, (unsigned short)ref->numBlocks);
    for (unsigned int i = 0; i < ref->numBlocks; i++) {
        writeBytes(fd, ref->blockHashes[i], 16);
    }

    unsigned int numTags = 3 + (res->numGaps * 2);
    writeDWord(fd, numTags);

    writeStringTag(fd, FT_FILENAME, ref->filename);
    writeIntTag(fd, FT_FILESIZE, ref->filesize);
    writeIntTag(fd, FT_TRANSFERRED, res->verifiedBytes);

    for (unsigned int i = 0; i < res->numGaps; i++) {
        writeGapTag(fd, 9, i, res->gaps[i].start);
        writeGapTag(fd, 10, i, res->gaps[i].end);
    }

    close(fd);
}

/* ------------------------------------------------------------------ */
/* CLI                                                                 */
/* ------------------------------------------------------------------ */

static void usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s verify  --ref <file.part.met|ed2k-link> --data <file.part> [--json]\n", prog);
    fprintf(stderr, "  %s rebuild --ref <file.part.met|ed2k-link> --data <file.part> -o <out.part.met> [--force] [--json]\n", prog);
    fprintf(stderr, "  %s -h | --help\n", prog);
    fprintf(stderr, "  %s -V | --version\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Re-hashes each block of a .part file and compares it against the block\n");
    fprintf(stderr, "hash array of a known-good reference (a .part.met file, or an ed2k link\n");
    fprintf(stderr, "with a p=hash1:hash2:... part-hash parameter). In rebuild mode it writes\n");
    fprintf(stderr, "a new, valid .part.met with correct Gap/Filename/Filesize tags.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "This is entirely offline: it never contacts any eDonkey/eMule server or\n");
    fprintf(stderr, "peer. You still need to obtain the reference block hashes yourself first\n");
    fprintf(stderr, "(e.g. by re-adding the same download once in eMule/aMule, which recreates\n");
    fprintf(stderr, "a fresh, correctly hashed but 0%%-downloaded .part.met).\n");
    fprintf(stderr, "See meta.md section 4.4 and README.md for background.\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    md4SelfTest();

    if (argc < 2) {
        usage(argv[0]);
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        usage(argv[0]);
    }
    if (strcmp(argv[1], "-V") == 0 || strcmp(argv[1], "--version") == 0) {
        printf("metrepair v1.0\n");
        printf("Offline .part.met block verification/reconstruction tool (companion to metinfo)\n");
        return EXIT_SUCCESS;
    }

    int isVerify = (strcmp(argv[1], "verify") == 0);
    int isRebuild = (strcmp(argv[1], "rebuild") == 0);
    if (!isVerify && !isRebuild) {
        fprintf(stderr, "Unknown command: %s\n\n", argv[1]);
        usage(argv[0]);
    }

    char *refArg = NULL;
    char *dataArg = NULL;
    char *outArg = NULL;
    int force = 0;
    int json_output = 0;

    static struct option longopts[] = {
        { "ref",    required_argument, NULL, 'r' },
        { "data",   required_argument, NULL, 'd' },
        { "output", required_argument, NULL, 'o' },
        { "force",  no_argument,       NULL, 'F' },
        { "json",   no_argument,       NULL, 'j' },
        { "help",   no_argument,       NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    optind = 2; /* skip argv[0] and the subcommand */
    int ch;
    while ((ch = getopt_long(argc, argv, "r:d:o:Fjh", longopts, NULL)) != -1) {
        switch (ch) {
            case 'r': refArg = optarg; break;
            case 'd': dataArg = optarg; break;
            case 'o': outArg = optarg; break;
            case 'F': force = 1; break;
            case 'j': json_output = 1; break;
            case 'h': usage(argv[0]); break;
            default: usage(argv[0]);
        }
    }

    if (refArg == NULL || dataArg == NULL) {
        fprintf(stderr, "Error: --ref and --data are required\n\n");
        usage(argv[0]);
    }
    if (isRebuild && outArg == NULL) {
        fprintf(stderr, "Error: rebuild requires -o/--output\n\n");
        usage(argv[0]);
    }

    RefInfo ref = isEd2kLink(refArg) ? parseEd2kLink(refArg) : readRefFromPartMet(refArg);

    unsigned int expectedBlocks = (unsigned int)(ref.filesize / PARTSIZE);
    if (ref.filesize % PARTSIZE != 0) expectedBlocks++;
    if (expectedBlocks != ref.numBlocks) {
        errx(EXIT_FAILURE,
            "Reference block count (%u) does not match what the declared file size "
            "(%llu bytes) implies (%u); the reference looks inconsistent.",
            ref.numBlocks, ref.filesize, expectedBlocks);
    }

    int dataFd = open(dataArg, O_RDONLY);
    if (dataFd == -1) {
        err(EXIT_FAILURE, "Unable to open data file %s", dataArg);
    }
    off_t dataSizeOff = lseek(dataFd, 0, SEEK_END);
    if (dataSizeOff == (off_t)-1) {
        err(EXIT_FAILURE, "Unable to determine size of data file %s", dataArg);
    }
    unsigned long long dataSize = (unsigned long long)dataSizeOff;

    VerifyResult res = verifyBlocks(&ref, dataFd, dataSize);
    close(dataFd);

    if (isVerify) {
        printVerifyReport(&ref, &res, json_output);
    } else {
        rebuildPartMet(outArg, force, &ref, &res);
        double pct = ref.filesize ? (res.verifiedBytes * 100.0 / (double)ref.filesize) : 0.0;
        if (json_output) {
            printf("{\"status\":\"ok\",\"output\":\"%s\",\"verified_bytes\":%llu,"
                   "\"filesize\":%llu,\"verified_percentage\":%.1f,\"gaps\":%u}\n",
                   outArg, res.verifiedBytes, ref.filesize, pct, res.numGaps);
        } else {
            printf("Rebuilt .part.met written to: %s\n", outArg);
            printf("Verified %llu / %llu bytes (%.1f%%), %u gap(s)\n",
                   res.verifiedBytes, ref.filesize, pct, res.numGaps);
            printf("Place the data file next to it, named like the .part.met but without\n");
            printf("\".met\" (e.g. \"1.part.met\" needs a sibling \"1.part\"), before resuming\n");
            printf("the download in eMule/aMule.\n");
        }
    }

    freeVerifyResult(&res);
    freeRefInfo(&ref);
    return EXIT_SUCCESS;
}
