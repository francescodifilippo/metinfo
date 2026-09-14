/**
 * metfmt.h - shared .part.met wire-format definitions
 *
 * Tag type/version constants, the MetaTag struct, and the low-level
 * fd readers + generic tag parser used by both metinfo (read-only
 * inspection) and metrepair (offline verification/reconstruction).
 * Kept as a header of plain (non-static) functions rather than a
 * third linked object, since each tool is still built as a single
 * translation unit; they are never linked together, so there is no
 * ODR conflict between the two copies.
 */

#ifndef METFMT_H
#define METFMT_H

#include <unistd.h>
#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Meta Tag value types.
 *
 * 0x01/0x02/0x03 (Hash16/String/UInt32) are the ones described in the
 * original ed2k .part.met format document (2003). The rest were added by
 * eMule/aMule over time (see meta.md, section 4.4) and are read here for
 * compatibility with current .part.met files, in particular the 0x0B
 * (UInt64) type used for the FILESIZE tag of files larger than 4GB.
 */
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

/**
 * .part.met file format versions (first byte of the file).
 */
#define PARTFILE_VERSION_14_0      224 /* 0xE0 */
#define PARTFILE_VERSION_14_1      225 /* 0xE1 */
#define PARTFILE_VERSION_LARGEFILE 226 /* 0xE2 - same layout as 14.0, allows a 64-bit FILESIZE tag */

/**
 * Special Meta Tag name IDs (1-byte tag name).
 */
#define FT_FILENAME    1
#define FT_FILESIZE    2
#define FT_TRANSFERRED 8
#define FT_GAPSTART    9
#define FT_GAPEND      10

/**
 * Structure to store a meta tag
 */
typedef struct {
    int type;                 // Tag type, see TAGTYPE_* above
    int nameLength;            // Length of the name
    char *name;                // Tag name
    unsigned int valueLength;  // Length of the value (String/Blob/Bsob/BoolArray)
    union {
        char *stringValue;           // String (and compressed Str1..Str16)
        unsigned long long intValue; // UInt8/UInt16/UInt32/UInt64/Bool
        float floatValue;            // Float32
        unsigned char hash[16];      // Hash16
        unsigned char *blobValue;    // Blob/Bsob/BoolArray raw bytes
    } value;
} MetaTag;

/**
 * Whether a tag type holds an integer-like value in value.intValue
 */
static int isIntType(int type) {
    return type == TAGTYPE_UINT8 || type == TAGTYPE_UINT16 ||
           type == TAGTYPE_UINT32 || type == TAGTYPE_UINT64 ||
           type == TAGTYPE_BOOL;
}

/**
 * Whether a tag type holds a string value in value.stringValue
 */
static int isStringType(int type) {
    return type == TAGTYPE_STRING;
}

/**
 * Read a byte from the file
 */
static unsigned char readByte(int fd) {
    unsigned char byte;
    if (read(fd, &byte, 1) != 1) {
        err(EXIT_FAILURE, "Error reading file");
    }
    return byte;
}

/**
 * Read a word (2 bytes) from the file
 */
static unsigned short readWord(int fd) {
    unsigned char bytes[2];
    if (read(fd, bytes, 2) != 2) {
        err(EXIT_FAILURE, "Error reading file");
    }
    return (unsigned short)(bytes[0] | (bytes[1] << 8));
}

/**
 * Read a dword (4 bytes) from the file
 */
static unsigned int readDWord(int fd) {
    unsigned char bytes[4];
    if (read(fd, bytes, 4) != 4) {
        err(EXIT_FAILURE, "Error reading file");
    }
    return (bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
}

/**
 * Read a qword (8 bytes) from the file
 */
static unsigned long long readQWord(int fd) {
    unsigned char bytes[8];
    if (read(fd, bytes, 8) != 8) {
        err(EXIT_FAILURE, "Error reading file");
    }
    unsigned long long value = 0;
    for (int i = 7; i >= 0; i--) {
        value = (value << 8) | bytes[i];
    }
    return value;
}

/**
 * Read len raw bytes from the file into an existing buffer
 */
static void readBytes(int fd, unsigned char *buf, size_t len) {
    if (len == 0) return;
    ssize_t got = read(fd, buf, len);
    if (got < 0 || (size_t)got != len) {
        err(EXIT_FAILURE, "Error reading file");
    }
}

/**
 * Read a 32-bit IEEE-754 float from the file
 */
static float readFloat32(int fd) {
    unsigned char bytes[4];
    readBytes(fd, bytes, 4);
    float value;
    memcpy(&value, bytes, sizeof(value));
    return value;
}

/**
 * Read a NUL-terminated string of length len from the file
 */
static char *readString(int fd, unsigned int len) {
    char *str = (char *)malloc(len + 1);
    if (str == NULL) {
        err(EXIT_FAILURE, "Memory allocation error");
    }
    readBytes(fd, (unsigned char *)str, len);
    str[len] = '\0';
    return str;
}

/**
 * Read and parse one meta tag from the file.
 *
 * Supports the classic String/Integer tags from the original format
 * document as well as the extra types used by current aMule/eMule
 * .part.met files (Hash16, Float32, Bool, BoolArray, Blob, UInt16,
 * UInt8, BSOB, UInt64, and the compressed Str1..Str16 strings). See
 * meta.md section 4.4 for the byte layout of each type.
 */
static MetaTag *readMetaTag(int fd) {
    MetaTag *tag = (MetaTag *)malloc(sizeof(MetaTag));
    if (tag == NULL) {
        err(EXIT_FAILURE, "Memory allocation error");
    }
    tag->valueLength = 0;

    // Read tag type
    tag->type = readByte(fd);

    // Read name length
    tag->nameLength = readWord(fd);

    // Read name
    tag->name = readString(fd, (unsigned int)tag->nameLength);

    // Read value based on type
    switch (tag->type) {
        case TAGTYPE_HASH16:
            readBytes(fd, tag->value.hash, 16);
            tag->valueLength = 16;
            break;

        case TAGTYPE_STRING:
            tag->valueLength = readWord(fd);
            tag->value.stringValue = readString(fd, tag->valueLength);
            break;

        case TAGTYPE_UINT8:
            tag->value.intValue = readByte(fd);
            break;

        case TAGTYPE_UINT16:
            tag->value.intValue = readWord(fd);
            break;

        case TAGTYPE_UINT32:
            tag->value.intValue = readDWord(fd);
            break;

        case TAGTYPE_UINT64:
            tag->value.intValue = readQWord(fd);
            break;

        case TAGTYPE_BOOL:
            tag->value.intValue = readByte(fd);
            break;

        case TAGTYPE_FLOAT32:
            tag->value.floatValue = readFloat32(fd);
            break;

        case TAGTYPE_BOOLARRAY: {
            // eMule/aMule encode a 2-byte bit count followed by
            // (bits/8)+1 packed bytes (off-by-one kept for compatibility
            // with eMule versions prior to 0.42e.29).
            unsigned short bitLen = readWord(fd);
            unsigned int byteLen = (unsigned int)(bitLen / 8) + 1;
            tag->valueLength = bitLen;
            tag->value.blobValue = (unsigned char *)malloc(byteLen);
            if (tag->value.blobValue == NULL) {
                err(EXIT_FAILURE, "Memory allocation error");
            }
            readBytes(fd, tag->value.blobValue, byteLen);
            break;
        }

        case TAGTYPE_BLOB:
            tag->valueLength = readDWord(fd);
            tag->value.blobValue = (unsigned char *)malloc(tag->valueLength > 0 ? tag->valueLength : 1);
            if (tag->value.blobValue == NULL) {
                err(EXIT_FAILURE, "Memory allocation error");
            }
            readBytes(fd, tag->value.blobValue, tag->valueLength);
            break;

        case TAGTYPE_BSOB: {
            unsigned char bsobSize = readByte(fd);
            tag->valueLength = bsobSize;
            tag->value.blobValue = (unsigned char *)malloc(bsobSize > 0 ? bsobSize : 1);
            if (tag->value.blobValue == NULL) {
                err(EXIT_FAILURE, "Memory allocation error");
            }
            readBytes(fd, tag->value.blobValue, bsobSize);
            break;
        }

        default:
            if (tag->type >= TAGTYPE_STR1 && tag->type <= TAGTYPE_STR16) {
                // Compressed string tag: the value length is encoded in
                // the type itself, there is no length prefix on disk.
                unsigned int len = (unsigned int)(tag->type - TAGTYPE_STR1 + 1);
                tag->valueLength = len;
                tag->value.stringValue = readString(fd, len);
                tag->type = TAGTYPE_STRING; // normalize for the rest of the program
            } else {
                fprintf(stderr, "Error: Unrecognized tag type: 0x%02X\n", tag->type);
                free(tag->name);
                free(tag);
                return NULL;
            }
    }

    return tag;
}

/**
 * Free memory used by a meta tag
 */
static void freeMetaTag(MetaTag *tag) {
    if (tag != NULL) {
        free(tag->name);
        if (isStringType(tag->type)) {
            free(tag->value.stringValue);
        } else if (tag->type == TAGTYPE_BLOB || tag->type == TAGTYPE_BSOB ||
                   tag->type == TAGTYPE_BOOLARRAY) {
            free(tag->value.blobValue);
        }
        free(tag);
    }
}

#endif /* METFMT_H */
