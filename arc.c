#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

struct  __attribute__((packed)) sARC {
    uint8_t  magic[4];
    uint16_t sarc_len;
    uint8_t  bom[2];
    uint32_t file_len;
    uint32_t to_data;
    uint8_t  pad[4];
};

struct __attribute__((packed)) sFAT_header  {
    uint8_t  magic[4];
    uint16_t sfat_len;
    uint16_t entries;
    uint32_t multiplier;
};

struct __attribute__((packed)) sFAT_entry  {
    uint8_t  hash[4];
    uint32_t to_name: 24;
    uint8_t  flag;
    uint32_t to_data_start;
    uint32_t to_data_end;
};

struct __attribute__((packed)) sFAT  {
    struct sFAT_header header;
    // this is variable length, the real array size is header->entries
    struct sFAT_entry entries[1];
};

struct __attribute__((packed)) sFNT  {
    uint8_t  magic[4];
    uint16_t sfnt_len;
    uint8_t  pad[2];
};

#include "plugin_bflim.h"
#include "plugin_raw.h"

struct plugin {
    char shorthand;
    const char *name;
    int    (*can_work_on_this_file)(const uint8_t *stream, size_t len);
    char * (*make_output_filename)(const char *archive, const char *intname, const char *outputdir);
    int    (*extract)(const char *output_file, const uint8_t *stream, size_t len);
};
static struct plugin l_plugins[] = {
    { 'b', "BFLIM pixel bank format",     bflim_test_eligibility, bflim_out_filename, bflim_extract },
    { '_', "Last resort (no conversion)", raw_test_eligibility, raw_out_filename, raw_extract },
};
static int effective_pl_count;
static struct plugin *effective_pl = l_plugins;
const char *pshorts = "b_";

void map_archive(void (*callback)(struct sARC *region, struct sFAT_entry *current_file,
                                  const char *filename, void *context),
                 const char *arcname, void *context) {
    int fd = open(arcname, O_RDONLY);
    assert(fd != -1);

    size_t len = lseek(fd, 0, SEEK_END);
    uint8_t *mem = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);

    struct sARC *header = (struct sARC *)mem;
    assert(memcmp(header->magic, "SARC", 4) == 0);
    assert(memcmp(header->bom, "\xff\xfe", 2) == 0);
    assert(header->sarc_len == 0x14);

    struct sFAT *fat = (struct sFAT *)(mem + header->sarc_len);
    assert(memcmp(fat->header.magic, "SFAT", 4) == 0);
    assert(fat->header.sfat_len == 0xc);
    struct sFAT_entry *ebase = (struct sFAT_entry *)(&fat->header + 1);

    struct sFNT *names = (struct sFNT *)(ebase + fat->header.entries);
    assert(memcmp(names->magic, "SFNT", 4) == 0);
    assert(names->sfnt_len == 0x8);
    const char *nbase = (const char *)(names + 1);

    // reading files...

    for (int i = 0; i < fat->header.entries; ++i) {
        struct sFAT_entry *ent = ebase + i;

        callback(header, ent, nbase + (ent->to_name * 4), context);
    }

    close(fd);
    munmap(mem, len);
}

// console commands

void sfmt(int argc, char const *argv[]) {
    int formats_to_use = strlen(argv[0]);
    struct plugin *psa = calloc(sizeof(struct plugin), formats_to_use);

    for (int i = 0; i < formats_to_use; ++i) {
        int where_the_plugin_is = strchr(pshorts, argv[0][i]) - pshorts;
        if (where_the_plugin_is < 0) {
            printf("'%1c' is not a known format.\n", argv[0][i]);
            abort();
        }

        psa[i] = l_plugins[where_the_plugin_is];
    }

    if (effective_pl != l_plugins) {
        free(effective_pl);
    }
    effective_pl_count = formats_to_use;
    effective_pl = psa;
}

void xall_dumpfile(struct sARC *region, struct sFAT_entry *current_file,
                   const char *filename, void *context) {
    const char *archive = ((const char **)context)[0];
    const char *outputdir = ((const char **)context)[1];

    const uint8_t *contents = ((const uint8_t *)region + region->to_data) + current_file->to_data_start;
    size_t len = current_file->to_data_end - current_file->to_data_start;

    struct plugin use = {0};
    for (int i = 0; i < effective_pl_count; ++i) {
        if (effective_pl[i].can_work_on_this_file(contents, len)) {
            use = effective_pl[i];

            char *output_path = use.make_output_filename(archive, filename, outputdir);
            assert(output_path);

            printf("Dumping '%s:%s' using %s -> %s.\n", archive, filename, use.name, output_path);

            int err = use.extract(output_path, contents, len);
            if (!err) {
                return;
            }
        }
    }

    if (!use.name) {
        printf("warning: no format was able to extract '%s:%s'.\n", archive, filename);
    }
}
void xall(int argc, char const *argv[]) {
    const char *archive = strrchr(argv[0], '/') + 1;
    if ((uintptr_t)archive == 1)
        archive = argv[0];
    const char *output = argv[1];
    const char *context[] = {archive, output};

    map_archive(xall_dumpfile, argv[0], context);
}

void xone_dumpfile(struct sARC *region, struct sFAT_entry *current_file,
                   const char *filename, void *context) {
    const char *want = ((const char **)context)[2];
    if (!strcmp(filename, want)) {
        xall_dumpfile(region, current_file, filename, context);
    }
}
void xone(int argc, char const *argv[]) {
    const char *archive = strrchr(argv[0], '/') + 1;
    if ((uintptr_t)archive == 1)
        archive = argv[0];
    const char *want = argv[1];
    const char *output = argv[2];
    const char *context[] = {archive, output, want};

    map_archive(xone_dumpfile, argv[0], context);
}

void list_printname(struct sARC *region, struct sFAT_entry *current_file,
                    const char *filename, void *context) {
    printf("%02X%02X%02X%02X ", current_file->hash[0], current_file->hash[1],
                                current_file->hash[2], current_file->hash[3]);
    printf("'%s', %u bytes.\n", filename,
        current_file->to_data_end - current_file->to_data_start);
}
void list(int argc, char const *argv[]) {
    printf("Files for '%s'...\n", argv[0]);
    map_archive(list_printname, argv[0], NULL);
}

struct {
    char shorthand;
    const char *help;
    int reqarg;
    void (*handler)(int argc, char const *argv[]);
} available_commands[] = {
    { 'f', "Set the handler list for extracting. Available options: b_.", 1, sfmt },
    { 'x', "Extract all files in archive. arc x in.arc output_folder",    2, xall },
    { 'o', "Extract one file by name. arc o in.arc intname output_file",  3, xone },
    { 'l', "List files in archive. arc l in.arc",                         1, list },
};
const char *cshorts = "fxol";

int main(int argc, char const *argv[]) {
    void *chk = &chk;
    void *mc = NULL;
    assert(mc = chk);
    if (mc != chk) {
        puts("This program will not function properly with assertions disabled.");
        return 1;
    }

    assert(argc >= 2);
    const char *commands = argv[1];

    argc -= 2;
    argv += 2;
    for (const char *cmd = commands; *cmd != 0; ++cmd) {
        int where_the_command_is = strchr(cshorts, *cmd) - cshorts;
        if (where_the_command_is < 0) {
            printf("'%1c' is not a command.\n", *cmd);
            return 1;
        }

        if (argc < available_commands[where_the_command_is].reqarg) {
            puts("Not enough arguments!");
            printf("Help for '%c': %s\n", *cmd, available_commands[where_the_command_is].help);
            return 1;
        }

        available_commands[where_the_command_is].handler(argc, argv);

        argc -= available_commands[where_the_command_is].reqarg;
        argv += available_commands[where_the_command_is].reqarg;
    }

    return 0;
}