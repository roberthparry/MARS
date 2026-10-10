/**
 * @file lab_widgets.c
 * @brief Calendar control projection through the scoped browser DOM bridge.
 *
 * Builds month options, weekday headings and the six-week date-picker grid from
 * the existing C calendar model. Owns popup lifecycle, anchor selection, placement,
 * date commits and local-versus-GMT Today policy. Browser code supplies structured
 * dates, clock readings, viewport dimensions and synchronous DOM capabilities.
 * The host state object retains browser references, never C handle numbers. Focus
 * and change events are deferred until projection finishes and handle scopes close.
 * No mathematical or date text is parsed; invalid months leave the DOM intact.
 */
#include "lab_dom.h"
#include "../include/lab_forms.h"
#include "lab_view.h"
#include "lab_widgets.h"

static const char *const lab_widgets_months[] = {"January", "February", "March",     "April",   "May",      "June",
                                                 "July",    "August",   "September", "October", "November", "December"};
static const char *const lab_widgets_weekdays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

static void lab_widgets_date(char text[11], unsigned year, unsigned month, unsigned day)
{
    text[0] = '0' + year / 1000;
    text[1] = '0' + year / 100 % 10;
    text[2] = '0' + year / 10 % 10;
    text[3] = '0' + year % 10;
    text[4] = '-';
    text[5] = '0' + month / 10;
    text[6] = '0' + month % 10;
    text[7] = '-';
    text[8] = '0' + day / 10;
    text[9] = '0' + day % 10;
    text[10] = 0;
}

/* Atomically project a valid calendar model during one synchronous host call. */
int lab_widgets_picker(double year, double month, double selected_year, double selected_month, double selected_day,
                       double today_year, double today_month, double today_day)
{
    const uint32_t *grid = lab_forms_calendar_grid(year, month, selected_year, selected_month, selected_day, today_year,
                                                   today_month, today_day);
    if (!grid)
        return 0;
    int picker = lab_dom_query(0, "#marsDatePicker"), days = lab_dom_query(0, "#marsDatePickerGrid");
    int weekdays = lab_dom_query(0, "#marsDatePickerWeekdays");
    if (!picker || !days || !weekdays || !lab_dom_query(0, "#marsDatePickerTitle"))
        return 0;
    int months = lab_dom_query(0, "#marsDatePickerMonth"), year_input = lab_dom_query(0, "#marsDatePickerYear");
    if (months && !lab_dom_count(lab_dom_all(months, "option"))) {
        for (unsigned i = 0; i < 12; ++i) {
            int option = lab_dom_create("option");
            lab_dom_write(option, 5, "", lab_dom_format(i + 1, "", ""));
            lab_dom_write(option, 0, "", lab_dom_string(lab_widgets_months[i]));
            lab_dom_append(months, option);
        }
    }
    lab_dom_write(months, 5, "", lab_dom_format(month, "", ""));
    char date[11];
    lab_widgets_date(date, year, month, 1);
    date[4] = 0;
    lab_dom_write(year_input, 5, "", lab_dom_string(date));
    lab_dom_write(weekdays, 0, "", lab_dom_string(""));
    for (unsigned i = 0; i < 7; ++i) {
        int cell = lab_dom_create("div");
        lab_dom_class(cell, "mars-date-picker-weekday", 1);
        lab_dom_write(cell, 0, "", lab_dom_string(lab_widgets_weekdays[i]));
        lab_dom_append(weekdays, cell);
    }
    lab_dom_write(days, 0, "", lab_dom_string(""));
    for (unsigned i = 0; i < 42; ++i) {
        const uint32_t *cell = grid + i * 4;
        unsigned flags = cell[3];
        int button = lab_dom_create("button");
        lab_dom_write(button, 2, "type", lab_dom_string("button"));
        lab_dom_class(button, "mars-date-picker-day", 1);
        lab_dom_class(button, "outside", (flags & 2) != 0);
        lab_dom_class(button, "today", (flags & 4) != 0);
        lab_dom_class(button, "selected", (flags & 8) != 0);
        lab_dom_disabled(button, !(flags & 1));
        if (flags & 1) {
            lab_widgets_date(date, cell[0], cell[1], cell[2]);
            lab_dom_write(button, 3, "isoDate", lab_dom_string(date));
            lab_dom_write(button, 0, "", lab_dom_format(cell[2], "", ""));
        }
        lab_dom_append(days, button);
    }
    lab_dom_class(picker, "hidden", 0);
    return 1;
}

typedef struct {
    double left;
    double top;
    double width;
    double height;
} lab_widgets_rect_t;

static int lab_widgets_box(int node, lab_widgets_rect_t *rect)
{
    if (!node)
        return 0;
    rect->width = lab_dom_measure(node, 3);
    rect->height = lab_dom_measure(node, 4);
    if (!(rect->width > 1 && rect->height > 1))
        return 0;
    rect->left = lab_dom_measure(node, 7);
    rect->top = lab_dom_measure(node, 8);
    return 1;
}

static int lab_widgets_anchor(int shell, lab_widgets_rect_t *rect)
{
    if (!shell)
        return 0;
    if (lab_widgets_box(shell, rect))
        return 1;
    /* A display:contents shell has no box; the two visible child controls form its anchor. */
    const int nodes[] = {lab_dom_query(shell, "input"), lab_dom_query(shell, "[data-date-target]")};
    int found = 0;
    for (unsigned i = 0; i < 2; ++i) {
        lab_widgets_rect_t box;
        if (!lab_widgets_box(nodes[i], &box))
            continue;
        if (!found) {
            *rect = box;
            found = 1;
            continue;
        }
        double right = rect->left + rect->width, bottom = rect->top + rect->height;
        if (box.left + box.width > right)
            right = box.left + box.width;
        if (box.top + box.height > bottom)
            bottom = box.top + box.height;
        if (box.left < rect->left)
            rect->left = box.left;
        if (box.top < rect->top)
            rect->top = box.top;
        rect->width = right - rect->left;
        rect->height = bottom - rect->top;
    }
    return found;
}

/* Return the shell's visible rectangle, or the union of its two child controls, through the host. */
void lab_widgets_picker_anchor(int shell)
{
    lab_widgets_rect_t rect;
    if (!lab_widgets_anchor(shell, &rect)) {
        lab_dom_return(0);
        return;
    }
    int result = lab_dom_object(5);
    lab_dom_set(result, "left", lab_dom_numeric(rect.left));
    lab_dom_set(result, "right", lab_dom_numeric(rect.left + rect.width));
    lab_dom_set(result, "top", lab_dom_numeric(rect.top));
    lab_dom_set(result, "bottom", lab_dom_numeric(rect.top + rect.height));
    lab_dom_set(result, "width", lab_dom_numeric(rect.width));
    lab_dom_set(result, "height", lab_dom_numeric(rect.height));
    lab_dom_return(result);
}

/* Select the anchor and project viewport-clamped popup geometry. */
void lab_widgets_picker_place(int state, int shell, double viewport_width, double viewport_height)
{
    int picker = lab_dom_query(0, "#marsDatePicker");
    lab_widgets_rect_t anchor;
    if (!picker || !lab_widgets_anchor(shell, &anchor))
        return;
    int button = lab_dom_get(state, "button");
    double bottom = button && lab_dom_measure(button, 4) > 1 ? lab_dom_measure(button, 9) : anchor.top + anchor.height;
    if (!viewport_width || !viewport_height) {
        int root = lab_dom_query(0, "html");
        if (!viewport_width)
            viewport_width = lab_dom_measure(root, 0);
        if (!viewport_height)
            viewport_height = lab_dom_measure(root, 10);
    }
    const double *rect = lab_view_picker_rect(viewport_width, viewport_height, anchor.left, anchor.width, bottom);
    if (!rect)
        return;
    static const char *const properties[] = {"width", "left", "top", "max-height"};
    for (unsigned i = 0; i < 4; ++i)
        lab_dom_write(picker, 4, properties[i], lab_dom_format(rect[i], "", "px"));
}

/* Hide the popup and clear both native navigation and host references before any focus event. */
void lab_widgets_picker_close(int state, int restore_focus)
{
    int picker = lab_dom_query(0, "#marsDatePicker");
    if (!picker)
        return;
    int button = lab_dom_get(state, "button");
    lab_dom_class(picker, "hidden", 1);
    lab_dom_class(lab_dom_get(state, "shell"), "open", 0);
    lab_dom_set(state, "input", 0);
    lab_dom_set(state, "button", 0);
    lab_dom_set(state, "shell", 0);
    lab_forms_picker_close();
    if (restore_focus && button)
        lab_dom_effect(button, 0);
}

/* Write a date and dispatch one deferred bubbling change only when its authored value differs. */
int lab_widgets_date_commit(int input, int value)
{
    if (!input)
        return 0;
    int changed = !lab_dom_equal(lab_dom_read(input, 5, ""), value);
    lab_dom_write(input, 5, "", value);
    if (changed)
        lab_dom_effect(input, 2);
    return changed;
}

/* Select GMT for the almanac date and the local calendar date for every other input. */
void lab_widgets_picker_today_date(int input, int local_date, int utc_date)
{
    int utc = input && lab_dom_equal(input, lab_dom_query(0, "#almanacDate"));
    lab_dom_return(utc ? utc_date : local_date);
}

/* Install the almanac GMT date and time together, emitting just one change for the complete moment. */
void lab_widgets_picker_today(int input, int local_date, int utc_moment)
{
    int time = lab_dom_query(0, "#almanacTime");
    if (input && time && lab_dom_equal(input, lab_dom_query(0, "#almanacDate"))) {
        int next_time = lab_dom_get(utc_moment, "time");
        int changed = !lab_dom_equal(lab_dom_read(time, 5, ""), next_time);
        lab_dom_write(time, 5, "", next_time);
        if (!lab_widgets_date_commit(input, lab_dom_get(utc_moment, "date")) && changed)
            lab_dom_effect(time, 2);
        return;
    }
    lab_widgets_date_commit(input, local_date);
}

static double lab_widgets_part(int date, const char *part)
{
    return lab_dom_number(lab_dom_get(date, part));
}

static void lab_widgets_render(int state, int selected, int today, double viewport_width, double viewport_height)
{
    int packed = lab_forms_picker_date();
    if (!lab_dom_get(state, "input") || !packed)
        return;
    if (lab_widgets_picker(lab_forms_date_year(packed), lab_forms_date_month(packed),
                           lab_widgets_part(selected, "year"), lab_widgets_part(selected, "month"),
                           lab_widgets_part(selected, "day"), lab_widgets_part(today, "year"),
                           lab_widgets_part(today, "month"), lab_widgets_part(today, "day")))
        lab_widgets_picker_place(state, lab_dom_get(state, "shell"), viewport_width, viewport_height);
}

/* Project the open native month and position it; a closed picker cannot be reopened by a stale render. */
void lab_widgets_picker_render(int state, int selected, int today, double viewport_width, double viewport_height)
{
    lab_widgets_render(state, selected, today, viewport_width, viewport_height);
}

/* Choose the selected date or today's date, transfer popup ownership and project one complete calendar. */
void lab_widgets_picker_open(int state, int input, int button, int selected, int today, double fallback_year,
                             double viewport_width, double viewport_height)
{
    if (!input || !button || !lab_dom_query(0, "#marsDatePicker"))
        return;
    double year = lab_widgets_part(selected, "year"), month = lab_widgets_part(selected, "month");
    if (!lab_forms_date_valid(year, month, lab_widgets_part(selected, "day"))) {
        year = lab_widgets_part(today, "year");
        month = lab_widgets_part(today, "month");
    }
    int shell = lab_dom_closest(button, ".mars-date-shell");
    lab_dom_class(lab_dom_get(state, "shell"), "open", 0);
    lab_dom_set(state, "input", input);
    lab_dom_set(state, "button", button);
    lab_dom_set(state, "shell", shell);
    lab_forms_picker_open(year, month, fallback_year);
    lab_dom_class(shell, "open", 1);
    lab_widgets_render(state, selected, today, viewport_width, viewport_height);
}

/* Format an accepted native date for commit; return whether the caller should refresh the calendar. */
int lab_widgets_picker_apply(int state, int packed, int commit)
{
    int input = lab_dom_get(state, "input");
    unsigned year = lab_forms_date_year(packed), month = lab_forms_date_month(packed), day = lab_forms_date_day(packed);
    if (!input || !lab_forms_picker_date() || !lab_forms_date_valid(year, month, day))
        return 0;
    if (commit) {
        char date[11];
        lab_widgets_date(date, year, month, day);
        lab_widgets_date_commit(input, lab_dom_string(date));
    }
    return 1;
}

/* Set a month/year (0), shift months (1) or shift years (2), preserving and clamping the selected day. */
int lab_widgets_picker_move(int state, unsigned operation, double first, double second, int selected,
                            double fallback_year, int commit)
{
    if (!lab_dom_get(state, "input") || operation > 2)
        return 0;
    double day = lab_widgets_part(selected, "day");
    if (!day)
        day = 1;
    int packed = operation ? lab_forms_picker_shift(first, operation == 2, day)
                           : lab_forms_picker_set(first, second, day, fallback_year);
    return lab_widgets_picker_apply(state, packed, commit);
}
