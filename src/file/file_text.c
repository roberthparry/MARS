#define MARS_FILE_INTERNAL_ACCESS
#include "file_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Validate complete UTF-8 sequences without normalising or discarding bytes. */
bool file_valid_utf8(const void *data, size_t size)
{
    if (!data && size)
        return false;
    const unsigned char *bytes = data;
    for (size_t i = 0; i < size;) {
        unsigned char first = bytes[i++];
        if (first < 0x80)
            continue;
        size_t continuation;
        uint32_t codepoint, minimum;
        if (first >= 0xc2 && first <= 0xdf) {
            continuation = 1; codepoint = first & 0x1f; minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            continuation = 2; codepoint = first & 0x0f; minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            continuation = 3; codepoint = first & 0x07; minimum = 0x10000;
        } else {
            return false;
        }
        if (continuation > size - i)
            return false;
        while (continuation--) {
            unsigned char next = bytes[i++];
            if ((next & 0xc0) != 0x80)
                return false;
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        if (codepoint < minimum || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff))
            return false;
    }
    return true;
}

bool file_valid_text(const string_t *text)
{
    return text && file_valid_utf8(string_c_str(text), string_byte_length(text));
}

static string_t *file_string_from_bytes(file_t *file, const void *data, size_t size)
{
    if (!file_valid_utf8(data, size)) {
        file_fail(file, EILSEQ);
        return NULL;
    }
    string_t *text = string_new();
    if (!text || (size && string_append_chars(text, data, size) != 0)) {
        string_free(text);
        file_fail(file, ENOMEM);
        return NULL;
    }
    return text;
}

static bool file_check_text(file_t *file, const string_t *text)
{
    if (!text)
        return file_fail(file, EINVAL);
    return file_valid_text(text) ? true : file_fail(file, EILSEQ);
}

static bool file_skip_bom(file_t *file)
{
    off_t position = ftello(file->stream);
    if (position < 0)
        return file_fail(file, errno);
    if (position != 0)
        return true;
    unsigned char bom[3];
    size_t count = fread(bom, 1, sizeof(bom), file->stream);
    if (ferror(file->stream))
        return file_fail(file, errno);
    if (count == 3 && memcmp(bom, "\xef\xbb\xbf", 3) == 0)
        return true;
    return fseeko(file->stream, 0, SEEK_SET) == 0 ? true : file_fail(file, errno);
}

/* Keep only one line in memory; stdio supplies bounded underlying read buffering. */
bool file_read_line(file_t *file, string_t **line)
{
    if (!line)
        return file_fail(file, EINVAL);
    *line = NULL;
    if (!file_prepare_io(file, false) || !file_skip_bom(file))
        return false;
    array_t *bytes = array_create(1, NULL, NULL);
    if (!bytes)
        return file_fail(file, ENOMEM);
    char block[4096];
    size_t used = 0;
    bool saw_byte = false;
    int c;
    while ((c = fgetc(file->stream)) != EOF) {
        saw_byte = true;
        if (c == '\n')
            break;
        if (c == '\r') {
            int next = fgetc(file->stream);
            if (next != '\n' && next != EOF)
                ungetc(next, file->stream);
            break;
        }
        block[used++] = (char)c;
        if (used == sizeof(block)) {
            if (!array_append_carray(bytes, block, used))
                goto allocation_failure;
            used = 0;
        }
    }
    if (ferror(file->stream)) {
        int error = errno;
        array_destroy(bytes);
        return file_fail(file, error);
    }
    if (used && !array_append_carray(bytes, block, used))
        goto allocation_failure;
    if (saw_byte) {
        *line = file_string_from_bytes(file, array_get(bytes, 0), array_size(bytes));
        if (!*line) {
            array_destroy(bytes);
            return false;
        }
    }
    array_destroy(bytes);
    return file_succeed(file);

allocation_failure:
    array_destroy(bytes);
    return file_fail(file, ENOMEM);
}

/* Preserve text bytes after validating the complete string. */
bool file_write_text(file_t *file, const string_t *text)
{
    size_t written;
    return file_check_text(file, text) &&
           file_write(file, string_c_str(text), string_byte_length(text), &written);
}

/* Add one portable LF line ending. */
bool file_write_line(file_t *file, const string_t *text)
{
    size_t written;
    return file_write_text(file, text) && file_write(file, "\n", 1, &written);
}

/* Read unknown-sized regular files incrementally into an opaque byte array. */
array_t *file_read_all_bytes(file_t *file)
{
    if (!file_open_read(file))
        return NULL;
    array_t *bytes = array_create(sizeof(unsigned char), NULL, NULL);
    bool ok = bytes != NULL;
    if (!ok)
        file_fail(file, ENOMEM);
    unsigned char block[8192];
    while (ok) {
        size_t count;
        ok = file_read(file, block, sizeof(block), &count);
        if (!ok || count == 0)
            break;
        if (!array_append_carray(bytes, block, count))
            ok = file_fail(file, ENOMEM);
    }
    if (!file_finish(file, ok)) {
        array_destroy(bytes);
        return NULL;
    }
    return bytes;
}

/* Decode only UTF-8, with optional BOM; embedded NUL bytes remain part of the string. */
string_t *file_read_all_text(file_t *file)
{
    array_t *bytes = file_read_all_bytes(file);
    if (!bytes)
        return NULL;
    size_t size = array_size(bytes), start = 0;
    const unsigned char *data = array_get(bytes, 0);
    if (size >= 3 && memcmp(data, "\xef\xbb\xbf", 3) == 0)
        start = 3;
    string_t *text = file_string_from_bytes(file, size ? data + start : NULL, size - start);
    array_destroy(bytes);
    return text;
}

static void file_destroy_line(void *element)
{
    string_free(*(string_t **)element);
}

/* Transfer every lazily read line into a container which owns its strings. */
array_t *file_read_all_lines(file_t *file)
{
    if (!file_open_text(file))
        return NULL;
    array_t *lines = array_create(sizeof(string_t *), NULL, file_destroy_line);
    bool ok = lines != NULL;
    if (!ok)
        file_fail(file, ENOMEM);
    while (ok) {
        string_t *line = NULL;
        ok = file_read_line(file, &line);
        if (!ok || !line)
            break;
        if (!array_add(lines, &line)) {
            string_free(line);
            ok = file_fail(file, ENOMEM);
        }
    }
    if (!file_finish(file, ok)) {
        array_destroy(lines);
        return NULL;
    }
    return lines;
}

/* Validate byte arguments before truncating the target. */
bool file_write_all_bytes(file_t *file, const void *data, size_t size)
{
    if (!data && size)
        return file_fail(file, EINVAL);
    if (!file_create_text(file))
        return false;
    size_t written;
    return file_finish(file, file_write(file, data, size, &written));
}

static bool file_store_text(file_t *file, const string_t *text, bool append)
{
    if (!file_check_text(file, text) || !(append ? file_append_text(file) : file_create_text(file)))
        return false;
    return file_finish(file, file_write_text(file, text));
}

/* Validate UTF-8 before replacing contents. */
bool file_write_all_text(file_t *file, const string_t *text) { return file_store_text(file, text, false); }

/* Validate UTF-8 before appending contents. */
bool file_append_all_text(file_t *file, const string_t *text) { return file_store_text(file, text, true); }

static bool file_store_lines(file_t *file, const array_t *lines, bool append)
{
    if (!lines || array_elem_size(lines) != sizeof(string_t *))
        return file_fail(file, EINVAL);
    /* A bounded pass over caller-supplied lines prevents truncation on invalid text. */
    for (size_t i = 0; i < array_size(lines); ++i)
        if (!file_check_text(file, *(string_t **)array_get(lines, i)))
            return false;
    if (!(append ? file_append_text(file) : file_create_text(file)))
        return false;
    bool ok = true;
    for (size_t i = 0; ok && i < array_size(lines); ++i)
        ok = file_write_line(file, *(string_t **)array_get(lines, i));
    return file_finish(file, ok);
}

/* Write caller-owned lines without taking ownership. */
bool file_write_all_lines(file_t *file, const array_t *lines) { return file_store_lines(file, lines, false); }

/* Append caller-owned lines without taking ownership. */
bool file_append_all_lines(file_t *file, const array_t *lines) { return file_store_lines(file, lines, true); }
