#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define ADVANCE(var) var = (var + 1) & 0xfff

struct stella_glow_cmp {
    unsigned char hdr[4];
    uint32_t infsize;
};

// LZSS impl ported from Ohana's C#.
int main(int argc, char const *argv[]) {
    // check whether assertions are enabled (we use them for error handling)
    void *chk = &chk;
    void *mc = NULL;
    assert(mc = chk);
    if (mc != chk) {
        fputs("This program will not function properly with assertions disabled.", stderr);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    assert(fd != -1);

    struct stella_glow_cmp header;
    read(fd, &header, 8);
    assert(memcmp("IECP", header.hdr, 4) == 0);

    off_t size = lseek(fd, 0, SEEK_END) - 8;
    uint8_t *in_buf = malloc(size);
    uint8_t *in_ptr = in_buf;
    lseek(fd, 8, SEEK_SET);
    read(fd, in_buf, size);

    uint8_t *out_buf = malloc(header.infsize);
    uint8_t *out_ptr = out_buf;

    uint8_t dict[4096];
    int d_offset = 4078;

    uint16_t mask = 0x80;
    uint8_t pkt_header = 0;

    while (out_ptr - out_buf < header.infsize) {
        mask <<= 1;
        if (mask == 0x100) {
            pkt_header = *in_ptr++;
            mask = 1;
        }

        if ((pkt_header & mask) > 0) {
            uint8_t next = *in_ptr++;
            *(out_ptr++) = next;
            dict[d_offset] = next;
            ADVANCE(d_offset);
        } else {
            uint16_t two = *(uint16_t *)in_ptr;
            in_ptr += 2;

            int length = ((two >> 8) & 0xf) + 3;
            int position = ((two & 0xf000) >> 4) | (two & 0xff);

            while (length --> 0) {
                dict[d_offset] = dict[position];
                *out_ptr++ = dict[d_offset];
                ADVANCE(d_offset);
                ADVANCE(position);
            }
        }
    }

    write(STDOUT_FILENO, out_buf, header.infsize);
    free(in_buf);
    free(out_buf);

    return 0;
}