/**
 * @file lab_photo_source.h
 * @brief Native pixel-grid planning for bounded photographic source reuse.
 *
 * Browser workers perform codec and storage operations. This interface owns the exact source crop, geographic
 * destination mapping and stable 512-pixel source-cell keys without retaining browser handles.
 */
#ifndef LAB_PHOTO_SOURCE_H
#define LAB_PHOTO_SOURCE_H

#include "lab_handle.h"

/**
 * @brief Describe fixed source cells, their one-pixel gutters and their destination rectangles.
 * @param photo Scoped source metadata containing url, sha256, width, source and target rectangles.
 * @return No C value; the scoped host result is an array of at most 64 cell records, or null for invalid metadata.
 * @note No ownership transfers. Records contain copied keys and numbers and survive independently of this call.
 * The source image is square and either 1800 or 3600 pixels wide. Each cached cell is a 514-square RGBA image;
 * its 512-square core is fixed in source coordinates. Browser resources remain owned by the worker.
 */
void lab_photo_source(jsref photo);

#endif
