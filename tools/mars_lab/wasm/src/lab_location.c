/**
 * @file lab_location.c
 * @brief Calendar location selection, state projection and summaries for the Lab browser.
 *
 * Uses the calendar profile schema to resolve defaults and project controls, and
 * builds town options from the native catalogue without interpreting mathematics.
 * Browser capabilities supply opaque values, Unicode operations and HTML form
 * conversion. Native continuations own asynchronous request policy; JavaScript
 * supplies promises and Intl timezone lookup. Handles belong to one synchronous call.
 * Native-owned generation tokens live as opaque objects on the select nodes;
 * restoration snapshots and change listeners reject obsolete asynchronous work.
 * Catalogue rendering and coordinate matching visit each candidate because the
 * catalogue has no coordinate index. Per-row scopes bound temporary handle usage.
 */
#include "lab_dom.h"
#include "../include/lab_forms.h"
#include "lab_profile.h"
#include "lab_select.h"
#include "lab_location.h"

static int lab_location_text(int value)
{
    return lab_dom_join(0, value, "");
}

static int lab_location_value(int node)
{
    return lab_dom_read(node, 5, "");
}

static int lab_location_is(int value, const char *text)
{
    return lab_dom_equal(value, lab_dom_string(text));
}

static int lab_location_element(unsigned mode, unsigned field)
{
    const char *name = lab_profile_text(mode, field, 1);
    char selector[80] = {'#'};
    unsigned length = 0;
    if (!*name)
        return 0;
    while (name[length] && length < sizeof selector - 2) {
        selector[length + 1] = name[length];
        ++length;
    }
    if (name[length])
        return 0;
    selector[length + 1] = 0;
    return lab_dom_query(0, selector);
}

static void lab_location_write(int node, int text)
{
    lab_dom_write(node, 5, "", text);
    if (node && lab_location_is(lab_dom_get(node, "tagName"), "SELECT"))
        lab_select_sync(node);
}

static int lab_location_valid(unsigned kind, int text, int jurisdictions)
{
    if (kind == 1) {
        int input = lab_dom_create("input");
        lab_dom_write(input, 2, "type", lab_dom_string("date"));
        lab_dom_write(input, 5, "", text);
        double milliseconds = lab_dom_to_number(lab_dom_get(input, "valueAsNumber"));
        /* UTC midnights for 0001-01-01 and 9999-12-31 bound the native year range. */
        return milliseconds >= -62135596800000.0 && milliseconds <= 253402214400000.0;
    }
    if (kind == 2)
        return lab_dom_member(jurisdictions, text);
    if (kind == 3)
        return lab_location_is(text, "visible") || lab_location_is(text, "all");
    return 1;
}

static int lab_location_validated(unsigned kind, int value, int fallback, int jurisdictions)
{
    int text = lab_dom_clean(value, kind == 3 ? 1 : 0);
    return lab_location_valid(kind, text, jurisdictions) ? text : fallback;
}

/* Validate an opaque form value using browser conversion and native calendar policy. */
void lab_location_validate(unsigned kind, int boxed, int fallback, int jurisdictions)
{
    lab_dom_return(lab_location_validated(kind, lab_dom_get(boxed, "value"), fallback, jurisdictions));
}

static int lab_location_town_list(int context, int value)
{
    int config = lab_dom_get(context, "config");
    int code = lab_location_validated(2, value, lab_dom_get(config, "DEFAULT_DATETIME_JURISDICTION"),
                                      lab_dom_get(context, "jurisdictions"));
    int catalogue = lab_dom_get(context, "towns"), towns = lab_dom_key_get(catalogue, code);
    if (lab_dom_type(towns) == 4)
        return towns;
    towns = lab_dom_key_get(catalogue, lab_dom_item(lab_dom_split(code, "-"), 0));
    return lab_dom_type(towns) == 4 ? towns : lab_dom_object(4);
}

/* Validate bootstrap data before projecting both jurisdiction menus and database availability. */
int lab_location_catalogue(int catalogue)
{
    int options = lab_dom_get(catalogue, "options"), towns = lab_dom_get(catalogue, "towns");
    int selects[] = {lab_location_element(5, 5), lab_location_element(6, 3)};
    if (lab_dom_type(options) != 4 || lab_dom_type(towns) != 5 || !selects[0] || !selects[1])
        return 0;
    unsigned count = lab_dom_count(options), mark = lab_dom_mark();
    /* Validate every row first so a malformed response cannot leave one menu partly replaced. */
    for (unsigned index = 0; index < count; ++index) {
        int row = lab_dom_item(options, index);
        int valid = lab_dom_type(row) == 4 && lab_dom_count(row) >= 2 && lab_dom_type(lab_dom_item(row, 0)) == 3 &&
                    lab_dom_type(lab_dom_item(row, 1)) == 3;
        lab_dom_release(mark);
        if (!valid)
            return 0;
    }
    int available = lab_dom_truth(lab_dom_get(catalogue, "available"));
    for (unsigned field = 0; field < 2; ++field) {
        int select = selects[field];
        lab_dom_write(select, 0, "", lab_dom_string(""));
        unsigned row_mark = lab_dom_mark();
        for (unsigned index = 0; index < count; ++index) {
            int row = lab_dom_item(options, index), option = lab_dom_create("option");
            lab_dom_write(option, 5, "", lab_dom_item(row, 0));
            lab_dom_write(option, 0, "", lab_dom_item(row, 1));
            lab_dom_append(select, option);
            lab_dom_release(row_mark);
        }
        lab_dom_disabled(select, !available);
        int panel = lab_dom_closest(select, ".mode-panel");
        int notice = panel ? lab_dom_query(panel, "[data-jurisdiction-notice]") : 0;
        if (!available && panel) {
            if (!notice) {
                notice = lab_dom_create("p");
                lab_dom_class(notice, "mode-hint", 1);
                lab_dom_write(notice, 3, "jurisdictionNotice", lab_dom_string("true"));
                lab_dom_prepend(panel, notice);
            }
            int message = lab_dom_get(catalogue, "error");
            lab_dom_write(notice, 0, "",
                          lab_dom_truth(message) ? lab_location_text(message)
                                                 : lab_dom_string("Jurisdiction database unavailable"));
        }
        lab_dom_class(notice, "hidden", available);
        lab_select_rebuild(select, 0);
        lab_dom_release(mark);
    }
    return 1;
}

/* Resolve a jurisdiction catalogue, retaining the country-level fallback. */
void lab_location_towns(int context, int boxed)
{
    lab_dom_return(lab_location_town_list(context, lab_dom_get(boxed, "value")));
}

/* Expose a native town key as opaque text. */
void lab_location_option_value(int town)
{
    lab_dom_return(lab_location_text(lab_dom_get(town, "value")));
}

/* Build town options and select the default or surviving previous value. */
void lab_location_populate(int select, int towns, int select_default)
{
    if (!select)
        return;
    lab_dom_set(select, "labTownGeneration", lab_dom_object(5));
    lab_dom_subscribe(select, "change", "lab_location_dispatch", 1, select, 0);
    static const char *const attributes[] = {"latitude", "longitude", "elevation", "timezone", "detail"};
    int previous = lab_location_value(select), empty = lab_dom_string("");
    lab_dom_write(select, 0, "", empty);
    unsigned count = lab_dom_count(towns), default_index = 0, found_default = 0, previous_exists = 0;
    unsigned mark = lab_dom_mark();
    for (unsigned index = 0; index < count; ++index) {
        int town = lab_dom_item(towns, index), option = lab_dom_create("option");
        int value = lab_location_text(lab_dom_get(town, "value"));
        if (!lab_dom_length(value))
            value = lab_dom_format(index, "", "");
        lab_dom_write(option, 5, "", value);
        previous_exists |= lab_dom_equal(previous, value);
        int name = lab_dom_get(town, "name");
        lab_dom_write(option, 0, "", lab_dom_truth(name) ? lab_location_text(name) : lab_dom_string("Location"));
        for (unsigned field = 0; field < sizeof attributes / sizeof *attributes; ++field)
            lab_dom_write(option, 3, attributes[field], lab_location_text(lab_dom_get(town, attributes[field])));
        if (lab_dom_truth(lab_dom_get(town, "default"))) {
            lab_dom_write(option, 3, "default", lab_dom_string("1"));
            if (!found_default) {
                default_index = index;
                found_default = 1;
            }
        }
        lab_dom_append(select, option);
        lab_dom_release(mark);
    }
    int chosen = empty;
    if (select_default && count)
        chosen = lab_location_text(lab_dom_get(lab_dom_item(towns, default_index), "value"));
    else if (lab_dom_length(previous) && previous_exists)
        chosen = previous;
    else if (count)
        chosen = lab_location_text(lab_dom_get(lab_dom_item(towns, 0), "value"));
    lab_dom_write(select, 5, "", chosen);
    lab_select_rebuild(select, 1);
}

/* Invalidate pending restoration on change without preventing other selection listeners. */
void lab_location_dispatch(unsigned action, int event, int select)
{
    int plan = lab_dom_object(5);
    lab_dom_set(plan, "calls", lab_dom_object(4));
    if (select && action == 1 && lab_dom_equal(lab_dom_get(event, "target"), select))
        lab_dom_set(select, "labTownGeneration", lab_dom_object(5));
    lab_dom_return(plan);
}

static int lab_location_selected_option(int select)
{
    return select && lab_dom_length(lab_location_value(select)) ? lab_dom_selected(select) : 0;
}

/* Borrow the native select's chosen option without scanning its children. */
void lab_location_selected(int select)
{
    lab_dom_return(lab_location_selected_option(select));
}

static double lab_location_number(int value)
{
    if (lab_dom_type(value) == 2)
        return lab_dom_to_number(value);
    int input = lab_dom_create("input");
    lab_dom_write(input, 2, "type", lab_dom_string("number"));
    lab_dom_write(input, 5, "", lab_dom_clean(value, 0));
    return lab_dom_to_number(lab_dom_get(input, "valueAsNumber"));
}

/* Find a coordinate pair with native tolerance policy and reject absent/non-finite numbers. */
int lab_location_coordinates(int select, int coordinates)
{
    if (!select)
        return 0;
    double latitude = lab_location_number(lab_dom_get(coordinates, "latitude"));
    double longitude = lab_location_number(lab_dom_get(coordinates, "longitude"));
    int options = lab_dom_get(select, "options");
    unsigned count = lab_dom_count(options), mark = lab_dom_mark();
    for (unsigned index = 0; index < count; ++index) {
        int option = lab_dom_item(options, index);
        if (lab_forms_nearly_equal(lab_location_number(lab_dom_read(option, 3, "latitude")), latitude, 0.000001) &&
            lab_forms_nearly_equal(lab_location_number(lab_dom_read(option, 3, "longitude")), longitude, 0.000001)) {
            lab_location_write(select, lab_location_value(option));
            lab_dom_release(mark);
            return 1;
        }
        lab_dom_release(mark);
    }
    return 0;
}

/* Clear named-town selection only when edited coordinates no longer describe that town. */
void lab_location_clear_custom(int select, int latitude, int longitude, int elevation)
{
    int option = lab_location_selected_option(select);
    if (!option)
        return;
    int selected = lab_dom_clean(lab_dom_read(option, 3, "elevation"), 0);
    int current = lab_dom_clean(lab_location_value(elevation), 0);
    if (lab_forms_nearly_equal(lab_location_number(lab_dom_read(option, 3, "latitude")),
                               lab_location_number(lab_location_value(latitude)), 0.000001) &&
        lab_forms_nearly_equal(lab_location_number(lab_dom_read(option, 3, "longitude")),
                               lab_location_number(lab_location_value(longitude)), 0.000001) &&
        (!lab_dom_length(selected) || !lab_dom_length(current) ||
         lab_forms_nearly_equal(lab_location_number(selected), lab_location_number(current), 0.01)))
        return;
    lab_location_write(select, lab_dom_string(""));
}

/* Snapshot real node identities and candidate values for asynchronous restoration guards. */
void lab_location_snapshot(int select)
{
    int result = lab_dom_object(5), options = select ? lab_dom_all(select, "option") : lab_dom_object(4);
    int candidates = lab_dom_object(4);
    lab_dom_set(result, "select", select);
    lab_dom_set(result, "generation", lab_dom_get(select, "labTownGeneration"));
    lab_dom_set(result, "options", options);
    lab_dom_set(result, "value", lab_location_value(select));
    lab_dom_set(result, "candidates", candidates);
    unsigned count = lab_dom_count(options), mark = lab_dom_mark();
    for (unsigned index = 0; index < count; ++index) {
        lab_dom_key_set(candidates, lab_dom_format(index, "", ""), lab_location_value(lab_dom_item(options, index)));
        lab_dom_release(mark);
    }
    lab_dom_return(result);
}

/* Reject replaced/reordered options even when the count and selected value are unchanged. */
int lab_location_unchanged(int select, int snapshot)
{
    if (!select || !lab_dom_equal(select, lab_dom_get(snapshot, "select")) ||
        !lab_dom_equal(lab_dom_get(select, "labTownGeneration"), lab_dom_get(snapshot, "generation")))
        return 0;
    int previous = lab_dom_get(snapshot, "options"), current = lab_dom_get(select, "options");
    int candidates = lab_dom_get(snapshot, "candidates");
    unsigned count = lab_dom_count(previous), mark = lab_dom_mark();
    if (!lab_dom_equal(lab_dom_get(snapshot, "value"), lab_location_value(select)) || count != lab_dom_count(current))
        return 0;
    /* Each snapshotted identity must be checked: equal counts cannot detect replacement or reordering. */
    for (unsigned index = 0; index < count; ++index) {
        int option = lab_dom_item(current, index);
        int equal = lab_dom_equal(lab_dom_item(previous, index), option) &&
                    lab_dom_equal(lab_dom_item(candidates, index), lab_location_value(option));
        lab_dom_release(mark);
        if (!equal)
            return 0;
    }
    return 1;
}

/* Select an exact saved key or a native compatibility-match index without changing failed selections. */
int lab_location_choose(int select, int selection)
{
    if (!select)
        return 0;
    int value;
    int match = lab_dom_get(selection, "index");
    if (lab_dom_type(match) == 2) {
        double index = lab_dom_to_number(match);
        int options = lab_dom_get(select, "options");
        if (!(index >= 0 && index < lab_dom_count(options)) || index != (unsigned)index)
            return 0;
        value = lab_location_value(lab_dom_item(options, (unsigned)index));
    } else {
        value = lab_dom_clean(lab_dom_get(selection, "value"), 0);
        if (!lab_dom_length(value))
            return 0;
    }
    int previous = lab_location_value(select);
    lab_dom_write(select, 5, "", value);
    if (!lab_dom_equal(lab_location_value(select), value)) {
        lab_dom_write(select, 5, "", previous);
        return 0;
    }
    lab_select_sync(select);
    return 1;
}

static void lab_location_reset_offset(int context, int offset)
{
    lab_dom_set(context, "automaticOffset", lab_dom_clean(lab_location_value(offset), 0));
    lab_dom_set(context, "offsetTouched", lab_dom_scalar(1, 0));
}

/* Copy selected coordinates and the browser-computed timezone offset into their controls. */
int lab_location_apply_town(int controls, int offset, int context)
{
    int select = lab_dom_get(controls, "townSelect"), option = lab_location_selected_option(select);
    if (!option)
        return 0;
    static const char *const attributes[] = {"latitude", "longitude", "elevation"};
    static const char *const inputs[] = {"latitudeInput", "longitudeInput", "elevationInput"};
    for (unsigned field = 0; field < 3; ++field) {
        int value = lab_dom_read(option, 3, attributes[field]);
        if (lab_dom_length(value))
            lab_dom_write(lab_dom_get(controls, inputs[field]), 5, "", value);
    }
    int zone = lab_dom_get(controls, "zoneInput");
    if (lab_dom_length(offset))
        lab_dom_write(zone, 5, "", offset);
    if (lab_dom_truth(lab_dom_get(controls, "resetOffsetTouched")))
        lab_location_reset_offset(context, zone);
    lab_select_sync(select);
    return 1;
}

/* Return schema metadata for existing persistence and event adapters. */
void lab_location_schema(unsigned mode)
{
    int schema = lab_dom_object(4);
    unsigned count = lab_profile_count(mode), mark = lab_dom_mark();
    for (unsigned field = 0; field < count; ++field) {
        int entry = lab_dom_object(5);
        lab_dom_set(entry, "id", lab_dom_scalar(2, mode));
        lab_dom_set(entry, "field", lab_dom_scalar(2, field));
        lab_dom_set(entry, "key", lab_dom_string(lab_profile_text(mode, field, 0)));
        lab_dom_set(entry, "element", lab_dom_string(lab_profile_text(mode, field, 1)));
        lab_dom_set(entry, "initial", lab_dom_string(lab_profile_text(mode, field, 2)));
        lab_dom_set(entry, "kind", lab_dom_scalar(2, lab_profile_kind(mode, field)));
        lab_dom_set(entry, "fallback", lab_dom_scalar(2, lab_profile_fallback(mode, field)));
        lab_dom_key_set(schema, lab_dom_format(field, "", ""), entry);
        lab_dom_release(mark);
    }
    lab_dom_return(schema);
}

static int lab_location_read_controls(unsigned mode, int context)
{
    int result = lab_dom_object(5);
    unsigned count = lab_profile_count(mode), mark = lab_dom_mark();
    for (unsigned field = 0; field < count; ++field) {
        int value = *lab_profile_text(mode, field, 1) ? lab_location_value(lab_location_element(mode, field))
                                                      : lab_dom_get(context, "visibility");
        lab_dom_set(result, lab_profile_text(mode, field, 0), value);
        lab_dom_release(mark);
    }
    return result;
}

/* Read calendar controls in the native schema order. */
void lab_location_read(unsigned mode, int context)
{
    lab_dom_return(lab_location_read_controls(mode, context));
}

static int lab_location_resolve(unsigned mode, unsigned operation, int source, int prefix, int context)
{
    if (!lab_profile_count(mode) || operation > 3)
        return 0;
    int result = lab_dom_object(5), config = lab_dom_get(context, "config");
    int jurisdictions = lab_dom_get(context, "jurisdictions");
    unsigned count = lab_profile_count(mode), mark = lab_dom_mark();
    for (unsigned field = 0; field < count; ++field) {
        const char *key = lab_profile_text(mode, field, 0);
        int raw = lab_location_text(lab_dom_key_get(source, lab_dom_join(prefix, lab_dom_string(key), "")));
        unsigned kind = lab_profile_kind(mode, field), fallback_kind = lab_profile_fallback(mode, field);
        int text = lab_dom_clean(raw, kind == 3 ? 1 : 0);
        int fallback = lab_location_text(lab_dom_get(config, lab_profile_text(mode, field, 2)));
        if (fallback_kind) {
            int date = lab_dom_get(result, "date");
            if (operation == 1) {
                date = lab_dom_key_get(source, lab_dom_join(prefix, lab_dom_string("date"), ""));
                if (!lab_dom_truth(date))
                    date = lab_dom_get(config, "DEFAULT_DATETIME_DATE");
                date = lab_location_text(date);
            }
            fallback = fallback_kind == 2 ? lab_dom_slice(date, 0, 4) : date;
        }
        if (operation == 1 && mode == 5 && field == 10)
            fallback = lab_dom_string("");
        int choice = lab_profile_choose(mode, field, operation, lab_dom_length(raw) != 0, lab_dom_length(text) != 0,
                                        lab_location_valid(kind, text, jurisdictions));
        lab_dom_set(result, key, choice == 0 ? raw : choice == 1 ? text : fallback);
        lab_dom_release(mark);
    }
    return result;
}

static void lab_location_project(unsigned mode, unsigned operation, int state, int context)
{
    if (!state)
        return;
    unsigned count = lab_profile_count(mode), mark = lab_dom_mark();
    for (unsigned field = 0; field < count; ++field) {
        if (!lab_profile_write(mode, field, operation))
            continue;
        int value = lab_dom_get(state, lab_profile_text(mode, field, 0));
        if (!*lab_profile_text(mode, field, 1))
            lab_dom_set(context, "visibility", value);
        else
            lab_location_write(lab_location_element(mode, field), value);
        lab_dom_release(mark);
    }
    if (mode == 5 && (operation == 0 || operation == 3))
        lab_location_reset_offset(context, lab_location_element(mode, 10));
}

/* Resolve or apply state with native restore/capture/fill/reset policy; invalid operations return null. */
void lab_location_state(unsigned mode, unsigned operation, int source, int prefix, int context, int apply)
{
    int state = lab_location_resolve(mode, operation, source, prefix, context);
    if (apply)
        lab_location_project(mode, operation, state, context);
    lab_dom_return(state);
}

/* Return the current DOM nodes keyed by the native calendar schema. */
void lab_location_elements(unsigned mode)
{
    int result = lab_dom_object(5);
    unsigned count = lab_profile_count(mode), mark = lab_dom_mark();
    for (unsigned field = 0; field < count; ++field) {
        lab_dom_set(result, lab_profile_text(mode, field, 0), lab_location_element(mode, field));
        lab_dom_release(mark);
    }
    lab_dom_return(result);
}

static int lab_location_control_nodes(unsigned mode, int coordinates)
{
    int result = lab_dom_object(5);
    if (mode == 5 || mode == 6) {
        unsigned town = mode == 5 ? 6 : 4;
        lab_dom_set(result, "townSelect", lab_location_element(mode, town));
        lab_dom_set(result, "dateInput", lab_location_element(mode, 0));
        lab_dom_set(result, "zoneInput", lab_location_element(mode, mode == 5 ? 10 : 2));
        lab_dom_set(result, "resetOffsetTouched", lab_dom_scalar(1, mode == 5));
        if (coordinates) {
            lab_dom_set(result, "latitudeInput", lab_location_element(mode, town + 1));
            lab_dom_set(result, "longitudeInput", lab_location_element(mode, town + 2));
            lab_dom_set(result, "elevationInput", lab_location_element(mode, town + 3));
        }
    }
    return result;
}

/* Select each mode's town/offset controls, optionally leaving authored coordinates untouched. */
void lab_location_controls(unsigned mode, int coordinates)
{
    lab_dom_return(lab_location_control_nodes(mode, coordinates));
}

/* Resolve restoration precedence and return only required asynchronous or timezone work to the host. */
void lab_location_restore(unsigned stage, int select, int snapshot, int selection, int response)
{
    if (stage > 1) {
        lab_dom_return(0);
        return;
    }
    int result = lab_dom_object(5), chosen = 0;
    if (select) {
        int candidate = lab_dom_object(5);
        if (!stage) {
            int wanted = lab_dom_clean(lab_dom_get(selection, "value"), 0);
            lab_dom_set(candidate, "value", wanted);
            chosen = lab_location_choose(select, candidate);
            if (!chosen && lab_dom_length(wanted)) {
                int request = lab_dom_object(5);
                lab_dom_set(request, "action", lab_dom_string("town"));
                lab_dom_set(request, "value", wanted);
                lab_dom_set(request, "candidates", lab_dom_get(snapshot, "candidates"));
                lab_dom_set(result, "request", request);
                lab_dom_return(result);
                return;
            }
        } else {
            lab_dom_set(candidate, "index", lab_dom_get(response, "match_index"));
            chosen = lab_location_choose(select, candidate);
        }
        if (chosen) {
            /* Only these two calendar selects own coordinate and timezone controls. */
            unsigned mode = lab_dom_equal(select, lab_location_element(5, 6)) ? 5 :
                            lab_dom_equal(select, lab_location_element(6, 4)) ? 6 : 0;
            if (mode)
                lab_dom_set(result, "controls", lab_location_control_nodes(mode, 1));
        } else {
            chosen = lab_location_coordinates(select, selection);
            if (!chosen)
                lab_location_write(select, lab_dom_string(""));
        }
    }
    lab_dom_set(result, "restored", lab_dom_scalar(1, chosen));
    lab_dom_return(result);
}

/* Fill blank defaults, then capture values with automatic DateTime offsets omitted. */
void lab_location_current(unsigned mode, int context)
{
    int empty = lab_dom_string("");
    int state = lab_location_resolve(mode, 2, lab_location_read_controls(mode, context), empty, context);
    lab_location_project(mode, 2, state, context);
    state = lab_location_resolve(mode, 1, lab_location_read_controls(mode, context), empty, context);
    if (mode == 5 && (!lab_dom_truth(lab_dom_get(context, "offsetTouched")) ||
                      lab_dom_equal(lab_dom_get(state, "gmt_offset"), lab_dom_get(context, "automaticOffset"))))
        lab_dom_set(state, "gmt_offset", empty);
    lab_dom_return(state);
}

static int lab_location_summary_line(int text, const char *label, int value, const char *suffix)
{
    return lab_dom_join(lab_dom_join(text, lab_dom_string(label), ""), value, suffix);
}

/* Format calendar summaries from already resolved opaque values. */
void lab_location_summary(unsigned mode, int state, int title)
{
    int text = mode == 5 ? lab_dom_string("MARS datetime observation") : title;
    text = lab_location_summary_line(text, "\nDate: ", lab_dom_get(state, "date"), "");
    if (mode == 5) {
        int jdn = lab_dom_get(state, "jdn");
        if (lab_dom_truth(jdn))
            text = lab_location_summary_line(text, "\nJulian Day Number: ", jdn, "");
        text = lab_location_summary_line(text, "\nRange: ", lab_dom_get(state, "start"), " to ");
        text = lab_dom_join(text, lab_dom_get(state, "end"), "");
        text = lab_location_summary_line(text, "\nYear: ", lab_dom_get(state, "year"), "");
        text = lab_location_summary_line(text, "\nHoliday jurisdiction: ", lab_dom_get(state, "jurisdiction"), "");
        text = lab_location_summary_line(text, "\nLocation: ", lab_dom_get(state, "latitude"), ", ");
        text = lab_dom_join(text, lab_dom_get(state, "longitude"), "");
        int offset = lab_dom_get(state, "gmt_offset");
        text = lab_location_summary_line(
            text, "\nGMT offset: ", lab_dom_truth(offset) ? offset : lab_dom_string("local machine offset"), "");
    } else {
        static const char *const keys[] = {"time", "jurisdiction", "zone", "latitude", "longitude", "elevation"};
        static const char *const labels[] = {
            "\nGMT time: ", "\nJurisdiction: ", "\nZone: ", "\nLatitude: ", "\nLongitude: ", "\nAltitude: "};
        for (unsigned field = 0; field < 6; ++field)
            text =
                lab_location_summary_line(text, labels[field], lab_dom_get(state, keys[field]), field == 5 ? " m" : "");
        text = lab_dom_join(text,
                            lab_dom_string(lab_location_is(lab_dom_get(state, "visibility"), "visible")
                                               ? "\nShow bodies: visible only"
                                               : "\nShow bodies: all bodies"),
                            "");
    }
    lab_dom_return(text);
}

/* Apply a current location response, preserving selected towns and manually edited offsets. */
void lab_location_response(unsigned mode, int data, int context, int update_coordinates, int town_applied)
{
    if (mode != 5 && mode != 6)
        return;
    unsigned town = mode == 5 ? 6 : 4;
    if (update_coordinates && !town_applied) {
        static const char *const keys[] = {"latitude", "longitude"};
        for (unsigned field = 0; field < 2; ++field) {
            int value = lab_dom_get(data, keys[field]);
            if (lab_dom_truth(value))
                lab_dom_write(lab_location_element(mode, town + 1 + field), 5, "", lab_location_text(value));
        }
    }
    int offset = lab_location_element(mode, mode == 5 ? 10 : 2);
    int current = lab_dom_clean(lab_location_value(offset), 0), suggested = lab_dom_get(data, "gmt_offset");
    if (offset && lab_profile_accept_offset(mode, town_applied, lab_dom_truth(lab_dom_get(context, "offsetTouched")),
                                            lab_dom_length(current) != 0,
                                            lab_dom_equal(current, lab_dom_get(context, "automaticOffset")),
                                            lab_dom_truth(suggested))) {
        lab_dom_write(offset, 5, "", mode == 5 ? lab_dom_clean(suggested, 0) : lab_location_text(suggested));
        if (mode == 5)
            lab_location_reset_offset(context, offset);
    }
}

/* Prepare an eclipse action before its guarded asynchronous town restoration. */
void lab_location_totality_prepare(int button, int context)
{
    int state = lab_dom_object(5), config = lab_dom_get(context, "config");
    for (unsigned field = 0; field < 8; ++field) {
        const char *key = lab_profile_text(6, field, 0);
        int value = lab_dom_clean(lab_dom_read(button, 3, key), 0);
        if (field == 0)
            value = lab_location_validated(1, value, lab_dom_string(""), 0);
        if (field == 3)
            value = lab_location_validated(2, value, lab_dom_get(config, "DEFAULT_DATETIME_JURISDICTION"),
                                           lab_dom_get(context, "jurisdictions"));
        lab_dom_set(state, key, value);
        if (field < 4 && (field == 3 || lab_dom_length(value)))
            lab_location_write(lab_location_element(6, field), value);
    }
    lab_dom_return(state);
}

/* Complete a still-current eclipse action, using its explicit coordinates when supplied. */
void lab_location_totality_finish(int state, int restored)
{
    if (!restored)
        lab_location_write(lab_location_element(6, 4), lab_dom_string(""));
    for (unsigned field = 5; field < 8; ++field) {
        int value = lab_dom_get(state, lab_profile_text(6, field, 0));
        if (lab_dom_length(value))
            lab_dom_write(lab_location_element(6, field), 5, "", value);
    }
}

/* Project evaluation metadata while retaining authored offsets and fallback guards. */
void lab_location_evaluation(unsigned mode, int data, int context)
{
    int fields = lab_dom_get(data, "fields"), config = lab_dom_get(context, "config");
    if (!lab_dom_truth(fields))
        return;
    if (mode == 5) {
        int date = lab_location_element(mode, 0), value = lab_dom_get(fields, "date");
        if (date && lab_dom_truth(value)) {
            int fallback = lab_location_value(date);
            if (!lab_dom_length(fallback))
                fallback = lab_dom_get(config, "DEFAULT_DATETIME_DATE");
            lab_dom_write(date, 5, "", lab_location_validated(1, value, fallback, 0));
        }
        lab_dom_write(lab_location_element(mode, 1), 5, "",
                      lab_location_text(lab_dom_get(fields, "julian_day_number")));
        value = lab_location_value(date);
        if (lab_dom_length(value))
            lab_dom_write(lab_location_element(mode, 4), 5, "", lab_dom_slice(value, 0, 4));
        int offset = lab_location_element(mode, 10), current = lab_dom_clean(lab_location_value(offset), 0);
        int returned = lab_dom_clean(lab_dom_get(fields, "gmt_offset"), 0);
        if (offset && lab_dom_length(returned) &&
            lab_profile_accept_offset(mode, 0, lab_dom_truth(lab_dom_get(context, "offsetTouched")),
                                      lab_dom_length(current) != 0,
                                      lab_dom_equal(current, lab_dom_get(context, "automaticOffset")), 1)) {
            lab_dom_write(offset, 5, "", returned);
            lab_location_reset_offset(context, offset);
        }
    } else if (mode == 6) {
        static const unsigned projection[] = {2, 5, 6, 7, 3};
        unsigned mark = lab_dom_mark();
        for (unsigned index = 0; index < sizeof projection / sizeof *projection; ++index) {
            unsigned field = projection[index];
            int value = lab_dom_get(fields, lab_profile_text(mode, field, 0));
            if (lab_dom_truth(value)) {
                value = lab_dom_clean(value, 0);
                if (field == 3)
                    value = lab_location_validated(2, value, lab_dom_get(config, "DEFAULT_DATETIME_JURISDICTION"),
                                                   lab_dom_get(context, "jurisdictions"));
                lab_location_write(lab_location_element(mode, field), value);
            }
            lab_dom_release(mark);
        }
    }
}
