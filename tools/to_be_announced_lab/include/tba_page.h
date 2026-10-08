/**
 * @file tba_page.h
 * @brief Safe server-side initial-page rendering for the forecasting Lab.
 *
 * Combines saved settings with native CSV metadata. Template substitutions are
 * single-pass: user data can never introduce further replacement tokens. Browser
 * assets are separate files and receive initial state in an escaped JSON block.
 */
#ifndef TBA_PAGE_H
#define TBA_PAGE_H
#include "tba_app.h"
/** @brief Render the initial page. @param app Borrowed app. @param mobile Borrowed network metadata. @return Owned HTML or NULL. */
string_t *tba_page_render(const tba_app_t *app, const json_t *mobile);
/** @brief Expand known template tokens once. @param text Borrowed template. @param values Raw replacement strings. @return Owned result or NULL for missing tokens. */
string_t *tba_page_expand(const string_t *text, const json_t *values);
#endif
