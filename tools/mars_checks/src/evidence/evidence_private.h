/**
 * @file evidence_private.h
 * @brief Private collectors and ownership helpers for release evidence.
 *
 * Only the evidence implementation consumes this interface. JSON insertion
 * helpers consume their values; collectors return owned trees or NULL on a
 * mandatory failure. Command callers should use checks_evidence.h instead.
 */
#ifndef MARS_EVIDENCE_PRIVATE_H
#define MARS_EVIDENCE_PRIVATE_H

#include "file.h"
#include "checks_process.h"

/**
 * @brief Consume a JSON value and store its copy, terminating on allocation failure.
 * @param object Borrowed destination object, modified in place.
 * @param key Borrowed member name.
 * @param value Owned value, released after insertion.
 * @return No value.
 */
void evidence_put(json_t *object, const char *key, json_t *value);

/**
 * @brief Store a nullable borrowed string by value.
 * @param object Borrowed destination object, modified in place.
 * @param key Borrowed member name.
 * @param value Borrowed string, or NULL to store JSON null.
 * @return No value; allocation failure terminates the command.
 */
void evidence_text(json_t *object, const char *key, const string_t *value);

/**
 * @brief Run a NULL-terminated literal argument vector with a thirty-second deadline.
 * @param root Borrowed working directory.
 * @param argv Borrowed arguments, each passed without shell interpretation.
 * @param include_errors Append standard error after standard output when true.
 * @return Owned output on successful exit, or NULL on execution failure; release with string_free.
 */
string_t *evidence_run(const string_t *root, const char *const *argv, bool include_errors);

/**
 * @brief Resolve a file path through file.h.
 * @param path Borrowed existing path, which may include symbolic links.
 * @return Owned canonical absolute name, or NULL on failure; release with string_free.
 */
string_t *evidence_resolve(const string_t *path);

/**
 * @brief Hash a closed regular file through the public file API.
 * @param path Borrowed path; a final symbolic link is rejected.
 * @return Owned lowercase SHA-256 text, or NULL on failure; release with string_free.
 */
string_t *evidence_hash(const string_t *path);

/**
 * @brief Collect validated Git state; unknown state always fails closed.
 * @param root Borrowed repository working directory.
 * @param allow_dirty Permit known dirty state for local trials when true.
 * @return Owned JSON object, or NULL on failure; release with json_free.
 */
json_t *evidence_source(const string_t *root, bool allow_dirty);

/**
 * @brief Collect optional executable versions without invoking Python.
 * @param root Borrowed working directory for subprocesses.
 * @return Owned JSON object with null entries for unavailable probes; release with json_free.
 */
json_t *evidence_tools(const string_t *root);

/**
 * @brief Collect optional pkg-config dependency versions.
 * @param root Borrowed working directory for subprocesses.
 * @return Owned JSON object with null entries for unavailable modules; release with json_free.
 */
json_t *evidence_modules(const string_t *root);

/**
 * @brief Collect mandatory compliance record hashes.
 * @param root Borrowed repository root containing the legal records.
 * @return Owned JSON object, or NULL if any record cannot be hashed; release with json_free.
 */
json_t *evidence_compliance(const string_t *root);

/**
 * @brief Collect the dynamic dependency closure, rejecting unresolved or malformed entries.
 * @param root Borrowed repository working directory for subprocesses.
 * @param artefact Borrowed canonical path of a trusted built artefact inspected by ldd.
 * @return Owned JSON array, or NULL on mandatory inspection failure; release with json_free.
 */
json_t *evidence_libraries(const string_t *root, const string_t *artefact);

#endif
