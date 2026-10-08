# `file_t`, `file_info_t` and `file_key_t`

Linux file and directory I/O with opaque handles, UTF-8 text helpers, permissions,
ownership, links and immutable metadata snapshots. Include `file.h`; include `ustring.h` or `array.h` when
using their returned objects. The API follows the operations described in the
[ZetCode file tutorial](https://zetcode.com/csharp/file/), adapted to Linux and
MARS ownership conventions rather than reproducing .NET classes or exceptions.

## Compression, encryption and object storage

These are explicit operations: ordinary reads and writes do not automatically
compress or encrypt. Include `file.h`; the database helpers also use `sqlite.h`.
See the [build guide](building.md#file-compression-and-encryption-dependencies)
for Zstandard and libsodium installation requirements.

### Bounded streaming compression

`file_compress_zstd(source, destination, overwrite)` writes one standard Zstandard
frame at level 3, with a checksum and a fixed 1 MiB compression window.
`file_decompress_zstd(source, destination, max_bytes, overwrite)` accepts one
ordinary frame, limits its window to 8 MiB, and rejects trailing bytes,
concatenated or skippable frames, malformed data and incomplete frames.
The size limit is on decompressed bytes; zero permits only empty output.
Buffers are 64 KiB and codec memory is bounded independently of file length.
A checksum is not cryptographic authentication.

### Authenticated streaming encryption

`file_key_generate()` creates an opaque `file_key_t` containing a random
32-byte key in libsodium's guarded allocation. `file_key_from_bytes(bytes, 32)`
imports an existing key; `file_key_export(key, bytes, 32)` copies it to caller-owned
secret storage. `file_key_free()` erases and releases the key. Key helper failures
use `errno`. Preserve the key separately and securely: losing it loses the data.
These functions accept raw keys, **not passwords**; no password derivation or
key-storage policy is implied. Callers must erase any exported copies themselves.

`file_encrypt(source, destination, key, compress, overwrite)` uses libsodium
XChaCha20-Poly1305 secretstream with fresh random stream headers. When `compress`
is true, Zstandard output feeds encryption directly without a plaintext
intermediate file. `file_decrypt(source, destination, key, max_bytes, overwrite)`
automatically recognises that flag; its limit applies to final plaintext,
including decompression. Wrong keys, altered headers or records, missing,
reordered or truncated records and trailing data fail verification.

The version-1 MARS encrypted container has a 40-byte header: `MARSENC`, a version
byte of 1, a compression flag (0 or 1), seven reserved zero bytes, and libsodium's
24-byte header. Records have a four-byte big-endian length followed by up to
64 KiB of plaintext plus the 17-byte authentication overhead. The complete header
and record length are authenticated as associated data. A separate empty
`TAG_FINAL` record and exact end of file are mandatory. This container is not
interchangeable with arbitrary secretstream applications without this framing.

Compression can reveal information through ciphertext length when an attacker
can influence data compressed alongside secrets. Disable it for that situation.

### Transactional SQLCipher file storage

`file_import_sqlite(source, db, name, max_bytes)` stores file contents in
64 KiB `mars_file_chunk` rows belonging to the existing `mars_object` store.
The object has type `file`, encoding `mars/file-chunks-v1`, and a 64-byte
versioned manifest containing size, ordinary permission bits, modification time
with nanosecond precision, and a SHA-256 content hash. No descriptor, pathname,
ownership, set-ID bits, ACLs or extended attributes are serialised. Source
metadata is checked again after reading to detect ordinary concurrent changes;
this is not a snapshot guarantee against a hostile writer.

`file_export_sqlite(db, name, destination, max_bytes, overwrite, restore_metadata)`
checks chunk order, lengths, total size and hash before publishing the file.
With `restore_metadata` true it restores only the stored 0777 permission bits
and modification time; otherwise the new file has mode 0600. Missing or malformed
file objects fail rather than creating partial output. A hash detects accidental
damage, not malicious rewriting by someone with database access.

Imports use a savepoint, rolling back the previous object and chunks on failure.
They participate in any surrounding transaction. Exports read a consistent
database snapshot; later rollback of an outer transaction does not undo an
already exported filesystem file. Do not concurrently use the same database
handle during a transfer. Object replacement clears obsolete chunks; deleting
an object cascades to its chunks when foreign keys are enabled (as they are by
the MARS database opener). Imports and exports reject the main database file
itself as source or destination.

SQLCipher already encrypts stored contents. Additional payload encryption is
optional: explicitly encrypt a file before importing it, then export and decrypt
it when needed. This also keeps the independent payload key out of the database.

### Publication and failure behaviour

All these operations require closed regular-file handles, reject final symlinks
and input/output inode aliases, and leave an existing destination unchanged on
failure. Output is written to an exclusive mode-0600 sibling temporary file and
renamed only after successful completion and close. Decryption additionally
requires complete authentication; SQLCipher export requires full hash verification.
`overwrite = false` atomically refuses an existing destination. Compression,
encryption and import errors belong to the source handle; export errors belong
to the destination. Limits report `EFBIG`, malformed streams `EBADMSG`, and
database-operation failures generally `EIO`.

Staging provides atomic visibility, not guaranteed power-loss durability.
Temporary decrypted plaintext is protected by filesystem permissions but is
not securely erased; a crash may leave a staging file. Parent directories must
be trusted and protected against hostile pathname replacement. Do not use
database journal or WAL files as transfer destinations.

## Transfer API conventions

The [complete public API reference](#complete-public-api-reference) documents
all file-module functions, not just compression, encryption and storage.
The following conventions apply specifically to those transfer APIs.
File handles, database handles, names and input keys are borrowed, never consumed.
File handles must be closed before a transfer. A rejected already-open handle
remains open; valid closed handles are closed again when the transfer finishes.
Generated/imported keys belong to the caller and require `file_key_free`.
Exported raw key bytes remain the caller's responsibility, including erasure.

`max_bytes` is an unsigned 64-bit byte count, not a compression ratio or memory
budget. Zero accepts only empty content. Decryption counts final plaintext after
decompression; SQLCipher export counts stored contents (ciphertext if the caller
previously imported an independently encrypted file).
`overwrite = false` rejects an existing destination; `true` replaces a regular
file only after success. Import always replaces an object of the same name,
subject to the enclosing transaction.

Transfer failures return false and set `errno` plus `file_last_error(source)`,
except export, which reports on `destination`. Key construction/export failures
use `errno` without a file handle. Successful transfers clear their initiating
handle's error; successful key helpers do not promise to clear `errno`.

| Error | Meaning |
| --- | --- |
| `EINVAL` | Invalid arguments, disallowed alias or unsupported file type |
| `EBUSY` | A required closed handle is already open |
| `EEXIST` | Destination exists and overwriting was not enabled |
| `ELOOP` | Refused final symbolic link |
| `EFBIG` | Output/source-size or decompression-window limit exceeded |
| `EBADMSG` | Invalid, incomplete or unauthenticated input; missing/malformed stored file |
| `ESTALE` | Source changed during SQLCipher import |
| `ENOMEM` | Allocation failure |
| `EIO` | General I/O, codec or database-operation failure |

Other filesystem errors, such as `ENOENT`, `EACCES` and `ENOSPC`, propagate.
See [publication and failure behaviour](#publication-and-failure-behaviour)
for staging limits and [examples](#examples) for complete calls and output.


## Ownership and errors

- `file_new` copies a MARS string path; `file_new_cstr` copies a Linux byte-string
  path without Unicode normalisation. Empty paths and embedded NUL bytes are
  rejected. Relative paths are resolved against the working directory at each
  operation. `file_path` returns a borrowed, immutable C string.
- A handle starts closed. `file_is_open` inspects its state. `file_close`
  closes the stream but keeps the path, and reports buffered write errors.
  `file_free` also closes and frees everything, but cannot report close errors.
  Both repeated close and freeing NULL are safe; closing NULL is invalid.
- Failures set `errno` and `file_last_error`; `file_last_error_message` supplies
  borrowed system error text. Successful operations clear the handle's error.
  Inspection getters do not clear it. Copy, move and replace report errors on
  the source handle. On construction failure inspect `errno`.
- `file_read_all_bytes` returns an `array_t` of `unsigned char`.
  `file_read_all_lines` returns an `array_t` of owned `string_t *` values.
  Use `array_destroy` for either result; it also frees the line strings.
  Treat the returned line array as owning its elements: do not shallow-copy its
  pointers into another owning array or destroy an individual borrowed line.
  `file_read_all_text` and `file_read_line` return strings freed with `string_free`.
- Stream operations require an open handle. Whole-file helpers and filesystem
  mutations require closed handles; they return `EBUSY` instead of silently
  closing or repositioning an existing stream. Metadata queries also require a
  closed handle. `file_exists` checks the pathname, not the currently open inode.
- Handles need external synchronisation if shared between threads. None of the
  operations changes the process working directory. Only `file_create_directory`
  with its parents flag creates missing parent directories.

## Opening and stream operations

| Mode | Behaviour |
|---|---|
| `FILE_MODE_OPEN` | Existing file only |
| `FILE_MODE_CREATE` | Create or truncate |
| `FILE_MODE_CREATE_NEW` | Exclusive creation; existing target gives `EEXIST` |
| `FILE_MODE_TRUNCATE` | Truncate an existing file |
| `FILE_MODE_OPEN_OR_CREATE` | Open at offset zero without truncation, or create |
| `FILE_MODE_APPEND` | Create or append; writes always reach the end |

`file_open` accepts `FILE_ACCESS_READ`, `FILE_ACCESS_WRITE` or
`FILE_ACCESS_READ_WRITE`. Creation with truncation, exclusive creation and
truncation require write access. Append requires write-only access. New files
use mode 0666 subject to the process umask. Convenience methods are:

- `file_create`: create/truncate, read/write.
- `file_create_text`: create/truncate, write-only.
- `file_open_read` and `file_open_text`: open an existing reader.
- `file_open_write`: create/open, write-only, **without truncation**. Writing
  fewer bytes than an existing file contains leaves the old tail in place.
- `file_append_text`: create/append, write-only.

`file_read` and `file_write` take byte counts and required progress outputs.
A successful read returning zero bytes means EOF. A failed operation may already
have transferred some bytes. Zero-length operations allow NULL data pointers.
`file_seek` accepts signed byte offsets and `FILE_SEEK_BEGIN`,
`FILE_SEEK_CURRENT` or `FILE_SEEK_END`; `file_tell` returns the logical byte
position as `int64_t`. The implementation handles read/write direction changes
on update streams. Seeking clears EOF, but does not disable append mode.
`file_flush` flushes stdio buffering; neither flush nor close promises power-loss
durability. `file_sync` flushes and requests `fsync`, or `fdatasync` when data-only
sync is selected; open directory handles support full metadata sync. Filesystem
and device guarantees still apply. `file_truncate` changes a regular file's length,
extending with zero bytes where necessary. It accepts a closed handle or an open
writable stream, preserves an existing stream's position and rejects negative
lengths. Check close after writing: a successful buffered write does not
prove the kernel accepted all the data.

## UTF-8 and whole-file helpers

`file_write_text` writes a MARS string; `file_write_line` adds LF. Writers do not
emit a BOM. `file_read_line` lazily reads one line, removes LF, CRLF or CR, and
accepts an optional UTF-8 BOM only at byte zero. It returns success with a NULL
line at EOF; an empty line is a non-NULL empty string. A trailing line ending
does not invent an extra empty line.

`file_read_all_text` strips an initial UTF-8 BOM but retains line endings.
Text reads validate complete UTF-8 before constructing a MARS string: malformed
input gives `EILSEQ`, not silent replacement. Returned strings preserve exact
UTF-8 spelling, including decomposed Unicode and literal credentials, without
NFC normalisation. Embedded NUL bytes are retained; use `string_byte_length`,
not `strlen`, to measure them. Text writers preserve these bytes too; ordinary
string mutators may normalise them, so use `string_append_utf8_exact` when adding
text whose spelling must be retained. Use binary helpers to preserve a BOM or
line endings removed by line readers. Other encodings, including UTF-16, are not decoded.

`file_write_all_bytes`, `file_write_all_text` and `file_write_all_lines`
create/truncate, write and close. `file_append_all_text` and
`file_append_all_lines` create/append and close. Line writers accept an array
whose element size is `sizeof(string_t *)`, borrow every string, and add LF
after each. Text and line arguments are checked before truncating the file.
Whole-file writers can leave partially written contents on I/O failure; use
staging plus `file_replace` when an existing destination must remain intact.

Whole-file readers use memory proportional to the complete file. Lazy line
reading uses memory proportional to the longest line plus bounded I/O buffers;
there is no fixed maximum line length. A file which grows continuously can keep
a whole-file read running: these are not snapshot readers or untrusted-input
size limiters.

## Copying, moving and replacing

`file_exists` checks any directory entry, including a dangling symlink, and returns
false with error zero for a missing path. `file_delete` unlinks a non-directory
entry and succeeds if it is already absent; it never follows a final symlink and
never recurses. Removing a link leaves its target alone. An open descriptor or
another hard link can keep the unlinked inode alive.

`file_copy(source, destination, overwrite)` stages the complete contents in a
private file beside the destination before installing it. The existing target
is left intact if staging fails. Overwriting requires an explicit true flag;
otherwise installation cannot replace an existing name. Copy preserves ordinary
permission bits, but not ownership, timestamps, ACLs, extended attributes or
sparse layout. It does not promise a consistent snapshot of a concurrently
modified source. Staging requires write permission in the destination directory.
Replacing a name does not update other hard links to the previous inode.

`file_move` handles files, links and directories using Linux `renameat2` for
no-overwrite moves and `rename` when overwrite is requested. Cross-filesystem
regular-file moves copy then unlink the source;
if the final unlink fails, both files remain and the operation reports failure.
Moves do not retarget either handle's stored path. Cross-filesystem directory and
symlink moves report `EXDEV` rather than silently performing recursive work.
Kernel rules govern directory replacement, including non-empty destinations.

`file_replace(source, destination, backup)` requires an existing destination on
the same filesystem. The source name is consumed by an atomic rename. An optional
backup first receives the old destination's contents; its name must not already
exist. If the final rename fails, the completed backup may remain. This is not
a transaction across three names, nor a power-loss-durable operation. Same-inode
source/destination pairs, including hard-link aliases, are rejected.

Regular-file stream and copy operations reject final symlinks by default.
`file_open_follow` explicitly allows final symlinks for regular-file streams,
using the same modes and access rules as `file_open`. Creation modes can create
a missing target through a dangling link; exclusive creation still refuses an
existing link. The opened target must be a regular file before truncation occurs.
This opt-in preserves link-following behaviour for migrated callers such as JSON;
the convenience open functions and whole-file helpers retain their no-follow policy.
Explicit link
operations and metadata queries act on the link itself. Parent-directory
symlinks and trailing separators follow ordinary Linux path resolution. Path checks and mutations are
not a security sandbox against an adversary changing directory entries: callers
must control the directories involved. An open descriptor continues to refer to
its inode if another process renames its pathname.

## Linux metadata, attributes and locking

`file_get_info` returns an immutable snapshot for any inode type without following
its final symlink; `file_info_free` releases it.
`file_info_size` reports bytes and `file_info_attributes` reports attribute bits.
`file_info_last_write_time` and `file_info_creation_time` return UTC Unix seconds
plus nanoseconds through required output pointers. Creation time comes from
Linux `statx` birth time. If the filesystem does not supply it, the creation-time
accessor returns false with `ENOTSUP`; inode change time is never substituted.
This implementation requires Linux with `statx` and `renameat2` support.

`file_info_name` returns the borrowed raw basename. `file_info_type` distinguishes
regular files, directories, symlinks, FIFOs, sockets, character devices and block
devices. `file_info_permissions` returns the 07777 permission/special bits;
`file_info_owner`, `file_info_group` and `file_info_link_count` return numeric
ownership and hard-link count. `file_info_last_access_time` and
`file_info_status_change_time` expose access and inode-change timestamps using
the same seconds/nanoseconds convention. Snapshots do not refresh themselves.

`file_chmod` sets permission and special bits on closed paths, including
directories, and refuses final symlinks. `file_chown` changes numeric UID/GID on
the entry itself; -1 preserves the corresponding field. `file_set_times` changes
access and modification times with nanosecond precision, without following final
symlinks. It does not set birth time. These calls use the caller's credentials:
owner/group changes may require root or capabilities, and Linux may clear set-ID
bits during ownership changes. No operation elevates privileges.

`file_check_access` checks requested read/write/execute access using effective
credentials, following symlinks. With no requested bits it checks target existence.
It is advisory: do not rely on a successful pre-check to avoid handling subsequent
operation errors or pathname races.

## Directories and links

`file_create_directory` creates one directory or, with parents enabled, each
missing component. Existing real directories are accepted; existing non-directory
components, including symlinks encountered during parent creation, are rejected.
Mode bits are subject to umask. A failure can leave already-created parents in
place. `file_remove_directory` uses `rmdir`: the directory must be empty and the
handle closed. There is deliberately no recursive deletion or recursive copy.

`file_open_directory` and `file_read_directory` provide a lazy iterator.
Successful reads return an owned `file_info_t`, or NULL at the end; close the
directory with `file_close`. `file_list_directory` collects the same snapshots
into an owned array. `array_destroy` frees both the array and its snapshots.
Do not shallow-copy ownership-bearing entry pointers. Listings exclude `.` and
`..`, include hidden entries and dangling links, and use filesystem order rather
than sorting. Metadata is queried relative to the open directory. Concurrent
changes can cause a read to fail; a listing is not an atomic filesystem snapshot.

`file_create_hard_link` creates another name for a regular inode.
`file_create_symlink` stores a target path verbatim, including relative or
non-existent targets. Relative targets resolve from the link's containing
directory. Both operations refuse an existing destination. `file_read_link`
returns an allocated raw target through its output pointer; release it with
`free`. `file_resolve` follows links explicitly using `realpath` and returns a
new closed handle containing the canonical absolute path.

`file_get_attributes` refreshes attributes. `FILE_ATTRIBUTE_READ_ONLY` means all
POSIX write permission bits are clear; it is not a promise against privileged
writers, existing descriptors, ACLs or deletion from a writable directory.
`file_set_attributes` sets that state by clearing write bits; clearing read-only
adds owner-write only, without inventing group/other write permissions.
`FILE_ATTRIBUTE_HIDDEN` comes from a dot-prefixed basename. Changing hidden status
through attributes gives `ENOTSUP`: rename the file instead. Pass existing hidden
status unchanged when altering other bits. Unknown attribute bits give `EINVAL`.

`file_lock` and `file_unlock` use advisory Linux `flock` locks. Locks can be shared
or exclusive, waiting or non-waiting. They coordinate cooperating open streams,
not pathname replacement, and are released when the stream closes. A non-waiting
conflict gives `EWOULDBLOCK`. Flush written data before unlocking. This deliberately
does not pretend to implement Windows mandatory `FileShare` restrictions.

### Symlink targets in directory listings

Every symlink snapshot now includes its raw target text and, when resolution
succeeds, a separate snapshot of the ultimate target inode. Use
`file_info_link_target(entry)` for the stored text and
`file_info_target_info(entry)` for the target's type, attributes, permissions,
size and timestamps. The original `file_info_type(entry)` remains
`FILE_TYPE_SYMLINK`. Target names and hidden flags describe the listed link name;
other target fields describe the resolved inode.

Both results are borrowed from the original snapshot and survive directory
closure; do not free the target snapshot separately. Relative targets resolve
from the link's directory, not the process working directory. Listings query
relative to their open directory descriptor, including after a directory rename.
Link and target queries are separate observations, not an atomic snapshot.

Dangling, looping and inaccessible links still appear in listings:
`file_info_target_info` returns NULL and `file_info_target_error` reports
`ENOENT`, `ELOOP` or another resolution error. A non-link has NULL target
results and error zero. Allocation failures or failure to read the link itself
still fail the metadata query/listing. Target resolution performs metadata I/O,
never opens target contents, and may traverse beyond the listed directory.

## Complete public API reference

Every public function in `include/file.h` has an entry below, in header order.
Signatures are declarations, not usage examples. `in` parameters are borrowed
unless explicitly described as owned; `out` pointers receive results; `in,out`
handles may change state or error status. Unless stated otherwise, Boolean
failure reports an errno-style error through the initiating file handle.
Metadata timestamp accessors and key helpers use `errno` directly.

### `file_new`

`file_t *file_new(const string_t *path);`

Allocates a closed handle with a copied path; rejects empty paths and embedded NUL bytes.

- `path` (in): Required non-empty borrowed path string without embedded NUL bytes; copied into the new handle.

Returns: New caller-owned, closed handle, or NULL with errno set; release with file_free.

### `file_new_cstr`

`file_t *file_new_cstr(const char *path);`

Allocates a closed handle with a copied, non-empty C path; returns NULL on failure.

- `path` (in): Required non-empty NUL-terminated Linux byte path; copied without Unicode normalisation.

Returns: New caller-owned, closed handle, or NULL with errno set; release with file_free.

### `file_free`

`void file_free(file_t *file);`

Closes and frees a handle; NULL is safe. Use file_close first to observe delayed write errors.

- `file` (in): Owned handle to close and destroy; NULL is safe. Delayed close errors are not returned.

### `file_path`

`const char *file_path(const file_t *file);`

Returns the borrowed immutable path, or NULL for a null handle.

- `file` (in): Borrowed handle to inspect; ownership is unchanged.

Returns: Borrowed immutable path valid until file_free, or NULL for a null handle; do not free it.

### `file_last_error`

`int file_last_error(const file_t *file);`

Returns the last errno-style error, zero for success, or EINVAL for a null handle.

- `file` (in): Borrowed handle to inspect; ownership is unchanged.

Returns: Last error code, zero after success, or EINVAL for NULL.

### `file_last_error_message`

`const char *file_last_error_message(const file_t *file);`

Returns borrowed system error text corresponding to file_last_error.

- `file` (in): Borrowed handle to inspect; ownership is unchanged.

Returns: Borrowed system error text; do not free or modify it.

### `file_open`

`bool file_open(file_t *file, file_mode_t mode, file_access_t access);`

Opens a closed handle; rejects symlinks, directories and special files. Already open gives EBUSY.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.
- `mode` (in): Opening or creation policy; see file_mode_t.
- `access` (in): Requested read/write access; must be compatible with mode.

Returns: True on success; false on failure (see file_last_error).

### `file_open_follow`

`bool file_open_follow(file_t *file, file_mode_t mode, file_access_t access);`

Open a regular file, explicitly allowing final symbolic links, including creation through a dangling link. Other mode, access and ownership rules match file_open; this is not a confined-path operation.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.
- `mode` (in): Opening or creation policy; see file_mode_t.
- `access` (in): Requested read/write access; must be compatible with mode.

Returns: True on success; false on failure (see file_last_error).

### `file_create`

`bool file_create(file_t *file);`

Creates or truncates a read/write stream.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.

Returns: True on success; false on failure (see file_last_error).

### `file_create_text`

`bool file_create_text(file_t *file);`

Creates or truncates a write-only UTF-8 stream; emits no BOM.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.

Returns: True on success; false on failure (see file_last_error).

### `file_open_read`

`bool file_open_read(file_t *file);`

Opens an existing read-only stream.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.

Returns: True on success; false on failure (see file_last_error).

### `file_open_text`

`bool file_open_text(file_t *file);`

Opens an existing UTF-8 stream for lazy file_read_line calls.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.

Returns: True on success; false on failure (see file_last_error).

### `file_open_write`

`bool file_open_write(file_t *file);`

Opens or creates a write-only stream at offset zero without truncating existing contents.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.

Returns: True on success; false on failure (see file_last_error).

### `file_append_text`

`bool file_append_text(file_t *file);`

Opens or creates an append-only UTF-8 stream.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.

Returns: True on success; false on failure (see file_last_error).

### `file_close`

`bool file_close(file_t *file);`

Closes the stream, retaining its path; closing an already closed handle succeeds.

- `file` (in,out): Borrowed handle whose stream is to be closed; the handle itself is retained.

Returns: True on success; false on failure (see file_last_error).

### `file_is_open`

`bool file_is_open(const file_t *file);`

Returns whether the handle currently owns an open stream.

- `file` (in): Borrowed handle to inspect; ownership is unchanged.

Returns: True for an open file or directory stream; false for a closed or NULL handle.

### `file_flush`

`bool file_flush(file_t *file);`

Flushes buffered output; this does not guarantee durable storage after power failure.

- `file` (in,out): Borrowed open regular-file stream with suitable access; receives errors.

Returns: True on success; false on failure (see file_last_error).

### `file_read`

`bool file_read(file_t *file, void *buffer, size_t capacity, size_t *read_count);`

Reads up to capacity bytes; required read_count is zero at EOF. Partial reads precede errors.

- `file` (in,out): Borrowed open regular-file stream with suitable access; receives errors.
- `buffer` (in): Caller-owned writable buffer of at least capacity bytes; NULL is allowed only at zero capacity.
- `capacity` (in): Available buffer capacity in bytes; may be zero.
- `read_count` (out): Required output pointer; receives bytes read, including partial progress on failure; zero at EOF.

Returns: True on success; false on failure (see file_last_error).

### `file_write`

`bool file_write(file_t *file, const void *data, size_t size, size_t *written_count);`

Writes bytes; required written_count reports progress even on failure. NULL data is valid only at size zero.

- `file` (in,out): Borrowed open regular-file stream with suitable access; receives errors.
- `data` (in): Borrowed raw bytes to write; NULL is allowed only when size is zero.
- `size` (in): Number of bytes to write.
- `written_count` (out): Required output pointer; receives bytes written, including partial progress on failure.

Returns: True on success; false on failure (see file_last_error).

### `file_seek`

`bool file_seek(file_t *file, int64_t offset, file_seek_t origin);`

Seeks by a signed byte offset, clearing EOF; subsequent append writes still go to the end.

- `file` (in,out): Borrowed open regular-file stream with suitable access; receives errors.
- `offset` (in): Signed byte displacement from origin; the resulting position must be valid.
- `origin` (in): Reference position: beginning, current position or end of file.

Returns: True on success; false on failure (see file_last_error).

### `file_tell`

`bool file_tell(file_t *file, int64_t *position);`

Stores the current byte position in the required output pointer.

- `file` (in,out): Borrowed open regular-file stream with suitable access; receives errors.
- `position` (out): Required output pointer; receives the byte offset on success.

Returns: True on success; false on failure (see file_last_error).

### `file_lock`

`bool file_lock(file_t *file, bool exclusive, bool wait);`

Takes a shared or exclusive advisory lock; non-waiting conflicts report EWOULDBLOCK.

- `file` (in,out): Borrowed open regular-file stream with suitable access; receives errors.
- `exclusive` (in): True requests an exclusive lock; false requests a shared lock.
- `wait` (in): True waits for the lock; false fails immediately if it conflicts.

Returns: True on success; false on failure (see file_last_error).

### `file_unlock`

`bool file_unlock(file_t *file);`

Releases this stream's advisory lock.

- `file` (in,out): Borrowed open regular-file stream with suitable access; receives errors.

Returns: True on success; false on failure (see file_last_error).

### `file_read_line`

`bool file_read_line(file_t *file, string_t **line);`

Reads one UTF-8 line without normalisation, stripping LF, CRLF or CR and an initial BOM; NULL output means EOF.

- `file` (in,out): Borrowed open regular-file stream with suitable access; receives errors.
- `line` (out): Required output pointer; receives an owned string or NULL at EOF. Release strings with string_free.

Returns: True on success; false on failure (see file_last_error).

### `file_write_text`

`bool file_write_text(file_t *file, const string_t *text);`

Writes validated UTF-8 text verbatim, including embedded NUL bytes.

- `file` (in,out): Borrowed open regular-file stream with suitable access; receives errors.
- `text` (in): Required borrowed UTF-8 string; not consumed or modified.

Returns: True on success; false on failure (see file_last_error).

### `file_write_line`

`bool file_write_line(file_t *file, const string_t *text);`

Writes validated UTF-8 text followed by LF.

- `file` (in,out): Borrowed open regular-file stream with suitable access; receives errors.
- `text` (in): Required borrowed UTF-8 string; not consumed or modified.

Returns: True on success; false on failure (see file_last_error).

### `file_read_all_bytes`

`array_t *file_read_all_bytes(file_t *file);`

Reads a closed handle's entire file into an owned array of unsigned char; NULL means failure.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.

Returns: Owned byte array, including an empty array at EOF; NULL on failure. Release with array_destroy.

### `file_read_all_text`

`string_t *file_read_all_text(file_t *file);`

Reads a closed handle's UTF-8 file into an owned string without normalisation, stripping an initial BOM.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.

Returns: Owned string preserving UTF-8 spelling, or NULL on failure; release with string_free.

### `file_read_all_lines`

`array_t *file_read_all_lines(file_t *file);`

Reads all lines into an owned array of string_t pointers; array_destroy also frees those strings.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.

Returns: Owned array, or NULL on failure; array_destroy also frees the contained strings.

### `file_write_all_bytes`

`bool file_write_all_bytes(file_t *file, const void *data, size_t size);`

Creates or truncates a closed handle's file and writes raw bytes, then closes it.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.
- `data` (in): Borrowed raw bytes to write; NULL is allowed only when size is zero.
- `size` (in): Number of bytes to write.

Returns: True on success; false on failure (see file_last_error).

### `file_write_all_text`

`bool file_write_all_text(file_t *file, const string_t *text);`

Validates text before creating or truncating the file, writes it, and closes the stream.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.
- `text` (in): Required borrowed UTF-8 string; not consumed or modified.

Returns: True on success; false on failure (see file_last_error).

### `file_write_all_lines`

`bool file_write_all_lines(file_t *file, const array_t *lines);`

Validates an array of string_t pointers, writes every line with LF, and closes the stream.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.
- `lines` (in): Required borrowed array of string_t pointers; neither array nor strings are consumed.

Returns: True on success; false on failure (see file_last_error).

### `file_append_all_text`

`bool file_append_all_text(file_t *file, const string_t *text);`

Appends validated UTF-8 text to a closed handle's file, creating it if necessary.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.
- `text` (in): Required borrowed UTF-8 string; not consumed or modified.

Returns: True on success; false on failure (see file_last_error).

### `file_append_all_lines`

`bool file_append_all_lines(file_t *file, const array_t *lines);`

Appends an array of validated string_t pointers with LF after each, creating the file if necessary.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.
- `lines` (in): Required borrowed array of string_t pointers; neither array nor strings are consumed.

Returns: True on success; false on failure (see file_last_error).

### `file_exists`

`bool file_exists(file_t *file);`

Reports existence of any directory entry, including dangling links; absence has error code zero.

- `file` (in): Borrowed handle to inspect; ownership is unchanged.

Returns: True if an entry exists; false for absence or failure. Absence leaves error code zero.

### `file_delete`

`bool file_delete(file_t *file);`

Unlinks a closed handle's non-directory entry without following symlinks; absence is a successful no-op.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.

Returns: True on success; false on failure (see file_last_error).

### `file_copy`

`bool file_copy(file_t *source, file_t *destination, bool overwrite);`

Copies between closed handles, staging data before installation; overwrite must be explicitly enabled.

- `source` (in,out): Borrowed, closed source handle; receives errors and remains caller-owned.
- `destination` (in,out): Borrowed, closed destination-path handle; ownership is unchanged.
- `overwrite` (in): True permits destination replacement; false refuses an existing destination.

Returns: True on success; false on failure (see file_last_error).

### `file_move`

`bool file_move(file_t *source, file_t *destination, bool overwrite);`

Moves files, directories or links; cross-device regular files copy then unlink. Paths are not retargeted.

- `source` (in,out): Borrowed, closed source handle; receives errors and remains caller-owned.
- `destination` (in,out): Borrowed, closed destination-path handle; ownership is unchanged.
- `overwrite` (in): True permits destination replacement; false refuses an existing destination.

Returns: True on success; false on failure (see file_last_error).

### `file_replace`

`bool file_replace(file_t *source, file_t *destination, file_t *backup);`

Atomically replaces an existing destination on one filesystem; optional backup must not already exist.

- `source` (in,out): Borrowed, closed source handle; receives errors and remains caller-owned.
- `destination` (in,out): Borrowed, closed destination-path handle; ownership is unchanged.
- `backup` (in): Optional borrowed, closed backup handle; NULL disables backup. Its path must not already exist.

Returns: True on success; false on failure (see file_last_error).

### `file_get_info`

`file_info_t *file_get_info(file_t *file);`

Returns an owned metadata snapshot for a closed path without following its final symlink.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.

Returns: Owned metadata snapshot, or NULL on failure; release with file_info_free.

### `file_info_free`

`void file_info_free(file_info_t *info);`

Frees a metadata snapshot; NULL is safe.

- `info` (in): Owned snapshot to destroy; NULL is safe.

### `file_info_size`

`uint64_t file_info_size(const file_info_t *info);`

Returns the snapshot's byte size, or zero for NULL.

- `info` (in): Borrowed immutable metadata snapshot; ownership is unchanged.

Returns: Byte size recorded in the snapshot, or zero for NULL.

### `file_info_attributes`

`unsigned file_info_attributes(const file_info_t *info);`

Returns snapshot attribute bits, or FILE_ATTRIBUTE_NONE for NULL.

- `info` (in): Borrowed immutable metadata snapshot; ownership is unchanged.

Returns: Bitwise file_attributes_t values, or FILE_ATTRIBUTE_NONE for NULL.

### `file_info_creation_time`

`bool file_info_creation_time(const file_info_t *info, int64_t *seconds, long *nanoseconds);`

Retrieves UTC Unix seconds and nanoseconds of creation; false if unavailable, never substitutes ctime.

- `info` (in): Borrowed immutable metadata snapshot; ownership is unchanged.
- `seconds` (out): Required output pointer; receives signed UTC Unix seconds on success.
- `nanoseconds` (out): Required output pointer; receives fractional nanoseconds (0 through 999999999) on success.

Returns: True on success; false with errno set to EINVAL for invalid arguments or ENOTSUP if unavailable.

### `file_info_last_write_time`

`bool file_info_last_write_time(const file_info_t *info, int64_t *seconds, long *nanoseconds);`

Retrieves UTC Unix seconds and nanoseconds of last content modification; outputs are required.

- `info` (in): Borrowed immutable metadata snapshot; ownership is unchanged.
- `seconds` (out): Required output pointer; receives signed UTC Unix seconds on success.
- `nanoseconds` (out): Required output pointer; receives fractional nanoseconds (0 through 999999999) on success.

Returns: True on success; false with errno set to EINVAL for invalid arguments or ENOTSUP if unavailable.

### `file_get_attributes`

`bool file_get_attributes(file_t *file, unsigned *attributes);`

Retrieves current attribute bits into a required output pointer.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.
- `attributes` (out): Required output pointer; receives the current file_attributes_t bitmask.

Returns: True on success; false on failure (see file_last_error).

### `file_set_attributes`

`bool file_set_attributes(file_t *file, unsigned attributes);`

Sets read-only permissions; clearing it restores owner-write only. Changing hidden status gives ENOTSUP.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.
- `attributes` (in): Desired file_attributes_t bitmask; hidden status must match the existing filename.

Returns: True on success; false on failure (see file_last_error).

### `file_create_directory`

`bool file_create_directory(file_t *directory, unsigned permissions, bool parents);`

Creates a directory with mode bits subject to umask; optionally creates missing parents.

- `directory` (in,out): Borrowed, closed directory-path handle. Receives errors; remains caller-owned.
- `permissions` (in): Linux permission and special mode bits (07777); other bits are invalid.
- `parents` (in): True creates missing parent directories; false creates only the named directory.

Returns: True on success; false on failure (see file_last_error).

### `file_remove_directory`

`bool file_remove_directory(file_t *directory);`

Removes an empty directory, never recursively and never through a final symlink.

- `directory` (in,out): Borrowed, closed directory-path handle. Receives errors; remains caller-owned.

Returns: True on success; false on failure (see file_last_error).

### `file_open_directory`

`bool file_open_directory(file_t *directory);`

Opens a directory for lazy listing; regular stream reads are unavailable on this handle.

- `directory` (in,out): Borrowed, closed directory-path handle. Receives errors; remains caller-owned.

Returns: True on success; false on failure (see file_last_error).

### `file_read_directory`

`bool file_read_directory(file_t *directory, file_info_t **entry);`

Returns the next owned entry snapshot, excluding dot entries; success with NULL output means EOF.

- `directory` (in,out): Borrowed directory handle opened with file_open_directory. Receives errors; remains caller-owned.
- `entry` (out): Required output pointer; receives an owned snapshot or NULL at EOF. Release with file_info_free.

Returns: True on success; false on failure (see file_last_error).

### `file_list_directory`

`array_t *file_list_directory(file_t *directory);`

Returns an owned array of file_info_t pointers; array_destroy frees snapshots. Order is filesystem-defined.

- `directory` (in,out): Borrowed, closed directory-path handle. Receives errors; remains caller-owned.

Returns: Owned snapshot array, or NULL on failure; array_destroy frees its snapshots.

### `file_info_name`

`const char *file_info_name(const file_info_t *info);`

Returns the snapshot's borrowed raw basename; it remains valid until file_info_free.

- `info` (in): Borrowed immutable metadata snapshot; ownership is unchanged.

Returns: Borrowed raw basename valid until file_info_free, or NULL for NULL; do not free it.

### `file_info_link_target`

`const char *file_info_link_target(const file_info_t *info);`

- `info`: Borrowed snapshot from `file_get_info` or a directory listing.

Returns: Borrowed NUL-terminated raw link target, or NULL for a non-link or NULL
snapshot. Valid until the owning snapshot is freed; do not free or modify it.

### `file_info_target_info`

`const file_info_t *file_info_target_info(const file_info_t *info);`

- `info`: Borrowed snapshot from `file_get_info` or a directory listing.

Returns: Borrowed ultimate-target metadata, or NULL for a non-link, NULL input
or unresolved target. Inspect it with the ordinary `file_info_*` getters.
The parent snapshot owns it; do not free it separately. Its name and hidden
attribute reflect the listed link's name, not a canonical target basename.

### `file_info_target_error`

`int file_info_target_error(const file_info_t *info);`

- `info`: Borrowed metadata snapshot.

Returns: Stored errno-style target-resolution error; zero for a resolved target
or non-link, and EINVAL for NULL. Does not perform I/O or change error state.

### `file_info_type`

`file_type_t file_info_type(const file_info_t *info);`

Returns the inode type, or FILE_TYPE_UNKNOWN for NULL.

- `info` (in): Borrowed immutable metadata snapshot; ownership is unchanged.

Returns: Recorded inode type, or FILE_TYPE_UNKNOWN for NULL.

### `file_info_permissions`

`unsigned file_info_permissions(const file_info_t *info);`

Returns permission and special mode bits (07777), or zero for NULL.

- `info` (in): Borrowed immutable metadata snapshot; ownership is unchanged.

Returns: Recorded mode bits masked to 07777, or zero for NULL.

### `file_info_owner`

`uint32_t file_info_owner(const file_info_t *info);`

Returns the numeric owner UID, or UINT32_MAX for NULL.

- `info` (in): Borrowed immutable metadata snapshot; ownership is unchanged.

Returns: Recorded numeric UID, or UINT32_MAX for NULL.

### `file_info_group`

`uint32_t file_info_group(const file_info_t *info);`

Returns the numeric group GID, or UINT32_MAX for NULL.

- `info` (in): Borrowed immutable metadata snapshot; ownership is unchanged.

Returns: Recorded numeric GID, or UINT32_MAX for NULL.

### `file_info_link_count`

`uint64_t file_info_link_count(const file_info_t *info);`

Returns the inode's hard-link count, or zero for NULL.

- `info` (in): Borrowed immutable metadata snapshot; ownership is unchanged.

Returns: Recorded hard-link count, or zero for NULL.

### `file_info_last_access_time`

`bool file_info_last_access_time(const file_info_t *info, int64_t *seconds, long *nanoseconds);`

Retrieves UTC Unix seconds and nanoseconds of last access; outputs are required.

- `info` (in): Borrowed immutable metadata snapshot; ownership is unchanged.
- `seconds` (out): Required output pointer; receives signed UTC Unix seconds on success.
- `nanoseconds` (out): Required output pointer; receives fractional nanoseconds (0 through 999999999) on success.

Returns: True on success; false with errno set to EINVAL for invalid arguments or ENOTSUP if unavailable.

### `file_info_status_change_time`

`bool file_info_status_change_time(const file_info_t *info, int64_t *seconds, long *nanoseconds);`

Retrieves UTC Unix seconds and nanoseconds of inode status change, not birth; outputs are required.

- `info` (in): Borrowed immutable metadata snapshot; ownership is unchanged.
- `seconds` (out): Required output pointer; receives signed UTC Unix seconds on success.
- `nanoseconds` (out): Required output pointer; receives fractional nanoseconds (0 through 999999999) on success.

Returns: True on success; false with errno set to EINVAL for invalid arguments or ENOTSUP if unavailable.

### `file_chmod`

`bool file_chmod(file_t *file, unsigned permissions);`

Applies chmod permission/special bits to a closed file or directory; refuses final symlinks.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.
- `permissions` (in): Linux permission and special mode bits (07777); other bits are invalid.

Returns: True on success; false on failure (see file_last_error).

### `file_chown`

`bool file_chown(file_t *file, int64_t owner, int64_t group);`

Changes numeric owner/group without following the final symlink; -1 leaves that field unchanged.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.
- `owner` (in): Numeric user ID, or -1 to leave the owner unchanged.
- `group` (in): Numeric group ID, or -1 to leave the group unchanged.

Returns: True on success; false on failure (see file_last_error).

### `file_set_times`

`bool file_set_times(file_t *file, int64_t access_seconds, long access_nanoseconds, int64_t write_seconds, long write_nanoseconds);`

Changes access and modification timestamps without following the final symlink; nanoseconds must be valid.

- `file` (in,out): Borrowed, closed path handle; receives errors and remains caller-owned.
- `access_seconds` (in): Access timestamp as signed seconds since the Unix epoch, in UTC.
- `access_nanoseconds` (in): Access timestamp's fractional nanoseconds, from 0 through 999999999.
- `write_seconds` (in): Modification timestamp as signed seconds since the Unix epoch, in UTC.
- `write_nanoseconds` (in): Modification timestamp's fractional nanoseconds, from 0 through 999999999.

Returns: True on success; false on failure (see file_last_error).

### `file_check_access`

`bool file_check_access(file_t *file, bool read_access, bool write_access, bool execute_access);`

Tests requested access using effective credentials; all false tests existence. Follows symlinks.

- `file` (in): Borrowed handle to inspect; ownership is unchanged.
- `read_access` (in): Whether to require read permission.
- `write_access` (in): Whether to require write permission.
- `execute_access` (in): Whether to require execute permission (search permission for directories).

Returns: True on success; false on failure (see file_last_error).

### `file_truncate`

`bool file_truncate(file_t *file, int64_t size);`

Truncates or extends a regular file to a non-negative byte size; an open stream must be writable.

- `file` (in,out): Borrowed closed regular-file handle or open writable stream; receives errors.
- `size` (in): Desired non-negative file length in bytes; extension adds zero-filled space.

Returns: True on success; false on failure (see file_last_error).

### `file_sync`

`bool file_sync(file_t *file, bool data_only);`

Flushes and fsyncs an open file or directory; data_only selects fdatasync for file streams.

- `file` (in,out): Borrowed open file or directory handle; receives errors.
- `data_only` (in): True uses fdatasync for files; false uses fsync. Directories always use fsync.

Returns: True on success; false on failure (see file_last_error).

### `file_create_hard_link`

`bool file_create_hard_link(file_t *source, file_t *destination);`

Creates a new hard link to a regular source; the destination must not exist.

- `source` (in,out): Borrowed, closed source handle; receives errors and remains caller-owned.
- `destination` (in,out): Borrowed, closed destination-path handle; ownership is unchanged.

Returns: True on success; false on failure (see file_last_error).

### `file_create_symlink`

`bool file_create_symlink(file_t *target, file_t *link);`

Creates a new symlink containing target's raw path, which need not exist; relative text is stored verbatim.

- `target` (in,out): Borrowed, closed handle containing the target path to store verbatim; receives errors.
- `link` (in,out): Borrowed, closed symbolic-link path handle; ownership is unchanged.

Returns: True on success; false on failure (see file_last_error).

### `file_read_link`

`bool file_read_link(file_t *link, char **target);`

Returns an allocated raw symlink target through the required output pointer; release it with free.

- `link` (in,out): Borrowed, closed symbolic-link path handle; ownership is unchanged.
- `target` (out): Required output pointer; receives allocated NUL-terminated raw target bytes, or NULL on failure. Free with free.

Returns: True on success; false on failure (see file_last_error).

### `file_resolve`

`file_t *file_resolve(file_t *file);`

Returns a new closed handle for realpath's canonical absolute path, resolving symlinks.

- `file` (in): Borrowed handle to inspect; ownership is unchanged.

Returns: New caller-owned, closed canonical-path handle, or NULL on failure; release with file_free.

### `file_compress_zstd`

`bool file_compress_zstd(file_t *source, file_t *destination, bool overwrite);`

Compresses a regular file into one checksummed Zstandard frame.

- `source`: Borrowed, closed source handle; receives errno-style errors.
- `destination`: Borrowed, closed destination handle.
- `overwrite`: Whether to atomically replace an existing regular destination.

Returns: True after successful publication; false on failure.

Precondition: Both paths have trusted parent directories and must not refer to the same inode.

Details: Uses level 3, a 1 MiB window and bounded streaming buffers. Final symlinks are rejected. A private mode-0600 sibling file is published only after successful completion and close; an existing destination is unchanged on failure. Handles remain caller-owned and are closed after the operation. No metadata is copied.

Note: Atomic visibility does not guarantee power-loss durability. A crash can leave a staging file.

See also: [`file_decompress_zstd`](#file_decompress_zstd).

### `file_decompress_zstd`

`bool file_decompress_zstd(file_t *source, file_t *destination, uint64_t max_bytes, bool overwrite);`

Decompresses exactly one ordinary Zstandard frame with bounded resource use.

- `source`: Borrowed, closed compressed-file handle; receives errors.
- `destination`: Borrowed, closed output handle.
- `max_bytes`: Maximum decompressed size in bytes; zero accepts only empty output.
- `overwrite`: Whether to atomically replace an existing regular destination.

Returns: True after successful publication; false on failure.

Details: Accepts windows up to 8 MiB. Rejects skippable or concatenated frames, trailing data, malformed input and truncation. EFBIG reports a window or output limit violation; EBADMSG reports invalid input. Checksums are verified when present, but do not provide cryptographic authentication.

Note: Ownership, closed-handle, alias, symlink and staging rules match file_compress_zstd.

See also: [`file_compress_zstd`](#file_compress_zstd).

### `file_key_generate`

`file_key_t *file_key_generate(void);`

Generates an unpredictable 32-byte secretstream encryption key.

Parameters: none.

Returns: A caller-owned key, or NULL with errno set on failure.

Details: Uses libsodium's guarded allocation and random generator. Release the key with file_key_free. This does not persist the key: retain it securely to decrypt later.

Note: Keys are not passwords; no password derivation or external key storage is performed.

See also: [`file_key_export`](#file_key_export), [`file_key_from_bytes`](#file_key_from_bytes).

### `file_key_from_bytes`

`file_key_t *file_key_from_bytes(const void *bytes, size_t size);`

Copies raw encryption key material into an opaque key.

- `bytes`: Required pointer to exactly 32 secret key bytes; ownership is unchanged.
- `size`: Length in bytes; must equal 32.

Returns: A caller-owned key, or NULL with errno set on failure.

Details: NULL input or a different length gives EINVAL. Input is copied and may be erased by the caller afterwards. This is not a password derivation function.

See also: [`file_key_free`](#file_key_free).

### `file_key_export`

`bool file_key_export(const file_key_t *key, void *bytes, size_t size);`

Copies an opaque key into caller-owned secret storage.

- `key`: Required borrowed key.
- `bytes`: Required writable buffer of exactly 32 bytes.
- `size`: Buffer size in bytes; must equal 32.

Returns: True on success; false with errno set to EINVAL for invalid arguments.

Warning: The caller must protect and erase the exported copy. Freeing the opaque key does not erase this buffer. No key ownership is transferred.

### `file_key_free`

`void file_key_free(file_key_t *key);`

Erases and releases an opaque encryption key.

- `key`: Owned key to destroy; NULL is safe.

Details: Invalidates the key and its aliases. Exported caller-owned copies are unaffected and must be erased separately.

### `file_encrypt`

`bool file_encrypt(file_t *source, file_t *destination, const file_key_t *key, bool compress, bool overwrite);`

Encrypts a file using authenticated XChaCha20-Poly1305 secretstream records.

- `source`: Borrowed, closed plaintext handle; receives errors.
- `destination`: Borrowed, closed encrypted-output handle.
- `key`: Required borrowed key; remains caller-owned and is not modified.
- `compress`: Whether to compress with Zstandard before encryption.
- `overwrite`: Whether to atomically replace an existing regular destination.

Returns: True after successful publication; false on failure.

Details: Writes the version-1 MARS container with a fresh random stream header, authenticated record framing and a mandatory final record. Compression feeds encryption directly without a plaintext intermediate file. A NULL key gives EINVAL. Ownership, alias, symlink and staging rules match file_compress_zstd.

Warning: Compression can leak information through ciphertext length when attacker-controlled input shares a compression context with secrets. Disable compression in that situation.

See also: [`file_decrypt`](#file_decrypt).

### `file_decrypt`

`bool file_decrypt(file_t *source, file_t *destination, const file_key_t *key, uint64_t max_bytes, bool overwrite);`

Authenticates and decrypts a complete MARS encrypted-file container.

- `source`: Borrowed, closed encrypted-file handle; receives errors.
- `destination`: Borrowed, closed plaintext-output handle.
- `key`: Required borrowed key; remains caller-owned and is not modified.
- `max_bytes`: Maximum final plaintext size, after any decompression; zero permits only empty output.
- `overwrite`: Whether to atomically replace an existing regular destination.

Returns: True after complete verification and publication; false on failure.

Details: The authenticated header selects decompression automatically. Wrong keys, corruption, truncation, invalid records and trailing data give EBADMSG; output or decompression-window limits give EFBIG. A NULL key gives EINVAL. Publication requires a valid final record and exact EOF. Staging rules match file_compress_zstd.

Warning: Plaintext staging has mode 0600 but is not securely erased. Normal failure cleanup removes it; a crash may leave it behind. Parent directories must be trusted.

See also: [`file_encrypt`](#file_encrypt).

### `file_import_sqlite`

`bool file_import_sqlite(file_t *source, sqlite_t *db, const string_t *name, uint64_t max_bytes);`

Imports file contents and selected metadata into a named SQLCipher object.

- `source`: Borrowed, closed regular-file handle; receives errors.
- `db`: Borrowed open SQLCipher handle, externally serialised during the transfer.
- `name`: Borrowed non-empty object name without embedded NUL bytes.
- `max_bytes`: Maximum source size in bytes; zero accepts only an empty file.

Returns: True after releasing the transfer savepoint; false on failure.

Details: Creates the object-store schema if needed and atomically replaces any object with this name. Stores bounded 64 KiB chunks, size, mode bits 0777, modification time and a SHA-256 hash, never descriptors, paths or ownership. Failure rolls back the previous object and chunks. Success participates in an enclosing transaction and is not an independent commit.

Note: Rejects final symlinks and the main database inode. EFBIG denotes the size limit; ESTALE denotes a detected source change; database failures generally give EIO. Before/after metadata checks are not a hostile-writer snapshot guarantee.

Note: SQLCipher already encrypts storage. Call file_encrypt first only when separate payload encryption is wanted. Source ownership stays with the caller; it is closed on return.

See also: [`file_export_sqlite`](#file_export_sqlite).

### `file_export_sqlite`

`bool file_export_sqlite(sqlite_t *db, const string_t *name, file_t *destination, uint64_t max_bytes, bool overwrite, bool restore_metadata);`

Exports a stored file through a consistent snapshot and verified staging file.

- `db`: Borrowed open SQLCipher handle, externally serialised during the transfer.
- `name`: Borrowed non-empty file-object name without embedded NUL bytes.
- `destination`: Borrowed, closed output handle; receives errors.
- `max_bytes`: Maximum stored content size in bytes; zero accepts only an empty file.
- `overwrite`: Whether to atomically replace an existing regular destination.
- `restore_metadata`: Whether to restore stored mode bits 0777 and modification time.

Returns: True after successful verification and publication; false on failure.

Details: Checks chunk sequence, lengths, total size and content hash. Missing or malformed objects give EBADMSG; excessive size gives EFBIG. Does not decrypt a separately encrypted payload: use file_decrypt afterwards. Without metadata restoration output mode is 0600; ownership, special mode bits, ACLs and extended attributes are never restored. Rejects the main database inode and final symlinks. Staged publication leaves an existing destination unchanged on failure.

Note: An outer database rollback cannot undo an already published filesystem file. A content hash detects accidental damage, not malicious rewriting with database access. Do not target database journal or WAL files. Staging limitations match file_compress_zstd.

See also: [`file_import_sqlite`](#file_import_sqlite).

### `file_sha256`

`string_t *file_sha256(file_t *source);`

Hashes a closed regular file in 64 KiB blocks and returns an owned string of 64
lower-case hexadecimal digits. Release the result with `string_free`. On failure,
returns NULL and sets `errno` and the handle's error code. The source is closed
before returning; an already open handle is rejected with `EBUSY` and left open.
Final symbolic links and non-regular files are rejected. Detected changes to size
or modification metadata during reading give `ESTALE`; callers still need
external synchronisation for a reliable snapshot against concurrent writers.
SHA-256 identifies contents but does not authenticate an untrusted file.

### `file_create_temp_directory`

`file_t *file_create_temp_directory(const string_t *parent);`

Creates an unpredictable directory exclusively beneath the supplied existing,
trusted parent, with mode 0700 (further restricted by the process umask).
Returns an owned closed handle, or NULL with `errno` set. Empty paths and embedded
NUL bytes are rejected. Parent components may resolve through symbolic links.
The caller owns cleanup: remove any created contents, call
`file_remove_directory`, then `file_free`. Freeing the handle alone does not
delete the directory. Native release-evidence staging uses this API so all
filesystem operations remain in the file module.

## Examples

Each block is a complete C program with `main()`. Compile one block at a time
as described in the [README-program testing guide](testing.md#runnable-c-examples).
Pass the fresh, disposable paths requested by that program as command-line
arguments, in the helper function's parameter order. The symlink example takes
a new directory, its `documents` child path and its `shortcut` child path.
Other examples take independent file paths. They create and, unless stated
otherwise, delete those named files. Use a directory you control and do not
pass paths containing data you want to keep.
Assertions keep the examples short; production callers should handle failures
using the error API.

Compression and encryption examples:

- [Compress and decompress a file](#compression-and-decompression-with-zstandard).
- [Encrypt and decrypt without compression](#encryption-and-decryption-without-compression).
- [Compress, encrypt, decrypt and decompress](#compression-followed-by-encryption-and-decryption).

### SHA-256 content hashing

Pass one fresh disposable file path. This writes the standard `abc` test vector,
hashes it, and removes the file afterwards.

```c
#include <assert.h>
#include "file.h"
#include "ustring.h"

int main(int argc, char **argv)
{
    assert(argc == 2);
    file_t *source = file_new_cstr(argv[1]);
    assert(source);
    assert(file_write_all_bytes(source, "abc", 3));
    string_t *digest = file_sha256(source);
    assert(digest);
    string_printf("%S\n", digest);
    string_free(digest);
    assert(file_delete(source));
    file_free(source);
    return 0;
}
```

Output:

```text
ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
```

### UTF-8 lines

```c
#include <assert.h>
#include <stdio.h>
#include "file.h"
#include "ustring.h"

void file_lines_example(const char *path)
{
    file_t *file = file_new_cstr(path);
    string_t *words = string_new_with("sky\ncloud\n");
    string_t *extra = string_new_with("falcon\n");
    assert(file && words && extra);
    assert(file_write_all_text(file, words));
    assert(file_append_all_text(file, extra));
    assert(file_open_text(file));
    string_t *line = NULL;
    while (file_read_line(file, &line) && line) {
        printf("%s\n", string_c_str(line));
        string_free(line);
    }
    assert(file_last_error(file) == 0);
    assert(file_close(file));
    assert(file_delete(file));
    string_free(extra);
    string_free(words);
    file_free(file);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Supply 1 fresh, disposable paths.\n");
        return 1;
    }
    file_lines_example(argv[1]);
    return 0;
}
```

Output:

```text
sky
cloud
falcon
```

### Replacement with a backup

```c
#include <assert.h>
#include <stdio.h>
#include "file.h"
#include "ustring.h"

void file_replace_example(const char *current_path, const char *replacement_path, const char *backup_path)
{
    file_t *current = file_new_cstr(current_path);
    file_t *replacement = file_new_cstr(replacement_path);
    file_t *backup = file_new_cstr(backup_path);
    string_t *old_text = string_new_with("old");
    string_t *new_text = string_new_with("new");
    assert(current && replacement && backup && old_text && new_text);
    assert(file_write_all_text(current, old_text));
    assert(file_write_all_text(replacement, new_text));
    assert(file_replace(replacement, current, backup));
    string_t *current_text = file_read_all_text(current);
    string_t *backup_text = file_read_all_text(backup);
    assert(current_text && backup_text);
    printf("current: %s\nbackup: %s\n", string_c_str(current_text), string_c_str(backup_text));
    assert(file_delete(current));
    assert(file_delete(backup));
    string_free(current_text);
    string_free(backup_text);
    string_free(old_text);
    string_free(new_text);
    file_free(backup);
    file_free(replacement);
    file_free(current);
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Supply 3 fresh, disposable paths.\n");
        return 1;
    }
    file_replace_example(argv[1], argv[2], argv[3]);
    return 0;
}
```

Output:

```text
current: new
backup: old
```


### Compression and decompression with Zstandard

This example creates a source file, compresses it, and decompresses it to a
different path. Compression does not encrypt the contents.

```c
#include <assert.h>
#include <stdio.h>
#include "file.h"
#include "ustring.h"

void compressed_file_example(const char *source_path, const char *compressed_path, const char *restored_path)
{
    file_t *source = file_new_cstr(source_path);
    file_t *compressed = file_new_cstr(compressed_path);
    file_t *restored = file_new_cstr(restored_path);
    assert(source && compressed && restored);
    assert(file_write_all_bytes(source, "sky sky sky\n", 12));
    /* false refuses to overwrite an existing destination. */
    assert(file_compress_zstd(source, compressed, false));
    /* Refuse decompressed output larger than 1024 bytes. */
    assert(file_decompress_zstd(compressed, restored, 1024, false));
    string_t *text = file_read_all_text(restored);
    assert(text);
    printf("decompressed: %s", string_c_str(text));
    assert(file_delete(restored));
    assert(file_delete(compressed));
    assert(file_delete(source));
    string_free(text);
    file_free(restored);
    file_free(compressed);
    file_free(source);
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Supply 3 fresh, disposable paths.\n");
        return 1;
    }
    compressed_file_example(argv[1], argv[2], argv[3]);
    return 0;
}
```

Output:

```text
decompressed: sky sky sky
```

### Encryption and decryption without compression

The same randomly generated key is used for both operations. Decryption publishes
the output only after complete authentication. This demonstration deletes the
files and frees the key; keep the key securely if you retain the encrypted file.
A password is not a replacement for a raw encryption key.

```c
#include <assert.h>
#include <stdio.h>
#include "file.h"
#include "ustring.h"

void encrypted_only_example(const char *source_path, const char *encrypted_path, const char *restored_path)
{
    file_t *source = file_new_cstr(source_path);
    file_t *encrypted = file_new_cstr(encrypted_path);
    file_t *restored = file_new_cstr(restored_path);
    file_key_t *key = file_key_generate();
    assert(source && encrypted && restored && key);
    assert(file_write_all_bytes(source, "Private notes.\n", 15));
    /* First false disables compression; second false refuses overwriting. */
    assert(file_encrypt(source, encrypted, key, false, false));
    /* Decrypt and authenticate, allowing at most 1024 plaintext bytes. */
    assert(file_decrypt(encrypted, restored, key, 1024, false));
    string_t *text = file_read_all_text(restored);
    assert(text);
    printf("decrypted: %s", string_c_str(text));
    assert(file_delete(restored));
    assert(file_delete(encrypted));
    assert(file_delete(source));
    string_free(text);
    file_key_free(key);
    file_free(restored);
    file_free(encrypted);
    file_free(source);
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Supply 3 fresh, disposable paths.\n");
        return 1;
    }
    encrypted_only_example(argv[1], argv[2], argv[3]);
    return 0;
}
```

Output:

```text
decrypted: Private notes.
```


### Compression followed by encryption and decryption

Here the first Boolean argument to `file_encrypt` is `true`, enabling compression
before encryption. `file_decrypt` automatically decompresses the authenticated
stream; its 1024-byte limit applies to the final restored contents.

The key stays in memory for this example; a real application must retain it
securely if the encrypted file is to be read in a later process.

```c
#include <assert.h>
#include <stdio.h>
#include "file.h"
#include "ustring.h"

void encrypted_file_example(const char *source_path, const char *encrypted_path, const char *restored_path)
{
    file_t *source = file_new_cstr(source_path);
    file_t *encrypted = file_new_cstr(encrypted_path);
    file_t *restored = file_new_cstr(restored_path);
    file_key_t *key = file_key_generate();
    string_t *message = string_new_with("Meet at noon.");
    assert(source && encrypted && restored && key && message);
    assert(file_write_all_text(source, message));
    assert(file_encrypt(source, encrypted, key, true, false));
    assert(file_decrypt(encrypted, restored, key, 1024, false));
    string_t *text = file_read_all_text(restored);
    assert(text);
    printf("restored: %s\n", string_c_str(text));
    assert(file_delete(restored));
    assert(file_delete(encrypted));
    assert(file_delete(source));
    string_free(text);
    string_free(message);
    file_key_free(key);
    file_free(restored);
    file_free(encrypted);
    file_free(source);
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Supply 3 fresh, disposable paths.\n");
        return 1;
    }
    encrypted_file_example(argv[1], argv[2], argv[3]);
    return 0;
}
```

Output:

```text
restored: Meet at noon.
```


### SQLCipher file round trip

Use fresh paths in a caller-owned temporary directory. The database and two
files remain afterwards for inspection. The literal key is only for this example.

```c
#include <assert.h>
#include <stdio.h>
#include "file.h"
#include "sqlite.h"
#include "ustring.h"

void stored_file_example(const char *database_path, const char *source_path, const char *destination_path)
{
    string_t *path = string_new_with(database_path);
    string_t *key = string_new_with("example database key");
    string_t *name = string_new_with("notes");
    file_t *source = file_new_cstr(source_path);
    file_t *destination = file_new_cstr(destination_path);
    assert(path && key && name && source && destination);
    sqlite_t *db = sqlite_open_encrypted(path, key);
    assert(db);
    assert(file_write_all_bytes(source, "sky\n", 4));
    assert(file_import_sqlite(source, db, name, 1024));
    assert(file_export_sqlite(db, name, destination, 1024, false, false));
    string_t *restored = file_read_all_text(destination);
    assert(restored);
    printf("restored: %s", string_c_str(restored));
    string_free(restored);
    sqlite_close(db);
    file_free(destination);
    file_free(source);
    string_free(name);
    string_free(key);
    string_free(path);
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Supply 3 fresh, disposable paths.\n");
        return 1;
    }
    stored_file_example(argv[1], argv[2], argv[3]);
    return 0;
}
```

Output:

```text
restored: sky
```


### Listing a symlink and identifying its directory target

Pass a fresh directory path and paths for its children named `documents` and
`shortcut`. The example creates and removes all three entries.

```c
#include <assert.h>
#include <stdio.h>
#include "file.h"
#include "array.h"

void symlink_listing_example(const char *directory_path, const char *documents_path, const char *shortcut_path)
{
    file_t *directory = file_new_cstr(directory_path);
    file_t *target_directory = file_new_cstr(documents_path);
    file_t *link = file_new_cstr(shortcut_path);
    file_t *relative_target = file_new_cstr("documents");
    assert(file_create_directory(directory, 0700, false));
    assert(file_create_directory(target_directory, 0700, false));
    assert(file_create_symlink(relative_target, link));
    array_t *entries = file_list_directory(directory);
    assert(entries);
    for (size_t i = 0; i < array_size(entries); ++i) {
        const file_info_t *entry = *(file_info_t **)array_get(entries, i);
        if (file_info_type(entry) != FILE_TYPE_SYMLINK)
            continue;
        const file_info_t *target = file_info_target_info(entry);
        assert(target);
        assert(file_info_target_error(entry) == 0);
        assert(file_info_type(target) == FILE_TYPE_DIRECTORY);
        printf("%s -> %s: %s\n", file_info_name(entry), file_info_link_target(entry),
               file_info_type(target) == FILE_TYPE_DIRECTORY ? "directory" : "not a directory");
    }
    array_destroy(entries);
    assert(file_delete(link));
    assert(file_remove_directory(target_directory));
    assert(file_remove_directory(directory));
    file_free(relative_target);
    file_free(link);
    file_free(target_directory);
    file_free(directory);
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Supply 3 fresh, disposable paths.\n");
        return 1;
    }
    symlink_listing_example(argv[1], argv[2], argv[3]);
    return 0;
}
```

Output:

```text
shortcut -> documents: directory
```

## Tests and implementation

`tests/file/test_file.c` tests the stream modes, binary and text round trips,
UTF-8 boundaries, invalid input, filesystem operations, metadata, attributes and
locks. `test_file_fs.c` adds directory, permission, ownership, link, timestamp,
truncation and sync checks. The main test file's README output cases mirror the
examples above and are registered last. `test_file_limits.c` isolates
child-process file-size limits to check delayed write errors and preservation of
an existing destination when copying fails, without altering the runner's limits
or signal handlers. These fault-injection tests release their file handles in
both child and parent processes before checking the child exit status, so
Valgrind can check their cleanup paths without a failed assertion hiding them.
Run the suite sequentially with `make -j1 test_file`.

### Coverage and failure-path tests

Run `make -j1 coverage-file` with GCC, gcov and gzip installed. It builds a
separate instrumented copy of the file module in `build/coverage/file`, runs
the file tests with README examples last, and uses the native
`mars_checks file-coverage` command to report per-source line and
branch-outcome counts. Normal library objects are not instrumented. Each run
clears only that target's generated counters so old runs cannot inflate the
result. The target fails if a test fails, any public file function remains
unexecuted, line coverage falls below 90%, or branch-outcome coverage falls
below 80%.

The coverage target reports current counts rather than relying on a historical
percentage. Public file functions must all be exercised. These are execution
measurements, not a claim that every behaviour or argument combination has been proved.

`test_file_coverage.c` checks explicit link-following modes, direct text-stream
operations, sparse-file offsets beyond 4 GiB, malformed UTF-8 classes, long link
targets and invalid paths and outputs. The sparse-file test writes only a tiny
amount of data rather than materialising a multi-gigabyte file.
`test_file_limits.c` also checks a child-process blocking-lock handoff.

`test_file_faults.c` uses executable-local linker wrappers to inject interrupted
reads and writes, short and zero-length writes, disk-full errors, allocation
failures, missing metadata, permission errors, stream-open failures, close
failures and sync failures. Allocation sweeps exercise cleanup during text and
directory reads. Copy and move tests verify destination preservation on failure,
and the documented case where both files remain if source removal fails.
The wrappers are not linked into the installed library.

Cross-filesystem move errors are exercised by injecting `EXDEV` into rename;
the subsequent copy and cleanup use real temporary files on the test filesystem.
Ownership rejection is injected without changing another user's files. Real
multi-filesystem and privileged ownership changes, arbitrary concurrent pathname
races, architecture-specific offset overflow and power-loss durability still
require environment-specific testing.

`test_file_transforms.c` covers compression and encryption round trips, empty
and large binary files, wrong keys, malformed input, truncation, trailing data,
size limits and destination preservation. `test_file_store.c` checks chunked
SQLCipher transfers, metadata, rollback, object replacement, corrupt manifests
and chunks, and separately encrypted payloads. Their README examples run after
all ordinary file tests. Staging allocation, close and rename failures are
also injected; third-party codec internals are not included in MARS coverage.

The implementation separates streams, text, filesystem operations, directories,
metadata, staged publication, compression, encryption and database transfers
into corresponding `file_*.c` files, sharing one private header. Public objects
expose neither `FILE *`, file descriptors, keys, nor Linux metadata layouts.
