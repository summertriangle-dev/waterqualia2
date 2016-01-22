#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <libgen.h>
#include <math.h>
#include "lodepng.h"

struct __attribute__((packed)) flim {
    uint8_t  magic[4];
    uint8_t  bom[2];
    uint16_t flim_len;
    uint32_t unknown;
    uint32_t file_size;
    uint16_t unknown2;
    uint8_t  pad[2];
};
struct __attribute__((packed)) imag {
    uint8_t  magic[4];
    uint32_t imag_len;
    uint16_t height;
    uint16_t width;
    uint16_t align;
    uint8_t  format;
    uint8_t  swizzle;
    uint32_t bank_size;
};
struct __attribute__((packed)) meta {
    struct flim flim;
    struct imag imag;
};

#include "efe_pixelformat.c"

void rotate_neg90(uint8_t *rgba, uint8_t *dest, uint16_t width, uint16_t height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t *pixelbase = dest + ((height - 1 - y) * width * 4) + (x * 4);
            memcpy(pixelbase, rgba + (x * height * 4) + (y * 4), 4);
        }
    }
}

int   bflim_test_eligibility(const uint8_t *stream, size_t len) {
    return (len >= sizeof(struct meta)) && (memcmp(stream + len - sizeof(struct meta), "FLIM", 4) == 0);
}

char *bflim_out_filename(const char *archive, const char *intname, const char *outputdir) {
    int cpy = 0;
    char *end = strrchr(intname, '.');
    if (!end) {
        cpy = strlen(intname);
    } else {
        cpy = end - intname;
    }

    char *mangled_intname = malloc(cpy + 5);
    memcpy(mangled_intname, intname, cpy);
    memcpy(mangled_intname + cpy, ".png", 5);

    char *output_path = calloc(strlen(outputdir) + 1 + strlen(archive) + 1 + strlen(mangled_intname) + 1, 1);
    strcat(output_path, outputdir);

    output_path[strlen(output_path)] = '/';
    strcat(output_path, archive);

    int start_cleansing = strlen(output_path) + 1;
    output_path[strlen(output_path)] = '/';
    strcat(output_path, mangled_intname);

    free(mangled_intname);

    char *ptr = output_path + start_cleansing;
    while (*ptr != 0) {
        if (*ptr == '/') {
            *ptr = '_';
        }
        ptr++;
    }

    return output_path;
}

static void ensure_directory_r(char *path, int perm) {
    if ((strcmp(path, "/") == 0) || (strcmp(path, ".") == 0))
        return;

    struct stat finfo;
    if (stat(path, &finfo) != 0) {
        if (errno != ENOENT) {
            printf("stat(%s): %s\n", path, strerror(errno));
            abort();
        }
    } else {
        return; // already exists
    }

    const char *parent = dirname(path);
    if (!parent)
        abort();

    char *parent_copy = strdup(parent);
    if (!parent_copy)
        abort();

    ensure_directory_r(parent_copy, perm);
    free(parent_copy);

    if (mkdir(path, perm) != 0 && errno != EEXIST) {
        printf("ensure_directory_r(%s): %s\n", path, strerror(errno));
        abort();
    }
}

int   bflim_extract(const char *output_file, const uint8_t *stream, size_t len) {
    const char *parent = dirname((char *)output_file);
    if (!parent)
        abort();

    char *parent_copy = strdup(parent);
    if (!parent_copy)
        abort();

    ensure_directory_r(parent_copy, 0755);
    free(parent_copy);

    struct meta *footer = (struct meta *)(stream + len - sizeof(struct meta));

    // when the image size is nonstandard rounding up seems to work better
    uint32_t tile_width = 1 << (uint32_t)(ceil(log2(footer->imag.width)));
    uint32_t tile_height = 1 << (uint32_t)(ceil(log2(footer->imag.height)));
    if (!tile_width || !tile_height) {
        return -1;
    }

    uint8_t *rgba = malloc(tile_width * tile_height * 4);

    image_svc converter = csvc_for_imag_format(footer->imag.format);
    if (converter) {
        printf("bflim debug! format 0x%02X, %u (%u) %u (%u)\n",
            footer->imag.format, tile_width, footer->imag.width, tile_height, footer->imag.height);
        converter(stream, rgba, tile_width, tile_height);
    } else {
        printf("There is no handler for format 0x%02X (%u).\n", footer->imag.format, footer->imag.format);
        free(rgba);
        return -1;
    }

    uint8_t *rota = malloc(tile_width * tile_height * 4);
    rotate_neg90(rgba, rota, tile_height, tile_width);
    lodepng_encode32_file(output_file, rota, tile_height, footer->imag.width);
    free(rgba);
    free(rota);

    return 0;
}
