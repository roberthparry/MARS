/**
 * @file tba_local.h
 * @brief Browser-local document ingestion and selected-cell projection.
 *
 * C owns parsing and selection. Opaque host strings retain Unicode text without
 * copying an entire document into the fixed WebAssembly linear memory.
 */
#ifndef TBA_LOCAL_H
#define TBA_LOCAL_H
#include "tba_handle.h"
#include "tba_dom.h"

/** @brief Parse a locally read document without network I/O.
 * @param text Scoped Unicode text.
 * @return Scoped headers/rows object, or zero for malformed or oversized input. */
jsref tba_local_parse(jsref text);

/** @brief Select a date column and required value columns from retained local data.
 * @param driver Non-zero for the second document.
 * @return Scoped rectangular document, or zero if no usable selection exists. */
jsref tba_local_project(int driver);

/** @brief List local value headings excluding the selected date column.
 * @param driver Non-zero for the second document.
 * @return Scoped heading array. */
jsref tba_local_columns(int driver);
#endif
