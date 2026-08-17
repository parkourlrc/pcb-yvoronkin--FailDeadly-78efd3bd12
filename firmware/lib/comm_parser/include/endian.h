#ifndef ENDIAN_H
#define ENDIAN_H

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
    #define IS_BIG_ENDIAN (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#elif defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN)
    #define IS_BIG_ENDIAN 1
#elif defined(__LITTLE_ENDIAN__) || defined(_LITTLE_ENDIAN) || defined(_X86_)
    #define IS_BIG_ENDIAN 0
#else
    #error "Can't detect endianess"
#endif

#define SWAP16(x) (((x) >> 8) | ((x) << 8))

#define SWAP32(x) (((x) >> 32) | \
                   (((x) >> 8) & 0xFF00) | \
                   (((x) << 8) & 0xFF0000) | \
                   ((x) << 24))

#define SWAP64(x) (((x) >> 56) | \
                   (((x) >> 40) & 0xFF00ULL) | \
                   (((x) >> 24) & 0xFF0000ULL) | \
                   (((x) >> 8)  & 0xFF000000ULL) | \
                   (((x) << 8)  & 0xFF00000000ULL) | \
                   (((x) << 24) & 0xFF0000000000ULL) | \
                   (((x) << 40) & 0xFF000000000000ULL) | \
                   ((x) << 56))

#if IS_BIG_ENDIAN
    #define htobe16(x) (x)
    #define htobe32(x) (x)
    #define htobe64(x) (x)
    #define betoh16(x) (x)
    #define betoh32(x) (x)
    #define betoh64(x) (x)

    #define htole16(x) SWAP16(x)
    #define htole32(x) SWAP32(x)
    #define htole64(x) SWAP64(x)
    #define letoh16(x) SWAP16(x)
    #define letoh32(x) SWAP32(x)
    #define letoh64(x) SWAP64(x)
#else
    #define htobe16(x) SWAP16(x)
    #define htobe32(x) SWAP32(x)
    #define htobe64(x) SWAP64(x)
    #define betoh16(x) SWAP16(x)
    #define betoh32(x) SWAP32(x)
    #define betoh64(x) SWAP64(x)

    #define htole16(x) (x)
    #define htole32(x) (x)
    #define htole64(x) (x)
    #define letoh16(x) (x)
    #define letoh32(x) (x)
    #define letoh64(x) (x)
#endif

#endif
