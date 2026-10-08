/**
 * @file file_directory.c
 * @brief Directory stream iteration and collected listings.
 *
 * Opens directory handles, reads entries and creates metadata-bearing listing arrays. It centralises entry and
 * container ownership for callers that need either incremental iteration or a complete listing. Also creates
 * ordinary directories and exclusive private temporary directories for staged tool output.
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

/* Create a private directory exclusively; handle destruction never removes its contents. */
file_t *file_create_temp_directory(const string_t *parent)
{
    file_t *validated = file_new(parent);
    if (!validated)
        return NULL;
    size_t length = strlen(validated->path);
    static const char suffix[] = "/.mars-directory-XXXXXX";
    if (length > SIZE_MAX - sizeof(suffix)) {
        file_free(validated);
        errno = EOVERFLOW;
        return NULL;
    }
    char *path = malloc(length + sizeof(suffix));
    if (!path) {
        int error = errno;
        file_free(validated);
        errno = error;
        return NULL;
    }
    memcpy(path, validated->path, length);
    memcpy(path + length, suffix, sizeof(suffix));
    /* Finish allocating the owned handle before creating anything to roll back. */
    free(validated->path);
    validated->path = path;
    if (!mkdtemp(validated->path)) {
        int error = errno;
        file_free(validated);
        errno = error;
        return NULL;
    }
    errno = 0;
    return validated;
}

static int file_mkdir_component(const char *path, mode_t permissions)
{
    if (mkdir(path, permissions) == 0)
        return 0;
    if (errno != EEXIST)
        return -1;
    struct stat status;
    if (lstat(path, &status) != 0)
        return -1;
    if (!S_ISDIR(status.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }
    return 0;
}

/* Create missing components without treating a symlink as an existing directory. */
bool file_create_directory(file_t *directory, unsigned permissions, bool parents)
{
    if (!file_require_closed(directory))
        return false;
    if (permissions & ~07777u)
        return file_fail(directory, EINVAL);
    if (!parents)
        return file_mkdir_component(directory->path, permissions) == 0
            ? file_succeed(directory) : file_fail(directory, errno);
    char *path = strdup(directory->path);
    if (!path)
        return file_fail(directory, ENOMEM);
    int error = 0;
    /* Each pathname component must be visited once to create missing parents. */
    for (char *cursor = path + 1; *cursor; ++cursor) {
        if (*cursor != '/' || cursor[-1] == '/')
            continue;
        *cursor = '\0';
        int rc = file_mkdir_component(path, permissions);
        *cursor = '/';
        if (rc != 0) {
            error = errno;
            break;
        }
    }
    if (!error && file_mkdir_component(path, permissions) != 0)
        error = errno;
    free(path);
    return error ? file_fail(directory, error) : file_succeed(directory);
}

/* Remove an empty directory only. */
bool file_remove_directory(file_t *directory)
{
    if (!file_require_closed(directory))
        return false;
    return rmdir(directory->path) == 0 ? file_succeed(directory) : file_fail(directory, errno);
}

/* Open an inode-backed directory iterator; do not follow the final symlink. */
bool file_open_directory(file_t *directory)
{
    if (!file_require_closed(directory))
        return false;
    int fd = open(directory->path, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0)
        return file_fail(directory, errno);
    directory->directory = fdopendir(fd);
    if (!directory->directory) {
        int error = errno;
        close(fd);
        return file_fail(directory, error);
    }
    return file_succeed(directory);
}

/* Obtain metadata relative to the open directory, keeping symlinks as symlinks. */
bool file_read_directory(file_t *directory, file_info_t **entry)
{
    if (!entry)
        return file_fail(directory, EINVAL);
    *entry = NULL;
    if (!directory || !directory->directory)
        return file_fail(directory, EBADF);
    struct dirent *item;
    for (;;) {
        errno = 0;
        item = readdir(directory->directory);
        if (!item)
            return errno ? file_fail(directory, errno) : file_succeed(directory);
        if (strcmp(item->d_name, ".") != 0 && strcmp(item->d_name, "..") != 0)
            break;
    }
    *entry = file_info_query(directory, dirfd(directory->directory), item->d_name, item->d_name,
                            AT_SYMLINK_NOFOLLOW);
    return *entry != NULL;
}

static void file_destroy_info(void *element)
{
    file_info_free(*(file_info_t **)element);
}

/* Collect directory snapshots; the returned array owns every entry. */
array_t *file_list_directory(file_t *directory)
{
    if (!file_open_directory(directory))
        return NULL;
    array_t *entries = array_create(sizeof(file_info_t *), NULL, file_destroy_info);
    bool ok = entries != NULL;
    if (!ok)
        file_fail(directory, ENOMEM);
    while (ok) {
        file_info_t *entry = NULL;
        ok = file_read_directory(directory, &entry);
        if (!ok || !entry)
            break;
        if (!array_add(entries, &entry)) {
            file_info_free(entry);
            ok = file_fail(directory, ENOMEM);
        }
    }
    if (!file_finish(directory, ok)) {
        array_destroy(entries);
        return NULL;
    }
    return entries;
}
