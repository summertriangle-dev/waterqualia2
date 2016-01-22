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

int   raw_test_eligibility(const uint8_t *stream, size_t len) {
    return 1;
}

char *raw_out_filename(const char *archive, const char *intname, const char *outputdir) {
    char *output_path = calloc(strlen(outputdir) + 1 + strlen(archive) + 1 + strlen(intname) + 1, 1);
    strcat(output_path, outputdir);

    output_path[strlen(output_path)] = '/';
    strcat(output_path, archive);

    int start_cleansing = strlen(output_path) + 1;
    output_path[strlen(output_path)] = '/';
    strcat(output_path, intname);

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

int   raw_extract(const char *output_file, const uint8_t *stream, size_t len) {
    const char *parent = dirname((char *)output_file);
    if (!parent)
        abort();

    char *parent_copy = strdup(parent);
    if (!parent_copy)
        abort();

    ensure_directory_r(parent_copy, 0755);
    free(parent_copy);

    int fd = open(output_file, O_WRONLY | O_CREAT, 0644);
    if (fd == -1) {
        return errno;
    }
    assert(write(fd, stream, len) == len);
    close(fd);
    return 0;
}
