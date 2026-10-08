/**
 * @file file_stream.c
 * @brief Linux file-handle lifecycle and byte-stream operations.
 *
 * Owns paths and open streams and implements reading, writing, seeking, locking, truncation and synchronisation.
 * Other file units build on its state checks and error reporting rather than exposing native handles.
 *
 * This belongs to the Linux-only file.h implementation. Filesystem operations and transforms must retain the
 * public error, ownership and output-publication contracts.
 */

#define MARS_FILE_INTERNAL_ACCESS
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#include "file_internal.h"

bool file_fail(file_t *file, int error)
{
    if (file)
        file->error = error ? error : EIO;
    errno = error ? error : EIO;
    return false;
}

bool file_succeed(file_t *file)
{
    file->error = 0;
    return true;
}

bool file_require_closed(file_t *file)
{
    return file && !file_is_open(file) ? true : file_fail(file, file ? EBUSY : EINVAL);
}

bool file_prepare_io(file_t *file, bool writing)
{
    if (!file || !file->stream)
        return file_fail(file, EBADF);
    if ((writing && file->access == FILE_ACCESS_READ) || (!writing && file->access == FILE_ACCESS_WRITE))
        return file_fail(file, EBADF);
    int direction = writing ? 2 : 1;
    if (file->direction && file->direction != direction && fseeko(file->stream, 0, SEEK_CUR) != 0)
        return file_fail(file, errno);
    file->direction = direction;
    return true;
}

bool file_finish(file_t *file, bool success)
{
    int error = file->error;
    bool closed = file_close(file);
    return success ? closed : file_fail(file, error);
}

/* Copy a counted path without normalising its bytes. */
file_t *file_new(const string_t *path)
{
    if (!path || !string_byte_length(path) || memchr(string_c_str(path), 0, string_byte_length(path))) {
        errno = EINVAL;
        return NULL;
    }
    return file_new_cstr(string_c_str(path));
}

/* Copy a conventional filesystem path. */
file_t *file_new_cstr(const char *path)
{
    if (!path || !*path) {
        errno = EINVAL;
        return NULL;
    }
    file_t *file = calloc(1, sizeof(*file));
    if (!file)
        return NULL;
    file->path = strdup(path);
    if (!file->path) {
        free(file);
        return NULL;
    }
    return file;
}

/* Release the stream and the copied path. */
void file_free(file_t *file)
{
    if (!file)
        return;
    if (file->stream)
        fclose(file->stream);
    if (file->directory)
        closedir(file->directory);
    free(file->path);
    free(file);
}

/* Borrow the handle's path. */
const char *file_path(const file_t *file) { return file ? file->path : NULL; }

/* Inspect the latest operation's error code. */
int file_last_error(const file_t *file) { return file ? file->error : EINVAL; }

/* Describe the latest error without changing it. */
const char *file_last_error_message(const file_t *file) { return strerror(file_last_error(file)); }

/* Open first, verify regular-file type, then truncate if requested. */
static bool file_open_policy(file_t *file, file_mode_t mode, file_access_t access, bool follow)
{
    static const int mode_flags[] = {0, O_CREAT, O_CREAT | O_EXCL, 0, O_CREAT, O_CREAT | O_APPEND};
    static const int access_flags[] = {O_RDONLY, O_WRONLY, O_RDWR};
    static const char *const stream_modes[] = {"rb", "wb", "r+b"};
    if (!file_require_closed(file))
        return false;
    if ((unsigned)mode > FILE_MODE_APPEND || (unsigned)access > FILE_ACCESS_READ_WRITE ||
        ((mode == FILE_MODE_CREATE || mode == FILE_MODE_TRUNCATE || mode == FILE_MODE_CREATE_NEW) &&
         access == FILE_ACCESS_READ) || (mode == FILE_MODE_APPEND && access != FILE_ACCESS_WRITE))
        return file_fail(file, EINVAL);
    int flags = mode_flags[mode] | access_flags[access] | O_CLOEXEC | O_NONBLOCK | (follow ? 0 : O_NOFOLLOW);
    int fd = open(file->path, flags, 0666);
    if (fd < 0)
        return file_fail(file, errno);
    struct stat status;
    int error = 0;
    if (fstat(fd, &status) != 0)
        error = errno;
    else if (!S_ISREG(status.st_mode))
        error = S_ISDIR(status.st_mode) ? EISDIR : ENOTSUP;
    else if ((mode == FILE_MODE_CREATE || mode == FILE_MODE_TRUNCATE) && ftruncate(fd, 0) != 0)
        error = errno;
    if (!error) {
        file->stream = fdopen(fd, stream_modes[access]);
        if (!file->stream)
            error = errno;
    }
    if (error) {
        close(fd);
        return file_fail(file, error);
    }
    file->access = access;
    file->direction = 0;
    if (mode == FILE_MODE_APPEND && fseeko(file->stream, 0, SEEK_END) != 0) {
        error = errno;
        file_close(file);
        return file_fail(file, error);
    }
    return file_succeed(file);
}

/* Open a regular file without following a final symbolic link. */
bool file_open(file_t *file, file_mode_t mode, file_access_t access)
{
    return file_open_policy(file, mode, access, false);
}

/* Opt into ordinary Linux symbolic-link resolution for a regular-file stream. */
bool file_open_follow(file_t *file, file_mode_t mode, file_access_t access)
{
    return file_open_policy(file, mode, access, true);
}

/* Create or truncate a read/write stream. */
bool file_create(file_t *file) { return file_open(file, FILE_MODE_CREATE, FILE_ACCESS_READ_WRITE); }
/* Create a BOM-free text writer. */
bool file_create_text(file_t *file) { return file_open(file, FILE_MODE_CREATE, FILE_ACCESS_WRITE); }
/* Open an existing reader. */
bool file_open_read(file_t *file) { return file_open(file, FILE_MODE_OPEN, FILE_ACCESS_READ); }
/* Open a lazy text reader. */
bool file_open_text(file_t *file) { return file_open_read(file); }
/* Open a non-truncating writer at the beginning. */
bool file_open_write(file_t *file) { return file_open(file, FILE_MODE_OPEN_OR_CREATE, FILE_ACCESS_WRITE); }
/* Open an append-only text writer. */
bool file_append_text(file_t *file) { return file_open(file, FILE_MODE_APPEND, FILE_ACCESS_WRITE); }

/* Close once, preserving any close error. */
bool file_close(file_t *file)
{
    if (!file)
        return file_fail(NULL, EINVAL);
    FILE *stream = file->stream;
    DIR *directory = file->directory;
    file->stream = NULL;
    file->directory = NULL;
    file->direction = 0;
    if (stream && fclose(stream) != 0)
        return file_fail(file, errno);
    if (directory && closedir(directory) != 0)
        return file_fail(file, errno);
    return file_succeed(file);
}

/* Inspect open state without touching errors. */
bool file_is_open(const file_t *file) { return file && (file->stream || file->directory); }

/* Flush userspace buffering only. */
bool file_flush(file_t *file)
{
    if (!file || !file->stream)
        return file_fail(file, EBADF);
    return fflush(file->stream) == 0 ? file_succeed(file) : file_fail(file, errno);
}

/* Read a counted block, distinguishing EOF from an error. */
bool file_read(file_t *file, void *buffer, size_t capacity, size_t *read_count)
{
    if (read_count)
        *read_count = 0;
    if (!read_count || (!buffer && capacity))
        return file_fail(file, EINVAL);
    if (!file_prepare_io(file, false))
        return false;
    if (capacity)
        *read_count = fread(buffer, 1, capacity, file->stream);
    return ferror(file->stream) ? file_fail(file, errno) : file_succeed(file);
}

/* Write a counted block, reporting partial progress. */
bool file_write(file_t *file, const void *data, size_t size, size_t *written_count)
{
    if (written_count)
        *written_count = 0;
    if (!written_count || (!data && size))
        return file_fail(file, EINVAL);
    if (!file_prepare_io(file, true))
        return false;
    if (size)
        *written_count = fwrite(data, 1, size, file->stream);
    return *written_count == size ? file_succeed(file) : file_fail(file, errno);
}

/* Seek without narrowing a large caller offset. */
bool file_seek(file_t *file, int64_t offset, file_seek_t origin)
{
    static const int origins[] = {SEEK_SET, SEEK_CUR, SEEK_END};
    if (!file || !file->stream)
        return file_fail(file, EBADF);
    if ((unsigned)origin > FILE_SEEK_END)
        return file_fail(file, EINVAL);
    off_t native = (off_t)offset;
    if ((int64_t)native != offset)
        return file_fail(file, EOVERFLOW);
    if (fseeko(file->stream, native, origins[origin]) != 0)
        return file_fail(file, errno);
    file->direction = 0;
    return file_succeed(file);
}

/* Return the logical stream position, including buffering. */
bool file_tell(file_t *file, int64_t *position)
{
    if (!position)
        return file_fail(file, EINVAL);
    if (!file || !file->stream)
        return file_fail(file, EBADF);
    off_t result = ftello(file->stream);
    if (result < 0)
        return file_fail(file, errno);
    *position = (int64_t)result;
    return file_succeed(file);
}

/* Coordinate cooperating processes through an advisory lock. */
bool file_lock(file_t *file, bool exclusive, bool wait)
{
    if (!file || !file->stream)
        return file_fail(file, EBADF);
    int operation = (exclusive ? LOCK_EX : LOCK_SH) | (wait ? 0 : LOCK_NB);
    return flock(fileno(file->stream), operation) == 0 ? file_succeed(file) : file_fail(file, errno);
}

/* Release an advisory lock owned by this stream. */
bool file_unlock(file_t *file)
{
    if (!file || !file->stream)
        return file_fail(file, EBADF);
    return flock(fileno(file->stream), LOCK_UN) == 0 ? file_succeed(file) : file_fail(file, errno);
}

/* Flush before changing length; an existing stream keeps its logical position. */
bool file_truncate(file_t *file, int64_t size)
{
    if (size < 0 || (int64_t)(off_t)size != size)
        return file_fail(file, size < 0 ? EINVAL : EOVERFLOW);
    if (!file)
        return file_fail(NULL, EINVAL);
    bool opened = !file_is_open(file);
    if (opened && !file_open(file, FILE_MODE_OPEN, FILE_ACCESS_WRITE))
        return false;
    bool ok = file_prepare_io(file, true) && file_flush(file);
    if (ok)
        ok = ftruncate(fileno(file->stream), (off_t)size) == 0 ? file_succeed(file) : file_fail(file, errno);
    return opened ? file_finish(file, ok) : ok;
}

/* Request kernel-backed durability; directory sync always includes metadata. */
bool file_sync(file_t *file, bool data_only)
{
    if (!file_is_open(file))
        return file_fail(file, EBADF);
    if (file->stream && !file_flush(file))
        return false;
    int fd = file->stream ? fileno(file->stream) : dirfd(file->directory);
    int rc = data_only && file->stream ? fdatasync(fd) : fsync(fd);
    return rc == 0 ? file_succeed(file) : file_fail(file, errno);
}
