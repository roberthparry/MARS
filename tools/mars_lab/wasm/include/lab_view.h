/**
 * @file lab_view.h
 * @brief Application API: mode presentation and result geometry policy for the C browser application.
 *
 * Owns mode titles, card visibility, control availability, per-card zoom and expansion state and
 * responsive variant selection, popup placement and conditional editor resizing.
 * The host supplies measured browser geometry
 * and applies returned decisions to the DOM. No text, SVG or mathematics is
 * parsed here. Static strings are borrowed for the lifetime of the module.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 * Boolean parameters use the wasm32 i32 ABI; browser callers must supply exactly 0 or 1.
 */
#ifndef LAB_WASM_VIEW_H
#define LAB_WASM_VIEW_H

#include <stdbool.h>

#include "lab_mode.h"

/** @brief Fixed label within a worksheet's presentation catalogue. */
typedef enum {
    LAB_VIEW_TITLE = 0,    /**< Editor pane heading. */
    LAB_VIEW_SUBTITLE = 1, /**< Mode description below the editor heading. */
    LAB_VIEW_RENDERED = 2, /**< Rendered-result card heading. */
    LAB_VIEW_PARSED = 3,   /**< Expression card heading. */
    LAB_VIEW_FUNCTION = 4, /**< Function card heading. */
    LAB_VIEW_VALUE = 5     /**< Numerical value card heading. */
} lab_view_label_t;

/** @brief Signed zoom-step request; zero and all positive values increase zoom. */
typedef enum {
    LAB_VIEW_ZOOM_DECREASE = -1, /**< Move to the preceding zoom level. */
    LAB_VIEW_ZOOM_INCREASE = 1   /**< Move to the following zoom level. */
} lab_view_zoom_direction_t;

/**
 * @brief Initialise up to 64 result cards at 100% zoom with none expanded.
 * @param count Number of cards in stable browser registration order; zero releases all cards.
 * @return True on success; false for excessive counts without changing existing state.
 */
bool lab_view_cards_reset(unsigned count);

/**
 * @brief Read a registered card's zoom index.
 * @param card Zero-based registered card index.
 * @return Current zoom index, or -1 for an unregistered card.
 */
int lab_view_card_zoom(unsigned card);

/**
 * @brief Set one card's zoom, clamping and rounding as for lab_view_zoom_index.
 * @param card Zero-based registered card index.
 * @param index Requested zoom index; non-finite values select 100%.
 * @return Stored index, or -1 for an unregistered card without changing state.
 */
int lab_view_card_set_zoom(unsigned card, double index);

/**
 * @brief Step one card's zoom without affecting any other card.
 * @param card Zero-based registered card index.
 * @param direction Negative decreases; zero or positive increases.
 * @return Stored index, or -1 for an unregistered card without changing state.
 */
int lab_view_card_step_zoom(unsigned card, lab_view_zoom_direction_t direction);

/**
 * @brief Toggle exclusive card expansion; selecting another card replaces the expanded card.
 * @param card Zero-based registered card index.
 * @return Expanded card index, -1 when collapsed, or -2 for an invalid card without mutation.
 */
int lab_view_card_toggle(unsigned card);

/** @brief Read the exclusive expanded card. @return Card index, or -1 when no card is expanded. */
int lab_view_card_expanded(void);

/** @brief Collapse any expanded card without changing per-card zoom levels. */
void lab_view_cards_collapse(void);

/**
 * @brief Decide conditional editor resizing from browser measurements.
 * @param viewport_height Finite non-negative viewport height in CSS pixels.
 * @param visible_count Number of visible editors; zero divides the budget by one.
 * @param scroll_height Finite non-negative content height in CSS pixels.
 * @param client_height Finite non-negative inner editor height in CSS pixels.
 * @param current_height Finite non-negative border-box height in CSS pixels.
 * @param automatic_height Finite non-negative original height, used when manual is non-zero.
 * @param manual Whether conditional manual resizing is already enabled.
 * @return Borrowed three-double array: flags (manual 1, limited 2), original height,
 * maximum height. Invalid geometry returns null without mutation. Copy before the next call.
 */
const double *lab_view_editor_resize(double viewport_height, unsigned visible_count, double scroll_height,
                                     double client_height, double current_height, double automatic_height, bool manual);

/**
 * @brief Place a tooltip below its anchor, or above when space is insufficient.
 * @param viewport_width Finite non-negative viewport width in CSS pixels.
 * @param viewport_height Finite non-negative viewport height in CSS pixels.
 * @param anchor_left Finite anchor left position in CSS pixels.
 * @param anchor_top Finite anchor top position in CSS pixels.
 * @param anchor_width Finite non-negative anchor width in CSS pixels.
 * @param anchor_bottom Finite anchor bottom position in CSS pixels.
 * @param tooltip_width Finite non-negative tooltip width in CSS pixels.
 * @param tooltip_height Finite non-negative tooltip height in CSS pixels.
 * @return Borrowed two-double array: left, top. Invalid geometry returns null without
 * mutation. Copy before the next call. Uses an eight-pixel margin and gap.
 */
const double *lab_view_tooltip_rect(double viewport_width, double viewport_height, double anchor_left,
                                    double anchor_top, double anchor_width, double anchor_bottom, double tooltip_width,
                                    double tooltip_height);

/**
 * @brief Fit the date-picker popup below its measured anchor.
 * @param viewport_width Finite non-negative viewport width in CSS pixels.
 * @param viewport_height Finite non-negative viewport height in CSS pixels.
 * @param anchor_left Finite horizontal anchor position in CSS pixels, possibly outside the viewport.
 * @param anchor_width Finite non-negative anchor width in CSS pixels.
 * @param anchor_bottom Finite lower anchor position in CSS pixels.
 * @return Borrowed four-double array: width, left, top, maximum height. Invalid
 * geometry returns null without changing the previous result. Copy before the
 * next call. Placement preserves a 12-pixel margin and an 8-pixel anchor gap.
 */
const double *lab_view_picker_rect(double viewport_width, double viewport_height, double anchor_left,
                                   double anchor_width, double anchor_bottom);

/**
 * @brief Borrow a static UTF-8 mode label.
 * @param mode Mode index 0..6; invalid indices select Expression.
 * @param field Title, subtitle, rendered, parsed, function or value label (0..5).
 * @return Borrowed static string; invalid fields return an empty string.
 */
const char *lab_view_text(lab_mode_t mode, lab_view_label_t field);

/**
 * @brief Measure a label in bytes, excluding its terminator.
 * @param mode Mode index, interpreted as for lab_view_text.
 * @param field Label index, interpreted as for lab_view_text.
 * @return Label byte count.
 */
unsigned lab_view_text_length(lab_mode_t mode, lab_view_label_t field);

/**
 * @brief Resolve card and action visibility.
 * @param mode Mode index 0..6; invalid indices select Expression.
 * @param has_value Whether the current numerical value card has content.
 * @return Flags: auxiliary cards 1, value 2, conditional value 4, calculus 8, goal 16.
 */
unsigned lab_view_mode_flags(lab_mode_t mode, bool has_value);

/** @brief Return the fixed zoom table's number of entries. @return Number of zoom levels. */
unsigned lab_view_zoom_count(void);

/**
 * @brief Round and clamp a zoom index.
 * @param index Requested index; non-finite values select 100%.
 * @return Valid index in the fixed zoom table.
 */
unsigned lab_view_zoom_index(double index);

/**
 * @brief Resolve a zoom scale.
 * @param index Requested index, rounded and clamped as above.
 * @return Positive scale multiplier.
 */
double lab_view_zoom(double index);

/**
 * @brief Step between zoom levels without wrapping.
 * @param index Requested current index, rounded and clamped first.
 * @param direction Negative decreases; zero or positive increases.
 * @return Resulting index.
 */
unsigned lab_view_zoom_step(double index, lab_view_zoom_direction_t direction);

/**
 * @brief Fit the matrix base scale before applying user zoom.
 * @param base Positive unzoomed scale.
 * @param zoom Positive user zoom multiplier.
 * @param available Finite available width in CSS pixels, clamped to at least one.
 * @param width Positive intrinsic image width in CSS pixels.
 * @return Fitted scale; invalid inputs return one.
 */
double lab_view_matrix_scale(double base, double zoom, double available, double width);

/**
 * @brief Select a wrapped native rendering when compact output exceeds the card.
 * @param width Positive intrinsic compact width in CSS pixels.
 * @param scale Positive current scale.
 * @param available Finite available card width in CSS pixels, clamped to zero.
 * @param has_variant Whether an alternate native rendering exists.
 * @return True when wrapped output is needed; invalid geometry returns false.
 */
bool lab_view_wrapped(double width, double scale, double available, bool has_variant);

/**
 * @brief Resolve the enabled worksheet controls.
 * @param mode Current mode index; only zero supports goal seek.
 * @param busy Whether an operation disables the controls.
 * @param ready Whether input is ready to evaluate.
 * @param back Number of previous history entries.
 * @param forward Number of forward history entries.
 * @param can_goal Whether variable bindings permit goal seek.
 * @param minimum Whether precision is at its minimum.
 * @param maximum Whether precision is at its maximum.
 * @return Enabled bits: run 1, back 2, forward 4, goal 8, less 16, more 32.
 */
unsigned lab_view_controls(lab_mode_t mode, bool busy, bool ready, unsigned back, unsigned forward, bool can_goal,
                           bool minimum, bool maximum);

#endif
