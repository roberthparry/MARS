/**
 * @file file_info.c
 * @brief Filesystem metadata, permissions and ownership.
 *
 * Inspects file attributes and implements chmod, chown, timestamps and access queries. Path-following and handle
 * policies remain explicit in the individual operations rather than hidden in caller-side stat calls.
 *
 * This belongs to the Linux-only file.h implementation. Filesystem operations and transforms must retain the
 * public error, ownership and output-publication contracts.
 */

#define MARS_FILE_INTERNAL_ACCESS
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "file_internal.h"

struct _file_info_t {
    struct statx status;
    char *name;
    unsigned attributes;
    char *link_target;
    file_info_t *target_info;
    int target_error;
};

file_info_t *file_info_query(file_t *file, int fd, const char *path, const char *name, int flags)
{
    struct statx status;
    if (statx(fd, path, flags, STATX_BASIC_STATS | STATX_BTIME, &status) != 0) {
        file_fail(file, errno);
        return NULL;
    }
    unsigned required = STATX_TYPE | STATX_MODE | STATX_SIZE | STATX_UID | STATX_GID | STATX_NLINK;
    if ((status.stx_mask & required) != required) {
        file_fail(file, ENOTSUP);
        return NULL;
    }
    file_info_t *info = calloc(1, sizeof(*info));
    if (!info) {
        file_fail(file, ENOMEM);
        return NULL;
    }
    info->name = strdup(name);
    if (!info->name) {
        free(info);
        file_fail(file, ENOMEM);
        return NULL;
    }
    info->status = status;
    info->attributes = status.stx_mode & 0222 ? 0 : FILE_ATTRIBUTE_READ_ONLY;
    if (*name == '.')
        info->attributes |= FILE_ATTRIBUTE_HIDDEN;
    if ((flags & AT_SYMLINK_NOFOLLOW) && S_ISLNK(status.stx_mode)) {
        if (!file_read_link_at(file, fd, path, &info->link_target)) {
            file_info_free(info);
            return NULL;
        }
        info->target_info = file_info_query(file, fd, path, name, 0);
        if (!info->target_info) {
            info->target_error = file_last_error(file);
            if (info->target_error == ENOMEM) {
                file_info_free(info);
                return NULL;
            }
        }
    }
    file_succeed(file);
    return info;
}

/* Snapshot the named entry itself, including dangling links and directories. */
file_info_t *file_get_info(file_t *file)
{
    if (!file_require_closed(file))
        return NULL;
    const char *base = strrchr(file->path, '/');
    return file_info_query(file, AT_FDCWD, file->path, base ? base + 1 : file->path, AT_SYMLINK_NOFOLLOW);
}

/* Release a snapshot and its copied raw basename. */
void file_info_free(file_info_t *info)
{
    if (info) {
        free(info->name);
        free(info->link_target);
        file_info_free(info->target_info);
    }
    free(info);
}

/* Return the copied link text, not a normalised or resolved pathname. */
const char *file_info_link_target(const file_info_t *info) { return info ? info->link_target : NULL; }

/* Return target metadata owned by the containing snapshot. */
const file_info_t *file_info_target_info(const file_info_t *info) { return info ? info->target_info : NULL; }

/* Distinguish dangling, looping or inaccessible targets without failing a listing. */
int file_info_target_error(const file_info_t *info) { return info ? info->target_error : EINVAL; }

/* Inspect the copied raw name. */
const char *file_info_name(const file_info_t *info) { return info ? info->name : NULL; }

/* Inspect the inode type through a fixed Linux mode table. */
file_type_t file_info_type(const file_info_t *info)
{
    static const file_type_t types[16] = {
        [S_IFREG >> 12]  = FILE_TYPE_REGULAR,
        [S_IFDIR >> 12]  = FILE_TYPE_DIRECTORY,
        [S_IFLNK >> 12]  = FILE_TYPE_SYMLINK,
        [S_IFIFO >> 12]  = FILE_TYPE_FIFO,
        [S_IFSOCK >> 12] = FILE_TYPE_SOCKET,
        [S_IFCHR >> 12]  = FILE_TYPE_CHARACTER_DEVICE,
        [S_IFBLK >> 12]  = FILE_TYPE_BLOCK_DEVICE,
    };
    return info ? types[(info->status.stx_mode & S_IFMT) >> 12] : FILE_TYPE_UNKNOWN;
}

/* Inspect the recorded size. */
uint64_t file_info_size(const file_info_t *info) { return info ? info->status.stx_size : 0; }

/* Inspect attribute bits. */
unsigned file_info_attributes(const file_info_t *info) { return info ? info->attributes : FILE_ATTRIBUTE_NONE; }

/* Inspect POSIX permission and special bits. */
unsigned file_info_permissions(const file_info_t *info) { return info ? info->status.stx_mode & 07777 : 0; }

/* Inspect the numeric owner. */
uint32_t file_info_owner(const file_info_t *info) { return info ? info->status.stx_uid : UINT32_MAX; }

/* Inspect the numeric group. */
uint32_t file_info_group(const file_info_t *info) { return info ? info->status.stx_gid : UINT32_MAX; }

/* Inspect the number of hard links to the inode. */
uint64_t file_info_link_count(const file_info_t *info) { return info ? info->status.stx_nlink : 0; }

static bool file_info_time(const file_info_t *info, const struct statx_timestamp *time, unsigned mask,
                           int64_t *seconds, long *nanoseconds)
{
    if (!info || !seconds || !nanoseconds) {
        errno = EINVAL;
        return false;
    }
    if (!(info->status.stx_mask & mask)) {
        errno = ENOTSUP;
        return false;
    }
    *seconds = time->tv_sec;
    *nanoseconds = time->tv_nsec;
    return true;
}

/* Never substitute inode change time for an unavailable birth time. */
bool file_info_creation_time(const file_info_t *info, int64_t *seconds, long *nanoseconds)
{
    return file_info_time(info, info ? &info->status.stx_btime : NULL, STATX_BTIME, seconds, nanoseconds);
}

/* Expose content modification time. */
bool file_info_last_write_time(const file_info_t *info, int64_t *seconds, long *nanoseconds)
{
    return file_info_time(info, info ? &info->status.stx_mtime : NULL, STATX_MTIME, seconds, nanoseconds);
}

/* Expose access time. */
bool file_info_last_access_time(const file_info_t *info, int64_t *seconds, long *nanoseconds)
{
    return file_info_time(info, info ? &info->status.stx_atime : NULL, STATX_ATIME, seconds, nanoseconds);
}

/* Expose inode status-change time separately from birth time. */
bool file_info_status_change_time(const file_info_t *info, int64_t *seconds, long *nanoseconds)
{
    return file_info_time(info, info ? &info->status.stx_ctime : NULL, STATX_CTIME, seconds, nanoseconds);
}

/* Refresh attributes. */
bool file_get_attributes(file_t *file, unsigned *attributes)
{
    if (!attributes)
        return file_fail(file, EINVAL);
    file_info_t *info = file_get_info(file);
    if (!info)
        return false;
    *attributes = info->attributes;
    file_info_free(info);
    return true;
}

/* Only write permission bits, not filename-based hidden status, are mutable as attributes. */
bool file_set_attributes(file_t *file, unsigned attributes)
{
    if (!file_require_closed(file))
        return false;
    if (attributes & ~(FILE_ATTRIBUTE_READ_ONLY | FILE_ATTRIBUTE_HIDDEN))
        return file_fail(file, EINVAL);
    file_info_t *info = file_get_info(file);
    if (!info)
        return false;
    unsigned current = file_info_attributes(info), permissions = file_info_permissions(info);
    file_info_free(info);
    if ((attributes & FILE_ATTRIBUTE_HIDDEN) != (current & FILE_ATTRIBUTE_HIDDEN))
        return file_fail(file, ENOTSUP);
    permissions = attributes & FILE_ATTRIBUTE_READ_ONLY ? permissions & ~0222u : permissions | S_IWUSR;
    return file_chmod(file, permissions);
}

/* Apply Linux permission bits without following the final symlink. */
bool file_chmod(file_t *file, unsigned permissions)
{
    if (!file_require_closed(file))
        return false;
    if (permissions & ~07777u)
        return file_fail(file, EINVAL);
    struct stat status;
    if (lstat(file->path, &status) != 0)
        return file_fail(file, errno);
    if (S_ISLNK(status.st_mode))
        return file_fail(file, ELOOP);
    return fchmodat(AT_FDCWD, file->path, permissions, AT_SYMLINK_NOFOLLOW) == 0
        ? file_succeed(file) : file_fail(file, errno);
}

/* A value of -1 leaves the corresponding owner field unchanged. */
bool file_chown(file_t *file, int64_t owner, int64_t group)
{
    if (!file_require_closed(file))
        return false;
    if (owner < -1 || group < -1 || owner >= UINT32_MAX || group >= UINT32_MAX)
        return file_fail(file, EINVAL);
    return fchownat(AT_FDCWD, file->path, (uid_t)owner, (gid_t)group, AT_SYMLINK_NOFOLLOW) == 0
        ? file_succeed(file) : file_fail(file, errno);
}

/* Linux does not generally provide a setter for filesystem birth time. */
bool file_set_times(file_t *file, int64_t access_seconds, long access_nanoseconds,
                    int64_t write_seconds, long write_nanoseconds)
{
    if (!file_require_closed(file))
        return false;
    if (access_nanoseconds < 0 || access_nanoseconds >= 1000000000 ||
        write_nanoseconds < 0 || write_nanoseconds >= 1000000000)
        return file_fail(file, EINVAL);
    struct timespec times[2] = {{(time_t)access_seconds, access_nanoseconds}, {(time_t)write_seconds, write_nanoseconds}};
    if ((int64_t)times[0].tv_sec != access_seconds || (int64_t)times[1].tv_sec != write_seconds)
        return file_fail(file, EOVERFLOW);
    return utimensat(AT_FDCWD, file->path, times, AT_SYMLINK_NOFOLLOW) == 0
        ? file_succeed(file) : file_fail(file, errno);
}

/* Check effective-user access; callers must still handle races and operation failures. */
bool file_check_access(file_t *file, bool read_access, bool write_access, bool execute_access)
{
    if (!file)
        return file_fail(NULL, EINVAL);
    int mode = (read_access ? R_OK : 0) | (write_access ? W_OK : 0) | (execute_access ? X_OK : 0);
    return faccessat(AT_FDCWD, file->path, mode, AT_EACCESS) == 0
        ? file_succeed(file) : file_fail(file, errno);
}
