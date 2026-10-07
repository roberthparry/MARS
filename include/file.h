/**
 * @file file.h
 * @brief Opaque Linux file and directory streams, text helpers and filesystem operations.
 *
 * Use this module as the Linux filesystem interface for MARS applications: opening,
 * reading, writing and seeking files; listing directories; inspecting attributes
 * and symbolic-link targets; changing permissions and ownership; and copying,
 * moving or deleting filesystem objects.
 *
 * The API also provides bounded streaming compression, authenticated file encryption
 * and SQLCipher import/export of file contents and selected metadata. These helpers
 * operate on stored data, not serialised open descriptors. Observe each operation's
 * path, overwrite, size-limit and verification policy; network transport belongs in
 * http.h rather than the file module.
 *
 * Paths are copied. Handles are not shared between threads without external
 * synchronisation. Failures set errno and the initiating handle's error code;
 * successful operations clear that code. Getters do not alter it.
 * Returned strings and arrays belong to the caller. No FILE or descriptor is exposed.
 */

#ifndef MARS_FILE_H
#define MARS_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct _array_t array_t;
typedef struct _string_t string_t;

/** Opaque copied path and optional file or directory stream. */
typedef struct _file_t file_t;

/** Opaque, immutable metadata snapshot. */
typedef struct _file_info_t file_info_t;

/** Opaque secretstream key; destroy with file_key_free, never serialise the structure. */
typedef struct _file_key_t file_key_t;
typedef struct _sqlite_t sqlite_t;

/** Linux inode type, including links queried without following them. */
typedef enum {
    FILE_TYPE_UNKNOWN,
    FILE_TYPE_REGULAR,
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_SYMLINK,
    FILE_TYPE_FIFO,
    FILE_TYPE_SOCKET,
    FILE_TYPE_CHARACTER_DEVICE,
    FILE_TYPE_BLOCK_DEVICE
} file_type_t;

/** File creation and opening policy. */
typedef enum {
    FILE_MODE_OPEN,           /**< Existing file only. */
    FILE_MODE_CREATE,         /**< Create or truncate. Requires write access. */
    FILE_MODE_CREATE_NEW,     /**< Create exclusively; fail if already present. */
    FILE_MODE_TRUNCATE,       /**< Truncate an existing file; requires write access. */
    FILE_MODE_OPEN_OR_CREATE, /**< Open without truncation, or create. */
    FILE_MODE_APPEND          /**< Create or append; requires write-only access. */
} file_mode_t;

/** Permitted stream operations. */
typedef enum {
    FILE_ACCESS_READ,
    FILE_ACCESS_WRITE,
    FILE_ACCESS_READ_WRITE
} file_access_t;

/** Origin for a byte-based seek. */
typedef enum {
    FILE_SEEK_BEGIN,
    FILE_SEEK_CURRENT,
    FILE_SEEK_END
} file_seek_t;

/** Portable subset of file attributes; hidden is inferred from a dot-prefixed basename. */
typedef enum {
    FILE_ATTRIBUTE_NONE = 0,
    FILE_ATTRIBUTE_READ_ONLY = 1,
    FILE_ATTRIBUTE_HIDDEN = 2
} file_attributes_t;

/**
 * @brief Allocates a closed handle with a copied path; rejects empty paths and embedded NUL bytes.
 * @param[in] path Required non-empty borrowed path string without embedded NUL bytes; copied into the new handle.
 * @return New caller-owned, closed handle, or NULL with errno set; release with file_free.
 */
file_t *file_new(const string_t *path);

/**
 * @brief Allocates a closed handle with a copied, non-empty C path; returns NULL on failure.
 * @param[in] path Required non-empty NUL-terminated Linux byte path; copied without Unicode normalisation.
 * @return New caller-owned, closed handle, or NULL with errno set; release with file_free.
 */
file_t *file_new_cstr(const char *path);

/**
 * @brief Closes and frees a handle; NULL is safe. Use file_close first to observe delayed write errors.
 * @param[in] file Owned handle to close and destroy; NULL is safe. Delayed close errors are not returned.
 */
void file_free(file_t *file);

/**
 * @brief Returns the borrowed immutable path, or NULL for a null handle.
 * @param[in] file Borrowed handle to inspect; ownership is unchanged.
 * @return Borrowed immutable path valid until file_free, or NULL for a null handle; do not free it.
 */
const char *file_path(const file_t *file);

/**
 * @brief Returns the last errno-style error, zero for success, or EINVAL for a null handle.
 * @param[in] file Borrowed handle to inspect; ownership is unchanged.
 * @return Last error code, zero after success, or EINVAL for NULL.
 */
int file_last_error(const file_t *file);

/**
 * @brief Returns borrowed system error text corresponding to file_last_error.
 * @param[in] file Borrowed handle to inspect; ownership is unchanged.
 * @return Borrowed system error text; do not free or modify it.
 */
const char *file_last_error_message(const file_t *file);

/**
 * @brief Opens a closed handle; rejects symlinks, directories and special files. Already open gives EBUSY.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @param[in] mode Opening or creation policy; see file_mode_t.
 * @param[in] access Requested read/write access; must be compatible with mode.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_open(file_t *file, file_mode_t mode, file_access_t access);

/**
 * @brief Open a regular file, explicitly allowing final symbolic links, including creation through a dangling link.
 * Other mode, access and ownership rules match file_open; this is not a confined-path operation.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @param[in] mode Opening or creation policy; see file_mode_t.
 * @param[in] access Requested read/write access; must be compatible with mode.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_open_follow(file_t *file, file_mode_t mode, file_access_t access);

/**
 * @brief Creates or truncates a read/write stream.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_create(file_t *file);

/**
 * @brief Creates or truncates a write-only UTF-8 stream; emits no BOM.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_create_text(file_t *file);

/**
 * @brief Opens an existing read-only stream.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_open_read(file_t *file);

/**
 * @brief Opens an existing UTF-8 stream for lazy file_read_line calls.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_open_text(file_t *file);

/**
 * @brief Opens or creates a write-only stream at offset zero without truncating existing contents.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_open_write(file_t *file);

/**
 * @brief Opens or creates an append-only UTF-8 stream.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_append_text(file_t *file);

/**
 * @brief Closes the stream, retaining its path; closing an already closed handle succeeds.
 * @param[in,out] file Borrowed handle whose stream is to be closed; the handle itself is retained.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_close(file_t *file);

/**
 * @brief Returns whether the handle currently owns an open stream.
 * @param[in] file Borrowed handle to inspect; ownership is unchanged.
 * @return True for an open file or directory stream; false for a closed or NULL handle.
 */
bool file_is_open(const file_t *file);

/**
 * @brief Flushes buffered output; this does not guarantee durable storage after power failure.
 * @param[in,out] file Borrowed open regular-file stream with suitable access; receives errors.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_flush(file_t *file);

/**
 * @brief Reads up to capacity bytes; required read_count is zero at EOF. Partial reads precede errors.
 * @param[in,out] file Borrowed open regular-file stream with suitable access; receives errors.
 * @param[in] buffer Caller-owned writable buffer of at least capacity bytes; NULL is allowed only at zero capacity.
 * @param[in] capacity Available buffer capacity in bytes; may be zero.
 * @param[out] read_count Required output pointer; receives bytes read, including partial progress on failure; zero at
 * EOF.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_read(file_t *file, void *buffer, size_t capacity, size_t *read_count);

/**
 * @brief Writes bytes; required written_count reports progress even on failure. NULL data is valid only at size zero.
 * @param[in,out] file Borrowed open regular-file stream with suitable access; receives errors.
 * @param[in] data Borrowed raw bytes to write; NULL is allowed only when size is zero.
 * @param[in] size Number of bytes to write.
 * @param[out] written_count Required output pointer; receives bytes written, including partial progress on failure.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_write(file_t *file, const void *data, size_t size, size_t *written_count);

/**
 * @brief Seeks by a signed byte offset, clearing EOF; subsequent append writes still go to the end.
 * @param[in,out] file Borrowed open regular-file stream with suitable access; receives errors.
 * @param[in] offset Signed byte displacement from origin; the resulting position must be valid.
 * @param[in] origin Reference position: beginning, current position or end of file.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_seek(file_t *file, int64_t offset, file_seek_t origin);

/**
 * @brief Stores the current byte position in the required output pointer.
 * @param[in,out] file Borrowed open regular-file stream with suitable access; receives errors.
 * @param[out] position Required output pointer; receives the byte offset on success.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_tell(file_t *file, int64_t *position);

/**
 * @brief Takes a shared or exclusive advisory lock; non-waiting conflicts report EWOULDBLOCK.
 * @param[in,out] file Borrowed open regular-file stream with suitable access; receives errors.
 * @param[in] exclusive True requests an exclusive lock; false requests a shared lock.
 * @param[in] wait True waits for the lock; false fails immediately if it conflicts.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_lock(file_t *file, bool exclusive, bool wait);

/**
 * @brief Releases this stream's advisory lock.
 * @param[in,out] file Borrowed open regular-file stream with suitable access; receives errors.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_unlock(file_t *file);

/**
 * @brief Reads one NFC-normalised UTF-8 line, stripping LF, CRLF or CR and an initial BOM; NULL output means EOF.
 * @param[in,out] file Borrowed open regular-file stream with suitable access; receives errors.
 * @param[out] line Required output pointer; receives an owned string or NULL at EOF. Release strings with string_free.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_read_line(file_t *file, string_t **line);

/**
 * @brief Writes validated UTF-8 text verbatim, including embedded NUL bytes.
 * @param[in,out] file Borrowed open regular-file stream with suitable access; receives errors.
 * @param[in] text Required borrowed UTF-8 string; not consumed or modified.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_write_text(file_t *file, const string_t *text);

/**
 * @brief Writes validated UTF-8 text followed by LF.
 * @param[in,out] file Borrowed open regular-file stream with suitable access; receives errors.
 * @param[in] text Required borrowed UTF-8 string; not consumed or modified.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_write_line(file_t *file, const string_t *text);

/**
 * @brief Reads a closed handle's entire file into an owned array of unsigned char; NULL means failure.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @return Owned byte array, including an empty array at EOF; NULL on failure. Release with array_destroy.
 */
array_t *file_read_all_bytes(file_t *file);

/**
 * @brief Reads a closed handle's UTF-8 file into an owned NFC-normalised string, stripping an initial BOM.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @return Owned NFC-normalised string, or NULL on failure; release with string_free.
 */
string_t *file_read_all_text(file_t *file);

/**
 * @brief Reads all lines into an owned array of string_t pointers; array_destroy also frees those strings.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @return Owned array, or NULL on failure; array_destroy also frees the contained strings.
 */
array_t *file_read_all_lines(file_t *file);

/**
 * @brief Creates or truncates a closed handle's file and writes raw bytes, then closes it.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @param[in] data Borrowed raw bytes to write; NULL is allowed only when size is zero.
 * @param[in] size Number of bytes to write.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_write_all_bytes(file_t *file, const void *data, size_t size);

/**
 * @brief Validates text before creating or truncating the file, writes it, and closes the stream.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @param[in] text Required borrowed UTF-8 string; not consumed or modified.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_write_all_text(file_t *file, const string_t *text);

/**
 * @brief Validates an array of string_t pointers, writes every line with LF, and closes the stream.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @param[in] lines Required borrowed array of string_t pointers; neither array nor strings are consumed.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_write_all_lines(file_t *file, const array_t *lines);

/**
 * @brief Appends validated UTF-8 text to a closed handle's file, creating it if necessary.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @param[in] text Required borrowed UTF-8 string; not consumed or modified.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_append_all_text(file_t *file, const string_t *text);

/**
 * @brief Appends an array of validated string_t pointers with LF after each, creating the file if necessary.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @param[in] lines Required borrowed array of string_t pointers; neither array nor strings are consumed.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_append_all_lines(file_t *file, const array_t *lines);

/**
 * @brief Reports existence of any directory entry, including dangling links; absence has error code zero.
 * @param[in] file Borrowed handle to inspect; ownership is unchanged.
 * @return True if an entry exists; false for absence or failure. Absence leaves error code zero.
 */
bool file_exists(file_t *file);

/**
 * @brief Unlinks a closed handle's non-directory entry without following symlinks; absence is a successful no-op.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_delete(file_t *file);

/**
 * @brief Copies between closed handles, staging data before installation; overwrite must be explicitly enabled.
 * @param[in,out] source Borrowed, closed source handle; receives errors and remains caller-owned.
 * @param[in,out] destination Borrowed, closed destination-path handle; ownership is unchanged.
 * @param[in] overwrite True permits destination replacement; false refuses an existing destination.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_copy(file_t *source, file_t *destination, bool overwrite);

/**
 * @brief Moves files, directories or links; cross-device regular files copy then unlink. Paths are not retargeted.
 * @param[in,out] source Borrowed, closed source handle; receives errors and remains caller-owned.
 * @param[in,out] destination Borrowed, closed destination-path handle; ownership is unchanged.
 * @param[in] overwrite True permits destination replacement; false refuses an existing destination.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_move(file_t *source, file_t *destination, bool overwrite);

/**
 * @brief Atomically replaces an existing destination on one filesystem; optional backup must not already exist.
 * @param[in,out] source Borrowed, closed source handle; receives errors and remains caller-owned.
 * @param[in,out] destination Borrowed, closed destination-path handle; ownership is unchanged.
 * @param[in] backup Optional borrowed, closed backup handle; NULL disables backup. Its path must not already exist.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_replace(file_t *source, file_t *destination, file_t *backup);

/**
 * @brief Returns an owned metadata snapshot for a closed path without following its final symlink.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @return Owned metadata snapshot, or NULL on failure; release with file_info_free.
 */
file_info_t *file_get_info(file_t *file);

/**
 * @brief Frees a metadata snapshot; NULL is safe.
 * @param[in] info Owned snapshot to destroy; NULL is safe.
 */
void file_info_free(file_info_t *info);

/**
 * @brief Returns the snapshot's byte size, or zero for NULL.
 * @param[in] info Borrowed immutable metadata snapshot; ownership is unchanged.
 * @return Byte size recorded in the snapshot, or zero for NULL.
 */
uint64_t file_info_size(const file_info_t *info);

/**
 * @brief Returns snapshot attribute bits, or FILE_ATTRIBUTE_NONE for NULL.
 * @param[in] info Borrowed immutable metadata snapshot; ownership is unchanged.
 * @return Bitwise file_attributes_t values, or FILE_ATTRIBUTE_NONE for NULL.
 */
unsigned file_info_attributes(const file_info_t *info);

/**
 * @brief Retrieves UTC Unix seconds and nanoseconds of creation; false if unavailable, never substitutes ctime.
 * @param[in] info Borrowed immutable metadata snapshot; ownership is unchanged.
 * @param[out] seconds Required output pointer; receives signed UTC Unix seconds on success.
 * @param[out] nanoseconds Required output pointer; receives fractional nanoseconds (0 through 999999999) on success.
 * @return True on success; false with errno set to EINVAL for invalid arguments or ENOTSUP if unavailable.
 */
bool file_info_creation_time(const file_info_t *info, int64_t *seconds, long *nanoseconds);

/**
 * @brief Retrieves UTC Unix seconds and nanoseconds of last content modification; outputs are required.
 * @param[in] info Borrowed immutable metadata snapshot; ownership is unchanged.
 * @param[out] seconds Required output pointer; receives signed UTC Unix seconds on success.
 * @param[out] nanoseconds Required output pointer; receives fractional nanoseconds (0 through 999999999) on success.
 * @return True on success; false with errno set to EINVAL for invalid arguments or ENOTSUP if unavailable.
 */
bool file_info_last_write_time(const file_info_t *info, int64_t *seconds, long *nanoseconds);

/**
 * @brief Retrieves current attribute bits into a required output pointer.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @param[out] attributes Required output pointer; receives the current file_attributes_t bitmask.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_get_attributes(file_t *file, unsigned *attributes);

/**
 * @brief Sets read-only permissions; clearing it restores owner-write only. Changing hidden status gives ENOTSUP.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @param[in] attributes Desired file_attributes_t bitmask; hidden status must match the existing filename.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_set_attributes(file_t *file, unsigned attributes);

/**
 * @brief Creates a directory with mode bits subject to umask; optionally creates missing parents.
 * @param[in,out] directory Borrowed, closed directory-path handle. Receives errors; remains caller-owned.
 * @param[in] permissions Linux permission and special mode bits (07777); other bits are invalid.
 * @param[in] parents True creates missing parent directories; false creates only the named directory.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_create_directory(file_t *directory, unsigned permissions, bool parents);

/**
 * @brief Removes an empty directory, never recursively and never through a final symlink.
 * @param[in,out] directory Borrowed, closed directory-path handle. Receives errors; remains caller-owned.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_remove_directory(file_t *directory);

/**
 * @brief Opens a directory for lazy listing; regular stream reads are unavailable on this handle.
 * @param[in,out] directory Borrowed, closed directory-path handle. Receives errors; remains caller-owned.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_open_directory(file_t *directory);

/**
 * @brief Returns the next owned entry snapshot, excluding dot entries; success with NULL output means EOF.
 * @param[in,out] directory Borrowed directory handle opened with file_open_directory. Receives errors; remains
 * caller-owned.
 * @param[out] entry Required output pointer; receives an owned snapshot or NULL at EOF. Release with file_info_free.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_read_directory(file_t *directory, file_info_t **entry);

/**
 * @brief Returns an owned array of file_info_t pointers; array_destroy frees snapshots. Order is filesystem-defined.
 * @param[in,out] directory Borrowed, closed directory-path handle. Receives errors; remains caller-owned.
 * @return Owned snapshot array, or NULL on failure; array_destroy frees its snapshots.
 */
array_t *file_list_directory(file_t *directory);

/**
 * @brief Returns the snapshot's borrowed raw basename; it remains valid until file_info_free.
 * @param[in] info Borrowed immutable metadata snapshot; ownership is unchanged.
 * @return Borrowed raw basename valid until file_info_free, or NULL for NULL; do not free it.
 */
const char *file_info_name(const file_info_t *info);

/**
 * @brief Returns the raw target text captured for a symbolic link.
 * @param info Borrowed metadata snapshot from a directory listing or file_get_info.
 * @return Borrowed NUL-terminated target text, or NULL for a non-link or NULL snapshot.
 * @details Relative text is interpreted from the link's parent, not the process working directory.
 * The text survives directory closure and remains valid until file_info_free; do not free or modify it.
 */
const char *file_info_link_target(const file_info_t *info);

/**
 * @brief Returns metadata for a symbolic link's ultimate target.
 * @param info Borrowed link snapshot from a directory listing or file_get_info.
 * @return Borrowed target snapshot, or NULL for a non-link, NULL input or unresolved target.
 * @details Use ordinary file_info getters to inspect the target's type, permissions and attributes.
 * The name and hidden flag describe the listed link name; other fields describe the target inode.
 * The containing snapshot owns this result; do not free it separately. No file contents are opened.
 * Link and target metadata are captured separately, not atomically. Inspect file_info_target_error
 * when resolution fails; dangling links and loops still appear in listings.
 */
const file_info_t *file_info_target_info(const file_info_t *info);

/**
 * @brief Reports the stored error from resolving a link target.
 * @param info Borrowed metadata snapshot.
 * @return Zero for a resolved target or non-link; errno-style resolution error otherwise; EINVAL for NULL.
 * @details ENOENT indicates a missing target, ELOOP a link loop, and EACCES denied traversal.
 * Does not perform I/O or change errno or the originating handle's error.
 */
int file_info_target_error(const file_info_t *info);

/**
 * @brief Returns the inode type, or FILE_TYPE_UNKNOWN for NULL.
 * @param[in] info Borrowed immutable metadata snapshot; ownership is unchanged.
 * @return Recorded inode type, or FILE_TYPE_UNKNOWN for NULL.
 */
file_type_t file_info_type(const file_info_t *info);

/**
 * @brief Returns permission and special mode bits (07777), or zero for NULL.
 * @param[in] info Borrowed immutable metadata snapshot; ownership is unchanged.
 * @return Recorded mode bits masked to 07777, or zero for NULL.
 */
unsigned file_info_permissions(const file_info_t *info);

/**
 * @brief Returns the numeric owner UID, or UINT32_MAX for NULL.
 * @param[in] info Borrowed immutable metadata snapshot; ownership is unchanged.
 * @return Recorded numeric UID, or UINT32_MAX for NULL.
 */
uint32_t file_info_owner(const file_info_t *info);

/**
 * @brief Returns the numeric group GID, or UINT32_MAX for NULL.
 * @param[in] info Borrowed immutable metadata snapshot; ownership is unchanged.
 * @return Recorded numeric GID, or UINT32_MAX for NULL.
 */
uint32_t file_info_group(const file_info_t *info);

/**
 * @brief Returns the inode's hard-link count, or zero for NULL.
 * @param[in] info Borrowed immutable metadata snapshot; ownership is unchanged.
 * @return Recorded hard-link count, or zero for NULL.
 */
uint64_t file_info_link_count(const file_info_t *info);

/**
 * @brief Retrieves UTC Unix seconds and nanoseconds of last access; outputs are required.
 * @param[in] info Borrowed immutable metadata snapshot; ownership is unchanged.
 * @param[out] seconds Required output pointer; receives signed UTC Unix seconds on success.
 * @param[out] nanoseconds Required output pointer; receives fractional nanoseconds (0 through 999999999) on success.
 * @return True on success; false with errno set to EINVAL for invalid arguments or ENOTSUP if unavailable.
 */
bool file_info_last_access_time(const file_info_t *info, int64_t *seconds, long *nanoseconds);

/**
 * @brief Retrieves UTC Unix seconds and nanoseconds of inode status change, not birth; outputs are required.
 * @param[in] info Borrowed immutable metadata snapshot; ownership is unchanged.
 * @param[out] seconds Required output pointer; receives signed UTC Unix seconds on success.
 * @param[out] nanoseconds Required output pointer; receives fractional nanoseconds (0 through 999999999) on success.
 * @return True on success; false with errno set to EINVAL for invalid arguments or ENOTSUP if unavailable.
 */
bool file_info_status_change_time(const file_info_t *info, int64_t *seconds, long *nanoseconds);

/**
 * @brief Applies chmod permission/special bits to a closed file or directory; refuses final symlinks.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @param[in] permissions Linux permission and special mode bits (07777); other bits are invalid.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_chmod(file_t *file, unsigned permissions);

/**
 * @brief Changes numeric owner/group without following the final symlink; -1 leaves that field unchanged.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @param[in] owner Numeric user ID, or -1 to leave the owner unchanged.
 * @param[in] group Numeric group ID, or -1 to leave the group unchanged.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_chown(file_t *file, int64_t owner, int64_t group);

/**
 * @brief Changes access and modification timestamps without following the final symlink; nanoseconds must be valid.
 * @param[in,out] file Borrowed, closed path handle; receives errors and remains caller-owned.
 * @param[in] access_seconds Access timestamp as signed seconds since the Unix epoch, in UTC.
 * @param[in] access_nanoseconds Access timestamp's fractional nanoseconds, from 0 through 999999999.
 * @param[in] write_seconds Modification timestamp as signed seconds since the Unix epoch, in UTC.
 * @param[in] write_nanoseconds Modification timestamp's fractional nanoseconds, from 0 through 999999999.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_set_times(file_t *file, int64_t access_seconds, long access_nanoseconds,
                    int64_t write_seconds, long write_nanoseconds);

/**
 * @brief Tests requested access using effective credentials; all false tests existence. Follows symlinks.
 * @param[in] file Borrowed handle to inspect; ownership is unchanged.
 * @param[in] read_access Whether to require read permission.
 * @param[in] write_access Whether to require write permission.
 * @param[in] execute_access Whether to require execute permission (search permission for directories).
 * @return True on success; false on failure (see file_last_error).
 */
bool file_check_access(file_t *file, bool read_access, bool write_access, bool execute_access);

/**
 * @brief Truncates or extends a regular file to a non-negative byte size; an open stream must be writable.
 * @param[in,out] file Borrowed closed regular-file handle or open writable stream; receives errors.
 * @param[in] size Desired non-negative file length in bytes; extension adds zero-filled space.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_truncate(file_t *file, int64_t size);

/**
 * @brief Flushes and fsyncs an open file or directory; data_only selects fdatasync for file streams.
 * @param[in,out] file Borrowed open file or directory handle; receives errors.
 * @param[in] data_only True uses fdatasync for files; false uses fsync. Directories always use fsync.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_sync(file_t *file, bool data_only);

/**
 * @brief Creates a new hard link to a regular source; the destination must not exist.
 * @param[in,out] source Borrowed, closed source handle; receives errors and remains caller-owned.
 * @param[in,out] destination Borrowed, closed destination-path handle; ownership is unchanged.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_create_hard_link(file_t *source, file_t *destination);

/**
 * @brief Creates a new symlink containing target's raw path, which need not exist; relative text is stored verbatim.
 * @param[in,out] target Borrowed, closed handle containing the target path to store verbatim; receives errors.
 * @param[in,out] link Borrowed, closed symbolic-link path handle; ownership is unchanged.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_create_symlink(file_t *target, file_t *link);

/**
 * @brief Returns an allocated raw symlink target through the required output pointer; release it with free.
 * @param[in,out] link Borrowed, closed symbolic-link path handle; ownership is unchanged.
 * @param[out] target Required output pointer; receives allocated NUL-terminated raw target bytes, or NULL on failure.
 * Free with free.
 * @return True on success; false on failure (see file_last_error).
 */
bool file_read_link(file_t *link, char **target);

/**
 * @brief Returns a new closed handle for realpath's canonical absolute path, resolving symlinks.
 * @param[in] file Borrowed handle to inspect; ownership is unchanged.
 * @return New caller-owned, closed canonical-path handle, or NULL on failure; release with file_free.
 */
file_t *file_resolve(file_t *file);

/**
 * @brief Compresses a regular file into one checksummed Zstandard frame.
 * @param source Borrowed, closed source handle; receives errno-style errors.
 * @param destination Borrowed, closed destination handle.
 * @param overwrite Whether to atomically replace an existing regular destination.
 * @return True after successful publication; false on failure.
 * @pre Both paths have trusted parent directories and must not refer to the same inode.
 * @details Uses level 3, a 1 MiB window and bounded streaming buffers. Final symlinks
 * are rejected. A private mode-0600 sibling file is published only after successful
 * completion and close; an existing destination is unchanged on failure. Handles
 * remain caller-owned and are closed after the operation. No metadata is copied.
 * @note Atomic visibility does not guarantee power-loss durability. A crash can leave a staging file.
 * @see file_decompress_zstd
 */
bool file_compress_zstd(file_t *source, file_t *destination, bool overwrite);

/**
 * @brief Decompresses exactly one ordinary Zstandard frame with bounded resource use.
 * @param source Borrowed, closed compressed-file handle; receives errors.
 * @param destination Borrowed, closed output handle.
 * @param max_bytes Maximum decompressed size in bytes; zero accepts only empty output.
 * @param overwrite Whether to atomically replace an existing regular destination.
 * @return True after successful publication; false on failure.
 * @details Accepts windows up to 8 MiB. Rejects skippable or concatenated frames,
 * trailing data, malformed input and truncation. EFBIG reports a window or output
 * limit violation; EBADMSG reports invalid input. Checksums are verified when present,
 * but do not provide cryptographic authentication.
 * @note Ownership, closed-handle, alias, symlink and staging rules match file_compress_zstd.
 * @see file_compress_zstd
 */
bool file_decompress_zstd(file_t *source, file_t *destination, uint64_t max_bytes, bool overwrite);

/**
 * @brief Generates an unpredictable 32-byte secretstream encryption key.
 * @return A caller-owned key, or NULL with errno set on failure.
 * @details Uses libsodium's guarded allocation and random generator. Release the key
 * with file_key_free. This does not persist the key: retain it securely to decrypt later.
 * @note Keys are not passwords; no password derivation or external key storage is performed.
 * @see file_key_export
 * @see file_key_from_bytes
 */
file_key_t *file_key_generate(void);

/**
 * @brief Copies raw encryption key material into an opaque key.
 * @param bytes Required pointer to exactly 32 secret key bytes; ownership is unchanged.
 * @param size Length in bytes; must equal 32.
 * @return A caller-owned key, or NULL with errno set on failure.
 * @details NULL input or a different length gives EINVAL. Input is copied and may be
 * erased by the caller afterwards. This is not a password derivation function.
 * @see file_key_free
 */
file_key_t *file_key_from_bytes(const void *bytes, size_t size);

/**
 * @brief Copies an opaque key into caller-owned secret storage.
 * @param key Required borrowed key.
 * @param bytes Required writable buffer of exactly 32 bytes.
 * @param size Buffer size in bytes; must equal 32.
 * @return True on success; false with errno set to EINVAL for invalid arguments.
 * @warning The caller must protect and erase the exported copy. Freeing the opaque
 * key does not erase this buffer. No key ownership is transferred.
 */
bool file_key_export(const file_key_t *key, void *bytes, size_t size);

/**
 * @brief Erases and releases an opaque encryption key.
 * @param key Owned key to destroy; NULL is safe.
 * @details Invalidates the key and its aliases. Exported caller-owned copies are
 * unaffected and must be erased separately.
 */
void file_key_free(file_key_t *key);

/**
 * @brief Encrypts a file using authenticated XChaCha20-Poly1305 secretstream records.
 * @param source Borrowed, closed plaintext handle; receives errors.
 * @param destination Borrowed, closed encrypted-output handle.
 * @param key Required borrowed key; remains caller-owned and is not modified.
 * @param compress Whether to compress with Zstandard before encryption.
 * @param overwrite Whether to atomically replace an existing regular destination.
 * @return True after successful publication; false on failure.
 * @details Writes the version-1 MARS container with a fresh random stream header,
 * authenticated record framing and a mandatory final record. Compression feeds
 * encryption directly without a plaintext intermediate file. A NULL key gives EINVAL.
 * Ownership, alias, symlink and staging rules match file_compress_zstd.
 * @warning Compression can leak information through ciphertext length when attacker-controlled
 * input shares a compression context with secrets. Disable compression in that situation.
 * @see file_decrypt
 */
bool file_encrypt(file_t *source, file_t *destination, const file_key_t *key, bool compress, bool overwrite);

/**
 * @brief Authenticates and decrypts a complete MARS encrypted-file container.
 * @param source Borrowed, closed encrypted-file handle; receives errors.
 * @param destination Borrowed, closed plaintext-output handle.
 * @param key Required borrowed key; remains caller-owned and is not modified.
 * @param max_bytes Maximum final plaintext size, after any decompression; zero permits only empty output.
 * @param overwrite Whether to atomically replace an existing regular destination.
 * @return True after complete verification and publication; false on failure.
 * @details The authenticated header selects decompression automatically. Wrong keys,
 * corruption, truncation, invalid records and trailing data give EBADMSG; output
 * or decompression-window limits give EFBIG. A NULL key gives EINVAL. Publication
 * requires a valid final record and exact EOF. Staging rules match file_compress_zstd.
 * @warning Plaintext staging has mode 0600 but is not securely erased. Normal failure
 * cleanup removes it; a crash may leave it behind. Parent directories must be trusted.
 * @see file_encrypt
 */
bool file_decrypt(file_t *source, file_t *destination, const file_key_t *key, uint64_t max_bytes, bool overwrite);

/**
 * @brief Imports file contents and selected metadata into a named SQLCipher object.
 * @param source Borrowed, closed regular-file handle; receives errors.
 * @param db Borrowed open SQLCipher handle, externally serialised during the transfer.
 * @param name Borrowed non-empty object name without embedded NUL bytes.
 * @param max_bytes Maximum source size in bytes; zero accepts only an empty file.
 * @return True after releasing the transfer savepoint; false on failure.
 * @details Creates the object-store schema if needed and atomically replaces any
 * object with this name. Stores bounded 64 KiB chunks, size, mode bits 0777,
 * modification time and a SHA-256 hash, never descriptors, paths or ownership.
 * Failure rolls back the previous object and chunks. Success participates in an
 * enclosing transaction and is not an independent commit.
 * @note Rejects final symlinks and the main database inode. EFBIG denotes the size
 * limit; ESTALE denotes a detected source change; database failures generally give EIO.
 * Before/after metadata checks are not a hostile-writer snapshot guarantee.
 * @note SQLCipher already encrypts storage. Call file_encrypt first only when separate
 * payload encryption is wanted. Source ownership stays with the caller; it is closed on return.
 * @see file_export_sqlite
 */
bool file_import_sqlite(file_t *source, sqlite_t *db, const string_t *name, uint64_t max_bytes);

/**
 * @brief Exports a stored file through a consistent snapshot and verified staging file.
 * @param db Borrowed open SQLCipher handle, externally serialised during the transfer.
 * @param name Borrowed non-empty file-object name without embedded NUL bytes.
 * @param destination Borrowed, closed output handle; receives errors.
 * @param max_bytes Maximum stored content size in bytes; zero accepts only an empty file.
 * @param overwrite Whether to atomically replace an existing regular destination.
 * @param restore_metadata Whether to restore stored mode bits 0777 and modification time.
 * @return True after successful verification and publication; false on failure.
 * @details Checks chunk sequence, lengths, total size and content hash. Missing or
 * malformed objects give EBADMSG; excessive size gives EFBIG. Does not decrypt a
 * separately encrypted payload: use file_decrypt afterwards. Without metadata
 * restoration output mode is 0600; ownership, special mode bits, ACLs and extended
 * attributes are never restored. Rejects the main database inode and final symlinks.
 * Staged publication leaves an existing destination unchanged on failure.
 * @note An outer database rollback cannot undo an already published filesystem file.
 * A content hash detects accidental damage, not malicious rewriting with database access.
 * Do not target database journal or WAL files. Staging limitations match file_compress_zstd.
 * @see file_import_sqlite
 */
bool file_export_sqlite(sqlite_t *db, const string_t *name, file_t *destination, uint64_t max_bytes,
                        bool overwrite, bool restore_metadata);

#endif
