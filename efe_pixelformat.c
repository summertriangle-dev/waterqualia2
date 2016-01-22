#include <arpa/inet.h>
#include "rg_etc1wrap.h"

#define DESTINATION_OFFSET(x, y) dst_ptr = out + ((dst_y + y) * (width * 4)) + ((dst_x + x) * 4);
  #define FLIP_64(word) ((((uint64_t)htonl((uint32_t) word)) << 32) | htonl(word >> 32));
#define NEXT_WORD(dst, src) dst = *src++; dst = FLIP_64(dst)
#define UNPACK_BLOCK(word, block, p) \
    unpack_etc1_block_c((uint8_t *) &word, block, p); \
    fourpixels = block
  #define COPY4(dst, src, width) \
      memcpy(dst, src, 16); \
      dst += (width * 4); \
      src += 16
#define COPY16(...) COPY4(__VA_ARGS__); COPY4(__VA_ARGS__); COPY4(__VA_ARGS__); COPY4(__VA_ARGS__);
#define UNIT(x, y, p) \
    DESTINATION_OFFSET(x, y); \
    NEXT_WORD(word, block_base); \
    UNPACK_BLOCK(word, block, p); \
    COPY16(dst_ptr, fourpixels, width)
#define PREFILL_ALPHA() \
    amask = long_rotate(*block_base++); \
    while (alph_ctr --> 0) { \
        (block + (4 * (15 - alph_ctr)))[3] = (amask & 0xF) * 17; \
        amask >>= 4; \
    } \
    alph_ctr = 16

#define NIBBLE(a, n) ((a & (0xFull << (4ull * n))) >> (4ull * n))
#define AT(a, n) (a << (4ull * n))
static uint64_t long_rotate(uint64_t a) {
    return AT(NIBBLE(a, 3), 12) | AT(NIBBLE(a, 7), 13) | AT(NIBBLE(a, 11), 14) | AT(NIBBLE(a, 15), 15) |
           AT(NIBBLE(a, 2),  8) | AT(NIBBLE(a, 6),  9) | AT(NIBBLE(a, 10), 10) | AT(NIBBLE(a, 14), 11) |
           AT(NIBBLE(a, 1),  4) | AT(NIBBLE(a, 5),  5) | AT(NIBBLE(a,  9),  6) | AT(NIBBLE(a, 13),  7) |
           AT(NIBBLE(a, 0),  0) | AT(NIBBLE(a, 4),  1) | AT(NIBBLE(a,  8),  2) | AT(NIBBLE(a, 12),  3) ;
}

void flip_image_sideways(uint8_t *buf, uint32_t width, uint32_t height) {
    uint8_t *work = malloc(width * 4);

    for (int row = 0; row < height; ++row) {
        uint8_t *crow = buf + (row * width * 4);
        uint8_t *worp = work;

        for (size_t i = (width - 1) * 4; i > 0; i -= 4) {
            memcpy(worp, crow + i, 4);
            worp += 4;
        }

        memcpy(crow, work, width * 4);
    }

    free(work);
}

void service_etc1(const uint8_t *stream, uint8_t *out, uint16_t width, uint16_t height) {
    uint32_t tile_width = 1 << (uint32_t)(ceil(log2(width)));
    uint32_t tile_height = 1 << (uint32_t)(ceil(log2(height)));

    int xblocks = tile_width / 8;
    int yblocks = tile_height / 8;

    int dst_x = 0, dst_y = 0;

    uint8_t block[4 * 4 * 4] = {0};

    const uint64_t *block_base = (uint64_t *)stream;
    uint64_t word = 0;

    for (int y = 0; y < yblocks; ++y) {
        for (int x = 0; x < xblocks; ++x) {
            uint8_t *dst_ptr;
            uint8_t *fourpixels;

            UNIT(0, 0, 0);
            UNIT(4, 0, 0);
            UNIT(0, 4, 0);
            UNIT(4, 4, 0);

            dst_x += 8;
        }
        dst_x = 0;
        dst_y += 8;
    }

    flip_image_sideways(out, width, height);
}

void service_etc1a4(const uint8_t *stream, uint8_t *out, uint16_t width, uint16_t height) {
    uint32_t tile_width = 1 << (uint32_t)(ceil(log2(width)));
    uint32_t tile_height = 1 << (uint32_t)(ceil(log2(height)));

    int xblocks = tile_width / 8;
    int yblocks = tile_height / 8;

    int dst_x = 0, dst_y = 0;

    uint8_t block[4 * 4 * 4] = {255};

    const uint64_t *block_base = (uint64_t *)stream;
    uint64_t word = 0;
    uint64_t amask = 0;
    int alph_ctr = 16;

    for (int y = 0; y < yblocks; ++y) {
        for (int x = 0; x < xblocks; ++x) {
            uint8_t *dst_ptr;
            uint8_t *fourpixels;

            PREFILL_ALPHA();
            UNIT(0, 0, 1);
            PREFILL_ALPHA();
            UNIT(4, 0, 1);
            PREFILL_ALPHA();
            UNIT(0, 4, 1);
            PREFILL_ALPHA();
            UNIT(4, 4, 1);

            dst_x += 8;
        }
        dst_x = 0;
        dst_y += 8;
    }

    flip_image_sideways(out, width, height);
}

////////////

int munge_p(int p) {
    static int tileorder[] = {
         0,  1,   4,  5,
         2,  3,   6,  7,
         8,  9,  12, 13,
        10, 11,  14, 15
    };
    return tileorder[p % 4 + (p / 8) % 4 * 4] + 16 * ((p % 8) / 4) + 32 * (p / 32);
}

void service_blocks(const uint8_t *stream, uint8_t *out, uint16_t width, uint16_t height,
                    int pixel_size,
                    void (*service)(const uint8_t *src, uint8_t *out)) {
    uint32_t tile_width = 1 << (uint32_t)(ceil(log2(width)));
    uint32_t tile_height = 1 << (uint32_t)(ceil(log2(height)));

    int xblocks = tile_width / 8;
    int yblocks = tile_height / 8;

    int dst_x = 0, dst_y = 0;

    for (int y = 0; y < yblocks; ++y) {
        for (int x = 0; x < xblocks; ++x) {
            const uint8_t *block_base = stream + (( (dst_y * tile_width) + (dst_x * 8) ) * pixel_size);
            for (int p = 0; p < 64; ++p) {
                int i_dst_x = p % 8;
                int i_dst_y = p / 8;

                if (dst_x + i_dst_x >= width) continue;
                if (dst_y + i_dst_y >= height) continue;

                const uint8_t *sptr = block_base + (munge_p(p) * pixel_size);
                uint8_t *dst_ptr = out + ((dst_y + i_dst_y) * (width * 4)) + ((dst_x + i_dst_x) * 4);
                service(sptr, dst_ptr);
            }
            dst_x += 8;
        }
        dst_x = 0;
        dst_y += 8;
    }
}

void crgba8888_svc_write(const uint8_t *src, uint8_t *out) {
    *out++ = src[3];
    *out++ = src[2];
    *out++ = src[1];
    *out++ = src[0];
}
void crgba8888_svc(const uint8_t *stream, uint8_t *out, uint16_t width, uint16_t height) {
    service_blocks(stream, out, width, height, 4, crgba8888_svc_write);
}

void crgb888_svc_write(const uint8_t *src, uint8_t *out) {
    *out++ = src[2];
    *out++ = src[1];
    *out++ = src[0];
    *out++ = 255;
}
void crgb888_svc(const uint8_t *stream, uint8_t *out, uint16_t width, uint16_t height) {
    service_blocks(stream, out, width, height, 3, crgb888_svc_write);
}

void crgb565_svc_write(const uint8_t *src, uint8_t *out) {
    uint16_t pixel = *(uint16_t *)(src);
    uint8_t shift = (pixel & 0xF800) >> 8;
    *out++ = shift | (shift >> 5);
    shift = (pixel & 0x07E0) >> 3;
    *out++ = shift | (shift >> 6);
    shift = (pixel & 0x001F) << 3;
    *out++ = shift | (shift >> 5);
    *out++ = 255;
}
void crgb565_svc(const uint8_t *stream, uint8_t *out, uint16_t width, uint16_t height) {
    service_blocks(stream, out, width, height, 2, crgb565_svc_write);
}

void crgba5551_svc_write(const uint8_t *src, uint8_t *out) {
    uint16_t pixel = *(uint16_t *)(src);
    uint8_t shift = (pixel & 0xF800) >> 8;
    *out++ = shift | (shift >> 5);
    shift = (pixel & 0x07C0) >> 3;
    *out++ = shift | (shift >> 5);
    shift = (pixel & 0x003E) << 3;
    *out++ = shift | (shift >> 5);
    *out++ = (pixel % 2)? 255 : 0;
}
void crgba5551_svc(const uint8_t *stream, uint8_t *out, uint16_t width, uint16_t height) {
    service_blocks(stream, out, width, height, 2, crgba5551_svc_write);
}

void crgba4444_svc_write(const uint8_t *src, uint8_t *out) {
    uint16_t pixel = *(uint16_t *)(src);
    uint8_t shift = (pixel & 0xF000) >> 8;
    *out++ = shift | (shift >> 4);
    shift = (pixel & 0x0F00) >> 4;
    *out++ = shift | (shift >> 4);
    shift = pixel & 0x00F0;
    *out++ = shift | (shift >> 4);
    shift = pixel & 0x000F;
    *out++ = shift | (shift << 4);
}
void crgba4444_svc(const uint8_t *stream, uint8_t *out, uint16_t width, uint16_t height) {
    service_blocks(stream, out, width, height, 2, crgba4444_svc_write);
}

typedef void (*image_svc)(const uint8_t *stream, uint8_t *out, uint16_t width, uint16_t height);

image_svc csvc_for_imag_format(uint32_t format) {
    switch (format) {
        case 11: return service_etc1a4;
        case 10: return service_etc1;
        case 9:  return crgba8888_svc;
        case 8:  return crgba4444_svc;
        case 7:  return crgba5551_svc;
        case 6:  return crgb888_svc;
        case 5:  return crgb565_svc;
    }
    return NULL;
}
