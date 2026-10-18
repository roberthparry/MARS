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

static void lab_select_text(node_jsref node, lab_dom_accessor_t kind, const char *key, const char *text)
{
    lab_dom_write(node, kind, key, lab_dom_string(text));
}

static jsref lab_select_child(jsref parent, const char *tag, const char *class_name)
{
    int node = lab_dom_create(tag);
    lab_dom_class(node, class_name, 1);
    lab_dom_append(parent, node);
    return node;
}

static jsref lab_select_shell(node_jsref select)
{
    return lab_dom_closest(select, ".select-shell");
}

static void lab_select_close(jsref shell)
{
    lab_dom_class(shell, "open", 0);
    lab_select_text(lab_dom_query(shell, ".select-button"), LAB_DOM_ATTRIBUTE, "aria-expanded", "false");
    lab_dom_class(lab_dom_query(shell, ".select-menu"), "hidden", 1);
}

/* Create the browser structure once, before native listener registration and option projection. */
bool lab_select_create(node_jsref select, jsref label, bool searchable, jsref placeholder,
                       jsref empty, jsref search_placeholder)
{
    if (!select || lab_select_shell(select))
        return false;
    int shell = lab_dom_create("div");
    lab_dom_class(shell, "select-shell", 1);
    lab_dom_wrap(select, shell);
    lab_dom_append(shell, select);
    lab_dom_class(select, "select-native-source", 1);
    lab_select_text(select, LAB_DOM_ATTRIBUTE, "tabindex", "-1");
    lab_select_text(select, LAB_DOM_ATTRIBUTE, "aria-hidden", "true");
    lab_dom_write(shell, LAB_DOM_DATASET, "placeholder", placeholder);
    int button = lab_select_child(shell, "button", "select-button");
    lab_select_text(button, LAB_DOM_ATTRIBUTE, "type", "button");
    lab_select_text(button, LAB_DOM_ATTRIBUTE, "aria-haspopup", "listbox");
    lab_select_text(button, LAB_DOM_ATTRIBUTE, "aria-expanded", "false");
    int label_id = lab_dom_read(label, LAB_DOM_ATTRIBUTE, "id");
    if (lab_dom_length(label_id))
        lab_dom_write(button, LAB_DOM_ATTRIBUTE, "aria-labelledby", label_id);
    else
        lab_select_text(button, LAB_DOM_ATTRIBUTE, "aria-label", "Select option");
    int menu = lab_select_child(shell, "div", "select-menu");
    lab_dom_class(menu, "hidden", 1);
    lab_select_text(menu, LAB_DOM_ATTRIBUTE, "role", "listbox");
    if (searchable) {
        int search = lab_select_child(menu, "input", "select-search");
        lab_select_text(search, LAB_DOM_ATTRIBUTE, "type", "search");
        lab_select_text(search, LAB_DOM_ATTRIBUTE, "autocomplete", "off");
        lab_select_text(search, LAB_DOM_ATTRIBUTE, "spellcheck", "false");
        lab_dom_write(search, LAB_DOM_ATTRIBUTE, "placeholder", search_placeholder);
        lab_dom_write(search, LAB_DOM_ATTRIBUTE, "aria-label", search_placeholder);
    }
    lab_select_child(menu, "div", "select-options");
    lab_dom_write(lab_select_child(menu, "div", "select-empty"), LAB_DOM_TEXT, "", empty);
    return true;
}

static jsref lab_select_visible(jsref shell)
{
    return lab_dom_all(shell, ".select-option:not(.hidden):not(:disabled)");
}

static jsref lab_select_selected(jsref shell)
{
    return lab_dom_query(shell, ".select-option.selected:not(.hidden):not(:disabled)");
}

static void lab_select_filter(jsref shell)
{
    int query = lab_dom_normalize(lab_dom_read(lab_dom_query(shell, ".select-search"), LAB_DOM_VALUE, ""));
    int items = lab_dom_all(shell, ".select-option");
    unsigned visible = 0, navigation = 0, count = lab_dom_count(items), mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int item = lab_dom_item(items, i);
        int match = lab_dom_contains(lab_dom_read(item, LAB_DOM_DATASET, "searchText"), query);
        lab_dom_class(item, "hidden", !match);
        visible += match != 0;
        if (match && !lab_dom_is_disabled(item))
            lab_dom_write(item, LAB_DOM_DATASET, "navIndex", lab_dom_format(navigation++, "", ""));
        lab_dom_release(mark);
    }
    lab_dom_class(lab_dom_query(shell, ".select-empty"), "visible", !visible);
}

/* Rebuild from the actual select options, including disabled optgroups and town details. */
void lab_select_rebuild(node_jsref select, bool details)
{
    int shell = lab_select_shell(select);
    if (!shell)
        return;
    int wrap = lab_dom_query(shell, ".select-options"), options = lab_dom_all(select, "option");
    lab_select_text(wrap, LAB_DOM_TEXT, "", "");
    unsigned count = lab_dom_count(options), mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int option = lab_dom_item(options, i), item = lab_select_child(wrap, "button", "select-option");
        int label = lab_dom_read(option, LAB_DOM_TEXT, ""), value = lab_dom_read(option, LAB_DOM_VALUE, "");
        int detail = details ? lab_dom_read(option, LAB_DOM_DATASET, "detail") : 0;
        lab_select_text(item, LAB_DOM_ATTRIBUTE, "type", "button");
        lab_select_text(item, LAB_DOM_ATTRIBUTE, "role", "option");
        lab_dom_write(item, LAB_DOM_DATASET, "value", value);
        lab_dom_disabled(item, lab_dom_is_disabled(option));
        if (lab_dom_length(detail)) {
            lab_dom_class(item, "two-column", 1);
            lab_dom_write(lab_select_child(item, "span", "select-option-label"), LAB_DOM_TEXT, "", label);
            lab_dom_write(lab_select_child(item, "span", "select-option-detail"), LAB_DOM_TEXT, "", detail);
        } else {
            lab_dom_write(item, LAB_DOM_TEXT, "", label);
        }
        int search = lab_dom_join(label, lab_dom_string(" "), "");
        search = lab_dom_join(search, lab_dom_read(option, LAB_DOM_DATASET, "latitude"), " ");
        search = lab_dom_join(search, lab_dom_read(option, LAB_DOM_DATASET, "longitude"), " ");
        search = lab_dom_join(search, detail, " ");
        search = lab_dom_join(search, value, "");
        lab_dom_write(item, LAB_DOM_DATASET, "searchText", lab_dom_normalize(search));
        lab_dom_release(mark);
    }
    lab_select_sync(select);
}

/* Project selection and disabled state before applying the current search. */
void lab_select_sync(node_jsref select)
{
    int shell = lab_select_shell(select);
    if (!shell)
        return;
    int button = lab_dom_query(shell, ".select-button"), selected = lab_dom_selected(select);
    lab_dom_write(button, LAB_DOM_TEXT, "",
                  selected ? lab_dom_read(selected, LAB_DOM_TEXT, "")
                           : lab_dom_read(shell, LAB_DOM_DATASET, "placeholder"));
    lab_dom_disabled(button, lab_dom_is_disabled(select));
    if (lab_dom_is_disabled(select))
        lab_select_close(shell);
    int value = lab_dom_read(select, LAB_DOM_VALUE, ""), items = lab_dom_all(shell, ".select-option");
    unsigned count = lab_dom_count(items), mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int item = lab_dom_item(items, i);
        int chosen = selected && lab_dom_equal(lab_dom_read(item, LAB_DOM_DATASET, "value"), value);
        lab_dom_class(item, "selected", chosen);
        lab_select_text(item, LAB_DOM_ATTRIBUTE, "aria-selected", chosen ? "true" : "false");
        lab_dom_release(mark);
    }
    lab_select_filter(shell);
}

static void lab_select_open(node_jsref select, jsref shell)
{
    lab_select_text(lab_dom_query(shell, ".select-search"), LAB_DOM_VALUE, "", "");
    lab_select_sync(select);
    lab_dom_class(shell, "open", 1);
    lab_select_text(lab_dom_query(shell, ".select-button"), LAB_DOM_ATTRIBUTE, "aria-expanded", "true");
    lab_dom_class(lab_dom_query(shell, ".select-menu"), "hidden", 0);
    lab_dom_effect(lab_select_selected(shell), LAB_DOM_EFFECT_SCROLL);
}

static void lab_select_focus(jsref shell, int step)
{
    int list = lab_select_visible(shell), selected = lab_select_selected(shell);
    unsigned count = lab_dom_count(list);
    if (!step) {
        int node = selected ? selected : lab_dom_item(list, 0);
        if (!node)
            node = lab_dom_query(shell, ".select-search");
        lab_dom_effect(node ? node : lab_dom_query(shell, ".select-button"), LAB_DOM_EFFECT_FOCUS);
    } else if (count) {
        int active = lab_dom_active();
        int current =
            lab_dom_has_class(active, "select-option") && lab_dom_equal(lab_dom_closest(active, ".select-shell"), shell)
                ? active
                : selected;
        unsigned index = current ? (unsigned)lab_dom_number(lab_dom_read(current, LAB_DOM_DATASET, "navIndex")) : 0;
        index = step > 0 ? (index + 1) % count : (index + count - 1) % count;
        lab_dom_effect(lab_dom_item(list, index), LAB_DOM_EFFECT_FOCUS);
    }
}

/* Commit only an enabled option belonging to this menu, then queue focus and change. */
static bool lab_select_option_event(node_jsref select, jsref shell, jsref button,
                                lab_select_key_t key, jsref target)
{
    if (key != LAB_SELECT_KEY_CLICK || !lab_dom_has_class(target, "select-option") ||
        lab_dom_has_class(target, "hidden") ||
        lab_dom_is_disabled(target) || !lab_dom_equal(lab_dom_closest(target, ".select-shell"), shell))
        return false;
    int value = lab_dom_read(target, LAB_DOM_DATASET, "value");
    int changed = !lab_dom_equal(value, lab_dom_read(select, LAB_DOM_VALUE, ""));
    lab_dom_write(select, LAB_DOM_VALUE, "", value);
    lab_select_sync(select);
    lab_select_close(shell);
    lab_dom_effect(button, LAB_DOM_EFFECT_FOCUS);
    if (changed)
        lab_dom_effect(select, LAB_DOM_EFFECT_CHANGE);
    return false;
}

/* Open for recognised activation keys; only a click toggles an already open menu. */
static bool lab_select_button_event(node_jsref select, jsref shell, jsref button,
                                lab_select_key_t key, jsref target)
{
    (void)button;
    (void)target;
    static const unsigned char activates[LAB_SELECT_KEY_INPUT + 1] = {
        [LAB_SELECT_KEY_CLICK] = 1,
        [LAB_SELECT_KEY_DOWN]  = 1,
        [LAB_SELECT_KEY_ENTER] = 1,
        [LAB_SELECT_KEY_SPACE] = 1,
    };
    if (!activates[key])
        return false;
    if (key == LAB_SELECT_KEY_CLICK && lab_dom_has_class(shell, "open"))
        lab_select_close(shell);
    else {
        lab_select_open(select, shell);
        if (key != LAB_SELECT_KEY_CLICK) {
            int search = lab_dom_query(shell, ".select-search");
            if (search)
                lab_dom_effect(search, LAB_DOM_EFFECT_FOCUS);
            else
                lab_select_focus(shell, 0);
        }
    }
    return key != LAB_SELECT_KEY_CLICK;
}

/* Keep search edits and initial keyboard focus as distinct ordered operations. */
static bool lab_select_search_event(node_jsref select, jsref shell, jsref button,
                                lab_select_key_t key, jsref target)
{
    (void)select;
    (void)button;
    (void)target;
    if (key == LAB_SELECT_KEY_INPUT) {
        lab_select_filter(shell);
        lab_dom_effect(lab_select_selected(shell), LAB_DOM_EFFECT_SCROLL);
    } else if (key == LAB_SELECT_KEY_DOWN) {
        lab_select_focus(shell, 0);
        return true;
    }
    return false;
}

/* Use the validated key as a direct navigation-step index. */
static bool lab_select_menu_event(node_jsref select, jsref shell, jsref button, lab_select_key_t key,
                                jsref target)
{
    (void)select;
    (void)button;
    (void)target;
    static const int steps[LAB_SELECT_KEY_INPUT + 1] = {
        [LAB_SELECT_KEY_DOWN] =  1,
        [LAB_SELECT_KEY_UP]   = -1,
    };
    if (!steps[key])
        return false;
    lab_select_focus(shell, steps[key]);
    return true;
}

/* Handle encoded browser events; focus/change effects run after scoped handles are released. */
bool lab_select_event(node_jsref select, lab_select_source_t source, lab_select_key_t key, jsref target)
{
    /** @brief Source-specific interaction after common selector and disabled-state validation. */
    typedef bool (*lab_select_event_handler_t)(node_jsref select, node_jsref shell, node_jsref button,
                                               lab_select_key_t key, node_jsref target);
    static const lab_select_event_handler_t handlers[LAB_SELECT_SOURCE_OPTION + 1] = {
        [LAB_SELECT_SOURCE_BUTTON] = lab_select_button_event,
        [LAB_SELECT_SOURCE_SEARCH] = lab_select_search_event,
        [LAB_SELECT_SOURCE_MENU]   = lab_select_menu_event,
        [LAB_SELECT_SOURCE_OPTION] = lab_select_option_event,
    };
    int shell = lab_select_shell(select);
    if (!shell || (unsigned)source > LAB_SELECT_SOURCE_OPTION || (unsigned)key > LAB_SELECT_KEY_INPUT)
        return false;
    int button = lab_dom_query(shell, ".select-button");
    if (source == LAB_SELECT_SOURCE_OUTSIDE || lab_dom_is_disabled(select)) {
        lab_select_close(shell);
        return false;
    }
    /* Options accept only validated clicks; Escape belongs to the other interaction sources. */
    if (source != LAB_SELECT_SOURCE_OPTION && key == LAB_SELECT_KEY_ESCAPE) {
        lab_select_close(shell);
        if (source != LAB_SELECT_SOURCE_BUTTON)
            lab_dom_effect(button, LAB_DOM_EFFECT_FOCUS);
        return source != LAB_SELECT_SOURCE_BUTTON;
    }
    return handlers[source] ? handlers[source](select, shell, button, key, target) : false;
}

static lab_select_key_t lab_select_key(event_jsref event)
{
    int key = lab_dom_get(event, "key");
    /* Five fixed browser key tokens have bounded cost; catalogue navigation uses direct indices. */
    static const char *const keys[] = {"ArrowDown", "ArrowUp", "Enter", " ", "Escape"};
    for (unsigned i = 0; i < sizeof keys / sizeof *keys; ++i) {
        if (lab_dom_equal(key, lab_dom_string(keys[i])))
            return (lab_select_key_t)(i + LAB_SELECT_KEY_DOWN);
    }
    return LAB_SELECT_KEY_CLICK;
}

static jsref lab_select_event_target(event_jsref event)
{
    int target = lab_dom_get(event, "target");
    return lab_dom_number(lab_dom_get(target, "nodeType")) == 1 ? target : lab_dom_get(target, "parentElement");
}

/* Decode recognised keys before forwarding them to their configured interaction source. */
static bool lab_select_key_action(node_jsref select, event_jsref event, jsref shell, lab_select_source_t source)
{
    (void)shell;
    lab_select_key_t key = lab_select_key(event);
    return key != LAB_SELECT_KEY_CLICK ? lab_select_event(select, source, key, 0) : false;
}

/* Resolve an option target only for menu clicks; button clicks need no event fields. */
static bool lab_select_click_action(node_jsref select, event_jsref event, jsref shell, lab_select_source_t source)
{
    (void)shell;
    int option = source == LAB_SELECT_SOURCE_OPTION
                     ? lab_dom_closest(lab_select_event_target(event), ".select-option") : false;
    lab_select_event(select, source, LAB_SELECT_KEY_CLICK, option);
    return false;
}

/* Forward a search edit without consuming its browser event. */
static bool lab_select_input_action(node_jsref select, event_jsref event, jsref shell, lab_select_source_t source)
{
    (void)event;
    (void)shell;
    lab_select_event(select, source, LAB_SELECT_KEY_INPUT, 0);
    return false;
}

/* Redirect label focus to the accessible button and suppress only the default action. */
static bool lab_select_label_action(node_jsref select, event_jsref event, jsref shell, lab_select_source_t source)
{
    (void)select;
    (void)event;
    (void)source;
    lab_dom_effect(lab_dom_query(shell, ".select-button"), LAB_DOM_EFFECT_FOCUS);
    return true;
}

/* Close only when the document click lies outside this selection menu. */
static bool lab_select_document_action(node_jsref select, event_jsref event, jsref shell, lab_select_source_t source)
{
    if (!lab_dom_node_contains(shell, lab_dom_get(event, "target")))
        lab_select_event(select, source, LAB_SELECT_KEY_CLICK, 0);
    return false;
}

/* Project a native selection change without consuming the event. */
static bool lab_select_change_action(node_jsref select, event_jsref event, jsref shell, lab_select_source_t source)
{
    (void)event;
    (void)shell;
    (void)source;
    lab_select_sync(select);
    return false;
}

/* Dispatch one subscribed event with only synchronous, borrowed handles and deferred browser effects. */
void lab_select_dispatch(lab_select_action_t action, event_jsref event, node_jsref select)
{
    /** @brief Subscribed action returning whether to suppress the browser's default behaviour. */
    typedef bool (*lab_select_action_handler_t)(node_jsref select, event_jsref event, node_jsref shell,
                                                lab_select_source_t source);
    static const struct {
        lab_select_action_handler_t handler;
        lab_select_source_t source;
        int stop;
    } actions[LAB_SELECT_ACTION_COUNT] = {
        [LAB_SELECT_BUTTON_CLICK]   = {lab_select_click_action,    LAB_SELECT_SOURCE_BUTTON,  0},
        [LAB_SELECT_BUTTON_KEY]     = {lab_select_key_action,      LAB_SELECT_SOURCE_BUTTON,  1},
        [LAB_SELECT_MENU_KEY]       = {lab_select_key_action,      LAB_SELECT_SOURCE_MENU,    1},
        [LAB_SELECT_MENU_CLICK]     = {lab_select_click_action,    LAB_SELECT_SOURCE_OPTION,  0},
        [LAB_SELECT_SEARCH_INPUT]   = {lab_select_input_action,    LAB_SELECT_SOURCE_SEARCH,  0},
        [LAB_SELECT_SEARCH_KEY]     = {lab_select_key_action,      LAB_SELECT_SOURCE_SEARCH,  1},
        [LAB_SELECT_LABEL_CLICK]    = {lab_select_label_action,    LAB_SELECT_SOURCE_BUTTON,  0},
        [LAB_SELECT_DOCUMENT_CLICK] = {lab_select_document_action, LAB_SELECT_SOURCE_OUTSIDE, 0},
        [LAB_SELECT_CHANGE]         = {lab_select_change_action,   LAB_SELECT_SOURCE_BUTTON,  0},
    };
    int plan = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(plan, "calls", lab_dom_object(LAB_VALUE_ARRAY));
    lab_dom_return(plan);
    int shell = lab_select_shell(select);
    if (!shell || (unsigned)action >= LAB_SELECT_ACTION_COUNT || !actions[action].handler)
        return;
    int prevent = actions[action].handler(select, event, shell, actions[action].source);
    if (prevent)
        lab_dom_set(plan, "prevent", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    if (prevent && actions[action].stop)
        lab_dom_set(plan, "stop", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
}

static int lab_select_subscribe(node_jsref select, jsref document)
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
        lab_select_action_t action;
    } listeners[] = {{button, "click", LAB_SELECT_BUTTON_CLICK}, {button, "keydown", LAB_SELECT_BUTTON_KEY},
                     {menu, "keydown", LAB_SELECT_MENU_KEY},     {menu, "click", LAB_SELECT_MENU_CLICK},
                     {search, "input", LAB_SELECT_SEARCH_INPUT}, {search, "keydown", LAB_SELECT_SEARCH_KEY},
                     {label, "click", LAB_SELECT_LABEL_CLICK},   {document, "click", LAB_SELECT_DOCUMENT_CLICK},
                     {select, "change", LAB_SELECT_CHANGE}};
    for (unsigned i = 0; i < sizeof listeners / sizeof *listeners; ++i) {
        if (listeners[i].node)
            lab_dom_subscribe(listeners[i].node, listeners[i].type, "lab_select_dispatch", listeners[i].action, select,
                              0);
    }
    lab_dom_set(select, "labSelectEventsBound", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    return 1;
}

/* Subscribe once per select, including a distinct outside-click context on its owning document. */
bool lab_select_register(node_jsref select)
{
    return lab_select_subscribe(select, lab_dom_get(select, "ownerDocument"));
}

static jsref lab_select_option_text(jsref options, const char *key, const char *fallback)
{
    int text = lab_dom_get(options, key);
    return lab_dom_truth(text) ? lab_dom_text(text) : lab_dom_string(fallback);
}

/* Create, subscribe and initially project a selection menu from its opaque browser options record. */
bool lab_select_enhance(node_jsref select, jsref options, jsref document)
{
    if (!select)
        return false;
    int label = lab_dom_item(lab_dom_get(select, "labels"), 0);
    if (!lab_select_create(select, label, lab_dom_truth(lab_dom_get(options, "searchable")),
                           lab_select_option_text(options, "placeholder", "Select option"),
                           lab_select_option_text(options, "emptyText", "No matches"),
                           lab_select_option_text(options, "searchPlaceholder", "Search")))
        return false;
    lab_select_subscribe(select, document ? document : lab_dom_get(select, "ownerDocument"));
    lab_select_refresh(select, options);
    return true;
}

/* Rebuild from the current options record, keeping presentation-option policy out of browser closures. */
void lab_select_refresh(node_jsref select, jsref options)
{
    lab_select_rebuild(select, lab_dom_truth(lab_dom_get(options, "details")));
}
