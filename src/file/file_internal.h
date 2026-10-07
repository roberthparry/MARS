#ifndef MARS_FILE_INTERNAL_H
#define MARS_FILE_INTERNAL_H

#ifndef __linux__
#error "The MARS file module requires Linux."
#endif

#if !defined(MARS_FILE_INTERNAL_ACCESS) && !defined(__INTELLISENSE__)
#error "file_internal.h is private to the file module; include file.h instead."
#endif

#include "file.h"
#include "array.h"
#include "ustring.h"
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

struct _file_t {
    char *path;
    FILE *stream;
    DIR *directory;
    file_access_t access;
    int direction;
    int error;
};

bool file_fail(file_t *file, int error);
bool file_succeed(file_t *file);
bool file_require_closed(file_t *file);
bool file_prepare_io(file_t *file, bool writing);
bool file_finish(file_t *file, bool success);
bool file_valid_text(const string_t *text);
bool file_valid_utf8(const void *data, size_t size);
bool file_stat_regular(file_t *file, struct stat *status);
file_info_t *file_info_query(file_t *file, int dirfd, const char *path, const char *name, int flags);
bool file_read_link_at(file_t *file, int fd, const char *path, char **target);

#define FILE_STREAM_BLOCK 65536u
typedef struct {
    file_t *destination;
    file_t *temporary;
    bool overwrite;
} file_stage_t;
typedef bool (*file_emit_fn)(void *context, const unsigned char *bytes, size_t size);
typedef bool (*file_transform_fn)(file_t *source, file_t *output, void *context);
typedef struct file_decoder_t file_decoder_t;
bool file_stage_begin(file_stage_t *stage, file_t *destination, bool overwrite);
bool file_stage_publish(file_stage_t *stage);
void file_stage_discard(file_stage_t *stage);
bool file_transform(file_t *source, file_t *destination, bool overwrite, file_transform_fn transform, void *context);
bool file_emit_output(void *context, const unsigned char *bytes, size_t size);
bool file_read_exact(file_t *file, void *bytes, size_t size);
bool file_compress_emit(file_t *source, file_emit_fn emit, void *context);
file_decoder_t *file_decoder_new(file_t *source, file_t *output, uint64_t limit);
bool file_decoder_feed(file_decoder_t *decoder, const unsigned char *bytes, size_t size);
bool file_decoder_finish(file_decoder_t *decoder);
void file_decoder_free(file_decoder_t *decoder);
void file_encode_u64(unsigned char *bytes, uint64_t value, size_t size);
uint64_t file_decode_u64(const unsigned char *bytes, size_t size);

#endif
