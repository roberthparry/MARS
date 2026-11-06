/**
 * @file texture_pyramid.h
 * @brief Private desktop-notification boundary for the offline photographic preparation utility.
 *
 * Isolates process waiting from image-array operations. Only the texture_pyramid executable uses this helper;
 * the library, server and ordinary installation require neither the notification program nor a desktop session.
 */
#ifndef MARS_TEXTURE_PYRAMID_H
#define MARS_TEXTURE_PYRAMID_H

#if !defined(MARS_TEXTURE_PYRAMID_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_TEXTURE_PYRAMID_INTERNAL_ACCESS."
#endif

#include <stdbool.h>

/**
 * @brief Send an optional local progress notification and reap the notification process.
 * @param enabled False suppresses all process creation.
 * @param message Borrowed UTF-8 message, copied by the child; NULL suppresses notification.
 * @return No value. An unavailable desktop or notification executable is ignored without interrupting preparation.
 * @note Synchronous and used only at phase boundaries or fifteen-minute intervals. No argument is retained or
 * consumed. The caller serialises calls; no release operation is required.
 */
void texture_pyramid_notify(bool enabled, const char *message);

#endif
