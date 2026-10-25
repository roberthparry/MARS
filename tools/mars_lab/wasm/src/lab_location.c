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
 * Catalogue rendering visits each output row; key and coordinate restoration use
 * native menu indexes. Per-row scopes bound temporary handle usage.
 */
#include <string.h>

#include "lab_mode.h"
#include "lab_dom.h"
#include "lab_forms.h"
#include "lab_profile.h"
#include "lab_select.h"
#include "lab_location.h"
#include "lab_town_store.h"
#include "lab_menu_index.h"

/* Omit catalogue disambiguation IDs from town display text, retaining all other descriptive text. */
jsref lab_location_town_text(jsref text)
{
    int parts = lab_dom_split(text, " (GeoNames ");
    if (lab_dom_count(parts) != 2)
        return text;
    int tail = lab_dom_item(parts, 1);
    unsigned digits = 0, length = lab_dom_length(tail);
    while (digits < length && digits < 10 &&
           lab_dom_contains(lab_dom_string("0123456789"), lab_dom_slice(tail, digits, digits + 1)))
        ++digits;
    if (!digits || !lab_dom_equal(lab_dom_slice(tail, digits, digits + 1), lab_dom_string(")")))
        return text;
    return lab_dom_join(lab_dom_item(parts, 0), lab_dom_slice(tail, digits + 1, length), "");
}

/** @brief Calendar control indices shared by selection and response projection. */
static const struct {
    unsigned town, offset;
} lab_location_control_fields[LAB_MODE_COUNT] = {
    [LAB_MODE_DATETIME] = {6, 10},
    [LAB_MODE_ALMANAC]  = {4, 2},
};

static jsref lab_location_text(jsref value)
{
    return lab_dom_join(0, value, "");
}

static jsref lab_location_value(node_jsref node)
{
    return lab_dom_read(node, LAB_DOM_VALUE, "");
}

static int lab_location_is(jsref value, const char *text)
{
    return lab_dom_equal(value, lab_dom_string(text));
}

static jsref lab_location_element(lab_mode_t mode, unsigned field)
{
    const char *name = lab_profile_text(mode, field, LAB_PROFILE_ELEMENT);
    char selector[80] = {'#'};
    size_t length = strlen(name);
    if (!length || length > sizeof selector - 2)
        return 0;
    memcpy(selector + 1, name, length + 1);
    return lab_dom_query(0, selector);
}

static void lab_location_write(node_jsref node, jsref text)
{
    lab_dom_write(node, LAB_DOM_VALUE, "", text);
    if (node && lab_location_is(lab_dom_get(node, "tagName"), "SELECT"))
        lab_select_sync(node);
}

static int lab_location_valid(lab_profile_kind_t kind, jsref text, jsref jurisdictions)
{
    if (kind == LAB_PROFILE_DATE) {
        int input = lab_dom_create("input");
        lab_dom_write(input, LAB_DOM_ATTRIBUTE, "type", lab_dom_string("date"));
        lab_dom_write(input, LAB_DOM_VALUE, "", text);
        double milliseconds = lab_dom_to_number(lab_dom_get(input, "valueAsNumber"));
        /* UTC midnights for 0001-01-01 and 9999-12-31 bound the native year range. */
        return milliseconds >= -62135596800000.0 && milliseconds <= 253402214400000.0;
    }
    if (kind == LAB_PROFILE_JURISDICTION)
        return lab_dom_member(jurisdictions, text);
    if (kind == LAB_PROFILE_VISIBILITY)
        return lab_location_is(text, "visible") || lab_location_is(text, "all");
    return 1;
}

static jsref lab_location_validated(lab_profile_kind_t kind, jsref value, jsref fallback,
                                jsref jurisdictions)
{
    int text = lab_dom_clean(value, kind == LAB_PROFILE_VISIBILITY ? LAB_DOM_CLEAN_LOWER : LAB_DOM_CLEAN_TRIM);
    return lab_location_valid(kind, text, jurisdictions) ? text : fallback;
}

/* Validate an opaque form value using browser conversion and native calendar policy. */
void lab_location_validate(lab_profile_kind_t kind, jsref boxed, jsref fallback, jsref jurisdictions)
{
    lab_dom_return(lab_location_validated(kind, lab_dom_get(boxed, "value"), fallback, jurisdictions));
}

static jsref lab_location_town_list(jsref context, jsref value)
{
    if (lab_dom_type(value) == LAB_VALUE_STRING && !lab_dom_length(value))
        return lab_dom_object(LAB_VALUE_ARRAY);
    int config = lab_dom_get(context, "config");
    int code = lab_location_validated(LAB_PROFILE_JURISDICTION, value, lab_dom_get(config, "DEFAULT_DATETIME_JURISDICTION"),
                                      lab_dom_get(context, "jurisdictions"));
    int catalogue = lab_dom_get(context, "towns");
    bool native = lab_dom_truth(lab_dom_get(catalogue, "nativeStore"));
    int towns = native ? lab_town_store_snapshot(code) : lab_dom_key_get(catalogue, code);
    if (lab_dom_type(towns) == LAB_VALUE_ARRAY)
        return towns;
    int parent = lab_dom_item(lab_dom_split(code, "-"), 0);
    towns = native ? lab_town_store_snapshot(parent) : lab_dom_key_get(catalogue, parent);
    return lab_dom_type(towns) == LAB_VALUE_ARRAY ? towns : lab_dom_object(LAB_VALUE_ARRAY);
}

/* Validate bootstrap data before projecting both jurisdiction menus and database availability. */
bool lab_location_catalogue(jsref catalogue)
{
    int options = lab_dom_get(catalogue, "options"), towns = lab_dom_get(catalogue, "towns");
    int selects[] = {lab_location_element(LAB_MODE_DATETIME, 5), lab_location_element(LAB_MODE_ALMANAC, 3)};
    if (lab_dom_type(options) != LAB_VALUE_ARRAY || lab_dom_type(towns) != LAB_VALUE_OBJECT || !selects[0] || !selects[1])
        return false;
    unsigned count = lab_dom_count(options), mark = lab_dom_mark();
    /* Validate every row first so a malformed response cannot leave one menu partly replaced. */
    for (unsigned index = 0; index < count; ++index) {
        int row = lab_dom_item(options, index);
        int valid = lab_dom_type(row) == LAB_VALUE_ARRAY && lab_dom_count(row) >= 2 &&
                    lab_dom_type(lab_dom_item(row, 0)) == LAB_VALUE_STRING &&
                    lab_dom_type(lab_dom_item(row, 1)) == LAB_VALUE_STRING;
        lab_dom_release(mark);
        if (!valid)
            return false;
    }
    int available = lab_dom_truth(lab_dom_get(catalogue, "available"));
    for (unsigned field = 0; field < 2; ++field) {
        int select = selects[field];
        lab_dom_write(select, LAB_DOM_TEXT, "", lab_dom_string(""));
        if (field == 1) {
            int unknown = lab_dom_create("option");
            lab_dom_write(unknown, LAB_DOM_VALUE, "", lab_dom_string(""));
            lab_dom_write(unknown, LAB_DOM_TEXT, "", lab_dom_string("Country not identified"));
            lab_dom_append(select, unknown);
        }
        unsigned row_mark = lab_dom_mark();
        for (unsigned index = 0; index < count; ++index) {
            int row = lab_dom_item(options, index), option = lab_dom_create("option");
            lab_dom_write(option, LAB_DOM_VALUE, "", lab_dom_item(row, 0));
            lab_dom_write(option, LAB_DOM_TEXT, "", lab_dom_item(row, 1));
            lab_dom_write(option, LAB_DOM_DATASET, "countryCode", lab_dom_item(row, 0));
            lab_dom_write(option, LAB_DOM_DATASET, "countryOption", lab_dom_string("1"));
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
                lab_dom_write(notice, LAB_DOM_DATASET, "jurisdictionNotice", lab_dom_string("true"));
                lab_dom_prepend(panel, notice);
            }
            int message = lab_dom_get(catalogue, "error");
            lab_dom_write(notice, LAB_DOM_TEXT, "",
                          lab_dom_truth(message) ? lab_location_text(message)
                                                 : lab_dom_string("Jurisdiction database unavailable"));
        }
        lab_dom_class(notice, "hidden", available);
        lab_select_rebuild(select, 0);
        lab_dom_release(mark);
    }
    return true;
}

/* Resolve a jurisdiction catalogue, retaining the country-level fallback. */
void lab_location_towns(jsref context, jsref boxed)
{
    lab_dom_return(lab_location_town_list(context, lab_dom_get(boxed, "value")));
}

/* Expose a native town key as opaque text. */
void lab_location_option_value(jsref town)
{
    lab_dom_return(lab_location_text(lab_dom_get(town, "value")));
}

/* Build town options and select the default or surviving previous value. */
void lab_location_populate(node_jsref select, jsref towns, bool select_default)
{
    if (!select)
        return;
    lab_dom_set(select, "labTownGeneration", lab_dom_object(LAB_VALUE_OBJECT));
    lab_dom_subscribe(select, "change", "lab_location_dispatch", LAB_LOCATION_SELECTION_CHANGED, select, 0);
    static const char *const attributes[] = {"latitude", "longitude", "elevation", "timezone", "detail"};
    int previous = lab_location_value(select), empty = lab_dom_string("");
    lab_dom_write(select, LAB_DOM_TEXT, "", empty);
    int jurisdiction = lab_dom_read(lab_dom_query(lab_dom_closest(select, ".mode-panel"),
                                                 "select[id$='Jurisdiction']"), LAB_DOM_VALUE, "");
    unsigned count = lab_dom_count(towns), default_index = 0, found_default = 0, previous_exists = 0;
    unsigned mark = lab_dom_mark();
    for (unsigned index = 0; index < count; ++index) {
        int town = lab_dom_item(towns, index), option = lab_dom_create("option");
        int value = lab_location_text(lab_dom_get(town, "value"));
        if (!lab_dom_length(value))
            value = lab_dom_format(index, "", "");
        lab_dom_write(option, LAB_DOM_VALUE, "", value);
        previous_exists |= lab_dom_equal(previous, value);
        int name = lab_dom_get(town, "name");
        lab_dom_write(option, LAB_DOM_TEXT, "", lab_dom_truth(name) ?
                      lab_location_town_text(lab_location_text(name)) : lab_dom_string("Location"));
        int country = lab_dom_get(town, "country_code");
        lab_dom_write(option, LAB_DOM_DATASET, "countryCode", lab_dom_length(country) ? country : jurisdiction);
        for (unsigned field = 0; field < sizeof attributes / sizeof *attributes; ++field)
            lab_dom_write(option, LAB_DOM_DATASET, attributes[field], lab_location_text(lab_dom_get(town, attributes[field])));
        if (lab_dom_truth(lab_dom_get(town, "default"))) {
            lab_dom_write(option, LAB_DOM_DATASET, "default", lab_dom_string("1"));
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
    lab_dom_write(select, LAB_DOM_VALUE, "", chosen);
    lab_select_rebuild(select, 1);
}

/* Invalidate pending restoration on change without preventing other selection listeners. */
void lab_location_dispatch(lab_location_action_t action, event_jsref event, node_jsref select)
{
    int plan = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(plan, "calls", lab_dom_object(LAB_VALUE_ARRAY));
    if (select && action == LAB_LOCATION_SELECTION_CHANGED && lab_dom_equal(lab_dom_get(event, "target"), select))
        lab_dom_set(select, "labTownGeneration", lab_dom_object(LAB_VALUE_OBJECT));
    if (select && action == LAB_LOCATION_ZONE_EDITED && lab_dom_equal(lab_dom_get(event, "target"), select))
        lab_dom_write(select, LAB_DOM_DATASET, "automaticZone", lab_dom_string(""));
    lab_dom_return(plan);
}

static jsref lab_location_selected_option(node_jsref select)
{
    return select && lab_dom_length(lab_location_value(select)) ? lab_dom_selected(select) : 0;
}

/* Borrow the native select's chosen option without scanning its children. */
void lab_location_selected(node_jsref select)
{
    lab_dom_return(lab_location_selected_option(select));
}

/* Find a coordinate pair with native tolerance policy and reject absent/non-finite numbers. */
bool lab_location_coordinates(node_jsref select, jsref coordinates)
{
    if (!select)
        return false;
    double latitude = lab_menu_index_number(lab_dom_get(coordinates, "latitude"));
    double longitude = lab_menu_index_number(lab_dom_get(coordinates, "longitude"));
    int indexed = lab_menu_index_coordinates(select, latitude, longitude);
    if (indexed >= 0) {
        lab_dom_select_index(select, indexed);
        lab_select_sync(select);
        return true;
    }
    if (indexed == -1) return false;
    /* Keep controls usable if a bounded index cannot be allocated. */
    int options = lab_dom_get(select, "options");
    unsigned count = lab_dom_count(options), mark = lab_dom_mark();
    for (unsigned index = 0; index < count; ++index) {
        int option = lab_dom_item(options, index);
        if (lab_forms_nearly_equal(lab_menu_index_number(lab_dom_read(option, LAB_DOM_DATASET, "latitude")), latitude, 0.000001) &&
            lab_forms_nearly_equal(lab_menu_index_number(lab_dom_read(option, LAB_DOM_DATASET, "longitude")),
                                   longitude, 0.000001)) {
            lab_location_write(select, lab_location_value(option));
            lab_dom_release(mark);
            return true;
        }
        lab_dom_release(mark);
    }
    return false;
}

/* Clear named-town selection only when edited coordinates no longer describe that town. */
void lab_location_clear_custom(node_jsref select, jsref latitude, jsref longitude, jsref elevation)
{
    int option = lab_location_selected_option(select);
    if (!option)
        return;
    int selected = lab_dom_clean(lab_dom_read(option, LAB_DOM_DATASET, "elevation"), LAB_DOM_CLEAN_TRIM);
    int current = lab_dom_clean(lab_location_value(elevation), LAB_DOM_CLEAN_TRIM);
    if (lab_forms_nearly_equal(lab_menu_index_number(lab_dom_read(option, LAB_DOM_DATASET, "latitude")),
                               lab_menu_index_number(lab_location_value(latitude)), 0.000001) &&
        lab_forms_nearly_equal(lab_menu_index_number(lab_dom_read(option, LAB_DOM_DATASET, "longitude")),
                               lab_menu_index_number(lab_location_value(longitude)), 0.000001) &&
        (!lab_dom_length(selected) || !lab_dom_length(current) ||
         lab_forms_nearly_equal(lab_menu_index_number(selected), lab_menu_index_number(current), 0.01)))
        return;
    lab_location_write(select, lab_dom_string(""));
}

/* Snapshot real node identities and candidate values for asynchronous restoration guards. */
void lab_location_snapshot(node_jsref select)
{
    int result = lab_dom_object(LAB_VALUE_OBJECT),
        options = select ? lab_dom_all(select, "option") : lab_dom_object(LAB_VALUE_ARRAY);
    int candidates = lab_dom_object(LAB_VALUE_ARRAY);
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
bool lab_location_unchanged(node_jsref select, snapshot_jsref snapshot)
{
    if (!select || !lab_dom_equal(select, lab_dom_get(snapshot, "select")) ||
        !lab_dom_equal(lab_dom_get(select, "labTownGeneration"), lab_dom_get(snapshot, "generation")))
        return false;
    int previous = lab_dom_get(snapshot, "options"), current = lab_dom_get(select, "options");
    int candidates = lab_dom_get(snapshot, "candidates");
    unsigned count = lab_dom_count(previous), mark = lab_dom_mark();
    if (!lab_dom_equal(lab_dom_get(snapshot, "value"), lab_location_value(select)) || count != lab_dom_count(current))
        return false;
    /* Each snapshotted identity must be checked: equal counts cannot detect replacement or reordering. */
    for (unsigned index = 0; index < count; ++index) {
        int option = lab_dom_item(current, index);
        int equal = lab_dom_equal(lab_dom_item(previous, index), option) &&
                    lab_dom_equal(lab_dom_item(candidates, index), lab_location_value(option));
        lab_dom_release(mark);
        if (!equal)
            return false;
    }
    return true;
}

/* Select an exact saved key or a native compatibility-match index without changing failed selections. */
bool lab_location_choose(node_jsref select, jsref selection)
{
    if (!select)
        return false;
    int value;
    int match = lab_dom_get(selection, "index");
    if (lab_dom_type(match) == LAB_VALUE_NUMBER) {
        double index = lab_dom_to_number(match);
        int options = lab_dom_get(select, "options");
        if (!(index >= 0 && index < lab_dom_count(options)) || index != (unsigned)index)
            return false;
        lab_dom_select_index(select, (int)index);
        lab_select_sync(select);
        return true;
    } else {
        value = lab_dom_clean(lab_dom_get(selection, "value"), LAB_DOM_CLEAN_TRIM);
        if (!lab_dom_length(value))
            return false;
        int indexed = lab_menu_index_key(select, value);
        if (indexed >= 0) {
            lab_dom_select_index(select, indexed);
            lab_select_sync(select);
            return true;
        }
        if (indexed == -1) return false;
    }
    int previous = lab_location_value(select);
    lab_dom_write(select, LAB_DOM_VALUE, "", value);
    if (!lab_dom_equal(lab_location_value(select), value)) {
        lab_dom_write(select, LAB_DOM_VALUE, "", previous);
        return false;
    }
    lab_select_sync(select);
    return true;
}

static void lab_location_reset_offset(jsref context, jsref offset)
{
    lab_dom_set(context, "automaticOffset", lab_dom_clean(lab_location_value(offset), LAB_DOM_CLEAN_TRIM));
    lab_dom_set(context, "offsetTouched", lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
}

/* Copy selected coordinates and the browser-computed timezone offset into their controls. */
bool lab_location_apply_town(jsref controls, jsref offset, jsref context)
{
    int select = lab_dom_get(controls, "townSelect"), option = lab_location_selected_option(select);
    if (!option)
        return false;
    static const char *const attributes[] = {"latitude", "longitude", "elevation"};
    static const char *const inputs[] = {"latitudeInput", "longitudeInput", "elevationInput"};
    for (unsigned field = 0; field < 3; ++field) {
        int value = lab_dom_read(option, LAB_DOM_DATASET, attributes[field]);
        if (lab_dom_length(value))
            lab_dom_write(lab_dom_get(controls, inputs[field]), LAB_DOM_VALUE, "", value);
    }
    int zone = lab_dom_get(controls, "zoneInput");
    if (lab_dom_length(offset)) {
        lab_dom_write(zone, LAB_DOM_VALUE, "", offset);
        lab_dom_write(zone, LAB_DOM_DATASET, "automaticZone", offset);
    }
    if (lab_dom_truth(lab_dom_get(controls, "resetOffsetTouched")))
        lab_location_reset_offset(context, zone);
    lab_select_sync(select);
    return true;
}

/* Return schema metadata for existing persistence and event adapters. */
void lab_location_schema(lab_mode_t mode)
{
    int schema = lab_dom_object(LAB_VALUE_ARRAY);
    unsigned count = lab_profile_count(mode), mark = lab_dom_mark();
    for (unsigned field = 0; field < count; ++field) {
        int entry = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(entry, "id", lab_dom_scalar(LAB_VALUE_NUMBER, mode));
        lab_dom_set(entry, "field", lab_dom_scalar(LAB_VALUE_NUMBER, field));
        lab_dom_set(entry, "key", lab_dom_string(lab_profile_text(mode, field, LAB_PROFILE_KEY)));
        lab_dom_set(entry, "element", lab_dom_string(lab_profile_text(mode, field, LAB_PROFILE_ELEMENT)));
        lab_dom_set(entry, "initial", lab_dom_string(lab_profile_text(mode, field, LAB_PROFILE_INITIAL)));
        lab_dom_set(entry, "kind", lab_dom_scalar(LAB_VALUE_NUMBER, lab_profile_kind(mode, field)));
        lab_dom_set(entry, "fallback", lab_dom_scalar(LAB_VALUE_NUMBER, lab_profile_fallback(mode, field)));
        lab_dom_key_set(schema, lab_dom_format(field, "", ""), entry);
        lab_dom_release(mark);
    }
    lab_dom_return(schema);
}

static jsref lab_location_read_controls(lab_mode_t mode, jsref context)
{
    int result = lab_dom_object(LAB_VALUE_OBJECT);
    unsigned count = lab_profile_count(mode), mark = lab_dom_mark();
    for (unsigned field = 0; field < count; ++field) {
        int value = *lab_profile_text(mode, field, LAB_PROFILE_ELEMENT)
                        ? lab_location_value(lab_location_element(mode, field))
                        : lab_dom_get(context, "visibility");
        lab_dom_set(result, lab_profile_text(mode, field, LAB_PROFILE_KEY), value);
        lab_dom_release(mark);
    }
    if (mode == LAB_MODE_ALMANAC) {
        int zone = lab_location_element(mode, 2);
        lab_dom_subscribe(zone, "input", "lab_location_dispatch", LAB_LOCATION_ZONE_EDITED, zone, 0);
        lab_dom_subscribe(zone, "change", "lab_location_dispatch", LAB_LOCATION_ZONE_EDITED, zone, 0);
        int automatic = lab_dom_read(zone, LAB_DOM_DATASET, "automaticZone");
        lab_dom_set(result, "zone_mode", lab_dom_string(lab_dom_length(automatic) &&
            lab_dom_equal(automatic, lab_location_value(zone)) ? "auto" : "manual"));
    }
    return result;
}

/* Read calendar controls in the native schema order. */
void lab_location_read(lab_mode_t mode, jsref context)
{
    lab_dom_return(lab_location_read_controls(mode, context));
}

static jsref lab_location_resolve(lab_mode_t mode, lab_calendar_operation_t operation, jsref source,
                                jsref prefix, jsref context)
{
    if (!lab_profile_count(mode) || (unsigned)operation > LAB_CALENDAR_RESET)
        return 0;
    int result = lab_dom_object(LAB_VALUE_OBJECT), config = lab_dom_get(context, "config");
    int jurisdictions = lab_dom_get(context, "jurisdictions");
    unsigned count = lab_profile_count(mode), mark = lab_dom_mark();
    for (unsigned field = 0; field < count; ++field) {
        const char *key = lab_profile_text(mode, field, LAB_PROFILE_KEY);
        int raw = lab_location_text(lab_dom_key_get(source, lab_dom_join(prefix, lab_dom_string(key), "")));
        lab_profile_kind_t kind = lab_profile_kind(mode, field);
        lab_profile_fallback_t fallback_kind = lab_profile_fallback(mode, field);
        int text = lab_dom_clean(raw, kind == LAB_PROFILE_VISIBILITY ? LAB_DOM_CLEAN_LOWER : LAB_DOM_CLEAN_TRIM);
        int fallback = lab_location_text(lab_dom_get(config, lab_profile_text(mode, field, LAB_PROFILE_INITIAL)));
        if (fallback_kind != LAB_PROFILE_DEFAULT) {
            int date = lab_dom_get(result, "date");
            if (operation == LAB_CALENDAR_CAPTURE) {
                date = lab_dom_key_get(source, lab_dom_join(prefix, lab_dom_string("date"), ""));
                if (!lab_dom_truth(date))
                    date = lab_dom_get(config, "DEFAULT_DATETIME_DATE");
                date = lab_location_text(date);
            }
            fallback = fallback_kind == LAB_PROFILE_RESOLVED_YEAR ? lab_dom_slice(date, 0, 4) : date;
        }
        if (operation == LAB_CALENDAR_CAPTURE && mode == LAB_MODE_DATETIME && field == 10)
            fallback = lab_dom_string("");
        lab_profile_choice_t choice =
            lab_profile_choose(mode, field, operation, lab_dom_length(raw) != 0, lab_dom_length(text) != 0,
                               lab_location_valid(kind, text, jurisdictions));
        /** @brief Already resolved values indexed by the native profile's selection. */
        const int choices[] = {
            [LAB_PROFILE_RAW]        = raw,
            [LAB_PROFILE_NORMALISED] = text,
            [LAB_PROFILE_FALLBACK]   = fallback,
        };
        int chosen = (unsigned)choice <= LAB_PROFILE_FALLBACK ? choices[choice] : fallback;
        if (mode == LAB_MODE_ALMANAC && kind == LAB_PROFILE_JURISDICTION && operation != LAB_CALENDAR_RESET &&
            !lab_dom_length(text) && lab_dom_type(lab_dom_key_get(source,
                lab_dom_join(prefix, lab_dom_string(key), ""))) == LAB_VALUE_STRING)
            chosen = text;
        lab_dom_set(result, key, chosen);
        lab_dom_release(mark);
    }
    if (mode == LAB_MODE_ALMANAC) {
        int provenance = lab_dom_key_get(source, lab_dom_join(prefix, lab_dom_string("zone_mode"), ""));
        /* Older histories have no provenance: never guess that an authored zone was automatic. */
        lab_dom_set(result, "zone_mode", lab_dom_string(lab_location_is(provenance, "auto") ? "auto" : "manual"));
    }
    return result;
}

static void lab_location_project(lab_mode_t mode, lab_calendar_operation_t operation, jsref state,
                                jsref context)
{
    if (!state)
        return;
    unsigned count = lab_profile_count(mode), mark = lab_dom_mark();
    for (unsigned field = 0; field < count; ++field) {
        if (!lab_profile_write(mode, field, operation))
            continue;
        int value = lab_dom_get(state, lab_profile_text(mode, field, LAB_PROFILE_KEY));
        if (!*lab_profile_text(mode, field, LAB_PROFILE_ELEMENT))
            lab_dom_set(context, "visibility", value);
        else
            lab_location_write(lab_location_element(mode, field), value);
        lab_dom_release(mark);
    }
    if (mode == LAB_MODE_DATETIME && (operation == LAB_CALENDAR_RESTORE || operation == LAB_CALENDAR_RESET))
        lab_location_reset_offset(context, lab_location_element(mode, 10));
    if (mode == LAB_MODE_ALMANAC)
        lab_dom_write(lab_location_element(mode, 2), LAB_DOM_DATASET, "automaticZone",
                      lab_location_is(lab_dom_get(state, "zone_mode"), "auto") ? lab_dom_get(state, "zone")
                                                                            : lab_dom_string(""));
}

/* Resolve or apply state with native restore/capture/fill/reset policy; invalid operations return null. */
void lab_location_state(lab_mode_t mode, lab_calendar_operation_t operation, jsref source,
                        jsref prefix, jsref context, bool apply)
{
    int state = lab_location_resolve(mode, operation, source, prefix, context);
    if (apply)
        lab_location_project(mode, operation, state, context);
    lab_dom_return(state);
}

/* Return the current DOM nodes keyed by the native calendar schema. */
void lab_location_elements(lab_mode_t mode)
{
    int result = lab_dom_object(LAB_VALUE_OBJECT);
    unsigned count = lab_profile_count(mode), mark = lab_dom_mark();
    for (unsigned field = 0; field < count; ++field) {
        lab_dom_set(result, lab_profile_text(mode, field, LAB_PROFILE_KEY), lab_location_element(mode, field));
        lab_dom_release(mark);
    }
    lab_dom_return(result);
}

static jsref lab_location_control_nodes(lab_mode_t mode, int coordinates)
{
    int result = lab_dom_object(LAB_VALUE_OBJECT);
    if (mode == LAB_MODE_DATETIME || mode == LAB_MODE_ALMANAC) {
        unsigned town = lab_location_control_fields[mode].town;
        lab_dom_set(result, "townSelect", lab_location_element(mode, town));
        lab_dom_set(result, "dateInput", lab_location_element(mode, 0));
        lab_dom_set(result, "zoneInput", lab_location_element(mode, lab_location_control_fields[mode].offset));
        lab_dom_set(result, "resetOffsetTouched", lab_dom_scalar(LAB_VALUE_BOOLEAN, mode == LAB_MODE_DATETIME));
        if (coordinates) {
            lab_dom_set(result, "latitudeInput", lab_location_element(mode, town + 1));
            lab_dom_set(result, "longitudeInput", lab_location_element(mode, town + 2));
            lab_dom_set(result, "elevationInput", lab_location_element(mode, town + 3));
        }
    }
    return result;
}

/* Select each mode's town/offset controls, optionally leaving authored coordinates untouched. */
void lab_location_controls(lab_mode_t mode, bool coordinates)
{
    lab_dom_return(lab_location_control_nodes(mode, coordinates));
}

/* Resolve restoration precedence and return only required asynchronous or timezone work to the host. */
void lab_location_restore(lab_location_restore_stage_t stage, node_jsref select, snapshot_jsref snapshot,
                          jsref selection, jsref response)
{
    if ((unsigned)stage > LAB_LOCATION_RESTORE_RESPONSE) {
        lab_dom_return(0);
        return;
    }
    int result = lab_dom_object(LAB_VALUE_OBJECT), chosen = 0;
    if (select) {
        int candidate = lab_dom_object(LAB_VALUE_OBJECT);
        if (stage == LAB_LOCATION_RESTORE_INITIAL) {
            int wanted = lab_dom_clean(lab_dom_get(selection, "value"), LAB_DOM_CLEAN_TRIM);
            lab_dom_set(candidate, "value", wanted);
            chosen = lab_location_choose(select, candidate);
            if (!chosen && lab_dom_length(wanted)) {
                int request = lab_dom_object(LAB_VALUE_OBJECT);
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
            lab_mode_t mode = lab_dom_equal(select, lab_location_element(LAB_MODE_DATETIME, 6)) ? LAB_MODE_DATETIME
                              : lab_dom_equal(select, lab_location_element(LAB_MODE_ALMANAC, 4)) ? LAB_MODE_ALMANAC
                                                                                               : LAB_MODE_EXPRESSION;
            if (mode != LAB_MODE_EXPRESSION)
                lab_dom_set(result, "controls", lab_location_control_nodes(mode, 1));
        } else {
            chosen = lab_location_coordinates(select, selection);
            if (!chosen)
                lab_location_write(select, lab_dom_string(""));
        }
    }
    lab_dom_set(result, "restored", lab_dom_scalar(LAB_VALUE_BOOLEAN, chosen));
    lab_dom_return(result);
}

/* Fill blank defaults, then capture values with automatic DateTime offsets omitted. */
void lab_location_current(lab_mode_t mode, jsref context)
{
    int empty = lab_dom_string("");
    int state = lab_location_resolve(mode, LAB_CALENDAR_FILL, lab_location_read_controls(mode, context), empty, context);
    lab_location_project(mode, LAB_CALENDAR_FILL, state, context);
    state = lab_location_resolve(mode, LAB_CALENDAR_CAPTURE, lab_location_read_controls(mode, context), empty, context);
    if (mode == LAB_MODE_DATETIME && (!lab_dom_truth(lab_dom_get(context, "offsetTouched")) ||
                      lab_dom_equal(lab_dom_get(state, "gmt_offset"), lab_dom_get(context, "automaticOffset"))))
        lab_dom_set(state, "gmt_offset", empty);
    lab_dom_return(state);
}

static jsref lab_location_summary_line(jsref text, const char *label, jsref value, const char *suffix)
{
    return lab_dom_join(lab_dom_join(text, lab_dom_string(label), ""), value, suffix);
}

/* Format calendar summaries from already resolved opaque values. */
void lab_location_summary(lab_mode_t mode, jsref state, jsref title)
{
    int text = mode == LAB_MODE_DATETIME ? lab_dom_string("MARS datetime observation") : title;
    text = lab_location_summary_line(text, "\nDate: ", lab_dom_get(state, "date"), "");
    if (mode == LAB_MODE_DATETIME) {
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
void lab_location_response(lab_mode_t mode, jsref data, jsref context, bool update_coordinates,
                           bool town_applied)
{
    if (mode != LAB_MODE_DATETIME && mode != LAB_MODE_ALMANAC)
        return;
    unsigned town = lab_location_control_fields[mode].town;
    if (update_coordinates && !town_applied) {
        static const char *const keys[] = {"latitude", "longitude"};
        for (unsigned field = 0; field < 2; ++field) {
            int value = lab_dom_get(data, keys[field]);
            if (lab_dom_truth(value))
                lab_dom_write(lab_location_element(mode, town + 1 + field), LAB_DOM_VALUE, "", lab_location_text(value));
        }
    }
    int offset = lab_location_element(mode, lab_location_control_fields[mode].offset);
    int current = lab_dom_clean(lab_location_value(offset), LAB_DOM_CLEAN_TRIM), suggested = lab_dom_get(data, "gmt_offset");
    if (offset && lab_profile_accept_offset(mode, town_applied, lab_dom_truth(lab_dom_get(context, "offsetTouched")),
                                            lab_dom_length(current) != 0,
                                            lab_dom_equal(current, lab_dom_get(context, "automaticOffset")),
                                            lab_dom_truth(suggested))) {
        lab_dom_write(offset, LAB_DOM_VALUE, "",
                      mode == LAB_MODE_DATETIME ? lab_dom_clean(suggested, LAB_DOM_CLEAN_TRIM)
                                                : lab_location_text(suggested));
        if (mode == LAB_MODE_DATETIME)
            lab_location_reset_offset(context, offset);
        else
            lab_dom_write(offset, LAB_DOM_DATASET, "automaticZone", lab_location_value(offset));
    }
}

/* Prepare an eclipse action before its guarded asynchronous town restoration. */
void lab_location_totality_prepare(jsref button, jsref context)
{
    int state = lab_dom_object(LAB_VALUE_OBJECT), config = lab_dom_get(context, "config");
    for (unsigned field = 0; field < 8; ++field) {
        const char *key = lab_profile_text(LAB_MODE_ALMANAC, field, LAB_PROFILE_KEY);
        int value = lab_dom_clean(lab_dom_read(button, LAB_DOM_DATASET, key), LAB_DOM_CLEAN_TRIM);
        if (field == 0)
            value = lab_location_validated(LAB_PROFILE_DATE, value, lab_dom_string(""), 0);
        if (field == 3)
            value = lab_location_validated(LAB_PROFILE_JURISDICTION, value, lab_dom_get(config, "DEFAULT_DATETIME_JURISDICTION"),
                                           lab_dom_get(context, "jurisdictions"));
        lab_dom_set(state, key, value);
        if (field < 4 && (field == 3 || lab_dom_length(value)))
            lab_location_write(lab_location_element(LAB_MODE_ALMANAC, field), value);
    }
    lab_dom_return(state);
}

/* Complete a still-current eclipse action, using its explicit coordinates when supplied. */
void lab_location_totality_finish(jsref state, bool restored)
{
    if (!restored)
        lab_location_write(lab_location_element(LAB_MODE_ALMANAC, 4), lab_dom_string(""));
    for (unsigned field = 5; field < 8; ++field) {
        int value = lab_dom_get(state, lab_profile_text(LAB_MODE_ALMANAC, field, LAB_PROFILE_KEY));
        if (lab_dom_length(value))
            lab_dom_write(lab_location_element(LAB_MODE_ALMANAC, field), LAB_DOM_VALUE, "", value);
    }
}

/* Project evaluation metadata while retaining authored offsets and fallback guards. */
void lab_location_evaluation(lab_mode_t mode, jsref data, jsref context)
{
    int fields = lab_dom_get(data, "fields"), config = lab_dom_get(context, "config");
    if (!lab_dom_truth(fields))
        return;
    if (mode == LAB_MODE_DATETIME) {
        int date = lab_location_element(mode, 0), value = lab_dom_get(fields, "date");
        if (date && lab_dom_truth(value)) {
            int fallback = lab_location_value(date);
            if (!lab_dom_length(fallback))
                fallback = lab_dom_get(config, "DEFAULT_DATETIME_DATE");
            lab_dom_write(date, LAB_DOM_VALUE, "", lab_location_validated(LAB_PROFILE_DATE, value, fallback, 0));
        }
        lab_dom_write(lab_location_element(mode, 1), LAB_DOM_VALUE, "",
                      lab_location_text(lab_dom_get(fields, "julian_day_number")));
        value = lab_location_value(date);
        if (lab_dom_length(value))
            lab_dom_write(lab_location_element(mode, 4), LAB_DOM_VALUE, "", lab_dom_slice(value, 0, 4));
        int offset = lab_location_element(mode, 10), current = lab_dom_clean(lab_location_value(offset), LAB_DOM_CLEAN_TRIM);
        int returned = lab_dom_clean(lab_dom_get(fields, "gmt_offset"), LAB_DOM_CLEAN_TRIM);
        if (offset && lab_dom_length(returned) &&
            lab_profile_accept_offset(mode, 0, lab_dom_truth(lab_dom_get(context, "offsetTouched")),
                                      lab_dom_length(current) != 0,
                                      lab_dom_equal(current, lab_dom_get(context, "automaticOffset")), 1)) {
            lab_dom_write(offset, LAB_DOM_VALUE, "", returned);
            lab_location_reset_offset(context, offset);
        }
    } else if (mode == LAB_MODE_ALMANAC) {
        if (lab_location_is(lab_dom_get(fields, "zone_mode"), "auto"))
            lab_dom_write(lab_location_element(mode, 2), LAB_DOM_DATASET, "automaticZone",
                          lab_dom_clean(lab_dom_get(fields, "zone"), LAB_DOM_CLEAN_TRIM));
        int jurisdiction = lab_dom_get(fields, "jurisdiction");
        bool unidentified = lab_dom_type(jurisdiction) == LAB_VALUE_STRING && !lab_dom_length(jurisdiction);
        if (unidentified) {
            lab_location_write(lab_location_element(mode, 3), lab_dom_string(""));
            lab_location_populate(lab_location_element(mode, 4), lab_dom_object(LAB_VALUE_ARRAY), false);
            lab_location_write(lab_location_element(mode, 4), lab_dom_string(""));
        }
        /* Observer height is authored input, not an evaluation result. Town selection and
         * explicit location actions own height changes; replies must not reset it. */
        static const unsigned projection[] = {2, 5, 6, 3};
        unsigned mark = lab_dom_mark();
        for (unsigned index = 0; index < sizeof projection / sizeof *projection; ++index) {
            unsigned field = projection[index];
            if (field == 3 && unidentified)
                continue;
            int value = lab_dom_get(fields, lab_profile_text(mode, field, LAB_PROFILE_KEY));
            if (field == 3 && lab_dom_type(value) == LAB_VALUE_STRING && !lab_dom_length(value)) {
                lab_location_write(lab_location_element(mode, field), value);
                continue;
            }
            if (lab_dom_truth(value)) {
                value = lab_dom_clean(value, LAB_DOM_CLEAN_TRIM);
                if (field == 3)
                    value = lab_location_validated(LAB_PROFILE_JURISDICTION, value,
                                                   lab_dom_string(""),
                                                   lab_dom_get(context, "jurisdictions"));
                lab_location_write(lab_location_element(mode, field), value);
            }
            lab_dom_release(mark);
        }
    }
}
