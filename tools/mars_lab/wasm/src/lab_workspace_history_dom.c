/**
 * @file lab_workspace_history_dom.c
 * @brief Native presentation of the editable worksheet timeline.
 *
 * Builds labelled rows and accessible action buttons from opaque snapshots. Ordering and mutations remain
 * in lab_workspace.c; the browser supplies decoded records and routes clicks, focus and dialog lifetime.
 * Calendar rows use saved town identities and the country catalogue instead of their raw worksheet summary.
 * Flag artwork arrives independently through the browser's asynchronous country requests. Authored text is
 * displayed with textContent, never treated as HTML or evaluated here; saved snapshots are never modified.
 */
#include <math.h>

#include "lab_dom.h"
#include "lab_globe_markers.h"
#include "lab_location.h"
#include "lab_workspace_dom.h"

static jsref history_text(jsref object, const char *field)
{
    return lab_dom_clean(lab_dom_join(0, lab_dom_get(object, field), ""), LAB_DOM_CLEAN_TRIM);
}

static void history_accessible_label(node_jsref button)
{
    int text = lab_dom_read(button, LAB_DOM_TEXT, "");
    lab_dom_write(button, LAB_DOM_ATTRIBUTE, "title", text);
    lab_dom_write(button, LAB_DOM_ATTRIBUTE, "aria-label", lab_dom_join(lab_dom_string("Open worksheet: "), text, ""));
}

static bool history_location(node_jsref button, jsref snapshot)
{
    int mode = history_text(snapshot, "mode");
    bool almanac = lab_dom_equal(mode, lab_dom_string("almanac"));
    if (!almanac && !lab_dom_equal(mode, lab_dom_string("datetime"))) return false;
    int calendar = lab_dom_get(snapshot, almanac ? "almanac" : "datetime");
    if (lab_dom_type(calendar) != LAB_VALUE_OBJECT) return false;
    int code = history_text(calendar, "jurisdiction");
    int select = lab_dom_id(almanac ? "almanacJurisdiction" : "datetimeJurisdiction");
    int labels = lab_dom_get(select, "labJurisdictionLabels"), country = lab_dom_key_get(labels, code);
    if (!lab_dom_length(country)) country = lab_dom_key_get(labels, lab_dom_slice(code, 0, 2));
    lab_dom_write(button, LAB_DOM_DATASET, "historyCountryResolved", lab_dom_string(lab_dom_length(country) ? "1" : ""));
    if (!lab_dom_length(country)) country = lab_dom_string("Country not identified");
    int town = lab_location_town_text(lab_dom_item(lab_dom_split(history_text(calendar, "town"), "|"), 0));
    bool named = lab_dom_length(town) != 0;
    int body = lab_dom_create("span"), heading = lab_dom_create("span"), town_label = lab_dom_create("span");
    int country_label = lab_dom_create("span"), detail = lab_dom_create("span"), flag = lab_dom_create("img");
    lab_dom_write(button, LAB_DOM_TEXT, "", lab_dom_string(""));
    lab_dom_class(button, "history-entry-location", true);
    lab_dom_write(flag, LAB_DOM_ATTRIBUTE, "class", lab_dom_string("history-entry-flag"));
    lab_dom_write(flag, LAB_DOM_ATTRIBUTE, "alt", lab_dom_string(""));
    lab_dom_write(flag, LAB_DOM_ATTRIBUTE, "hidden", lab_dom_string(""));
    lab_dom_write(flag, LAB_DOM_ATTRIBUTE, "decoding", lab_dom_string("async"));
    lab_dom_write(body, LAB_DOM_ATTRIBUTE, "class", lab_dom_string("history-entry-place"));
    lab_dom_write(heading, LAB_DOM_ATTRIBUTE, "class", lab_dom_string("history-entry-place-name"));
    lab_dom_write(town_label, LAB_DOM_TEXT, "", lab_dom_join(named ? town : lab_dom_string("Custom location"), 0, " — "));
    lab_dom_write(country_label, LAB_DOM_ATTRIBUTE, "class", lab_dom_string("history-entry-country"));
    lab_dom_write(country_label, LAB_DOM_TEXT, "", country);
    lab_dom_append(heading, town_label);
    lab_dom_append(heading, country_label);
    int date = history_text(calendar, "date"), time = history_text(calendar, "time");
    int description = almanac ? lab_dom_join(lab_dom_join(date, lab_dom_string(" "), ""), time, " GMT") : date;
    if (!named) {
        int latitude = history_text(calendar, "latitude"), longitude = history_text(calendar, "longitude");
        double lat = lab_dom_to_number(latitude), lon = lab_dom_to_number(longitude);
        if (lab_dom_length(latitude) && lab_dom_length(longitude) && isfinite(lat) && isfinite(lon) &&
            fabs(lat) <= 90 && fabs(lon) <= 180) {
            if (lab_dom_length(description)) description = lab_dom_join(description, 0, " · ");
            description = lab_dom_join(description,
                lab_globe_coordinate_text(lat, lab_dom_string("N"), lab_dom_string("S")), ", ");
            description = lab_dom_join(description,
                lab_globe_coordinate_text(lon, lab_dom_string("E"), lab_dom_string("W")), "");
        }
    }
    lab_dom_write(detail, LAB_DOM_ATTRIBUTE, "class", lab_dom_string("history-entry-place-detail"));
    lab_dom_write(detail, LAB_DOM_TEXT, "", description);
    lab_dom_append(body, heading);
    lab_dom_append(body, detail);
    lab_dom_append(button, flag);
    lab_dom_append(button, body);
    lab_dom_write(button, LAB_DOM_DATASET, "historyLocation", lab_dom_string("1"));
    lab_dom_write(button, LAB_DOM_DATASET, "historyCountryCode", code);
    lab_dom_write(button, LAB_DOM_DATASET, "historyLatitude", history_text(calendar, "latitude"));
    lab_dom_write(button, LAB_DOM_DATASET, "historyLongitude", history_text(calendar, "longitude"));
    history_accessible_label(button);
    return true;
}

/* Install database-owned country artwork without rebuilding the row or changing its saved observer. */
void lab_workspace_dom_history_country(node_jsref button, jsref metadata)
{
    int country = lab_dom_query(button, ".history-entry-country"), image = lab_dom_query(button, ".history-entry-flag");
    if (!country || !image || lab_dom_type(metadata) != LAB_VALUE_OBJECT) return;
    int name = history_text(metadata, "country"), flag = history_text(metadata, "country_flag");
    if (!lab_dom_length(name)) name = history_text(metadata, "name");
    if (!lab_dom_length(flag)) flag = history_text(metadata, "flag");
    if (lab_dom_length(name) && !lab_dom_length(lab_dom_read(button, LAB_DOM_DATASET, "historyCountryResolved")))
        lab_dom_write(country, LAB_DOM_TEXT, "", name);
    if (lab_dom_length(flag) <= 1048576 && (lab_dom_starts_with(flag, "data:image/svg+xml;base64,") ||
                                         lab_dom_equal(flag, lab_dom_string("/offshore_flag.svg")))) {
        lab_dom_write(image, LAB_DOM_ATTRIBUTE, "src", flag);
        lab_dom_remove(image, "hidden");
    }
    history_accessible_label(button);
}

/* Project one snapshot and its native timeline position without retaining browser handles. */
void lab_workspace_dom_history_row(jsref list, jsref snapshot, unsigned position, unsigned current, unsigned count)
{
    int row = lab_dom_create("li"), label = lab_dom_create("button"), actions = lab_dom_create("span");
    lab_dom_write(row, LAB_DOM_ATTRIBUTE, "class", lab_dom_string("history-entry"));
    lab_dom_write(row, LAB_DOM_DATASET, "historyPosition", lab_dom_format(position, "", ""));
    if (position == current) lab_dom_write(row, LAB_DOM_ATTRIBUTE, "aria-current", lab_dom_string("step"));
    lab_dom_write(label, LAB_DOM_ATTRIBUTE, "type", lab_dom_string("button"));
    lab_dom_write(label, LAB_DOM_ATTRIBUTE, "class", lab_dom_string("history-entry-open secondary"));
    lab_dom_write(label, LAB_DOM_DATASET, "historyAction", lab_dom_string("open"));
    int source = lab_dom_text(lab_dom_get(snapshot, "text"));
    int text = lab_dom_clean(lab_dom_slice(source, 0, 180), LAB_DOM_CLEAN_COLLAPSE);
    lab_dom_write(label, LAB_DOM_ATTRIBUTE, "title", lab_dom_slice(source, 0, 4096));
    if (lab_dom_length(source) > 180) text = lab_dom_join(text, lab_dom_string("…"), "");
    if (!lab_dom_truth(text)) text = lab_dom_string("Empty worksheet");
    lab_dom_write(label, LAB_DOM_TEXT, "", text);
    lab_dom_write(label, LAB_DOM_ATTRIBUTE, "aria-label", lab_dom_join(lab_dom_string("Open worksheet: "), text, ""));
    history_location(label, snapshot);
    lab_dom_append(row, label);
    lab_dom_write(actions, LAB_DOM_ATTRIBUTE, "class", lab_dom_string("history-entry-actions"));
    if (position == current) {
        int marker = lab_dom_create("span");
        lab_dom_write(marker, LAB_DOM_TEXT, "", lab_dom_string("Current"));
        lab_dom_write(marker, LAB_DOM_ATTRIBUTE, "class", lab_dom_string("history-current"));
        lab_dom_append(actions, marker);
    }
    static const char *const names[] = {"earlier", "later", "remove"};
    static const char *const labels[] = {"↑ Earlier", "↓ Later", "Remove"};
    static const char *const titles[] = {"Move earlier", "Move later", "Remove entry"};
    for (unsigned i = 0; i < 3; ++i) {
        unsigned mark = lab_dom_mark();
        int button = lab_dom_create("button");
        lab_dom_write(button, LAB_DOM_ATTRIBUTE, "type", lab_dom_string("button"));
        lab_dom_write(button, LAB_DOM_ATTRIBUTE, "class", lab_dom_string("secondary"));
        lab_dom_write(button, LAB_DOM_TEXT, "", lab_dom_string(labels[i]));
        lab_dom_write(button, LAB_DOM_ATTRIBUTE, "title", lab_dom_string(titles[i]));
        lab_dom_write(button, LAB_DOM_ATTRIBUTE, "aria-label", lab_dom_string(titles[i]));
        lab_dom_write(button, LAB_DOM_DATASET, "historyAction", lab_dom_string(names[i]));
        lab_dom_disabled(button, i == 0 ? position == 0 : i == 1 ? position + 1 == count : count == 1);
        lab_dom_append(actions, button);
        lab_dom_release(mark);
    }
    lab_dom_append(row, actions);
    lab_dom_append(list, row);
}
