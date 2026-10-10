/**
 * @file lab_select.c
 * @brief Accessible selection-menu construction and interaction in C/WebAssembly.
 *
 * Projects native select options into searchable menus, owns selection/filtering
 * policy, listener registration and keyboard navigation, and uses the browser for
 * Unicode normalisation, DOM access and deferred focus/change events. The host
 * retains real select objects as subscription contexts; handles never survive a call.
 * Rendering necessarily visits each option; temporary handles are released after
 * each row, so even large town catalogues use bounded bridge storage.
 */
#include "lab_dom.h"
#include "lab_select.h"

static void lab_select_text(int node, unsigned kind, const char *key, const char *text)
{
    lab_dom_write(node, kind, key, lab_dom_string(text));
}

static int lab_select_child(int parent, const char *tag, const char *class_name)
{
    int node = lab_dom_create(tag);
    lab_dom_class(node, class_name, 1);
    lab_dom_append(parent, node);
    return node;
}

static int lab_select_shell(int select)
{
    return lab_dom_closest(select, ".select-shell");
}

static void lab_select_close(int shell)
{
    lab_dom_class(shell, "open", 0);
    lab_select_text(lab_dom_query(shell, ".select-button"), 2, "aria-expanded", "false");
    lab_dom_class(lab_dom_query(shell, ".select-menu"), "hidden", 1);
}

/* Create the browser structure once, before native listener registration and option projection. */
int lab_select_create(int select, int label, int searchable, int placeholder, int empty, int search_placeholder)
{
    if (!select || lab_select_shell(select))
        return 0;
    int shell = lab_dom_create("div");
    lab_dom_class(shell, "select-shell", 1);
    lab_dom_wrap(select, shell);
    lab_dom_append(shell, select);
    lab_dom_class(select, "select-native-source", 1);
    lab_select_text(select, 2, "tabindex", "-1");
    lab_select_text(select, 2, "aria-hidden", "true");
    lab_dom_write(shell, 3, "placeholder", placeholder);
    int button = lab_select_child(shell, "button", "select-button");
    lab_select_text(button, 2, "type", "button");
    lab_select_text(button, 2, "aria-haspopup", "listbox");
    lab_select_text(button, 2, "aria-expanded", "false");
    int label_id = lab_dom_read(label, 2, "id");
    if (lab_dom_length(label_id))
        lab_dom_write(button, 2, "aria-labelledby", label_id);
    else
        lab_select_text(button, 2, "aria-label", "Select option");
    int menu = lab_select_child(shell, "div", "select-menu");
    lab_dom_class(menu, "hidden", 1);
    lab_select_text(menu, 2, "role", "listbox");
    if (searchable) {
        int search = lab_select_child(menu, "input", "select-search");
        lab_select_text(search, 2, "type", "search");
        lab_select_text(search, 2, "autocomplete", "off");
        lab_select_text(search, 2, "spellcheck", "false");
        lab_dom_write(search, 2, "placeholder", search_placeholder);
        lab_dom_write(search, 2, "aria-label", search_placeholder);
    }
    lab_select_child(menu, "div", "select-options");
    lab_dom_write(lab_select_child(menu, "div", "select-empty"), 0, "", empty);
    return 1;
}

static int lab_select_visible(int shell)
{
    return lab_dom_all(shell, ".select-option:not(.hidden):not(:disabled)");
}

static int lab_select_selected(int shell)
{
    return lab_dom_query(shell, ".select-option.selected:not(.hidden):not(:disabled)");
}

static void lab_select_filter(int shell)
{
    int query = lab_dom_normalize(lab_dom_read(lab_dom_query(shell, ".select-search"), 5, ""));
    int items = lab_dom_all(shell, ".select-option");
    unsigned visible = 0, navigation = 0, count = lab_dom_count(items), mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int item = lab_dom_item(items, i);
        int match = lab_dom_contains(lab_dom_read(item, 3, "searchText"), query);
        lab_dom_class(item, "hidden", !match);
        visible += match != 0;
        if (match && !lab_dom_is_disabled(item))
            lab_dom_write(item, 3, "navIndex", lab_dom_format(navigation++, "", ""));
        lab_dom_release(mark);
    }
    lab_dom_class(lab_dom_query(shell, ".select-empty"), "visible", !visible);
}

/* Rebuild from the actual select options, including disabled optgroups and town details. */
void lab_select_rebuild(int select, int details)
{
    int shell = lab_select_shell(select);
    if (!shell)
        return;
    int wrap = lab_dom_query(shell, ".select-options"), options = lab_dom_all(select, "option");
    lab_select_text(wrap, 0, "", "");
    unsigned count = lab_dom_count(options), mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int option = lab_dom_item(options, i), item = lab_select_child(wrap, "button", "select-option");
        int label = lab_dom_read(option, 0, ""), value = lab_dom_read(option, 5, "");
        int detail = details ? lab_dom_read(option, 3, "detail") : 0;
        lab_select_text(item, 2, "type", "button");
        lab_select_text(item, 2, "role", "option");
        lab_dom_write(item, 3, "value", value);
        lab_dom_disabled(item, lab_dom_is_disabled(option));
        if (lab_dom_length(detail)) {
            lab_dom_class(item, "two-column", 1);
            lab_dom_write(lab_select_child(item, "span", "select-option-label"), 0, "", label);
            lab_dom_write(lab_select_child(item, "span", "select-option-detail"), 0, "", detail);
        } else {
            lab_dom_write(item, 0, "", label);
        }
        int search = lab_dom_join(label, lab_dom_string(" "), "");
        search = lab_dom_join(search, lab_dom_read(option, 3, "latitude"), " ");
        search = lab_dom_join(search, lab_dom_read(option, 3, "longitude"), " ");
        search = lab_dom_join(search, detail, " ");
        search = lab_dom_join(search, value, "");
        lab_dom_write(item, 3, "searchText", lab_dom_normalize(search));
        lab_dom_release(mark);
    }
    lab_select_sync(select);
}

/* Project selection and disabled state before applying the current search. */
void lab_select_sync(int select)
{
    int shell = lab_select_shell(select);
    if (!shell)
        return;
    int button = lab_dom_query(shell, ".select-button"), selected = lab_dom_selected(select);
    lab_dom_write(button, 0, "", selected ? lab_dom_read(selected, 0, "") : lab_dom_read(shell, 3, "placeholder"));
    lab_dom_disabled(button, lab_dom_is_disabled(select));
    if (lab_dom_is_disabled(select))
        lab_select_close(shell);
    int value = lab_dom_read(select, 5, ""), items = lab_dom_all(shell, ".select-option");
    unsigned count = lab_dom_count(items), mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int item = lab_dom_item(items, i);
        int chosen = selected && lab_dom_equal(lab_dom_read(item, 3, "value"), value);
        lab_dom_class(item, "selected", chosen);
        lab_select_text(item, 2, "aria-selected", chosen ? "true" : "false");
        lab_dom_release(mark);
    }
    lab_select_filter(shell);
}

static void lab_select_open(int select, int shell)
{
    lab_select_text(lab_dom_query(shell, ".select-search"), 5, "", "");
    lab_select_sync(select);
    lab_dom_class(shell, "open", 1);
    lab_select_text(lab_dom_query(shell, ".select-button"), 2, "aria-expanded", "true");
    lab_dom_class(lab_dom_query(shell, ".select-menu"), "hidden", 0);
    lab_dom_effect(lab_select_selected(shell), 1);
}

static void lab_select_focus(int shell, int step)
{
    int list = lab_select_visible(shell), selected = lab_select_selected(shell);
    unsigned count = lab_dom_count(list);
    if (!step) {
        int node = selected ? selected : lab_dom_item(list, 0);
        if (!node)
            node = lab_dom_query(shell, ".select-search");
        lab_dom_effect(node ? node : lab_dom_query(shell, ".select-button"), 0);
    } else if (count) {
        int active = lab_dom_active();
        int current =
            lab_dom_has_class(active, "select-option") && lab_dom_equal(lab_dom_closest(active, ".select-shell"), shell)
                ? active
                : selected;
        unsigned index = current ? (unsigned)lab_dom_number(lab_dom_read(current, 3, "navIndex")) : 0;
        index = step > 0 ? (index + 1) % count : (index + count - 1) % count;
        lab_dom_effect(lab_dom_item(list, index), 0);
    }
}

/* Handle encoded browser events; focus/change effects run after scoped handles are released. */
int lab_select_event(int select, unsigned source, unsigned key, int target)
{
    int shell = lab_select_shell(select);
    if (!shell || source > 4 || key > 6)
        return 0;
    int button = lab_dom_query(shell, ".select-button");
    if (source == 3 || lab_dom_is_disabled(select)) {
        lab_select_close(shell);
        return 0;
    }
    if (source == 4) {
        if (key || !lab_dom_has_class(target, "select-option") || lab_dom_has_class(target, "hidden") ||
            lab_dom_is_disabled(target) || !lab_dom_equal(lab_dom_closest(target, ".select-shell"), shell))
            return 0;
        int value = lab_dom_read(target, 3, "value");
        int changed = !lab_dom_equal(value, lab_dom_read(select, 5, ""));
        lab_dom_write(select, 5, "", value);
        lab_select_sync(select);
        lab_select_close(shell);
        lab_dom_effect(button, 0);
        if (changed)
            lab_dom_effect(select, 2);
        return 0;
    }
    if (key == 5) {
        lab_select_close(shell);
        if (source)
            lab_dom_effect(button, 0);
        return source != 0;
    }
    if (!source && (key == 0 || key == 1 || key == 3 || key == 4)) {
        if (!key && lab_dom_has_class(shell, "open"))
            lab_select_close(shell);
        else {
            lab_select_open(select, shell);
            if (key) {
                int search = lab_dom_query(shell, ".select-search");
                if (search)
                    lab_dom_effect(search, 0);
                else
                    lab_select_focus(shell, 0);
            }
        }
        return key != 0;
    }
    if (source == 1 && key == 6) {
        lab_select_filter(shell);
        lab_dom_effect(lab_select_selected(shell), 1);
    } else if (source == 1 && key == 1) {
        lab_select_focus(shell, 0);
        return 1;
    } else if (source == 2 && (key == 1 || key == 2)) {
        lab_select_focus(shell, key == 1 ? 1 : -1);
        return 1;
    }
    return 0;
}

enum {
    lab_select_button_click,
    lab_select_button_key,
    lab_select_menu_key,
    lab_select_menu_click,
    lab_select_search_input,
    lab_select_search_key,
    lab_select_label_click,
    lab_select_document_click,
    lab_select_change,
    lab_select_action_count
};

static unsigned lab_select_key(int event)
{
    int key = lab_dom_get(event, "key");
    /* Five fixed browser key tokens have bounded cost; catalogue navigation uses direct indices. */
    static const char *const keys[] = {"ArrowDown", "ArrowUp", "Enter", " ", "Escape"};
    for (unsigned i = 0; i < sizeof keys / sizeof *keys; ++i) {
        if (lab_dom_equal(key, lab_dom_string(keys[i])))
            return i + 1;
    }
    return 0;
}

static int lab_select_event_target(int event)
{
    int target = lab_dom_get(event, "target");
    return lab_dom_number(lab_dom_get(target, "nodeType")) == 1 ? target : lab_dom_get(target, "parentElement");
}

/* Dispatch one subscribed event with only synchronous, borrowed handles and deferred browser effects. */
void lab_select_dispatch(unsigned action, int event, int select)
{
    int plan = lab_dom_object(5);
    lab_dom_set(plan, "calls", lab_dom_object(4));
    lab_dom_return(plan);
    int shell = lab_select_shell(select);
    if (!shell || action >= lab_select_action_count)
        return;
    int prevent = 0, stop = 0;
    if (action == lab_select_button_key || action == lab_select_menu_key || action == lab_select_search_key) {
        unsigned key = lab_select_key(event);
        unsigned source = action == lab_select_button_key ? 0 : action == lab_select_menu_key ? 2 : 1;
        if (key)
            prevent = stop = lab_select_event(select, source, key, 0);
    } else if (action == lab_select_button_click) {
        lab_select_event(select, 0, 0, 0);
    } else if (action == lab_select_menu_click) {
        lab_select_event(select, 4, 0, lab_dom_closest(lab_select_event_target(event), ".select-option"));
    } else if (action == lab_select_search_input) {
        lab_select_event(select, 1, 6, 0);
    } else if (action == lab_select_label_click) {
        prevent = 1;
        lab_dom_effect(lab_dom_query(shell, ".select-button"), 0);
    } else if (action == lab_select_document_click) {
        if (!lab_dom_node_contains(shell, lab_dom_get(event, "target")))
            lab_select_event(select, 3, 0, 0);
    } else if (action == lab_select_change) {
        lab_select_sync(select);
    }
    if (prevent)
        lab_dom_set(plan, "prevent", lab_dom_scalar(1, 1));
    if (stop)
        lab_dom_set(plan, "stop", lab_dom_scalar(1, 1));
}

static int lab_select_subscribe(int select, int document)
{
    if (!select || lab_dom_truth(lab_dom_get(select, "labSelectEventsBound")))
        return 0;
    int shell = lab_select_shell(select);
    if (!shell)
        return 0;
    int button = lab_dom_query(shell, ".select-button"), menu = lab_dom_query(shell, ".select-menu");
    if (!button || !menu)
        return 0;
    int search = lab_dom_query(shell, ".select-search");
    int label = lab_dom_item(lab_dom_get(select, "labels"), 0);
    const struct {
        int node;
        const char *type;
        unsigned action;
    } listeners[] = {{button, "click", lab_select_button_click}, {button, "keydown", lab_select_button_key},
                     {menu, "keydown", lab_select_menu_key},     {menu, "click", lab_select_menu_click},
                     {search, "input", lab_select_search_input}, {search, "keydown", lab_select_search_key},
                     {label, "click", lab_select_label_click},   {document, "click", lab_select_document_click},
                     {select, "change", lab_select_change}};
    for (unsigned i = 0; i < sizeof listeners / sizeof *listeners; ++i) {
        if (listeners[i].node)
            lab_dom_subscribe(listeners[i].node, listeners[i].type, "lab_select_dispatch", listeners[i].action, select,
                              0);
    }
    lab_dom_set(select, "labSelectEventsBound", lab_dom_scalar(1, 1));
    return 1;
}

/* Subscribe once per select, including a distinct outside-click context on its owning document. */
int lab_select_register(int select)
{
    return lab_select_subscribe(select, lab_dom_get(select, "ownerDocument"));
}

static int lab_select_option_text(int options, const char *key, const char *fallback)
{
    int text = lab_dom_get(options, key);
    return lab_dom_truth(text) ? lab_dom_text(text) : lab_dom_string(fallback);
}

/* Create, subscribe and initially project a selection menu from its opaque browser options record. */
int lab_select_enhance(int select, int options, int document)
{
    if (!select)
        return 0;
    int label = lab_dom_item(lab_dom_get(select, "labels"), 0);
    if (!lab_select_create(select, label, lab_dom_truth(lab_dom_get(options, "searchable")),
                           lab_select_option_text(options, "placeholder", "Select option"),
                           lab_select_option_text(options, "emptyText", "No matches"),
                           lab_select_option_text(options, "searchPlaceholder", "Search")))
        return 0;
    lab_select_subscribe(select, document ? document : lab_dom_get(select, "ownerDocument"));
    lab_select_refresh(select, options);
    return 1;
}

/* Rebuild from the current options record, keeping presentation-option policy out of browser closures. */
void lab_select_refresh(int select, int options)
{
    lab_select_rebuild(select, lab_dom_truth(lab_dom_get(options, "details")));
}
