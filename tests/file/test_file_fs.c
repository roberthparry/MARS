#include "file.h"
#include "array.h"
#include "test_harness.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static file_t *child_path(file_t *parent, const char *name)
{
    char *path = NULL;
    if (asprintf(&path, "%s/%s", file_path(parent), name) < 0)
        return NULL;
    file_t *child = file_new_cstr(path);
    free(path);
    return child;
}

/* Symlink snapshots retain both identities, including failed target resolution. */
void test_file_listing_symlink_targets(void)
{
    file_t *directory = file_new_cstr(test_case_temp_path("links"));
    file_t *moved = file_new_cstr(test_case_temp_path("moved-links"));
    ASSERT_TRUE(file_create_directory(directory, 0700, false));
    file_t *data = child_path(directory, "data");
    file_t *subdir = child_path(directory, "subdir");
    ASSERT_TRUE(file_write_all_bytes(data, "abc", 3));
    ASSERT_TRUE(file_chmod(data, 0444));
    ASSERT_TRUE(file_create_directory(subdir, 0700, false));
    const char *names[] = {".file-link", "dir-link", "chain", "dangling", "loop"};
    const char *targets[] = {"data", "subdir", "dir-link", "missing", "loop"};
    for (size_t i = 0; i < 5; ++i) {
        file_t *target = file_new_cstr(targets[i]);
        file_t *link = child_path(directory, names[i]);
        ASSERT_TRUE(file_create_symlink(target, link));
        file_info_t *info = file_get_info(link);
        ASSERT_NOT_NULL(info);
        TEST_ASSERT_STR_EQ(file_info_link_target(info), targets[i]);
        file_info_free(info);
        file_free(link);
        file_free(target);
    }
    ASSERT_TRUE(file_info_link_target(NULL) == NULL);
    ASSERT_TRUE(file_info_target_info(NULL) == NULL);
    ASSERT_EQ_INT(file_info_target_error(NULL), EINVAL);
    array_t *entries = file_list_directory(directory);
    ASSERT_NOT_NULL(entries);
    ASSERT_EQ_LONG(array_size(entries), 7);
    unsigned seen = 0;
    for (size_t i = 0; i < array_size(entries); ++i) {
        file_info_t *info = *(file_info_t **)array_get(entries, i);
        const char *name = file_info_name(info);
        const file_info_t *target = file_info_target_info(info);
        if (file_info_type(info) != FILE_TYPE_SYMLINK) {
            ASSERT_TRUE(target == NULL);
            ASSERT_TRUE(file_info_link_target(info) == NULL);
            ASSERT_EQ_INT(file_info_target_error(info), 0);
        } else if (!strcmp(name, ".file-link")) {
            ASSERT_NOT_NULL(target);
            ASSERT_EQ_INT(file_info_type(target), FILE_TYPE_REGULAR);
            ASSERT_EQ_LONG(file_info_size(target), 3);
            ASSERT_EQ_INT(file_info_permissions(target), 0444);
            ASSERT_TRUE(file_info_attributes(target) & FILE_ATTRIBUTE_READ_ONLY);
            ASSERT_TRUE(file_info_attributes(target) & FILE_ATTRIBUTE_HIDDEN);
            ASSERT_EQ_INT(file_info_target_error(info), 0);
            seen |= 1;
        } else if (!strcmp(name, "dir-link") || !strcmp(name, "chain")) {
            ASSERT_NOT_NULL(target);
            ASSERT_EQ_INT(file_info_type(target), FILE_TYPE_DIRECTORY);
            ASSERT_EQ_INT(file_info_permissions(target), 0700);
            ASSERT_EQ_INT(file_info_target_error(info), 0);
            seen |= !strcmp(name, "chain") ? 4 : 2;
        } else {
            ASSERT_TRUE(target == NULL);
            ASSERT_EQ_INT(file_info_target_error(info), !strcmp(name, "loop") ? ELOOP : ENOENT);
            seen |= !strcmp(name, "loop") ? 16 : 8;
        }
    }
    ASSERT_EQ_INT(seen, 31);
    ASSERT_TRUE(file_open_directory(directory));
    file_t *old_path = file_new_cstr(file_path(directory));
    ASSERT_TRUE(file_move(old_path, moved, false));
    file_info_t *entry = NULL;
    size_t count = 0;
    while (file_read_directory(directory, &entry) && entry) {
        if (file_info_type(entry) == FILE_TYPE_SYMLINK) {
            ASSERT_NOT_NULL(file_info_link_target(entry));
            if (!strcmp(file_info_name(entry), "dir-link"))
                ASSERT_EQ_INT(file_info_type(file_info_target_info(entry)), FILE_TYPE_DIRECTORY);
        }
        ++count;
        file_info_free(entry);
    }
    ASSERT_EQ_INT(file_last_error(directory), 0);
    ASSERT_EQ_LONG(count, 7);
    ASSERT_TRUE(file_close(directory));
    /* Earlier snapshots remain valid after closure, rename and target removal. */
    file_t *removed = child_path(moved, "data");
    ASSERT_TRUE(file_delete(removed));
    for (size_t i = 0; i < array_size(entries); ++i) {
        file_info_t *info = *(file_info_t **)array_get(entries, i);
        if (!strcmp(file_info_name(info), ".file-link"))
            ASSERT_EQ_LONG(file_info_size(file_info_target_info(info)), 3);
    }
    array_destroy(entries);
    file_free(removed);
    file_free(old_path);
    file_free(data);
    file_free(subdir);
    file_free(moved);
    file_free(directory);
}

/* README example: inspect a directory symlink and its target without losing link identity. */
void example_file_symlink_target(void)
{
    file_t *directory = file_new_cstr(test_case_temp_path("listing"));
    file_t *target_directory = child_path(directory, "documents");
    file_t *link = child_path(directory, "shortcut");
    file_t *relative_target = file_new_cstr("documents");
    ASSERT_TRUE(file_create_directory(directory, 0700, false));
    ASSERT_TRUE(file_create_directory(target_directory, 0700, false));
    ASSERT_TRUE(file_create_symlink(relative_target, link));
    array_t *entries = file_list_directory(directory);
    ASSERT_NOT_NULL(entries);
    for (size_t i = 0; i < array_size(entries); ++i) {
        const file_info_t *entry = *(file_info_t **)array_get(entries, i);
        if (file_info_type(entry) != FILE_TYPE_SYMLINK)
            continue;
        const file_info_t *target = file_info_target_info(entry);
        ASSERT_NOT_NULL(target);
        ASSERT_EQ_INT(file_info_target_error(entry), 0);
        ASSERT_EQ_INT(file_info_type(target), FILE_TYPE_DIRECTORY);
        printf("%s -> %s: %s\n", file_info_name(entry), file_info_link_target(entry),
               file_info_type(target) == FILE_TYPE_DIRECTORY ? "directory" : "not a directory");
    }
    array_destroy(entries);
    ASSERT_TRUE(file_delete(link));
    ASSERT_TRUE(file_remove_directory(target_directory));
    ASSERT_TRUE(file_remove_directory(directory));
    file_free(relative_target);
    file_free(link);
    file_free(target_directory);
    file_free(directory);
}

/* Listing snapshots own their names and exclude dot entries, but include hidden files and dangling links. */
void test_file_directory_listing_and_removal(void)
{
    file_t *directory = file_new_cstr(test_case_temp_path("directory"));
    ASSERT_NOT_NULL(directory);
    ASSERT_TRUE(file_create_directory(directory, 0700, false));
    ASSERT_TRUE(file_create_directory(directory, 0700, false));
    ASSERT_TRUE(!file_create_directory(directory, 010000, false));
    file_t *nested = child_path(directory, "parent/child");
    file_t *parent = child_path(directory, "parent");
    file_t *hidden = child_path(directory, ".hidden");
    file_t *link = child_path(directory, "dangling");
    file_t *absent = file_new_cstr("not-present");
    ASSERT_TRUE(file_create_directory(nested, 0700, true));
    ASSERT_TRUE(file_write_all_bytes(hidden, "abc", 3));
    ASSERT_TRUE(file_create_symlink(absent, link));
    ASSERT_TRUE(file_exists(link));
    ASSERT_TRUE(!file_remove_directory(directory));
    ASSERT_TRUE(!file_delete(directory));
    ASSERT_EQ_INT(file_last_error(directory), EISDIR);
    array_t *entries = file_list_directory(directory);
    ASSERT_NOT_NULL(entries);
    ASSERT_EQ_LONG(array_size(entries), 3);
    unsigned seen = 0;
    for (size_t i = 0; i < array_size(entries); ++i) {
        file_info_t *info = *(file_info_t **)array_get(entries, i);
        const char *name = file_info_name(info);
        if (strcmp(name, "parent") == 0) {
            ASSERT_EQ_INT(file_info_type(info), FILE_TYPE_DIRECTORY);
            seen |= 1;
        } else if (strcmp(name, ".hidden") == 0) {
            ASSERT_EQ_INT(file_info_type(info), FILE_TYPE_REGULAR);
            ASSERT_EQ_LONG(file_info_size(info), 3);
            ASSERT_TRUE(file_info_attributes(info) & FILE_ATTRIBUTE_HIDDEN);
            seen |= 2;
        } else if (strcmp(name, "dangling") == 0) {
            ASSERT_EQ_INT(file_info_type(info), FILE_TYPE_SYMLINK);
            seen |= 4;
        } else {
            ASSERT_TRUE(false);
        }
    }
    ASSERT_EQ_INT(seen, 7);
    ASSERT_TRUE(file_open_directory(directory));
    ASSERT_TRUE(!file_open_read(directory));
    ASSERT_EQ_INT(file_last_error(directory), EBUSY);
    ASSERT_TRUE(!file_remove_directory(directory));
    ASSERT_TRUE(file_sync(directory, false));
    file_info_t *entry = NULL;
    size_t count = 0;
    while (file_read_directory(directory, &entry) && entry) {
        ++count;
        file_info_free(entry);
    }
    ASSERT_EQ_INT(file_last_error(directory), 0);
    ASSERT_EQ_LONG(count, 3);
    ASSERT_TRUE(file_close(directory));
    ASSERT_TRUE(!file_read_directory(directory, &entry));
    ASSERT_EQ_INT(file_last_error(directory), EBADF);
    ASSERT_TRUE(file_delete(link));
    ASSERT_TRUE(file_delete(hidden));
    ASSERT_TRUE(file_remove_directory(nested));
    ASSERT_TRUE(file_remove_directory(parent));
    ASSERT_TRUE(file_remove_directory(directory));
    ASSERT_EQ_LONG(array_size(entries), 3);
    array_destroy(entries);
    file_free(absent);
    file_free(link);
    file_free(hidden);
    file_free(parent);
    file_free(nested);
    file_free(directory);
}

/* Exercise owner-preserving chown without requiring root, exact chmod bits and nanosecond timestamps. */
void test_file_chmod_chown_times_and_access(void)
{
    file_t *file = file_new_cstr(test_case_temp_path("metadata"));
    ASSERT_TRUE(file_write_all_bytes(file, "data", 4));
    ASSERT_TRUE(file_chmod(file, 0640));
    ASSERT_TRUE(file_chown(file, geteuid(), getegid()));
    ASSERT_TRUE(file_chown(file, -1, -1));
    ASSERT_TRUE(!file_chown(file, -2, -1));
    ASSERT_EQ_INT(file_last_error(file), EINVAL);
    ASSERT_TRUE(!file_chown(file, UINT32_MAX, -1));
    ASSERT_TRUE(!file_chmod(file, 010000));
    ASSERT_TRUE(file_set_times(file, 123456789, 123456789, 234567890, 987654321));
    ASSERT_TRUE(!file_set_times(file, 0, -1, 0, 0));
    ASSERT_TRUE(!file_set_times(file, 0, 0, 0, 1000000000));
    file_info_t *info = file_get_info(file);
    ASSERT_NOT_NULL(info);
    ASSERT_EQ_INT(file_info_permissions(info), 0640);
    ASSERT_EQ_LONG(file_info_owner(info), geteuid());
    ASSERT_EQ_LONG(file_info_group(info), getegid());
    ASSERT_EQ_LONG(file_info_link_count(info), 1);
    int64_t seconds;
    long nanoseconds;
    ASSERT_TRUE(file_info_last_access_time(info, &seconds, &nanoseconds));
    ASSERT_EQ_LONG(seconds, 123456789);
    ASSERT_EQ_LONG(nanoseconds, 123456789);
    ASSERT_TRUE(file_info_last_write_time(info, &seconds, &nanoseconds));
    ASSERT_EQ_LONG(seconds, 234567890);
    ASSERT_EQ_LONG(nanoseconds, 987654321);
    ASSERT_TRUE(file_info_status_change_time(info, &seconds, &nanoseconds));
    ASSERT_TRUE(seconds > 0);
    file_info_free(info);
    ASSERT_TRUE(file_check_access(file, true, true, false));
    ASSERT_TRUE(!file_check_access(file, false, false, true));
    ASSERT_TRUE(file_chmod(file, 0440));
    if (geteuid() != 0)
        ASSERT_TRUE(!file_check_access(file, false, true, false));
    ASSERT_TRUE(file_chmod(file, 0640));
    ASSERT_TRUE(file_delete(file));
    ASSERT_TRUE(!file_check_access(file, false, false, false));
    ASSERT_EQ_INT(file_last_error(file), ENOENT);
    file_free(file);
}

/* Link operations preserve target bytes, unlink the link rather than its target, and move directories safely. */
void test_file_links_and_directory_moves(void)
{
    file_t *source = file_new_cstr(test_case_temp_path("source"));
    file_t *hard = file_new_cstr(test_case_temp_path("hard"));
    file_t *symbolic = file_new_cstr(test_case_temp_path("symbolic"));
    file_t *moved = file_new_cstr(test_case_temp_path("moved"));
    ASSERT_TRUE(file_write_all_bytes(source, "value", 5));
    ASSERT_TRUE(file_create_hard_link(source, hard));
    ASSERT_TRUE(!file_create_hard_link(source, hard));
    file_info_t *info = file_get_info(source);
    ASSERT_EQ_LONG(file_info_link_count(info), 2);
    file_info_free(info);
    ASSERT_TRUE(file_create_symlink(source, symbolic));
    ASSERT_TRUE(!file_create_symlink(source, symbolic));
    char *target = NULL;
    ASSERT_TRUE(file_read_link(symbolic, &target));
    TEST_ASSERT_STR_EQ(target, file_path(source));
    free(target);
    ASSERT_TRUE(!file_chmod(symbolic, 0600));
    ASSERT_EQ_INT(file_last_error(symbolic), ELOOP);
    ASSERT_TRUE(file_chown(symbolic, -1, -1));
    ASSERT_TRUE(file_set_times(symbolic, 100, 0, 100, 0));
    file_t *resolved = file_resolve(symbolic);
    ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_STR_EQ(file_path(resolved), file_path(source));
    file_free(resolved);
    ASSERT_TRUE(file_move(symbolic, moved, false));
    ASSERT_TRUE(!file_exists(symbolic));
    ASSERT_TRUE(file_read_link(moved, &target));
    TEST_ASSERT_STR_EQ(target, file_path(source));
    free(target);
    ASSERT_TRUE(file_delete(moved));
    ASSERT_TRUE(file_exists(source));
    ASSERT_TRUE(!file_read_link(source, &target));
    ASSERT_TRUE(target == NULL);
    ASSERT_TRUE(file_delete(hard));
    ASSERT_TRUE(file_delete(source));
    ASSERT_TRUE(file_resolve(source) == NULL);

    ASSERT_TRUE(file_create_directory(source, 0700, false));
    file_t *child = child_path(source, "child");
    ASSERT_TRUE(file_write_all_bytes(child, "inside", 6));
    ASSERT_TRUE(file_move(source, moved, false));
    file_free(child);
    child = child_path(moved, "child");
    ASSERT_TRUE(file_exists(child));
    ASSERT_TRUE(!file_exists(source));
    ASSERT_TRUE(file_delete(child));
    ASSERT_TRUE(file_remove_directory(moved));
    file_free(child);
    file_free(moved);
    file_free(symbolic);
    file_free(hard);
    file_free(source);
}

/* Length changes preserve positions and report invalid modes; sync flushes both data and metadata paths. */
void test_file_truncate_and_sync(void)
{
    file_t *file = file_new_cstr(test_case_temp_path("length"));
    ASSERT_TRUE(!file_truncate(file, -1));
    ASSERT_TRUE(!file_sync(file, false));
    ASSERT_TRUE(file_write_all_bytes(file, "abcdef", 6));
    ASSERT_TRUE(file_truncate(file, 3));
    ASSERT_TRUE(file_truncate(file, 6));
    array_t *bytes = file_read_all_bytes(file);
    ASSERT_NOT_NULL(bytes);
    ASSERT_EQ_LONG(array_size(bytes), 6);
    ASSERT_TRUE(memcmp(array_get(bytes, 0), "abc\0\0\0", 6) == 0);
    array_destroy(bytes);
    ASSERT_TRUE(file_open(file, FILE_MODE_OPEN, FILE_ACCESS_READ_WRITE));
    ASSERT_TRUE(file_seek(file, 2, FILE_SEEK_BEGIN));
    ASSERT_TRUE(file_truncate(file, 1));
    int64_t position;
    ASSERT_TRUE(file_tell(file, &position));
    ASSERT_EQ_LONG(position, 2);
    ASSERT_TRUE(file_sync(file, true));
    ASSERT_TRUE(file_sync(file, false));
    ASSERT_TRUE(file_close(file));
    ASSERT_TRUE(file_open_read(file));
    ASSERT_TRUE(!file_truncate(file, 0));
    ASSERT_EQ_INT(file_last_error(file), EBADF);
    ASSERT_TRUE(file_close(file));
    file_info_t *info = file_get_info(file);
    ASSERT_EQ_LONG(file_info_size(info), 1);
    file_info_free(info);
    ASSERT_TRUE(file_delete(file));
    file_free(file);
}
