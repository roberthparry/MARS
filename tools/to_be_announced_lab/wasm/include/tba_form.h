/**
 * @file tba_form.h
 * @brief Native worksheet controls, selections and metadata presentation.
 *
 * Owns field relationships and edited-order tracking. The browser supplies DOM
 * access only; numeric suggestions and horizons use the model and period modules.
 */
#ifndef TBA_FORM_H
#define TBA_FORM_H

#include "tba_handle.h"
/**
 * @brief Initialise worksheet controls.
 * @param config Scoped decoded configuration.
 */
void tba_form_init(browser_jsref config);

/**
 * @brief Gather recognised settings.
 * @return Scoped object for Protobuf.
 */
browser_jsref tba_form_collect(void);

/**
 * @brief Update dependent controls after an edit.
 * @param event Scoped event.
 */
void tba_form_change(event_jsref event);

/**
 * @brief Apply suggested settings.
 * @param force Non-zero for an explicit request.
 */
void tba_form_suggest(int force);

/**
 * @brief Apply series metadata.
 * @param data Scoped response.
 * @param driver Non-zero for driver data.
 */
void tba_form_meta(browser_jsref data, int driver);

/**
 * @brief Refresh a variable picker.
 * @param columns Scoped list.
 * @param driver Non-zero for drivers.
 */
void tba_form_columns(browser_jsref columns, int driver);

/**
 * @brief Synchronise uploaded paths and filenames.
 * @param path Scoped path.
 * @param name Scoped filename.
 * @param driver Driver selector.
 */
void tba_form_upload(browser_jsref path, browser_jsref name, int driver);

/**
 * @brief Update the seasonal advisory.
 */
void tba_form_warning(void);
#endif
