/**
 * @file lab_widget_events.h
 * @brief Calendar and tooltip event ownership for the freestanding Lab browser.
 *
 * Registers date-picker, observer-offset and tooltip events through the generic
 * browser bridge. C chooses event actions and cancellation; deferred browser
 * services supply clocks, date-control conversion and asynchronous DOM effects.
 * Scoped integer handles are never retained. The host owns the picker context
 * object and listener targets for their browser lifetime.
 */
#ifndef LAB_WASM_WIDGET_EVENTS_H
#define LAB_WASM_WIDGET_EVENTS_H

#include "lab_handle.h"

/** @brief Browser event selectors; numeric values are stable ABI identifiers. */
typedef enum {
    LAB_WIDGET_EVENT_OPEN           =  0, /**< Toggle a date picker. */
    LAB_WIDGET_EVENT_GRID_DAY       =  1, /**< Select a grid day. */
    LAB_WIDGET_EVENT_PREVIOUS_MONTH =  2, /**< Move to the previous month. */
    LAB_WIDGET_EVENT_NEXT_MONTH     =  3, /**< Move to the next month. */
    LAB_WIDGET_EVENT_PREVIOUS_YEAR  =  4, /**< Move to the previous year. */
    LAB_WIDGET_EVENT_NEXT_YEAR      =  5, /**< Move to the next year. */
    LAB_WIDGET_EVENT_MONTH          =  6, /**< Change the month. */
    LAB_WIDGET_EVENT_YEAR           =  7, /**< Change the year. */
    LAB_WIDGET_EVENT_YEAR_KEY       =  8, /**< Accept the year with Enter. */
    LAB_WIDGET_EVENT_TODAY          =  9, /**< Commit today's date. */
    LAB_WIDGET_EVENT_CLOSE          = 10, /**< Close the picker. */
    LAB_WIDGET_EVENT_OUTSIDE        = 11, /**< Handle an outside click. */
    LAB_WIDGET_EVENT_REPOSITION     = 12, /**< Reposition the picker. */
    LAB_WIDGET_EVENT_TOOLTIP_OVER   = 13, /**< Show a pointer tooltip. */
    LAB_WIDGET_EVENT_TOOLTIP_OUT    = 14, /**< Hide a pointer tooltip. */
    LAB_WIDGET_EVENT_TOOLTIP_FOCUS  = 15, /**< Show a focus tooltip. */
    LAB_WIDGET_EVENT_TOOLTIP_HIDE   = 16, /**< Hide a tooltip. */
    LAB_WIDGET_EVENT_TOOLTIP_ESCAPE = 17, /**< Dismiss a tooltip with Escape. */
    LAB_WIDGET_EVENT_OFFSET_TOUCHED = 18, /**< Mark the observer offset as authored. */
} lab_widget_event_action_t;

/**
 * @brief Install date-picker and tooltip subscriptions idempotently.
 * @param document Scoped browser Document.
 * @param window Scoped browser Window.
 * @param state Scoped picker context containing input, button and shell nodes and native date accessors.
 */
void lab_widget_events_install(jsref document, jsref window, jsref state);

/**
 * @brief Select actions for a delivered browser event.
 * @param action Open picker 0, select grid day 1, previous/next month 2/3, previous/next year 4/5,
 * month change 6, year change 7, year key 8, Today 9, close 10, outside click 11, reposition 12,
 * tooltip pointer-over/out 13/14, tooltip focus-in 15, hide tooltip 16, tooltip Escape 17,
 * or mark observer offset touched 18; unknown codes produce an empty plan.
 * @param event Scoped browser Event handle with currentTarget, target and keyboard/pointer fields.
 * @param state Scoped mutable picker context containing input, button, shell, year and month.
 * @details Publishes a browser event plan via lab_dom_return. Service arguments are browser values copied out
 * of the handle scope. Date text remains opaque; focus/commit services execute only after dispatch returns.
 */
void lab_widget_events_dispatch(lab_widget_event_action_t action, event_jsref event, jsref state);

#endif
