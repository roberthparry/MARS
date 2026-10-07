/**
 * @file file_stage.c
 * @brief Temporary output staging and shared transform helpers.
 *
 * Provides output publication, exact reads, bounded emission and binary length encoding for file transforms.
 * Compression, encryption and database export share these helpers to keep failure cleanup consistent.
 *
 * This belongs to the Linux-only file.h implementation. Filesystem operations and transforms must retain the
 * public error, ownership and output-publication contracts.
 */

#define MARS_FILE_INTERNAL_ACCESS
#include "file_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Private staged outputs are always created exclusively with mode 0600. */
bool file_stage_begin(file_stage_t *stage, file_t *destination, bool overwrite)
{
    memset(stage, 0, sizeof(*stage));
    stage->destination = destination;
    stage->overwrite = overwrite;
    if (!file_require_closed(destination))
        return false;
    struct stat status;
    if (lstat(destination->path, &status) == 0) {
        if (!S_ISREG(status.st_mode))
            return file_fail(destination, S_ISLNK(status.st_mode) ? ELOOP : EINVAL);
        if (!overwrite)
            return file_fail(destination, EEXIST);
    } else if (errno != ENOENT) {
        return file_fail(destination, errno);
    }
    size_t size = strlen(destination->path);
    if (size > SIZE_MAX - 32)
        return file_fail(destination, EOVERFLOW);
    char *path = malloc(size + 32);
    if (!path)
        return file_fail(destination, ENOMEM);
    const char *slash = strrchr(destination->path, '/');
    size_t parent = slash ? (size_t)(slash - destination->path + 1) : 0;
    memcpy(path, destination->path, parent);
    strcpy(path + parent, ".mars-output-XXXXXX");
    int fd = mkostemp(path, O_CLOEXEC);
    int error = fd < 0 ? errno : 0;
    if (!error) {
        stage->temporary = file_new_cstr(path);
        if (!stage->temporary)
            error = errno;
        else {
            stage->temporary->stream = fdopen(fd, "wb");
            stage->temporary->access = FILE_ACCESS_WRITE;
            if (!stage->temporary->stream)
                error = errno;
        }
        if (error) {
            close(fd);
            unlink(path);
        }
    }
    free(path);
    if (error) {
        file_free(stage->temporary);
        stage->temporary = NULL;
        return file_fail(destination, error);
    }
    return true;
}

bool file_stage_publish(file_stage_t *stage)
{
    if (!file_close(stage->temporary))
        return file_fail(stage->destination, file_last_error(stage->temporary));
    int rc = stage->overwrite ? rename(stage->temporary->path, stage->destination->path)
        : renameat2(AT_FDCWD, stage->temporary->path, AT_FDCWD, stage->destination->path, RENAME_NOREPLACE);
    if (rc != 0)
        return file_fail(stage->destination, errno);
    file_free(stage->temporary);
    stage->temporary = NULL;
    return file_succeed(stage->destination);
}

void file_stage_discard(file_stage_t *stage)
{
    if (stage->temporary) {
        file_close(stage->temporary);
        unlink(stage->temporary->path);
        file_free(stage->temporary);
        stage->temporary = NULL;
    }
}

bool file_transform(file_t *source, file_t *destination, bool overwrite, file_transform_fn transform, void *context)
{
    if (!file_require_closed(source))
        return false;
    if (!destination || file_is_open(destination))
        return file_fail(source, destination ? EBUSY : EINVAL);
    if (!file_open_read(source))
        return false;
    struct stat input, output;
    bool ok = fstat(fileno(source->stream), &input) == 0;
    if (!ok)
        file_fail(source, errno);
    if (ok && lstat(destination->path, &output) == 0 &&
        input.st_dev == output.st_dev && input.st_ino == output.st_ino)
        ok = file_fail(source, EINVAL);
    file_stage_t stage = {0};
    if (ok && !file_stage_begin(&stage, destination, overwrite))
        ok = file_fail(source, file_last_error(destination));
    if (ok) {
        ok = transform(source, stage.temporary, context);
        if (!ok && !file_last_error(source))
            file_fail(source, file_last_error(stage.temporary));
    }
    ok = file_finish(source, ok);
    if (ok && !file_stage_publish(&stage))
        ok = file_fail(source, file_last_error(destination));
    int error = file_last_error(source);
    file_stage_discard(&stage);
    return ok ? file_succeed(source) : file_fail(source, error);
}

bool file_emit_output(void *context, const unsigned char *bytes, size_t size)
{
    size_t written;
    return file_write(context, bytes, size, &written);
}

bool file_read_exact(file_t *file, void *bytes, size_t size)
{
    size_t count;
    if (!file_read(file, bytes, size, &count))
        return false;
    return count == size ? true : file_fail(file, EBADMSG);
}

/* Binary containers use explicit big-endian fields, never host structure layouts. */
void file_encode_u64(unsigned char *bytes, uint64_t value, size_t size)
{
    for (size_t i = size; i > 0; --i) {
        bytes[i - 1] = (unsigned char)value;
        value >>= 8;
    }
}

uint64_t file_decode_u64(const unsigned char *bytes, size_t size)
{
    uint64_t value = 0;
    for (size_t i = 0; i < size; ++i)
        value = (value << 8) | bytes[i];
    return value;
}
