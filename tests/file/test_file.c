/**
 * @file test_file.c
 * @brief File-module suite entry point and text examples.
 *
 * Registers filesystem, stream, error and transform test groups and checks ordinary file operations. Documented
 * text and replacement examples run after ordinary assertions.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

#include "file.h"
#include "array.h"
#include "test_harness.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static file_t *fixture(const char *leaf)
{
    return file_new_cstr(test_case_temp_path(leaf));
}

static const char *path(const file_t *file)
{
    return file_path(file);
}

static bool put(file_t *file, const char *content)
{
    string_t *text = string_new_with(content);
    bool ok = file_write_all_text(file, text);
    string_free(text);
    return ok;
}

static void expect_text(file_t *file, const char *expected)
{
    string_t *text = file_read_all_text(file);
    ASSERT_NOT_NULL(text);
    TEST_ASSERT_STR_EQ(string_c_str(text), expected);
    string_free(text);
}

static void test_file_lifecycle_and_invalid_arguments(void)
{
    ASSERT_TRUE(file_new(NULL) == NULL);
    ASSERT_TRUE(file_new_cstr("") == NULL);
    ASSERT_TRUE(file_new_cstr(NULL) == NULL);
    string_t *nul = string_new();
    ASSERT_EQ_INT(string_append_chars(nul, "a\0b", 3), 0);
    ASSERT_TRUE(file_new(nul) == NULL);
    string_free(nul);
    file_free(NULL);
    file_info_free(NULL);
    ASSERT_TRUE(!file_close(NULL));
    ASSERT_EQ_INT(file_last_error(NULL), EINVAL);
    ASSERT_TRUE(file_path(NULL) == NULL);
    ASSERT_TRUE(!file_is_open(NULL));

    file_t *file = fixture("π-words.txt");
    ASSERT_NOT_NULL(file);
    ASSERT_TRUE(!file_exists(file));
    ASSERT_EQ_INT(file_last_error(file), 0);
    ASSERT_TRUE(!file_open_read(file));
    ASSERT_EQ_INT(file_last_error(file), ENOENT);
    ASSERT_NOT_NULL(file_last_error_message(file));
    ASSERT_TRUE(!file_open(file, (file_mode_t)99, FILE_ACCESS_READ));
    ASSERT_EQ_INT(file_last_error(file), EINVAL);
    ASSERT_TRUE(!file_open(file, FILE_MODE_APPEND, FILE_ACCESS_READ_WRITE));
    ASSERT_TRUE(!file_open(file, FILE_MODE_CREATE, FILE_ACCESS_READ));
    ASSERT_TRUE(!file_open(file, FILE_MODE_OPEN, (file_access_t)99));
    ASSERT_TRUE(file_close(file));
    ASSERT_EQ_INT(file_last_error(file), 0);
    ASSERT_TRUE(file_delete(file));
    file_free(file);
}

static void test_file_modes_and_update_stream(void)
{
    file_t *file = fixture("modes.bin");
    ASSERT_NOT_NULL(file);
    ASSERT_TRUE(!file_open(file, FILE_MODE_TRUNCATE, FILE_ACCESS_WRITE));
    ASSERT_TRUE(file_open(file, FILE_MODE_CREATE_NEW, FILE_ACCESS_READ_WRITE));
    ASSERT_TRUE(file_is_open(file));
    ASSERT_TRUE(!file_open_read(file));
    ASSERT_EQ_INT(file_last_error(file), EBUSY);
    ASSERT_TRUE(!file_delete(file));
    ASSERT_TRUE(file_get_info(file) == NULL);
    ASSERT_TRUE(file_read_all_bytes(file) == NULL);
    size_t count;
    ASSERT_TRUE(file_write(file, "abcdef", 6, &count));
    ASSERT_EQ_LONG(count, 6);
    ASSERT_TRUE(file_flush(file));
    ASSERT_TRUE(file_seek(file, 0, FILE_SEEK_BEGIN));
    char buffer[8] = {0};
    ASSERT_TRUE(file_read(file, buffer, 2, &count));
    ASSERT_EQ_LONG(count, 2);
    ASSERT_TRUE(file_write(file, "XY", 2, &count));
    ASSERT_TRUE(file_read(file, buffer, 2, &count));
    ASSERT_EQ_LONG(count, 2);
    ASSERT_TRUE(memcmp(buffer, "ef", 2) == 0);
    ASSERT_TRUE(file_read(file, buffer, sizeof(buffer), &count));
    ASSERT_EQ_LONG(count, 0);
    int64_t position;
    ASSERT_TRUE(file_tell(file, &position));
    ASSERT_EQ_LONG(position, 6);
    ASSERT_TRUE(!file_seek(file, 0, (file_seek_t)99));
    ASSERT_TRUE(!file_tell(file, NULL));
    ASSERT_TRUE(file_close(file));
    ASSERT_TRUE(!file_open(file, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE));
    ASSERT_EQ_INT(file_last_error(file), EEXIST);
    expect_text(file, "abXYef");

    ASSERT_TRUE(file_open_write(file));
    ASSERT_TRUE(file_write(file, "Z", 1, &count));
    ASSERT_TRUE(file_close(file));
    expect_text(file, "ZbXYef");
    ASSERT_TRUE(file_open(file, FILE_MODE_TRUNCATE, FILE_ACCESS_WRITE));
    ASSERT_TRUE(!file_read(file, buffer, 1, &count));
    ASSERT_EQ_INT(file_last_error(file), EBADF);
    ASSERT_TRUE(file_close(file));
    expect_text(file, "");
    ASSERT_TRUE(file_create(file));
    ASSERT_TRUE(file_write(file, "q", 1, &count));
    ASSERT_TRUE(file_close(file));
    ASSERT_TRUE(file_open_read(file));
    ASSERT_TRUE(!file_write(file, "r", 1, &count));
    ASSERT_TRUE(!file_read(file, NULL, 1, &count));
    ASSERT_TRUE(!file_read(file, buffer, 1, NULL));
    ASSERT_TRUE(file_close(file));
    ASSERT_TRUE(!file_read(file, buffer, 1, &count));
    ASSERT_TRUE(!file_flush(file));
    ASSERT_TRUE(!file_seek(file, 0, FILE_SEEK_BEGIN));
    ASSERT_TRUE(file_delete(file));
    file_free(file);
}

static void test_file_append_and_lines(void)
{
    file_t *file = fixture("append.txt");
    string_t *first = string_new_with("first");
    string_t *second = string_new_with("δεύτερο");
    ASSERT_TRUE(file_append_text(file));
    ASSERT_TRUE(file_write_line(file, first));
    ASSERT_TRUE(file_seek(file, 0, FILE_SEEK_BEGIN));
    ASSERT_TRUE(file_write_line(file, second));
    ASSERT_TRUE(file_close(file));
    expect_text(file, "first\nδεύτερο\n");
    array_t *lines = array_create(sizeof(string_t *), NULL, NULL);
    ASSERT_TRUE(array_add(lines, &first));
    ASSERT_TRUE(array_add(lines, &second));
    ASSERT_TRUE(file_write_all_lines(file, lines));
    ASSERT_TRUE(file_append_all_lines(file, lines));
    array_t *loaded = file_read_all_lines(file);
    ASSERT_NOT_NULL(loaded);
    ASSERT_EQ_LONG(array_size(loaded), 4);
    TEST_ASSERT_STR_EQ(string_c_str(*(string_t **)array_get(loaded, 3)), "δεύτερο");
    array_destroy(loaded);
    array_destroy(lines);
    string_free(second);
    string_free(first);
    ASSERT_TRUE(file_delete(file));
    file_free(file);
}

static void test_file_binary_round_trip_and_empty(void)
{
    file_t *file = fixture("binary.dat");
    size_t size = 200000;
    unsigned char *data = malloc(size);
    ASSERT_NOT_NULL(data);
    for (size_t i = 0; i < size; ++i)
        data[i] = (unsigned char)i;
    ASSERT_TRUE(file_write_all_bytes(file, data, size));
    array_t *loaded = file_read_all_bytes(file);
    ASSERT_NOT_NULL(loaded);
    ASSERT_EQ_LONG(array_elem_size(loaded), 1);
    ASSERT_EQ_LONG(array_size(loaded), size);
    ASSERT_TRUE(memcmp(array_get(loaded, 0), data, size) == 0);
    array_destroy(loaded);
    free(data);
    ASSERT_TRUE(!file_write_all_bytes(file, NULL, 1));
    ASSERT_EQ_INT(file_last_error(file), EINVAL);
    ASSERT_TRUE(file_write_all_bytes(file, NULL, 0));
    loaded = file_read_all_bytes(file);
    ASSERT_NOT_NULL(loaded);
    ASSERT_EQ_LONG(array_size(loaded), 0);
    array_destroy(loaded);
    expect_text(file, "");
    loaded = file_read_all_lines(file);
    ASSERT_NOT_NULL(loaded);
    ASSERT_EQ_LONG(array_size(loaded), 0);
    array_destroy(loaded);
    ASSERT_TRUE(file_delete(file));
    file_free(file);
}

static void test_file_utf8_bom_endings_and_embedded_nul(void)
{
    file_t *file = fixture("utf8.txt");
    const char data[] = "\xef\xbb\xbf" "one\r\n\rδύο\nthree\rfour";
    ASSERT_TRUE(file_write_all_bytes(file, data, sizeof(data) - 1));
    array_t *lines = file_read_all_lines(file);
    ASSERT_NOT_NULL(lines);
    static const char *const expected[] = {"one", "", "δύο", "three", "four"};
    ASSERT_EQ_LONG(array_size(lines), 5);
    for (size_t i = 0; i < 5; ++i)
        TEST_ASSERT_STR_EQ(string_c_str(*(string_t **)array_get(lines, i)), expected[i]);
    array_destroy(lines);
    expect_text(file, "one\r\n\rδύο\nthree\rfour");
    ASSERT_TRUE(file_write_all_bytes(file, "\xef\xbb\xbf", 3));
    lines = file_read_all_lines(file);
    ASSERT_NOT_NULL(lines);
    ASSERT_EQ_LONG(array_size(lines), 0);
    array_destroy(lines);
    string_t *text = string_new();
    ASSERT_EQ_INT(string_append_chars(text, "a\0b", 3), 0);
    ASSERT_TRUE(file_write_all_text(file, text));
    string_free(text);
    text = file_read_all_text(file);
    ASSERT_NOT_NULL(text);
    ASSERT_EQ_LONG(string_byte_length(text), 3);
    ASSERT_TRUE(memcmp(string_c_str(text), "a\0b", 3) == 0);
    string_free(text);
    ASSERT_TRUE(file_write_all_bytes(file, "e\xcc\x81", 3));
    text = file_read_all_text(file);
    ASSERT_NOT_NULL(text);
    TEST_ASSERT_STR_EQ(string_c_str(text), "é");
    string_free(text);
    ASSERT_TRUE(file_delete(file));
    file_free(file);
}

static void test_file_linux_path_bytes_are_not_normalised(void)
{
    char original[4096];
    int length = snprintf(original, sizeof(original), "%s-e\xcc\x81.txt", test_case_temp_path("raw"));
    ASSERT_TRUE(length > 0 && (size_t)length < sizeof(original));
    file_t *file = file_new_cstr(original);
    ASSERT_NOT_NULL(file);
    TEST_ASSERT_STR_EQ(file_path(file), original);
    ASSERT_TRUE(put(file, "raw pathname"));
    struct stat status;
    ASSERT_EQ_INT(lstat(original, &status), 0);
    ASSERT_TRUE(file_delete(file));
    file_free(file);
}

static void test_file_invalid_utf8_does_not_truncate(void)
{
    file_t *file = fixture("invalid.txt");
    static const unsigned char invalid[][4] = {
        {0xc0, 0x80, 0, 0}, {0xed, 0xa0, 0x80, 0}, {0xf4, 0x90, 0x80, 0x80},
        {0xe2, 0x82, 0, 0}, {0x80, 0, 0, 0}
    };
    static const size_t sizes[] = {2, 3, 4, 2, 1};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        ASSERT_TRUE(put(file, "preserved"));
        string_t *bad = NULL;
        ASSERT_TRUE(!file_write_all_text(file, bad));
        ASSERT_EQ_INT(file_last_error(file), EINVAL);
        expect_text(file, "preserved");
        ASSERT_TRUE(!file_append_all_text(file, bad));
        expect_text(file, "preserved");
        array_t *lines = array_create(sizeof(string_t *), NULL, NULL);
        ASSERT_TRUE(array_add(lines, &bad));
        ASSERT_TRUE(!file_write_all_lines(file, lines));
        expect_text(file, "preserved");
        array_destroy(lines);
        ASSERT_TRUE(file_write_all_bytes(file, invalid[i], sizes[i]));
        ASSERT_TRUE(file_read_all_text(file) == NULL);
        ASSERT_EQ_INT(file_last_error(file), EILSEQ);
        ASSERT_TRUE(file_read_all_lines(file) == NULL);
        ASSERT_TRUE(!file_is_open(file));
    }
    ASSERT_TRUE(file_delete(file));
    file_free(file);
}

static void test_file_long_line_crosses_buffer_boundaries(void)
{
    file_t *file = fixture("long.txt");
    string_t *text = string_new();
    for (size_t i = 0; i < 4095; ++i)
        ASSERT_EQ_INT(string_append_chars(text, "x", 1), 0);
    ASSERT_EQ_INT(string_append_chars(text, "π", 2), 0);
    for (size_t i = 0; i < 20000; ++i)
        ASSERT_EQ_INT(string_append_chars(text, "y", 1), 0);
    ASSERT_TRUE(file_write_all_text(file, text));
    ASSERT_TRUE(file_open_text(file));
    string_t *line = NULL;
    ASSERT_TRUE(file_read_line(file, &line));
    ASSERT_NOT_NULL(line);
    ASSERT_EQ_LONG(string_byte_length(line), string_byte_length(text));
    ASSERT_TRUE(memcmp(string_c_str(line), string_c_str(text), string_byte_length(text)) == 0);
    string_free(line);
    ASSERT_TRUE(file_read_line(file, &line));
    ASSERT_TRUE(line == NULL);
    ASSERT_TRUE(file_close(file));
    string_free(text);
    ASSERT_TRUE(file_delete(file));
    file_free(file);
}

static void test_file_copy_move_and_alias_protection(void)
{
    file_t *source = fixture("source.txt");
    file_t *target = fixture("target.txt");
    file_t *alias = fixture("hardlink.txt");
    ASSERT_TRUE(put(source, "original"));
    ASSERT_TRUE(file_copy(source, target, false));
    expect_text(target, "original");
    ASSERT_TRUE(!file_copy(source, target, false));
    ASSERT_EQ_INT(file_last_error(source), EEXIST);
    ASSERT_TRUE(put(source, "updated"));
    ASSERT_TRUE(file_copy(source, target, true));
    expect_text(target, "updated");
    ASSERT_TRUE(!file_copy(source, source, true));
    ASSERT_EQ_INT(link(path(source), path(alias)), 0);
    ASSERT_TRUE(!file_copy(source, alias, true));
    ASSERT_TRUE(!file_move(source, alias, true));
    expect_text(source, "updated");
    ASSERT_TRUE(!file_move(source, target, false));
    ASSERT_TRUE(file_move(source, target, true));
    ASSERT_TRUE(!file_exists(source));
    expect_text(target, "updated");
    ASSERT_TRUE(file_delete(alias));
    ASSERT_TRUE(file_move(target, source, false));
    expect_text(source, "updated");
    ASSERT_TRUE(!file_exists(target));
    ASSERT_TRUE(file_delete(source));
    file_free(alias);
    file_free(target);
    file_free(source);
}

static void test_file_replace_and_backup(void)
{
    file_t *source = fixture("replacement.txt");
    file_t *target = fixture("current.txt");
    file_t *backup = fixture("backup.txt");
    ASSERT_TRUE(put(source, "new"));
    ASSERT_TRUE(!file_replace(source, target, NULL));
    ASSERT_EQ_INT(file_last_error(source), ENOENT);
    ASSERT_TRUE(put(target, "old"));
    ASSERT_TRUE(!file_replace(source, target, source));
    ASSERT_TRUE(file_replace(source, target, backup));
    ASSERT_TRUE(!file_exists(source));
    expect_text(target, "new");
    expect_text(backup, "old");
    ASSERT_TRUE(put(source, "newer"));
    ASSERT_TRUE(!file_replace(source, target, backup));
    expect_text(source, "newer");
    expect_text(target, "new");
    expect_text(backup, "old");
    ASSERT_TRUE(file_replace(source, target, NULL));
    expect_text(target, "newer");
    ASSERT_TRUE(file_delete(target));
    ASSERT_TRUE(file_delete(backup));
    file_free(backup);
    file_free(target);
    file_free(source);
}

static void test_file_metadata_and_linux_attributes(void)
{
    file_t *file = fixture(".metadata.txt");
    ASSERT_TRUE(put(file, "12345"));
    struct timespec times[2] = {{123456789, 123456789}, {123456789, 987654321}};
    ASSERT_EQ_INT(utimensat(AT_FDCWD, path(file), times, 0), 0);
    file_info_t *info = file_get_info(file);
    ASSERT_NOT_NULL(info);
    ASSERT_EQ_LONG(file_info_size(info), 5);
    ASSERT_TRUE(file_info_attributes(info) & FILE_ATTRIBUTE_HIDDEN);
    int64_t seconds;
    long nanoseconds;
    ASSERT_TRUE(file_info_last_write_time(info, &seconds, &nanoseconds));
    ASSERT_EQ_LONG(seconds, 123456789);
    ASSERT_EQ_LONG(nanoseconds, 987654321);
    if (file_info_creation_time(info, &seconds, &nanoseconds)) {
        ASSERT_TRUE(nanoseconds >= 0 && nanoseconds < 1000000000);
    } else {
        ASSERT_EQ_INT(errno, ENOTSUP);
    }
    ASSERT_TRUE(put(file, "longer"));
    ASSERT_EQ_LONG(file_info_size(info), 5);
    file_info_free(info);
    unsigned attributes;
    ASSERT_TRUE(file_set_attributes(file, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READ_ONLY));
    ASSERT_TRUE(file_get_attributes(file, &attributes));
    ASSERT_TRUE(attributes & FILE_ATTRIBUTE_READ_ONLY);
    struct stat status;
    ASSERT_EQ_INT(stat(path(file), &status), 0);
    ASSERT_EQ_INT(status.st_mode & 0222, 0);
    ASSERT_TRUE(!file_set_attributes(file, FILE_ATTRIBUTE_NONE));
    ASSERT_EQ_INT(file_last_error(file), ENOTSUP);
    ASSERT_TRUE(!file_set_attributes(file, 0x80));
    ASSERT_TRUE(file_set_attributes(file, FILE_ATTRIBUTE_HIDDEN));
    ASSERT_EQ_INT(stat(path(file), &status), 0);
    ASSERT_TRUE(status.st_mode & S_IWUSR);
    ASSERT_TRUE(!file_get_attributes(file, NULL));
    ASSERT_TRUE(!file_info_last_write_time(NULL, &seconds, &nanoseconds));
    ASSERT_TRUE(!file_info_creation_time(NULL, &seconds, &nanoseconds));
    ASSERT_TRUE(file_delete(file));
    file_free(file);
}

static void test_file_rejects_symlinks_directories_and_fifos(void)
{
    file_t *target = fixture("target.txt");
    file_t *symbolic = fixture("symbolic.txt");
    file_t *directory = fixture("directory");
    file_t *fifo = fixture("fifo");
    ASSERT_TRUE(put(target, "do not truncate"));
    ASSERT_EQ_INT(symlink(path(target), path(symbolic)), 0);
    ASSERT_TRUE(!file_create(symbolic));
    ASSERT_EQ_INT(file_last_error(symbolic), ELOOP);
    ASSERT_TRUE(file_exists(symbolic));
    ASSERT_TRUE(!file_copy(target, symbolic, true));
    file_info_t *info = file_get_info(symbolic);
    ASSERT_NOT_NULL(info);
    ASSERT_EQ_INT(file_info_type(info), FILE_TYPE_SYMLINK);
    file_info_free(info);
    expect_text(target, "do not truncate");
    ASSERT_EQ_INT(mkdir(path(directory), 0700), 0);
    ASSERT_TRUE(!file_create(directory));
    ASSERT_TRUE(!file_delete(directory));
    info = file_get_info(directory);
    ASSERT_NOT_NULL(info);
    ASSERT_EQ_INT(file_info_type(info), FILE_TYPE_DIRECTORY);
    file_info_free(info);
    ASSERT_EQ_INT(mkfifo(path(fifo), 0600), 0);
    ASSERT_TRUE(!file_open_read(fifo));
    ASSERT_EQ_INT(file_last_error(fifo), ENOTSUP);
    ASSERT_TRUE(file_delete(symbolic));
    ASSERT_TRUE(file_delete(fifo));
    ASSERT_TRUE(file_remove_directory(directory));
    ASSERT_TRUE(file_delete(target));
    file_free(fifo);
    file_free(directory);
    file_free(symbolic);
    file_free(target);
}

static void test_file_advisory_locks(void)
{
    file_t *first = fixture("locked.txt");
    file_t *second = file_new_cstr(file_path(first));
    ASSERT_TRUE(file_create(first));
    ASSERT_TRUE(file_open_read(second));
    ASSERT_TRUE(file_lock(first, true, false));
    ASSERT_TRUE(!file_lock(second, false, false));
    ASSERT_EQ_INT(file_last_error(second), EWOULDBLOCK);
    ASSERT_TRUE(file_unlock(first));
    ASSERT_TRUE(file_lock(second, false, false));
    ASSERT_TRUE(file_lock(first, false, false));
    ASSERT_TRUE(file_close(second));
    ASSERT_TRUE(file_close(first));
    ASSERT_TRUE(!file_lock(first, true, false));
    ASSERT_TRUE(!file_unlock(first));
    ASSERT_TRUE(file_delete(first));
    file_free(second);
    file_free(first);
}

/* The POSIX signal test is isolated from array.h's existing stack_t typedef. */
void test_file_delayed_write_failure_is_reported(void);
void test_file_copy_failure_preserves_destination(void);
void test_file_directory_listing_and_removal(void);
void test_file_chmod_chown_times_and_access(void);
void test_file_links_and_directory_moves(void);
void test_file_truncate_and_sync(void);
void test_file_explicit_follow_and_text_streams(void);
void test_file_large_sparse_offsets(void);
void test_file_null_and_invalid_outputs(void);
void test_file_utf8_validation_classes(void);
void test_file_copy_injected_io_failures(void);
void test_file_move_cross_device_paths(void);
void test_file_allocation_and_sync_failures(void);
void test_file_blocking_lock_handoff(void);
void test_file_allocation_failure_sweeps(void);
void test_file_stream_and_metadata_failures(void);
void test_file_path_and_directory_failures(void);
void test_file_transform_wire_validation(void);
void test_file_sqlite_round_trip_sizes(void);
void test_file_sqlite_optional_metadata(void);
void test_file_sqlite_limits_and_destination_preservation(void);
void test_file_sqlite_corrupt_chunks(void);
void test_file_sqlite_corrupt_or_missing_manifest(void);
void test_file_sqlite_import_rollback(void);
void test_file_sqlite_replacement_clears_chunks(void);
void test_file_sqlite_value_api_guards(void);
void test_file_sqlite_database_path_and_argument_guards(void);
void test_file_sqlite_encrypted_payload_round_trip(void);
void example_file_sqlite_round_trip(void);
void test_file_transform_allocation_failures(void);
void test_file_transform_staging_failures(void);
void test_file_transforms_round_trip_and_empty(void);
void test_file_transforms_large_round_trip(void);
void test_file_transforms_corrupt_truncated_and_trailing(void);
void test_file_transforms_limits_and_destination_preservation(void);
void test_file_encryption_keys_and_wrong_key(void);
void test_file_key_invalid_arguments(void);
void test_file_transforms_invalid_arguments_and_aliases(void);
void example_file_encrypted_round_trip(void);
void test_file_listing_symlink_targets(void);
void test_file_symlink_target_failures(void);
void example_file_symlink_target(void);
void example_file_compression_round_trip(void);
void example_file_encryption_only(void);

/* README example: docs/file.md, UTF-8 lines. */
static void example_file_utf8_lines(void)
{
    file_t *file = fixture("words.txt");
    string_t *words = string_new_with("sky\ncloud\n");
    string_t *extra = string_new_with("falcon\n");
    ASSERT_TRUE(file_write_all_text(file, words));
    ASSERT_TRUE(file_append_all_text(file, extra));
    ASSERT_TRUE(file_open_text(file));
    string_t *line = NULL;
    static const char *const expected[] = {"sky", "cloud", "falcon"};
    size_t count = 0;
    while (file_read_line(file, &line) && line) {
        ASSERT_TRUE(count < 3);
        TEST_ASSERT_STR_EQ(string_c_str(line), expected[count++]);
        printf("%s\n", string_c_str(line));
        string_free(line);
    }
    ASSERT_EQ_INT(file_last_error(file), 0);
    ASSERT_EQ_LONG(count, 3);
    ASSERT_TRUE(file_close(file));
    ASSERT_TRUE(file_delete(file));
    string_free(extra);
    string_free(words);
    file_free(file);
}

/* README example: docs/file.md, replacement and backup. */
static void example_file_replace_backup(void)
{
    file_t *current = fixture("current.txt");
    file_t *replacement = fixture("replacement.txt");
    file_t *backup = fixture("backup.txt");
    string_t *old_text = string_new_with("old");
    string_t *new_text = string_new_with("new");
    ASSERT_TRUE(file_write_all_text(current, old_text));
    ASSERT_TRUE(file_write_all_text(replacement, new_text));
    ASSERT_TRUE(file_replace(replacement, current, backup));
    string_t *current_text = file_read_all_text(current);
    string_t *backup_text = file_read_all_text(backup);
    ASSERT_NOT_NULL(current_text);
    ASSERT_NOT_NULL(backup_text);
    TEST_ASSERT_STR_EQ(string_c_str(current_text), "new");
    TEST_ASSERT_STR_EQ(string_c_str(backup_text), "old");
    printf("current: %s\nbackup: %s\n", string_c_str(current_text), string_c_str(backup_text));
    ASSERT_TRUE(file_delete(current));
    ASSERT_TRUE(file_delete(backup));
    string_free(current_text);
    string_free(backup_text);
    string_free(old_text);
    string_free(new_text);
    file_free(backup);
    file_free(replacement);
    file_free(current);
}

int tests_main(void)
{
    TEST_SECTION("Linux File Operations");
    TEST_RUN_IN_GROUP(test_file_listing_symlink_targets, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_symlink_target_failures, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_lifecycle_and_invalid_arguments, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_modes_and_update_stream, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_append_and_lines, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_binary_round_trip_and_empty, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_utf8_bom_endings_and_embedded_nul, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_linux_path_bytes_are_not_normalised, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_invalid_utf8_does_not_truncate, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_long_line_crosses_buffer_boundaries, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_copy_move_and_alias_protection, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_replace_and_backup, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_metadata_and_linux_attributes, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_rejects_symlinks_directories_and_fifos, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_advisory_locks, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_delayed_write_failure_is_reported, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_copy_failure_preserves_destination, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_directory_listing_and_removal, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_chmod_chown_times_and_access, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_links_and_directory_moves, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_truncate_and_sync, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_explicit_follow_and_text_streams, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_large_sparse_offsets, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_null_and_invalid_outputs, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_utf8_validation_classes, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_copy_injected_io_failures, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_move_cross_device_paths, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_allocation_and_sync_failures, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_blocking_lock_handoff, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_allocation_failure_sweeps, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_stream_and_metadata_failures, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_path_and_directory_failures, tests, NULL);
    TEST_SECTION("Streaming File Operations");
    TEST_RUN_IN_GROUP(test_file_transform_wire_validation, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_transform_allocation_failures, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_transform_staging_failures, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_transforms_round_trip_and_empty, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_transforms_large_round_trip, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_transforms_corrupt_truncated_and_trailing, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_transforms_limits_and_destination_preservation, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_encryption_keys_and_wrong_key, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_key_invalid_arguments, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_transforms_invalid_arguments_and_aliases, tests, NULL);
    TEST_SECTION("SQLCipher File Operations");
    TEST_RUN_IN_GROUP(test_file_sqlite_round_trip_sizes, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_sqlite_optional_metadata, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_sqlite_limits_and_destination_preservation, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_sqlite_corrupt_chunks, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_sqlite_corrupt_or_missing_manifest, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_sqlite_import_rollback, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_sqlite_replacement_clears_chunks, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_sqlite_value_api_guards, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_sqlite_database_path_and_argument_guards, tests, NULL);
    TEST_RUN_IN_GROUP(test_file_sqlite_encrypted_payload_round_trip, tests, NULL);
    TEST_SECTION("README Output Examples");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_file_utf8_lines, readme_examples, "file,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_file_replace_backup, readme_examples, "file,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_file_compression_round_trip, readme_examples, "file,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_file_encryption_only, readme_examples, "file,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_file_encrypted_round_trip, readme_examples, "file,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_file_sqlite_round_trip, readme_examples, "file,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_file_symlink_target, readme_examples, "file,readme,output");
    return TEST_EXIT_CODE();
}
