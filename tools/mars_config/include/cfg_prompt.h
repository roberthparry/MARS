/**
 * @file cfg_prompt.h
 * @brief Bounded interactive input shared by native configuration menus.
 *
 * Owns no persistent state. Returned strings are caller-owned. Secret prompts
 * disable terminal echo and require a terminal; ordinary prompts accept stdin.
 * Call only from the main, single-threaded installer process.
 */
#ifndef MARS_CONFIG_PROMPT_H
#define MARS_CONFIG_PROMPT_H

#include <stdbool.h>

#include "ustring.h"

/**
 * @brief Read and trim one line of at most 4096 bytes.
 * @param[in] prompt Borrowed text displayed before reading; must not contain secrets.
 * @param[in] secret Suppress terminal echo; false allows ordinary redirected stdin.
 * @return Owned string, possibly empty, or NULL on EOF, signal, terminal or input error.
 */
string_t *cfg_prompt_read(const char *prompt, bool secret);

#endif
