/**
 * @file test_file_coverage.c
 * @brief File API invalid-input and boundary regressions.
 *
 * Checks null outputs, UTF-8 validation classes and path or directory failures. These cases exercise negative
 * paths beyond successful filesystem round trips.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

#include "file.h"
#include "array.h"
#include "test_harness.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

void test_file_explicit_follow_and_text_streams(void)
{
    file_t *file = file_new_cstr(test_case_temp_path("text"));
    file_t *link = file_new_cstr(test_case_temp_path("link"));
    string_t *text = string_new_with("π");
    ASSERT_TRUE(file_create_text(file));
    ASSERT_TRUE(file_write_text(file, text));
    ASSERT_TRUE(file_close(file));
    ASSERT_TRUE(file_create_symlink(file, link));
    ASSERT_TRUE(!file_open_follow(link, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE));
    ASSERT_EQ_INT(file_last_error(link), EEXIST);
    ASSERT_TRUE(file_open_follow(link, FILE_MODE_OPEN, FILE_ACCESS_READ_WRITE));
    ASSERT_TRUE(!file_open_follow(link, FILE_MODE_OPEN, FILE_ACCESS_READ));
    ASSERT_EQ_INT(file_last_error(link), EBUSY);
    ASSERT_TRUE(file_close(link));
    ASSERT_TRUE(file_open_follow(link, FILE_MODE_TRUNCATE, FILE_ACCESS_WRITE));
    ASSERT_TRUE(file_write_text(link, text));
    ASSERT_TRUE(file_close(link));
    ASSERT_TRUE(file_open_follow(link, FILE_MODE_APPEND, FILE_ACCESS_WRITE));
    ASSERT_TRUE(file_seek(link, 0, FILE_SEEK_BEGIN));
    ASSERT_TRUE(file_write_text(link, text));
    ASSERT_TRUE(file_close(link));
    string_t *actual = file_read_all_text(file);
    TEST_ASSERT_STR_EQ(string_c_str(actual), "ππ");
    string_free(actual);
    ASSERT_TRUE(file_delete(file));
    ASSERT_TRUE(file_open_follow(link, FILE_MODE_OPEN_OR_CREATE, FILE_ACCESS_WRITE));
    ASSERT_TRUE(file_write_line(link, text));
    ASSERT_TRUE(file_close(link));
    ASSERT_TRUE(file_delete(file));
    ASSERT_TRUE(file_open_follow(link, FILE_MODE_CREATE, FILE_ACCESS_WRITE));
    ASSERT_TRUE(file_close(link));
    ASSERT_TRUE(file_delete(file));
    ASSERT_TRUE(file_create_directory(file, 0700, false));
    ASSERT_TRUE(!file_open_follow(link, FILE_MODE_OPEN, FILE_ACCESS_READ));
    ASSERT_TRUE(file_delete(link));
    ASSERT_TRUE(file_remove_directory(file));
    ASSERT_TRUE(!file_open_follow(NULL, FILE_MODE_OPEN, FILE_ACCESS_READ));
    ASSERT_TRUE(!file_open_follow(file, (file_mode_t)99, FILE_ACCESS_READ));
    string_free(text);
    file_free(link);
    file_free(file);
}

void test_file_large_sparse_offsets(void)
{
    file_t *file = file_new_cstr(test_case_temp_path("sparse"));
    const int64_t offset = INT64_C(4294967296) + 17;
    size_t count;
    int64_t position;
    unsigned char buffer[2];
    ASSERT_TRUE(file_create(file));
    ASSERT_TRUE(file_seek(file, offset, FILE_SEEK_BEGIN));
    ASSERT_TRUE(file_write(file, "x", 1, &count));
    ASSERT_TRUE(file_tell(file, &position));
    ASSERT_EQ_LONG(position, offset + 1);
    ASSERT_TRUE(file_seek(file, -2, FILE_SEEK_END));
    ASSERT_TRUE(file_read(file, buffer, 2, &count));
    ASSERT_EQ_LONG(count, 2);
    ASSERT_TRUE(buffer[0] == 0 && buffer[1] == 'x');
    ASSERT_TRUE(file_seek(file, -1, FILE_SEEK_CURRENT));
    ASSERT_TRUE(file_tell(file, &position));
    ASSERT_EQ_LONG(position, offset);
    ASSERT_TRUE(file_close(file));
    file_info_t *info = file_get_info(file);
    ASSERT_NOT_NULL(info);
    ASSERT_EQ_LONG(file_info_size(info), offset + 1);
    file_info_free(info);
    ASSERT_TRUE(file_truncate(file, 0));
    ASSERT_TRUE(file_delete(file));
    file_free(file);
}

void test_file_null_and_invalid_outputs(void)
{
    file_t *file = file_new_cstr(test_case_temp_path("invalid"));
    file_info_t *entry = NULL;
    string_t *line = NULL;
    int64_t seconds;
    long nanoseconds;
    size_t count;
    ASSERT_TRUE(!file_read(NULL, NULL, 1, &count));
    ASSERT_TRUE(!file_write(NULL, NULL, 1, &count));
    ASSERT_TRUE(!file_read(file, NULL, 0, NULL));
    ASSERT_TRUE(!file_write(file, NULL, 0, NULL));
    ASSERT_TRUE(!file_read(file, NULL, 0, &count));
    ASSERT_TRUE(!file_write(file, NULL, 0, &count));
    ASSERT_TRUE(!file_tell(file, NULL));
    ASSERT_TRUE(!file_tell(file, &seconds));
    ASSERT_TRUE(!file_seek(file, 0, FILE_SEEK_BEGIN));
    ASSERT_TRUE(!file_flush(file));
    ASSERT_TRUE(!file_read_line(file, NULL));
    ASSERT_TRUE(!file_read_line(file, &line));
    ASSERT_TRUE(!file_read_directory(file, NULL));
    ASSERT_TRUE(!file_read_directory(file, &entry));
    ASSERT_TRUE(!file_exists(NULL));
    ASSERT_TRUE(!file_delete(NULL));
    ASSERT_TRUE(!file_copy(NULL, file, false));
    ASSERT_TRUE(!file_move(NULL, file, false));
    ASSERT_TRUE(!file_replace(NULL, file, NULL));
    ASSERT_TRUE(!file_create_directory(file, 010000, false));
    ASSERT_TRUE(!file_remove_directory(NULL));
    ASSERT_TRUE(!file_chmod(NULL, 0600));
    ASSERT_TRUE(!file_chown(NULL, -1, -1));
    ASSERT_TRUE(!file_set_times(NULL, 0, 0, 0, 0));
    ASSERT_TRUE(!file_check_access(NULL, false, false, false));
    ASSERT_TRUE(!file_truncate(NULL, 0));
    ASSERT_TRUE(!file_read_link(file, NULL));
    ASSERT_TRUE(file_resolve(NULL) == NULL);
    ASSERT_TRUE(file_info_name(NULL) == NULL);
    ASSERT_EQ_INT(file_info_type(NULL), FILE_TYPE_UNKNOWN);
    ASSERT_EQ_LONG(file_info_size(NULL), 0);
    ASSERT_EQ_INT(file_info_attributes(NULL), FILE_ATTRIBUTE_NONE);
    ASSERT_EQ_INT(file_info_permissions(NULL), 0);
    ASSERT_EQ_LONG(file_info_owner(NULL), UINT32_MAX);
    ASSERT_EQ_LONG(file_info_group(NULL), UINT32_MAX);
    ASSERT_EQ_LONG(file_info_link_count(NULL), 0);
    ASSERT_TRUE(!file_info_last_access_time(NULL, &seconds, &nanoseconds));
    ASSERT_TRUE(!file_info_status_change_time(NULL, &seconds, &nanoseconds));
    ASSERT_TRUE(file_create(file));
    ASSERT_TRUE(!file_seek(file, 0, (file_seek_t)99));
    ASSERT_TRUE(!file_seek(file, -1, FILE_SEEK_BEGIN));
    ASSERT_TRUE(file_read(file, NULL, 0, &count));
    ASSERT_TRUE(file_write(file, NULL, 0, &count));
    ASSERT_TRUE(file_close(file));
    file_free(file);
}

void test_file_utf8_validation_classes(void)
{
    static const struct { unsigned char bytes[4]; size_t size; } invalid[] = {
        {{0x80}, 1}, {{0xc0, 0x80}, 2}, {{0xc2}, 1}, {{0xc2, 0x20}, 2},
        {{0xe0, 0x80, 0x80}, 3}, {{0xed, 0xa0, 0x80}, 3},
        {{0xf0, 0x80, 0x80, 0x80}, 4}, {{0xf4, 0x90, 0x80, 0x80}, 4}, {{0xf5}, 1}
    };
    file_t *file = file_new_cstr(test_case_temp_path("utf8"));
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        ASSERT_TRUE(file_write_all_bytes(file, invalid[i].bytes, invalid[i].size));
        ASSERT_TRUE(file_read_all_text(file) == NULL);
        ASSERT_EQ_INT(file_last_error(file), EILSEQ);
        ASSERT_TRUE(file_read_all_lines(file) == NULL);
        ASSERT_EQ_INT(file_last_error(file), EILSEQ);
        ASSERT_TRUE(!file_is_open(file));
    }
    const char *valid = "π€😀";
    ASSERT_TRUE(file_write_all_bytes(file, valid, strlen(valid)));
    string_t *text = file_read_all_text(file);
    ASSERT_NOT_NULL(text);
    TEST_ASSERT_STR_EQ(string_c_str(text), valid);
    string_free(text);
    file_free(file);
}

void test_file_path_and_directory_failures(void)
{
    file_t *file = file_new_cstr(test_case_temp_path("regular"));
    file_t *missing = file_new_cstr(test_case_temp_path("missing"));
    file_t *directory = file_new_cstr(test_case_temp_path("directory"));
    file_t *link = file_new_cstr(test_case_temp_path("link"));
    string_t *nested_path = string_sprintf("%s/child/grandchild", file_path(file));
    file_t *nested = file_new(nested_path);
    string_free(nested_path);
    ASSERT_TRUE(file_write_all_bytes(file, "data", 4));
    ASSERT_TRUE(file_create_directory(directory, 0700, false));
    ASSERT_TRUE(!file_create_directory(file, 0700, false));
    ASSERT_EQ_INT(file_last_error(file), ENOTDIR);
    ASSERT_TRUE(!file_create_directory(nested, 0700, true));
    ASSERT_EQ_INT(file_last_error(nested), ENOTDIR);
    ASSERT_TRUE(!file_create_directory(nested, 0700, false));
    ASSERT_TRUE(!file_open_directory(file));
    ASSERT_TRUE(!file_open_directory(missing));
    ASSERT_TRUE(!file_exists(nested));
    ASSERT_EQ_INT(file_last_error(nested), 0);
    ASSERT_TRUE(!file_delete(nested));
    ASSERT_TRUE(file_get_info(missing) == NULL);
    ASSERT_TRUE(!file_set_attributes(missing, FILE_ATTRIBUTE_NONE));
    ASSERT_TRUE(!file_get_attributes(missing, &(unsigned){0}));
    ASSERT_TRUE(!file_chmod(missing, 0600));
    ASSERT_TRUE(!file_copy(missing, file, false));
    ASSERT_TRUE(!file_move(missing, file, false));
    ASSERT_TRUE(!file_copy(file, NULL, false));
    ASSERT_TRUE(!file_copy(directory, missing, false));
    ASSERT_TRUE(!file_copy(file, directory, true));
    ASSERT_TRUE(!file_replace(file, directory, NULL));
    ASSERT_TRUE(file_create_symlink(file, link));
    ASSERT_TRUE(!file_copy(link, missing, false));
    ASSERT_TRUE(!file_copy(file, link, true));
    ASSERT_TRUE(file_open_read(file));
    ASSERT_TRUE(!file_copy(link, file, true));
    ASSERT_TRUE(!file_create_directory(file, 0700, false));
    ASSERT_TRUE(!file_create_hard_link(file, missing));
    ASSERT_TRUE(!file_create_symlink(file, missing));
    ASSERT_TRUE(!file_set_attributes(file, FILE_ATTRIBUTE_NONE));
    ASSERT_TRUE(!file_chmod(file, 0600));
    ASSERT_TRUE(!file_chown(file, -1, -1));
    ASSERT_TRUE(!file_set_times(file, 0, 0, 0, 0));
    ASSERT_TRUE(!file_replace(link, missing, file));
    ASSERT_TRUE(file_close(file));
    ASSERT_TRUE(!file_create_symlink(file, NULL));
    ASSERT_TRUE(file_create(missing));
    ASSERT_TRUE(!file_create_symlink(file, missing));
    ASSERT_TRUE(!file_replace(file, link, missing));
    ASSERT_TRUE(file_close(missing));
    ASSERT_TRUE(file_delete(link));

    char long_target[700];
    memset(long_target, 'x', sizeof(long_target) - 1);
    long_target[sizeof(long_target) - 1] = 0;
    file_t *target = file_new_cstr(long_target);
    ASSERT_TRUE(file_create_symlink(target, link));
    char *actual = NULL;
    ASSERT_TRUE(file_read_link(link, &actual));
    TEST_ASSERT_STR_EQ(actual, long_target);
    free(actual);
    file_free(target);
    file_free(link);
    file_free(nested);
    file_free(directory);
    file_free(missing);
    file_free(file);
}
