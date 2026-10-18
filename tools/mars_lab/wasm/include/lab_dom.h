/**
 * @file lab_dom.h
 * @brief Scoped browser DOM and value capabilities imported by WebAssembly.
 *
 * Declares the generic JavaScript bridge supplied by transport.js. Browser modules use these capabilities
 * without owning DOM nodes or retaining handles across calls. Handles are borrowed for one synchronous
 * labDOM.call; zero denotes absence, or the document for query/all. Static NUL-terminated labels are
 * copied synchronously and bounded to 4095 bytes. Release temporary handles at their scope boundary.
 * Boolean capabilities use C bool; the host supplies canonical zero/one i32 values at the WASM boundary.
 * Browser-value handles remain integers, including handles referring to JavaScript booleans.
 */
#ifndef LAB_WASM_DOM_H
#define LAB_WASM_DOM_H

#include <stdbool.h>

#include "lab_handle.h"
#include "lab_value.h"

/** @brief DOM text accessors shared by reads and writes. */
typedef enum {
    LAB_DOM_TEXT = 0, /**< Text content. */
    LAB_DOM_HTML = 1, /**< Native HTML markup. */
    LAB_DOM_ATTRIBUTE = 2, /**< Named element attribute. */
    LAB_DOM_DATASET = 3, /**< Named dataset property. */
    LAB_DOM_STYLE = 4, /**< Named inline CSS property. */
    LAB_DOM_VALUE = 5 /**< Form control value. */
} lab_dom_accessor_t;

/** @brief Deferred browser effects. */
typedef enum {
    LAB_DOM_EFFECT_FOCUS = 0, /**< Focus the element. */
    LAB_DOM_EFFECT_SCROLL = 1, /**< Scroll to the nearest visible position. */
    LAB_DOM_EFFECT_CHANGE = 2, /**< Dispatch a bubbling change event. */
    LAB_DOM_EFFECT_SELECT = 3, /**< Select the input text. */
    LAB_DOM_EFFECT_BLUR = 4 /**< Remove focus. */
} lab_dom_effect_t;

/** @brief Opaque text normalisation policies. */
typedef enum {
    LAB_DOM_CLEAN_TRIM = 0, /**< Trim both ends. */
    LAB_DOM_CLEAN_LOWER = 1, /**< Trim and lower-case. */
    LAB_DOM_CLEAN_COLLAPSE = 2, /**< Trim and collapse whitespace. */
    LAB_DOM_CLEAN_TRIM_END = 3 /**< Trim trailing whitespace. */
} lab_dom_clean_t;

/** @brief Browser layout and SVG-transform measurements. */
typedef enum {
    LAB_DOM_MEASURE_CLIENT_WIDTH = 0, /**< Client width in CSS pixels. */
    LAB_DOM_MEASURE_SVG_WIDTH = 1, /**< SVG width in CSS pixels. */
    LAB_DOM_MEASURE_SVG_HEIGHT = 2, /**< SVG height in CSS pixels. */
    LAB_DOM_MEASURE_RECT_WIDTH = 3, /**< Bounding rectangle width. */
    LAB_DOM_MEASURE_RECT_HEIGHT = 4, /**< Bounding rectangle height. */
    LAB_DOM_MEASURE_VIEWBOX_WIDTH = 5, /**< SVG view-box width. */
    LAB_DOM_MEASURE_VIEWBOX_HEIGHT = 6, /**< SVG view-box height. */
    LAB_DOM_MEASURE_RECT_LEFT = 7, /**< Bounding rectangle left edge. */
    LAB_DOM_MEASURE_RECT_TOP = 8, /**< Bounding rectangle top edge. */
    LAB_DOM_MEASURE_RECT_BOTTOM = 9, /**< Bounding rectangle bottom edge. */
    LAB_DOM_MEASURE_CLIENT_HEIGHT = 10, /**< Client height in CSS pixels. */
    LAB_DOM_MEASURE_SCROLL_HEIGHT = 11, /**< Scroll height in CSS pixels. */
    LAB_DOM_MEASURE_VISIBLE = 12, /**< Connected and has a visible layout box. */
    LAB_DOM_MEASURE_CTM_A = 13, /**< Screen-transform a coefficient. */
    LAB_DOM_MEASURE_CTM_B = 14, /**< Screen-transform b coefficient. */
    LAB_DOM_MEASURE_CTM_C = 15, /**< Screen-transform c coefficient. */
    LAB_DOM_MEASURE_CTM_D = 16, /**< Screen-transform d coefficient. */
    LAB_DOM_MEASURE_CTM_E = 17, /**< Screen-transform e coefficient. */
    LAB_DOM_MEASURE_CTM_F = 18 /**< Screen-transform f coefficient. */
} lab_dom_measure_t;

/** @brief Deferred browser layout scheduling. */
typedef enum {
    LAB_DOM_SCHEDULE_FIT = 0, /**< Responsive layout fitting. */
    LAB_DOM_SCHEDULE_ZOOM = 1 /**< Card zoom animation frame. */
} lab_dom_schedule_t;

/**
 * @brief Mark the current synchronous browser-handle scope.
 * @return Handle-table boundary to pass to lab_dom_release; valid only within the current call.
 */
unsigned lab_dom_mark(void);

/**
 * @brief Resolve an exact document ID through the browser API.
 * @param id Borrowed NUL-terminated UTF-8 control ID, without a selector prefix.
 * @return Scoped DOM node handle, or zero when absent.
 */
node_jsref lab_dom_id(const char *id);

/**
 * @brief Subscribe once to lab_events_dispatch without retaining native handles.
 * @param node Scoped EventTarget handle; zero is ignored.
 * @param type Borrowed NUL-terminated DOM event name.
 * @param action Dispatch selector accepted by lab_events_dispatch, including packed calendar mode/field codes.
 * @param passive True requests passive delivery; false allows preventDefault.
 * @details The browser retains the target and suppresses duplicate subscriptions.
 */
void lab_dom_listen(node_jsref node, const char *type, unsigned action, bool passive);

/**
 * @brief Read a browser property, including inherited properties and getters.
 * @param object Scoped browser value to inspect; a nullish value supplies undefined.
 * @param key Borrowed NUL-terminated property name.
 * @return Scoped property-value handle, or zero for null or undefined.
 */
browser_jsref lab_dom_get(browser_jsref object, const char *key);

/**
 * @brief Test an object's property presence, including inherited properties.
 * @param object Scoped non-null object; other values return false.
 * @param key Static property name; presence does not invoke the property's getter.
 * @return True when the property exists, even when its value is undefined.
 */
bool lab_dom_property_present(browser_jsref object, const char *key);

/**
 * @brief Read an own property using a browser-owned key.
 * @param object Scoped browser object to inspect; a falsey value has no readable properties.
 * @param key Scoped property key, converted using browser property-key semantics.
 * @return Scoped own-property value, or zero for a missing or nullish value; inherited properties are excluded.
 */
browser_jsref lab_dom_key_get(browser_jsref object, browser_jsref key);

/**
 * @brief Classify a scoped browser value without coercion.
 * @param value Scoped handle to classify; zero denotes null.
 * @return Corresponding LAB_VALUE kind; nullish values use LAB_VALUE_NULL, other browser values LAB_VALUE_OBJECT.
 */
lab_value_kind_t lab_dom_type(browser_jsref value);

/**
 * @brief Apply browser boolean coercion.
 * @param value Scoped browser value to inspect.
 * @return True for a truthy value; false for a falsey value.
 */
bool lab_dom_truth(browser_jsref value);

/**
 * @brief Create a browser-owned data container.
 * @param kind LAB_VALUE_ARRAY creates an array; any other selector creates a plain object.
 * @return Scoped handle to the new empty container.
 */
browser_jsref lab_dom_object(lab_value_kind_t kind);

/**
 * @brief Wrap a native number in a browser-value handle.
 * @param value Scalar number to retain, including non-finite values; not a browser handle.
 * @return Scoped handle to the browser number.
 */
browser_jsref lab_dom_numeric(double value);

/**
 * @brief Append a browser value to an array.
 * @param array Scoped destination array handle.
 * @param value Scoped value handle; the array retains the browser value, not the handle.
 */
void lab_dom_push(browser_jsref array, browser_jsref value);

/**
 * @brief Wrap a native scalar as a browser value.
 * @param kind LAB_VALUE_BOOLEAN, LAB_VALUE_NUMBER, or any other selector for null.
 * @param value Scalar input; boolean conversion treats zero and NaN as false, and other numbers as true.
 * @return Scoped boolean or number handle, or zero for null.
 */
browser_jsref lab_dom_scalar(lab_value_kind_t kind, double value);

/**
 * @brief Apply browser String conversion without a falsey-value fallback.
 * @param value Scoped browser value to convert; zero converts to the text "null".
 * @return Scoped string handle.
 */
browser_jsref lab_dom_text(browser_jsref value);

/**
 * @brief Apply browser Number conversion without replacing NaN.
 * @param value Scoped browser value to convert; zero denotes null and converts to numeric zero.
 * @return Converted scalar number, including NaN or infinity when produced by the browser.
 */
double lab_dom_to_number(browser_jsref value);

/**
 * @brief Slice browser text by UTF-16 code-unit offsets.
 * @param value Scoped text source; falsey values select empty text.
 * @param start Inclusive offset; negative offsets count backwards from the end.
 * @param end Exclusive offset; negative offsets count backwards from the end.
 * @return Scoped substring, using browser String.slice bounds handling.
 */
browser_jsref lab_dom_slice(browser_jsref value, int start, int end);

/**
 * @brief Split browser text on a literal separator.
 * @param value Scoped text source; falsey values select empty text.
 * @param separator Borrowed NUL-terminated separator; empty text splits into UTF-16 code units.
 * @return Scoped array of string fragments.
 */
browser_jsref lab_dom_split(browser_jsref value, const char *separator);

/**
 * @brief Test a literal prefix using browser string semantics.
 * @param value Scoped text source; falsey values select empty text.
 * @param prefix Borrowed NUL-terminated literal prefix, not a regular expression.
 * @return True when the text begins with the prefix; false otherwise.
 */
bool lab_dom_starts_with(browser_jsref value, const char *prefix);

/**
 * @brief Compare opaque labels using the browser's current locale.
 * @param left Scoped first value, converted to a string.
 * @param right Scoped second value, converted to a string.
 * @param numeric True selects numeric collation with base sensitivity; false uses default locale collation.
 * @return Negative, zero or positive according to the selected collation.
 */
int lab_dom_locale_compare(browser_jsref left, browser_jsref right, bool numeric);

/**
 * @brief Test membership in a browser Set or Map.
 * @param set Scoped Set or Map handle; other value types have no members.
 * @param value Scoped element or Map key to look up using browser collection equality.
 * @return True when the element or key exists; false otherwise.
 */
bool lab_dom_member(browser_jsref set, browser_jsref value);

/**
 * @brief Delete a property by its native literal name.
 * @param object Scoped object whose own property is removed; falsey values are ignored.
 * @param key Borrowed NUL-terminated property name.
 */
void lab_dom_delete(browser_jsref object, const char *key);

/**
 * @brief Delete a property using a browser-owned key.
 * @param object Scoped object whose own property is removed; falsey values are ignored.
 * @param key Scoped key, converted using browser property-key semantics.
 */
void lab_dom_key_delete(browser_jsref object, browser_jsref key);

/**
 * @brief Create an empty browser-owned Map.
 * @return Scoped Map handle; stored keys and values are browser values, never native handle numbers.
 */
browser_jsref lab_dom_map_new(void);

/**
 * @brief Read a browser Map entry.
 * @param map Scoped Map handle, or zero for no map.
 * @param key Scoped lookup key, compared using browser Map equality.
 * @return Scoped stored-value handle, or zero for a missing map, key or nullish value.
 */
browser_jsref lab_dom_map_get(browser_jsref map, browser_jsref key);

/**
 * @brief Insert or replace a browser Map entry.
 * @param map Scoped destination Map handle.
 * @param key Scoped entry key; its browser value is retained.
 * @param value Scoped entry value; its browser value is retained.
 */
void lab_dom_map_set(browser_jsref map, browser_jsref key, browser_jsref value);

/**
 * @brief Count browser Map entries.
 * @param map Scoped Map handle, or zero for no map.
 * @return Number of entries, or zero when the map is absent.
 */
unsigned lab_dom_map_size(browser_jsref map);

/**
 * @brief Read the earliest remaining key in a browser Map.
 * @param map Scoped Map handle, or zero for no map.
 * @return Scoped first-key handle in insertion order, or zero for an empty map or a nullish key.
 */
browser_jsref lab_dom_map_first(browser_jsref map);

/**
 * @brief Remove a browser Map entry.
 * @param map Scoped Map handle; an absent map is ignored.
 * @param key Scoped key to remove, compared using browser Map equality.
 */
void lab_dom_map_delete(browser_jsref map, browser_jsref key);

/**
 * @brief Enumerate own enumerable string property names.
 * @param object Scoped browser value to enumerate; falsey values supply an empty object.
 * @return Scoped array of property-name strings, in browser Object.keys order.
 */
browser_jsref lab_dom_keys(browser_jsref object);

/**
 * @brief Define an own writable, enumerable and configurable data property.
 * @param object Scoped destination object.
 * @param key Borrowed NUL-terminated property name; even "__proto__" is defined as ordinary data.
 * @param value Scoped value handle; the property retains the browser value by reference.
 */
void lab_dom_set(browser_jsref object, const char *key, browser_jsref value);

/**
 * @brief Copy a property without conflating undefined and null in the handle bridge.
 * @param target Scoped destination object; the property is defined as ordinary data.
 * @param target_key Static destination property name.
 * @param source Scoped source value; nullish sources supply undefined.
 * @param source_key Static source property name; its getter is read once.
 */
void lab_dom_property_copy(browser_jsref target, const char *target_key, browser_jsref source, const char *source_key);

/**
 * @brief Compare property values using browser strict equality, reading the left getter first.
 * @param left Scoped left value; nullish values supply undefined.
 * @param left_key Static left property name.
 * @param right Scoped right value; nullish values supply undefined.
 * @param right_key Static right property name.
 * @return True when the property values are strictly equal, including undefined versus undefined.
 */
bool lab_dom_properties_equal(browser_jsref left, const char *left_key, browser_jsref right, const char *right_key);

/**
 * @brief Define an own data property using a browser-owned key.
 * @param object Scoped destination object.
 * @param key Scoped key, converted using browser property-key semantics.
 * @param value Scoped value handle; the property retains the browser value by reference.
 * @details The property is writable, enumerable and configurable, without invoking an inherited setter.
 */
void lab_dom_key_set(browser_jsref object, browser_jsref key, browser_jsref value);

/**
 * @brief Publish the browser result of the current labDOM.call.
 * @param value Scoped result handle, or zero to publish null.
 * @details The host retains the value before releasing the scope, never the integer handle.
 */
void lab_dom_return(browser_jsref value);

/**
 * @brief Release temporary handles without destroying the referenced browser resources.
 * @param mark Boundary previously obtained from lab_dom_mark in the current active scope.
 * @details Handles at or above the boundary become invalid. An invalid boundary raises a host exception.
 */
void lab_dom_release(unsigned mark);

/**
 * @brief Normalise opaque browser text for accent-insensitive search.
 * @param text Scoped text source; falsey values select empty text.
 * @return Scoped NFD text with U+0300 through U+036F removed, lower-cased and trimmed.
 */
browser_jsref lab_dom_normalize(browser_jsref text);

/**
 * @brief Search opaque strings without performing normalisation.
 * @param text Scoped haystack; falsey values select empty text.
 * @param query Scoped literal substring; falsey values select the empty substring.
 * @return True when the haystack contains the substring, including an empty substring; false otherwise.
 */
bool lab_dom_contains(browser_jsref text, browser_jsref query);

/**
 * @brief Obtain a native select control's first selected option.
 * @param select Scoped HTMLSelectElement handle; other value types have no selected option.
 * @return Scoped option handle, or zero when no option is selected.
 */
node_jsref lab_dom_selected(node_jsref select);

/**
 * @brief Obtain the document's currently focused element.
 * @return Scoped document.activeElement handle, or zero when absent.
 */
node_jsref lab_dom_active(void);

/**
 * @brief Test the browser's effective disabled state.
 * @param node Scoped element handle, or zero; the browser applies the :disabled selector, including inherited rules.
 * @return True when the element matches :disabled; false otherwise.
 */
bool lab_dom_is_disabled(node_jsref node);

/**
 * @brief Insert a detached wrapper immediately before an existing node.
 * @param node Scoped existing node whose parent receives the wrapper; a parentless node causes no insertion.
 * @param shell Scoped detached wrapper node to insert.
 * @details This operation does not move node into shell; append it separately if required.
 */
void lab_dom_wrap(node_jsref node, browser_jsref shell);

/**
 * @brief Queue a DOM effect until the synchronous native call releases its handles.
 * @param node Scoped target element handle; zero causes no effect.
 * @param action Focus 0, scroll into nearest view 1, bubbling change event 2, select input text 3, or blur 4.
 * @details Invalid actions raise a host exception. Failed calls discard their queued effects.
 */
void lab_dom_effect(node_jsref node, lab_dom_effect_t action);

/**
 * @brief Copy native UTF-8 text into a browser-owned string.
 * @param text Borrowed NUL-terminated UTF-8 text, bounded to 4095 bytes by the host literal reader.
 * @return Scoped string handle.
 */
browser_jsref lab_dom_string(const char *text);

/**
 * @brief Measure text using browser UTF-16 code units.
 * @param value Scoped text source; falsey values select empty text before string conversion.
 * @return Number of UTF-16 code units, not UTF-8 bytes or Unicode code points.
 */
unsigned lab_dom_length(browser_jsref value);

/**
 * @brief Compare browser values using strict equality.
 * @param left Scoped first browser value.
 * @param right Scoped second browser value.
 * @return True for strict equality; false otherwise. Objects compare by identity and NaN is not equal to itself.
 */
bool lab_dom_equal(browser_jsref left, browser_jsref right);

/**
 * @brief Order strings lexically, or other browser values numerically.
 * @param left Scoped first value; compared as text only when both operands are strings.
 * @param right Scoped second value; otherwise both operands undergo Number conversion.
 * @return Negative, zero or positive ordering; strings use UTF-16 code units and incomparable numbers yield zero.
 */
int lab_dom_compare(browser_jsref left, browser_jsref right);

/**
 * @brief Apply browser whitespace and case operations to opaque text.
 * @param text Scoped text source; falsey values select empty text.
 * @param kind Trim both ends 0, trim and lower-case 1, trim and collapse whitespace 2, or trim trailing whitespace 3.
 * @return Scoped cleaned string; unknown selectors use ordinary trimming.
 */
browser_jsref lab_dom_clean(browser_jsref text, lab_dom_clean_t kind);

/**
 * @brief Test attribute presence, including empty attribute values.
 * @param node Scoped element handle, or zero.
 * @param key Borrowed NUL-terminated attribute name.
 * @return True when the attribute is present; false for a missing element or attribute.
 */
bool lab_dom_has(node_jsref node, const char *key);

/**
 * @brief Remove an element attribute.
 * @param node Scoped element handle; zero causes no action.
 * @param key Borrowed NUL-terminated attribute name.
 */
void lab_dom_remove(node_jsref node, const char *key);

/**
 * @brief Convert a browser value to a number with a zero fallback.
 * @param value Scoped browser value to convert using Number.
 * @return Converted scalar number; NaN and negative zero become positive zero, while infinities are preserved.
 */
double lab_dom_number(browser_jsref value);

/**
 * @brief Format a scalar number between literal text fragments.
 * @param value Native scalar number, not a browser handle.
 * @param prefix Borrowed NUL-terminated text preceding the browser-formatted number.
 * @param suffix Borrowed NUL-terminated text following the number, such as a CSS unit.
 * @return Scoped concatenated string.
 */
browser_jsref lab_dom_format(double value, const char *prefix, const char *suffix);

/**
 * @brief Format a number to significant digits and remove redundant trailing zeroes.
 * @param value Native scalar number to format using browser Number.toPrecision.
 * @param digits Requested significant decimal digits, 1 through 100; values outside this range raise a host exception.
 * @return Scoped string after converting the rounded decimal text back through Number.
 */
browser_jsref lab_dom_precision(double value, unsigned digits);

/**
 * @brief Concatenate two opaque browser values and a native suffix.
 * @param left Scoped first text source; falsey values select empty text.
 * @param right Scoped second text source; falsey values select empty text.
 * @param suffix Borrowed NUL-terminated literal suffix.
 * @return Scoped concatenated string.
 */
browser_jsref lab_dom_join(browser_jsref left, browser_jsref right, const char *suffix);

/**
 * @brief Join browser-owned text fragments with a literal separator.
 * @param array Scoped array of fragments, converted using browser Array.join semantics; non-arrays raise an exception.
 * @param separator Borrowed NUL-terminated separator inserted between adjacent fragments.
 * @return Scoped joined string; nullish entries and holes contribute empty fragments.
 */
browser_jsref lab_dom_join_array(browser_jsref array, const char *separator);

/**
 * @brief Find the first matching descendant of a browser query root.
 * @param parent Scoped query-root handle, or zero to search the document.
 * @param selector Borrowed NUL-terminated CSS selector; invalid selector syntax raises a browser exception.
 * @return Scoped matching node handle, or zero when no node matches.
 */
node_jsref lab_dom_query(browser_jsref parent, const char *selector);

/**
 * @brief Snapshot all matching descendants of a browser query root.
 * @param parent Scoped query-root handle, or zero to search the document.
 * @param selector Borrowed NUL-terminated CSS selector; invalid selector syntax raises a browser exception.
 * @return Scoped static NodeList handle in document order.
 */
browser_jsref lab_dom_all(browser_jsref parent, const char *selector);

/**
 * @brief Read the length of a browser collection.
 * @param list Scoped array, NodeList or other value with a numeric length property.
 * @return Length converted to a number, or zero for an absent or non-numeric length.
 */
unsigned lab_dom_count(browser_jsref list);

/**
 * @brief Read an indexed browser collection member.
 * @param list Scoped array, NodeList or other indexed browser value.
 * @param index Zero-based property index to read.
 * @return Scoped member handle, or zero for a missing or nullish entry.
 */
browser_jsref lab_dom_item(browser_jsref list, unsigned index);

/**
 * @brief Test whether an array index exists, distinguishing holes from explicit null entries.
 * @param array Scoped array.
 * @param index Zero-based index.
 * @return True for a present property, including an inherited index.
 */
bool lab_dom_index_present(browser_jsref array, unsigned index);

/**
 * @brief Find the nearest matching element, starting with the supplied element.
 * @param node Scoped element handle; non-elements produce no match.
 * @param selector Borrowed NUL-terminated CSS selector applied to the element and its ancestors.
 * @return Scoped matching element handle, or zero when absent.
 */
node_jsref lab_dom_closest(node_jsref node, const char *selector);

/**
 * @brief Create a detached HTML element in the current document.
 * @param tag Borrowed NUL-terminated HTML tag name; this is not an SVG namespace creation operation.
 * @return Scoped handle to the new detached element.
 */
node_jsref lab_dom_create(const char *tag);

/**
 * @brief Append a node, moving it from its previous parent if needed.
 * @param parent Scoped destination parent-node handle.
 * @param child Scoped child-node handle to append.
 */
void lab_dom_append(browser_jsref parent, browser_jsref child);

/**
 * @brief Insert a node as an element's first child.
 * @param parent Scoped destination element handle; zero causes no action.
 * @param child Scoped child-node handle to insert, moving it from its previous parent if needed.
 */
void lab_dom_prepend(browser_jsref parent, browser_jsref child);

/**
 * @brief Read browser text through the selected DOM accessor.
 * @param node Scoped DOM node or control handle; zero supplies empty text.
 * @param kind Text, HTML, attribute, dataset, inline CSS or control-value accessor.
 * @param key Borrowed NUL-terminated attribute, dataset or CSS name; pass empty text for selectors 0, 1 and 5.
 * @return Scoped accessor result; absent or falsey results become empty text.
 */
browser_jsref lab_dom_read(node_jsref node, lab_dom_accessor_t kind, const char *key);

/**
 * @brief Write browser text through the selected DOM setter.
 * @param node Scoped DOM node or control handle; zero causes no action.
 * @param kind Text, HTML, attribute, dataset, inline CSS or control-value accessor.
 * @param key Borrowed NUL-terminated attribute, dataset or CSS name; pass empty text for selectors 0, 1 and 5.
 * @param value Scoped text source; falsey values become empty text. HTML must come from native presentation only.
 */
void lab_dom_write(node_jsref node, lab_dom_accessor_t kind, const char *key, browser_jsref value);

/**
 * @brief Set membership of an element's CSS class list.
 * @param node Scoped element handle; zero causes no action.
 * @param name Borrowed NUL-terminated single CSS class name.
 * @param enabled True adds the class; false removes it.
 */
void lab_dom_class(node_jsref node, const char *name, bool enabled);

/**
 * @brief Inspect an element's CSS class list.
 * @param node Scoped element handle, or zero.
 * @param name Borrowed NUL-terminated single CSS class name.
 * @return True when the class is present; false otherwise.
 */
bool lab_dom_has_class(node_jsref node, const char *name);

/**
 * @brief Set a control's live disabled property when its state differs.
 * @param node Scoped control handle; zero causes no action.
 * @param disabled True disables the control; false enables it.
 */
void lab_dom_disabled(node_jsref node, bool disabled);

/**
 * @brief Set a checkbox or radio control's live checked state through its browser setter.
 * @param node Scoped input-control handle; zero causes no action.
 * @param checked True checks the control; false clears it.
 */
void lab_dom_checked(node_jsref node, bool checked);

/**
 * @brief Read a browser layout measurement or SVG screen-transform coefficient.
 * @param node Scoped element handle, or zero.
 * @param kind Client width 0, absolute SVG width/height 1/2, rectangle width/height 3/4,
 * SVG view-box width/height 5/6, rectangle left/top/bottom 7/8/9, client height 10, scroll height 11,
 * connected visible-layout flag 12, or SVG screen-transform coefficients a/b/c/d/e/f 13 through 18.
 * @return CSS pixels for layout lengths, SVG user units for view-box lengths, or zero/one for selector 12.
 * Selectors 13..18 map SVG user coordinates to viewport CSS pixels; unavailable transforms yield NaN.
 * An absent node returns zero for every selector.
 */
double lab_dom_measure(node_jsref node, lab_dom_measure_t kind);

/**
 * @brief Parse a numeric prefix from a computed CSS property.
 * @param node Scoped element handle used with getComputedStyle.
 * @param key Borrowed NUL-terminated CSS property name, in CSS hyphenated notation.
 * @return Parsed scalar prefix or zero when non-numeric; units are not converted.
 */
double lab_dom_css(node_jsref node, const char *key);

/**
 * @brief Resolve an element's registered result-card index.
 * @param node Scoped HTMLElement handle to look up in the presentation registry.
 * @return Stable registered card index, or -1 for an unregistered or non-HTML element.
 */
int lab_dom_card_id(node_jsref node);

/**
 * @brief Schedule browser-owned layout work.
 * @param action Responsive-fit scheduling 0 or card-zoom animation-frame scheduling 1.
 * @param card Scoped card-element handle for action 1; unused for action 0.
 * @details The browser retains the actual element until its callback, not the scoped handle.
 */
void lab_dom_schedule(lab_dom_schedule_t action, browser_jsref card);

/**
 * @brief Apply the browser's integer-prefix conversion to a value.
 * @param value Scoped browser value converted to text before parsing.
 * @param radix Base 2 through 36, or zero for browser prefix-based inference; other bases yield NaN.
 * @return Parsed scalar number, or NaN when no valid integer prefix is present.
 */
double lab_dom_parse_int(browser_jsref value, unsigned radix);

/**
 * @brief Subscribe a browser target to a named native dispatcher with a stable browser context.
 * @param node Scoped EventTarget handle; zero is ignored.
 * @param type Static DOM event name.
 * @param entry Static exported dispatcher name, accepting action, event and context handles.
 * @param action Dispatcher-specific action index.
 * @param context Scoped browser object retained by the listener as a value, never as a handle.
 * @param passive True requests a passive listener.
 * @details Repeated registration of the same target, event, entry, action and context is ignored.
 * Dispatch returns an event plan; browser services run only after its synchronous handle scope ends.
 */
void lab_dom_subscribe(node_jsref node, const char *type, const char *entry, unsigned action, browser_jsref context,
                       bool passive);

/**
 * @brief Check browser node containment, including equality.
 * @param parent Scoped parent node, or zero.
 * @param child Scoped child node, or zero.
 * @return True when both nodes exist and parent contains child; false otherwise.
 */
bool lab_dom_node_contains(browser_jsref parent, browser_jsref child);

/**
 * @brief Resolve an exact document ID supplied as a browser string.
 * @param id Scoped string handle; no selector syntax is interpreted.
 * @return Scoped matching node handle, or zero when absent.
 */
node_jsref lab_dom_value_id(browser_jsref id);

/**
 * @brief Copy an opaque browser string to a bounded UTF-8 buffer.
 * @param value Scoped browser value converted using String.
 * @param target Writable WebAssembly buffer with at least capacity bytes available.
 * @param capacity Maximum number of UTF-8 bytes to copy; no NUL terminator is written.
 * @return Byte count, or -1 for overflow or an unpaired Unicode surrogate; no partial copy is published.
 */
int lab_dom_utf8_copy(browser_jsref value, unsigned char *target, unsigned capacity);

/**
 * @brief Wrap length-delimited UTF-8 bytes in a scoped browser string.
 * @param source Borrowed WebAssembly bytes; embedded NUL bytes are preserved.
 * @param length Byte count.
 * @return Scoped string handle. Invalid UTF-8 raises a host exception.
 */
browser_jsref lab_dom_utf8_string(const unsigned char *source, unsigned length);

#endif
