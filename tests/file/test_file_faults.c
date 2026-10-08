/**
 * @file test_file_faults.c
 * @brief Injected file I/O and allocation failure regressions.
 *
 * Exercises copy, move, synchronisation and allocation failures, including cross-device paths. Child-process
 * isolation and cleanup prevent fault injection from contaminating later cases.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "file.h"
#include "array.h"
#include "test_harness.h"

/* Link-time fault injection is private to this test executable, never the library. */
enum fault_id { F_MALLOC, F_CALLOC, F_REALLOC, F_STRDUP, F_READ, F_WRITE, F_RENAME, F_RENAMEAT2,
                F_UNLINK, F_FSYNC, F_FDATASYNC, F_CHOWN, F_FDOPEN, F_FDOPENDIR, F_READDIR,
                F_STATX, F_FSTAT, F_FCLOSE, F_CLOSE, F_FCHMOD, F_FTELLO, F_FSEEKO, F_FERROR, F_COUNT };
static struct { bool armed; unsigned skip; int error; } faults[F_COUNT];

static void inject(enum fault_id id, unsigned skip, int error)
{
    faults[id].armed = true;
    faults[id].skip = skip;
    faults[id].error = error;
}

static bool hit(enum fault_id id)
{
    if (!faults[id].armed)
        return false;
    if (faults[id].skip) {
        --faults[id].skip;
        return false;
    }
    faults[id].armed = false;
    errno = faults[id].error;
    return true;
}

static void reset_faults(void) { memset(faults, 0, sizeof(faults)); }

static void expect_text(file_t *file, const char *expected);

/* Digest failures close their stream and retain the first error. */
void test_file_sha256_failures(void)
{
    file_t *source = file_new_cstr(test_case_temp_path("hash-fault"));
    ASSERT_NOT_NULL(source);
    ASSERT_TRUE(file_write_all_bytes(source, "abc", 3));
    static const struct {
        enum fault_id id;
        unsigned skip;
    } cases[] = {{F_FSTAT, 1}, {F_FSTAT, 2}, {F_FERROR, 0}, {F_FCLOSE, 0}, {F_MALLOC, 0}};
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        int expected = cases[i].id == F_MALLOC ? ENOMEM : EIO;
        inject(cases[i].id, cases[i].skip, expected);
        string_t *digest = file_sha256(source);
        int error = file_last_error(source);
        reset_faults();
        bool rejected = digest == NULL;
        string_free(digest);
        ASSERT_TRUE(rejected);
        ASSERT_EQ_INT(error, expected);
        ASSERT_TRUE(!file_is_open(source));
    }
    inject(F_FSTAT, 2, 0);
    string_t *changed_digest = file_sha256(source);
    int changed_error = file_last_error(source);
    reset_faults();
    bool changed_rejected = changed_digest == NULL;
    string_free(changed_digest);
    ASSERT_TRUE(changed_rejected);
    ASSERT_EQ_INT(changed_error, ESTALE);
    ASSERT_TRUE(!file_is_open(source));
    static const struct {
        enum fault_id id;
        unsigned skip;
        int error;
    } paired[] = {{F_FERROR, 0, EIO}, {F_FSTAT, 1, EACCES}, {F_FSTAT, 2, EIO}};
    for (size_t i = 0; i < sizeof(paired) / sizeof(*paired); ++i) {
        inject(paired[i].id, paired[i].skip, paired[i].error);
        inject(F_FCLOSE, 0, ENOSPC);
        string_t *digest = file_sha256(source);
        int error = file_last_error(source);
        bool both_injected = !faults[paired[i].id].armed && !faults[F_FCLOSE].armed;
        reset_faults();
        bool rejected = digest == NULL;
        string_free(digest);
        ASSERT_TRUE(both_injected);
        ASSERT_TRUE(rejected);
        ASSERT_EQ_INT(error, paired[i].error);
        ASSERT_TRUE(!file_is_open(source));
    }
    ASSERT_TRUE(file_delete(source));
    file_free(source);
}

/* Target lookup errors do not hide the link; allocation failure remains a listing error. */
void test_file_symlink_target_failures(void)
{
    file_t *source = file_new_cstr(test_case_temp_path("target"));
    file_t *link = file_new_cstr(test_case_temp_path("link"));
    ASSERT_TRUE(file_write_all_bytes(source, "data", 4));
    ASSERT_TRUE(file_create_symlink(source, link));
    inject(F_STATX, 1, EACCES);
    file_info_t *info = file_get_info(link);
    reset_faults();
    ASSERT_NOT_NULL(info);
    ASSERT_EQ_INT(file_info_type(info), FILE_TYPE_SYMLINK);
    ASSERT_TRUE(file_info_target_info(info) == NULL);
    ASSERT_EQ_INT(file_info_target_error(info), EACCES);
    ASSERT_EQ_INT(file_last_error(link), 0);
    file_info_free(info);
    const enum fault_id ids[] = {F_MALLOC, F_CALLOC, F_STRDUP};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        for (unsigned skip = 0; skip < 5; ++skip) {
            inject(ids[i], skip, ENOMEM);
            info = file_get_info(link);
            int error = file_last_error(link);
            reset_faults();
            if (info)
                ASSERT_NOT_NULL(file_info_target_info(info));
            else
                ASSERT_EQ_INT(error, ENOMEM);
            file_info_free(info);
        }
    }
    ASSERT_TRUE(file_delete(link));
    ASSERT_TRUE(file_delete(source));
    file_free(link);
    file_free(source);
}


/* Sweep staging and decoder allocations without depending on library allocator internals. */
void test_file_transform_allocation_failures(void)
{
    file_t *source = file_new_cstr(test_case_temp_path("source"));
    file_t *compressed = file_new_cstr(test_case_temp_path("compressed"));
    file_t *encrypted = file_new_cstr(test_case_temp_path("encrypted"));
    file_t *destination = file_new_cstr(test_case_temp_path("destination"));
    file_key_t *key = file_key_generate();
    ASSERT_NOT_NULL(key);
    ASSERT_TRUE(file_write_all_bytes(source, "payload", 7));
    ASSERT_TRUE(file_compress_zstd(source, compressed, false));
    ASSERT_TRUE(file_encrypt(source, encrypted, key, true, false));
    const enum fault_id ids[] = {F_MALLOC, F_CALLOC, F_STRDUP};
    for (size_t kind = 0; kind < sizeof(ids) / sizeof(ids[0]); ++kind) {
        for (unsigned skip = 0; skip < 12; ++skip) {
            for (unsigned operation = 0; operation < 4; ++operation) {
                ASSERT_TRUE(file_write_all_bytes(destination, "old", 3));
                inject(ids[kind], skip, ENOMEM);
                bool ok;
                file_t *input = operation == 1 ? compressed : operation == 3 ? encrypted : source;
                if (operation == 0)
                    ok = file_compress_zstd(input, destination, true);
                else if (operation == 1)
                    ok = file_decompress_zstd(input, destination, 7, true);
                else if (operation == 2)
                    ok = file_encrypt(input, destination, key, true, true);
                else
                    ok = file_decrypt(input, destination, key, 7, true);
                int error = file_last_error(input);
                reset_faults();
                if (!ok) {
                    ASSERT_EQ_INT(error, ENOMEM);
                    expect_text(destination, "old");
                }
                ASSERT_TRUE(!file_is_open(input));
                ASSERT_TRUE(!file_is_open(destination));
            }
        }
    }
    file_key_free(key);
    file_free(source);
    file_free(compressed);
    file_free(encrypted);
    file_free(destination);
}

void test_file_transform_staging_failures(void)
{
    file_t *source = file_new_cstr(test_case_temp_path("source"));
    file_t *destination = file_new_cstr(test_case_temp_path("destination"));
    ASSERT_TRUE(file_write_all_bytes(source, "payload", 7));
    static const struct { enum fault_id id; unsigned skip; int error; } cases[] = {
        {F_FSTAT, 1, EIO}, {F_FDOPEN, 1, EMFILE}, {F_FCLOSE, 0, EIO},
        {F_FCLOSE, 1, ENOSPC}, {F_RENAME, 0, EACCES}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        ASSERT_TRUE(file_write_all_bytes(destination, "old", 3));
        inject(cases[i].id, cases[i].skip, cases[i].error);
        bool ok = file_compress_zstd(source, destination, true);
        int error = file_last_error(source);
        reset_faults();
        ASSERT_TRUE(!ok);
        ASSERT_EQ_INT(error, cases[i].error);
        expect_text(destination, "old");
    }
    ASSERT_TRUE(file_delete(destination));
    inject(F_RENAMEAT2, 0, EACCES);
    bool ok = file_compress_zstd(source, destination, false);
    int error = file_last_error(source);
    reset_faults();
    ASSERT_TRUE(!ok);
    ASSERT_EQ_INT(error, EACCES);
    ASSERT_TRUE(!file_exists(destination));
    file_free(source);
    file_free(destination);
}


void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
char *__real_strdup(const char *);
ssize_t __real_read(int, void *, size_t);
ssize_t __real_write(int, const void *, size_t);
int __real_rename(const char *, const char *);
int __real_renameat2(int, const char *, int, const char *, unsigned);
int __real_unlink(const char *);
int __real_fsync(int);
int __real_fdatasync(int);
int __real_fchownat(int, const char *, uid_t, gid_t, int);
FILE *__real_fdopen(int, const char *);
DIR *__real_fdopendir(int);
struct dirent *__real_readdir(DIR *);
int __real_statx(int, const char *, int, unsigned, struct statx *);
int __real_fstat(int, struct stat *);
int __real_fclose(FILE *);
int __real_ferror(FILE *);
int __real_close(int);
int __real_fchmod(int, mode_t);
off_t __real_ftello(FILE *);
int __real_fseeko(FILE *, off_t, int);

FILE *__wrap_fdopen(int fd, const char *mode) { return hit(F_FDOPEN) ? NULL : __real_fdopen(fd, mode); }
DIR *__wrap_fdopendir(int fd) { return hit(F_FDOPENDIR) ? NULL : __real_fdopendir(fd); }
struct dirent *__wrap_readdir(DIR *dir) { return hit(F_READDIR) ? NULL : __real_readdir(dir); }
int __wrap_statx(int fd, const char *p, int flags, unsigned mask, struct statx *status)
{
    if (hit(F_STATX)) {
        int error = errno;
        if (error > 0)
            return -1;
        int rc = __real_statx(fd, p, flags, mask, status);
        if (!rc)
            status->stx_mask &= error == 0 ? ~STATX_SIZE : ~STATX_BTIME;
        return rc;
    }
    return __real_statx(fd, p, flags, mask, status);
}
int __wrap_fstat(int fd, struct stat *status)
{
    if (!hit(F_FSTAT))
        return __real_fstat(fd, status);
    if (errno)
        return -1;
    int rc = __real_fstat(fd, status);
    if (!rc)
        ++status->st_size;
    return rc;
}
int __wrap_ferror(FILE *stream) { return hit(F_FERROR) ? 1 : __real_ferror(stream); }
int __wrap_fclose(FILE *stream)
{
    if (hit(F_FCLOSE)) {
        int error = errno;
        __real_fclose(stream);
        errno = error;
        return EOF;
    }
    return __real_fclose(stream);
}
int __wrap_close(int fd)
{
    if (hit(F_CLOSE)) {
        int error = errno;
        __real_close(fd);
        errno = error;
        return -1;
    }
    return __real_close(fd);
}
int __wrap_fchmod(int fd, mode_t mode) { return hit(F_FCHMOD) ? -1 : __real_fchmod(fd, mode); }
off_t __wrap_ftello(FILE *s) { return hit(F_FTELLO) ? -1 : __real_ftello(s); }
int __wrap_fseeko(FILE *s, off_t n, int origin) { return hit(F_FSEEKO) ? -1 : __real_fseeko(s, n, origin); }

void *__wrap_malloc(size_t n) { return hit(F_MALLOC) ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t size) { return hit(F_CALLOC) ? NULL : __real_calloc(n, size); }
void *__wrap_realloc(void *p, size_t n) { return hit(F_REALLOC) ? NULL : __real_realloc(p, n); }
char *__wrap_strdup(const char *s) { return hit(F_STRDUP) ? NULL : __real_strdup(s); }
ssize_t __wrap_read(int fd, void *p, size_t n)
{
    if (hit(F_READ))
        return errno == 0 ? 0 : -1;
    return __real_read(fd, p, n);
}
ssize_t __wrap_write(int fd, const void *p, size_t n)
{
    if (hit(F_WRITE))
        return errno == 0 ? 0 : errno == -1 ? __real_write(fd, p, n ? 1 : 0) : -1;
    return __real_write(fd, p, n);
}
int __wrap_rename(const char *a, const char *b) { return hit(F_RENAME) ? -1 : __real_rename(a, b); }
int __wrap_renameat2(int a, const char *b, int c, const char *d, unsigned flags)
{
    return hit(F_RENAMEAT2) ? -1 : __real_renameat2(a, b, c, d, flags);
}
int __wrap_unlink(const char *p) { return hit(F_UNLINK) ? -1 : __real_unlink(p); }
int __wrap_fsync(int fd) { return hit(F_FSYNC) ? -1 : __real_fsync(fd); }
int __wrap_fdatasync(int fd) { return hit(F_FDATASYNC) ? -1 : __real_fdatasync(fd); }
int __wrap_fchownat(int fd, const char *p, uid_t uid, gid_t gid, int flags)
{
    return hit(F_CHOWN) ? -1 : __real_fchownat(fd, p, uid, gid, flags);
}

static void expect_text(file_t *file, const char *expected)
{
    string_t *text = file_read_all_text(file);
    ASSERT_NOT_NULL(text);
    TEST_ASSERT_STR_EQ(string_c_str(text), expected);
    string_free(text);
}

void test_file_allocation_failure_sweeps(void)
{
    file_t *file = file_new_cstr(test_case_temp_path("lines"));
    file_t *directory = file_new_cstr(test_case_temp_dir());
    ASSERT_TRUE(file_write_all_bytes(file, "one\ntwo\n", 8));
    const enum fault_id ids[] = {F_MALLOC, F_CALLOC, F_REALLOC, F_STRDUP};
    for (size_t op = 0; op < sizeof(ids) / sizeof(ids[0]); ++op) {
        for (unsigned allocation = 0; allocation < 32; ++allocation) {
            inject(ids[op], allocation, ENOMEM);
            array_t *lines = file_read_all_lines(file);
            int error = file_last_error(file);
            reset_faults();
            if (lines)
                ASSERT_EQ_LONG(array_size(lines), 2);
            else
                ASSERT_EQ_INT(error, ENOMEM);
            array_destroy(lines);
            ASSERT_TRUE(!file_is_open(file));

            inject(ids[op], allocation, ENOMEM);
            string_t *text = file_read_all_text(file);
            error = file_last_error(file);
            reset_faults();
            if (text)
                TEST_ASSERT_STR_EQ(string_c_str(text), "one\ntwo\n");
            else
                ASSERT_EQ_INT(error, ENOMEM);
            string_free(text);
            ASSERT_TRUE(!file_is_open(file));

            inject(ids[op], allocation, ENOMEM);
            array_t *entries = file_list_directory(directory);
            error = file_last_error(directory);
            reset_faults();
            if (entries)
                ASSERT_EQ_LONG(array_size(entries), 1);
            else
                ASSERT_EQ_INT(error, ENOMEM);
            array_destroy(entries);
            ASSERT_TRUE(!file_is_open(directory));
        }
    }
    file_free(directory);
    file_free(file);
}

void test_file_stream_and_metadata_failures(void)
{
    file_t *file = file_new_cstr(test_case_temp_path("data"));
    file_t *directory = file_new_cstr(test_case_temp_dir());
    ASSERT_TRUE(file_write_all_bytes(file, "abc", 3));
    const enum fault_id open_faults[] = {F_FDOPEN, F_FSTAT};
    for (size_t i = 0; i < sizeof(open_faults) / sizeof(open_faults[0]); ++i) {
        inject(open_faults[i], 0, EIO);
        bool ok = file_open_read(file);
        reset_faults();
        ASSERT_TRUE(!ok && !file_is_open(file));
        ASSERT_EQ_INT(file_last_error(file), EIO);
    }
    inject(F_FDOPENDIR, 0, EMFILE);
    bool ok = file_open_directory(directory);
    reset_faults();
    ASSERT_TRUE(!ok && !file_is_open(directory));
    inject(F_READDIR, 0, EIO);
    array_t *entries = file_list_directory(directory);
    reset_faults();
    ASSERT_TRUE(entries == NULL && !file_is_open(directory));
    ASSERT_EQ_INT(file_last_error(directory), EIO);
    inject(F_STATX, 0, EIO);
    file_info_t *info = file_get_info(file);
    reset_faults();
    ASSERT_TRUE(info == NULL);
    ASSERT_EQ_INT(file_last_error(file), EIO);
    inject(F_STATX, 0, 0);
    info = file_get_info(file);
    reset_faults();
    ASSERT_TRUE(info == NULL);
    ASSERT_EQ_INT(file_last_error(file), ENOTSUP);
    inject(F_STATX, 0, -1);
    info = file_get_info(file);
    reset_faults();
    ASSERT_NOT_NULL(info);
    int64_t seconds;
    long nanoseconds;
    ASSERT_TRUE(!file_info_creation_time(info, &seconds, &nanoseconds));
    ASSERT_EQ_INT(errno, ENOTSUP);
    ASSERT_TRUE(!file_info_last_write_time(info, NULL, &nanoseconds));
    ASSERT_TRUE(!file_info_last_write_time(info, &seconds, NULL));
    file_info_free(info);

    ASSERT_TRUE(file_open(file, FILE_MODE_OPEN, FILE_ACCESS_READ_WRITE));
    inject(F_FTELLO, 0, EIO);
    ok = file_tell(file, &seconds);
    reset_faults();
    ASSERT_TRUE(!ok);
    inject(F_FTELLO, 0, EIO);
    string_t *line = NULL;
    ok = file_read_line(file, &line);
    reset_faults();
    ASSERT_TRUE(!ok && line == NULL);
    /* Changing I/O direction must propagate a positioning failure. */
    inject(F_FSEEKO, 0, EIO);
    size_t count;
    ok = file_write(file, "x", 1, &count);
    reset_faults();
    ASSERT_TRUE(!ok);
    inject(F_FCLOSE, 0, EIO);
    ok = file_close(file);
    reset_faults();
    ASSERT_TRUE(!ok && !file_is_open(file));
    inject(F_FCLOSE, 0, EIO);
    array_t *bytes = file_read_all_bytes(file);
    reset_faults();
    ASSERT_TRUE(bytes == NULL && !file_is_open(file));
    ASSERT_EQ_INT(file_last_error(file), EIO);
    file_free(directory);
    file_free(file);
}

void test_file_copy_injected_io_failures(void)
{
    file_t *source = file_new_cstr(test_case_temp_path("source"));
    file_t *destination = file_new_cstr(test_case_temp_path("destination"));
    ASSERT_TRUE(file_write_all_bytes(source, "contents", 8));
    ASSERT_TRUE(file_write_all_bytes(destination, "old", 3));
    const struct { enum fault_id id; int error; } cases[] = {
        {F_READ, EIO}, {F_WRITE, ENOSPC}, {F_WRITE, 0}, {F_RENAME, EACCES}, {F_MALLOC, ENOMEM},
        {F_FSTAT, EIO}, {F_FCHMOD, EPERM}, {F_CLOSE, EIO}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        inject(cases[i].id, 0, cases[i].error);
        bool ok = file_copy(source, destination, true);
        int error = file_last_error(source);
        reset_faults();
        ASSERT_TRUE(!ok);
        ASSERT_EQ_INT(error, cases[i].error ? cases[i].error : EIO);
        expect_text(destination, "old");
        expect_text(source, "contents");
    }
    inject(F_READ, 0, EINTR);
    inject(F_WRITE, 0, EINTR);
    bool ok = file_copy(source, destination, true);
    reset_faults();
    ASSERT_TRUE(ok);
    expect_text(destination, "contents");
    inject(F_WRITE, 0, -1);
    ok = file_copy(source, destination, true);
    reset_faults();
    ASSERT_TRUE(ok);
    expect_text(destination, "contents");
    file_free(destination);
    file_free(source);
}

void test_file_move_cross_device_paths(void)
{
    file_t *source = file_new_cstr(test_case_temp_path("source"));
    file_t *destination = file_new_cstr(test_case_temp_path("destination"));
    ASSERT_TRUE(file_write_all_bytes(source, "moved", 5));
    inject(F_RENAMEAT2, 0, EXDEV);
    bool ok = file_move(source, destination, false);
    reset_faults();
    ASSERT_TRUE(ok);
    ASSERT_TRUE(!file_exists(source));
    expect_text(destination, "moved");
    ASSERT_TRUE(file_write_all_bytes(source, "again", 5));
    inject(F_RENAME, 0, EXDEV);
    inject(F_WRITE, 0, ENOSPC);
    ok = file_move(source, destination, true);
    int error = file_last_error(source);
    reset_faults();
    ASSERT_TRUE(!ok);
    ASSERT_EQ_INT(error, ENOSPC);
    expect_text(source, "again");
    expect_text(destination, "moved");
    inject(F_RENAME, 0, EXDEV);
    inject(F_UNLINK, 1, EACCES); /* Permit staging cleanup, fail source removal. */
    ok = file_move(source, destination, true);
    error = file_last_error(source);
    reset_faults();
    ASSERT_TRUE(!ok);
    ASSERT_EQ_INT(error, EACCES);
    expect_text(source, "again");
    expect_text(destination, "again");
    ASSERT_TRUE(file_delete(source));
    ASSERT_TRUE(file_delete(destination));
    ASSERT_TRUE(file_create_directory(source, 0700, false));
    inject(F_RENAMEAT2, 0, EXDEV);
    ok = file_move(source, destination, false);
    error = file_last_error(source);
    reset_faults();
    ASSERT_TRUE(!ok);
    ASSERT_EQ_INT(error, EXDEV);
    ASSERT_TRUE(file_remove_directory(source));
    file_free(destination);
    file_free(source);
}

static void test_file_temp_directory_allocation_failures(void)
{
    file_t *parent = file_new_cstr(test_case_temp_path("temporary-fault-parent"));
    ASSERT_NOT_NULL(parent);
    string_t *parent_path = string_new_with(file_path(parent));
    ASSERT_NOT_NULL(parent_path);
    ASSERT_TRUE(file_create_directory(parent, 0700, false));
    const enum fault_id allocations[] = {F_CALLOC, F_STRDUP, F_MALLOC};
    for (size_t i = 0; i < sizeof(allocations) / sizeof(*allocations); ++i) {
        inject(allocations[i], 0, ENOMEM);
        file_t *temporary = file_create_temp_directory(parent_path);
        int error = errno;
        bool injected = !faults[allocations[i]].armed;
        reset_faults();
        bool rejected = temporary == NULL;
        if (temporary)
            file_remove_directory(temporary);
        file_free(temporary);
        ASSERT_TRUE(injected);
        ASSERT_TRUE(rejected);
        ASSERT_EQ_INT(error, ENOMEM);
        ASSERT_TRUE(file_open_directory(parent));
        file_info_t *entry = NULL;
        bool listed = file_read_directory(parent, &entry);
        bool empty = entry == NULL;
        file_info_free(entry);
        bool closed = file_close(parent);
        ASSERT_TRUE(listed && empty && closed);
    }
    ASSERT_TRUE(file_remove_directory(parent));
    file_t *temporary = file_create_temp_directory(parent_path);
    int error = errno;
    bool rejected = temporary == NULL;
    file_free(temporary);
    ASSERT_TRUE(rejected);
    ASSERT_EQ_INT(error, ENOENT);
    ASSERT_TRUE(file_write_all_bytes(parent, "not a directory", 15));
    temporary = file_create_temp_directory(parent_path);
    error = errno;
    rejected = temporary == NULL;
    file_free(temporary);
    ASSERT_TRUE(rejected);
    ASSERT_EQ_INT(error, ENOTDIR);
    ASSERT_TRUE(file_delete(parent));
    string_free(parent_path);
    file_free(parent);
}

void test_file_allocation_and_sync_failures(void)
{
    test_file_temp_directory_allocation_failures();
    inject(F_CALLOC, 0, ENOMEM);
    file_t *failed = file_new_cstr("unused");
    reset_faults();
    ASSERT_TRUE(failed == NULL);
    inject(F_STRDUP, 0, ENOMEM);
    failed = file_new_cstr("unused");
    reset_faults();
    ASSERT_TRUE(failed == NULL);
    file_t *file = file_new_cstr(test_case_temp_path("data"));
    ASSERT_TRUE(file_write_all_bytes(file, "value", 5));
    inject(F_CALLOC, 0, ENOMEM);
    file_info_t *info = file_get_info(file);
    reset_faults();
    ASSERT_TRUE(info == NULL);
    ASSERT_EQ_INT(file_last_error(file), ENOMEM);
    inject(F_STRDUP, 0, ENOMEM);
    info = file_get_info(file);
    reset_faults();
    ASSERT_TRUE(info == NULL);
    ASSERT_EQ_INT(file_last_error(file), ENOMEM);
    inject(F_CHOWN, 0, EPERM);
    bool ok = file_chown(file, 12345, 12345);
    reset_faults();
    ASSERT_TRUE(!ok);
    ASSERT_EQ_INT(file_last_error(file), EPERM);
    ASSERT_TRUE(file_open_read(file));
    inject(F_FSYNC, 0, EIO);
    ok = file_sync(file, false);
    reset_faults();
    ASSERT_TRUE(!ok);
    ASSERT_EQ_INT(file_last_error(file), EIO);
    inject(F_FDATASYNC, 0, EIO);
    ok = file_sync(file, true);
    reset_faults();
    ASSERT_TRUE(!ok);
    ASSERT_TRUE(file_close(file));
    expect_text(file, "value");
    file_free(file);
}
