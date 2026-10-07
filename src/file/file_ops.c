/**
 * @file file_ops.c
 * @brief Filesystem object creation, copying, movement and links.
 *
 * Implements path operations including deletion, moves, links and symbolic-link target queries. This unit handles
 * filesystem changes; stream byte I/O belongs to file_stream.c.
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

bool file_stat_regular(file_t *file, struct stat *status)
{
    if (!file)
        return file_fail(NULL, EINVAL);
    if (lstat(file->path, status) != 0)
        return file_fail(file, errno);
    if (!S_ISREG(status->st_mode))
        return file_fail(file, S_ISLNK(status->st_mode) ? ELOOP : S_ISDIR(status->st_mode) ? EISDIR : ENOTSUP);
    return true;
}

/* Absence is a normal negative answer, not an error. */
bool file_exists(file_t *file)
{
    struct stat status;
    if (!file)
        return file_fail(NULL, EINVAL);
    if (lstat(file->path, &status) != 0) {
        file_fail(file, errno);
        if (file->error == ENOENT || file->error == ENOTDIR)
            file_succeed(file);
        return false;
    }
    return file_succeed(file);
}

/* Unlink non-directory entries without following a final symlink. */
bool file_delete(file_t *file)
{
    if (!file_require_closed(file))
        return false;
    struct stat status;
    if (lstat(file->path, &status) != 0)
        return errno == ENOENT ? file_succeed(file) : file_fail(file, errno);
    if (S_ISDIR(status.st_mode))
        return file_fail(file, EISDIR);
    return unlink(file->path) == 0 ? file_succeed(file) : file_fail(file, errno);
}

static bool file_destination(file_t *source, file_t *destination, bool overwrite, bool regular,
                             struct stat *source_stat)
{
    if (!file_require_closed(source))
        return false;
    if (!destination || file_is_open(destination))
        return file_fail(source, destination ? EBUSY : EINVAL);
    if (regular && !file_stat_regular(source, source_stat))
        return false;
    if (!regular && lstat(source->path, source_stat) != 0)
        return file_fail(source, errno);
    struct stat target;
    if (lstat(destination->path, &target) == 0) {
        if (regular && !S_ISREG(target.st_mode))
            return file_fail(source, S_ISLNK(target.st_mode) ? ELOOP : EISDIR);
        if (target.st_dev == source_stat->st_dev && target.st_ino == source_stat->st_ino)
            return file_fail(source, EINVAL);
        if (!overwrite)
            return file_fail(source, EEXIST);
    } else if (errno != ENOENT) {
        return file_fail(source, errno);
    }
    return true;
}

static bool file_copy_data(int input, int output)
{
    unsigned char buffer[65536];
    for (;;) {
        ssize_t count = read(input, buffer, sizeof(buffer));
        if (count < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (!count)
            return true;
        size_t offset = 0;
        while (offset < (size_t)count) {
            ssize_t written = write(output, buffer + offset, (size_t)count - offset);
            if (written < 0 && errno == EINTR)
                continue;
            if (written <= 0) {
                if (!written)
                    errno = EIO;
                return false;
            }
            offset += (size_t)written;
        }
    }
}

/* Stage a complete copy beside the destination before changing the destination name. */
bool file_copy(file_t *source, file_t *destination, bool overwrite)
{
    struct stat status;
    if (!file_destination(source, destination, overwrite, true, &status))
        return false;
    int input = open(source->path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (input < 0)
        return file_fail(source, errno);
    int stat_result = fstat(input, &status);
    if (stat_result != 0 || !S_ISREG(status.st_mode)) {
        int error = stat_result != 0 ? errno : ENOTSUP;
        close(input);
        return file_fail(source, error);
    }
    size_t length = strlen(destination->path);
    if (length > SIZE_MAX - 32) {
        close(input);
        return file_fail(source, EOVERFLOW);
    }
    char *temporary = malloc(length + 32);
    if (!temporary) {
        close(input);
        return file_fail(source, ENOMEM);
    }
    const char *destination_path = destination->path;
    const char *slash = strrchr(destination_path, '/');
    size_t parent_length = slash ? (size_t)(slash - destination_path + 1) : 0;
    memcpy(temporary, destination_path, parent_length);
    strcpy(temporary + parent_length, ".mars-copy-XXXXXX");
    int output = mkostemp(temporary, O_CLOEXEC);
    int error = output < 0 ? errno : 0;
    if (!error && !file_copy_data(input, output))
        error = errno;
    if (!error && fchmod(output, status.st_mode & 0777) != 0)
        error = errno;
    if (output >= 0 && close(output) != 0 && !error)
        error = errno;
    if (close(input) != 0 && !error)
        error = errno;
    if (!error) {
        int rc = overwrite ? rename(temporary, destination->path)
                           : link(temporary, destination->path);
        if (rc != 0)
            error = errno;
    }
    /* Only this operation's private staging file is removed. */
    if (output >= 0)
        unlink(temporary);
    free(temporary);
    return error ? file_fail(source, error) : file_succeed(source);
}

static int file_rename_without_replace(const char *source, const char *destination)
{
    return renameat2(AT_FDCWD, source, AT_FDCWD, destination, RENAME_NOREPLACE);
}

/* Rename where possible; a cross-filesystem move copies completely before unlinking the source. */
bool file_move(file_t *source, file_t *destination, bool overwrite)
{
    struct stat status;
    if (!file_destination(source, destination, overwrite, false, &status))
        return false;
    int rc = overwrite ? rename(source->path, destination->path)
                       : file_rename_without_replace(source->path, destination->path);
    if (rc == 0)
        return file_succeed(source);
    if (errno != EXDEV)
        return file_fail(source, errno);
    if (!S_ISREG(status.st_mode))
        return file_fail(source, EXDEV);
    if (!file_copy(source, destination, overwrite))
        return false;
    return unlink(source->path) == 0 ? file_succeed(source) : file_fail(source, errno);
}

/* Preserve the old destination first, then perform one atomic replacement rename. */
bool file_replace(file_t *source, file_t *destination, file_t *backup)
{
    struct stat source_status, destination_status;
    if (!file_destination(source, destination, true, true, &source_status))
        return false;
    if (!file_stat_regular(destination, &destination_status))
        return file_fail(source, destination->error);
    if (source_status.st_dev != destination_status.st_dev)
        return file_fail(source, EXDEV);
    if (backup) {
        if (!file_require_closed(backup))
            return file_fail(source, backup->error);
        if (strcmp(backup->path, source->path) == 0 ||
            strcmp(backup->path, destination->path) == 0)
            return file_fail(source, EINVAL);
        if (!file_copy(destination, backup, false))
            return file_fail(source, destination->error);
    }
    return rename(source->path, destination->path) == 0
        ? file_succeed(source) : file_fail(source, errno);
}

/* Create a hard link without overwriting any existing directory entry. */
bool file_create_hard_link(file_t *source, file_t *destination)
{
    struct stat status;
    if (!file_destination(source, destination, false, true, &status))
        return false;
    return link(source->path, destination->path) == 0 ? file_succeed(source) : file_fail(source, errno);
}

/* Store a raw target path verbatim; a dangling target is permitted. */
bool file_create_symlink(file_t *target, file_t *link)
{
    if (!file_require_closed(target))
        return false;
    if (!link || file_is_open(link))
        return file_fail(target, link ? EBUSY : EINVAL);
    return symlink(target->path, link->path) == 0 ? file_succeed(target) : file_fail(target, errno);
}

/* Read unknown-length link targets without Unicode normalisation or fixed PATH_MAX assumptions. */
bool file_read_link(file_t *link, char **target)
{
    if (!target)
        return file_fail(link, EINVAL);
    *target = NULL;
    if (!file_require_closed(link))
        return false;
    return file_read_link_at(link, AT_FDCWD, link->path, target);
}

/* Read relative to the listing descriptor so relative targets survive directory renames. */
bool file_read_link_at(file_t *link, int fd, const char *path, char **target)
{
    size_t capacity = 256;
    for (;;) {
        char *buffer = malloc(capacity + 1);
        if (!buffer)
            return file_fail(link, ENOMEM);
        ssize_t size = readlinkat(fd, path, buffer, capacity);
        if (size < 0) {
            int error = errno;
            free(buffer);
            return file_fail(link, error);
        }
        if ((size_t)size < capacity) {
            buffer[size] = '\0';
            *target = buffer;
            return file_succeed(link);
        }
        free(buffer);
        if (capacity > (SIZE_MAX - 1) / 2)
            return file_fail(link, EOVERFLOW);
        capacity *= 2;
    }
}

/* Resolve an existing path explicitly; ordinary stream opening never silently follows a final symlink. */
file_t *file_resolve(file_t *file)
{
    if (!file) {
        file_fail(NULL, EINVAL);
        return NULL;
    }
    char *path = realpath(file->path, NULL);
    if (!path) {
        file_fail(file, errno);
        return NULL;
    }
    file_t *resolved = file_new_cstr(path);
    free(path);
    if (!resolved)
        file_fail(file, errno);
    else
        file_succeed(file);
    return resolved;
}
