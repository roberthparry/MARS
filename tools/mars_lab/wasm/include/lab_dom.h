/**
 * @file lab_dom.h
 * @brief Scoped browser DOM and value capabilities imported by WebAssembly.
 *
 * Declares the generic JavaScript bridge supplied by transport.js. Browser modules use these capabilities
 * without owning DOM nodes or retaining handles across calls. Handles are borrowed for one synchronous
 * labDOM.call; zero denotes absence, or the document for query/all. Static NUL-terminated labels are
 * copied synchronously and bounded to 4095 bytes. Release temporary handles at their scope boundary.
 */
#ifndef LAB_WASM_DOM_H
#define LAB_WASM_DOM_H

/** @brief Mark the current handle table. @return Scope boundary for lab_dom_release. */
unsigned lab_dom_mark(void);

/** @brief Resolve an exact document ID through the browser API.
 * @param id Borrowed static, NUL-terminated control ID, without a selector prefix.
 * @return Scoped DOM node handle, or zero when absent.
 */
int lab_dom_id(const char *id);

/** @brief Subscribe once to native event dispatch; retain only browser-owned targets.
 * @param node Scoped EventTarget handle; zero is ignored.
 * @param type Static, NUL-terminated browser event name.
 * @param action Native dispatch index, or packed calendar mode and field.
 * @param passive Non-zero permits passive delivery; zero permits preventDefault.
 */
void lab_dom_listen(int node, const char *type, unsigned action, unsigned passive);

/** @brief Read a browser value's property. @param object Scoped value. @param key Static property name.
 * @return Scoped property handle, or zero when absent. */
int lab_dom_get(int object, const char *key);

/** @brief Read a property using an opaque key. @param object Scoped value. @param key Scoped string.
 * @return Scoped property handle, or zero when absent. */
int lab_dom_key_get(int object, int key);

/** @brief Classify a scoped value. @param value Handle. @return Null 0, boolean 1, number 2, string 3, array 4,
 * object 5. */
unsigned lab_dom_type(int value);

/** @brief Inspect browser truthiness. @param value Scoped value. @return Boolean coercion. */
int lab_dom_truth(int value);

/** @brief Create a scoped data container. @param kind Array 4, otherwise plain object. @return New handle. */
int lab_dom_object(unsigned kind);

/** @brief Wrap a scalar number in a scoped value handle. @param value Number. @return Scoped handle. */
int lab_dom_numeric(double value);

/** @brief Append a scoped value to an array. @param array Array handle. @param value Value handle. */
void lab_dom_push(int array, int value);

/** @brief Wrap a scalar value. @param kind Boolean 1, number 2, otherwise null. @param value Scalar input.
 * @return Scoped value handle. */
int lab_dom_scalar(unsigned kind, double value);

/** @brief Apply browser String conversion. @param value Scoped value. @return Scoped string. */
int lab_dom_text(int value);

/** @brief Apply browser Number conversion without replacing NaN. @param value Scoped value. @return Converted number.
 */
double lab_dom_to_number(int value);

/** @brief Slice browser text by UTF-16 indices. @param value String handle. @param start Inclusive index.
 * @param end Exclusive index; negative indices count from the end. @return Scoped string. */
int lab_dom_slice(int value, int start, int end);

/** @brief Split browser text on a literal separator. @param value String handle. @param separator Static separator.
 * @return Scoped string array. */
int lab_dom_split(int value, const char *separator);

/** @brief Test a literal prefix. @param value String handle. @param prefix Static prefix. @return Matching flag. */
int lab_dom_starts_with(int value, const char *prefix);

/** @brief Compare opaque labels with browser locale collation. @param left First text. @param right Second text.
 * @param numeric Non-zero requests numeric base-sensitive ordering; zero requests default collation.
 * @return Negative, zero or positive ordering. */
int lab_dom_locale_compare(int left, int right, int numeric);

/** @brief Query a browser Set. @param set Scoped Set. @param value Scoped key. @return Membership flag. */
int lab_dom_member(int set, int value);

/** @brief Delete a data property. @param object Scoped object. @param key Static key. */
void lab_dom_delete(int object, const char *key);

/** @brief Delete a data property using an opaque key. @param object Scoped object. @param key Scoped key. */
void lab_dom_key_delete(int object, int key);

/** @brief Create a browser Map without retaining its handle. @return Scoped Map handle. */
int lab_dom_map_new(void);

/** @brief Read a Map entry. @param map Scoped Map. @param key Scoped key. @return Value handle or zero. */
int lab_dom_map_get(int map, int key);

/** @brief Set a Map entry. @param map Scoped Map. @param key Scoped key. @param value Scoped value. */
void lab_dom_map_set(int map, int key, int value);

/** @brief Count Map entries. @param map Scoped Map. @return Entry count, or zero when absent. */
unsigned lab_dom_map_size(int map);

/** @brief Read a Map's first inserted key. @param map Scoped Map. @return Key handle or zero. */
int lab_dom_map_first(int map);

/** @brief Remove a Map entry. @param map Scoped Map. @param key Scoped key. */
void lab_dom_map_delete(int map, int key);

/** @brief Enumerate a value's own enumerable keys. @param object Scoped object. @return Scoped string array. */
int lab_dom_keys(int object);

/** @brief Set an own data property without prototype mutation. @param object Scoped object.
 * @param key Static property name. @param value Scoped value, copied by reference. */
void lab_dom_set(int object, const char *key, int value);

/** @brief Copy a property without conflating undefined and null in the handle bridge.
 * @param target Scoped destination object; the property is defined as ordinary data.
 * @param target_key Static destination property name.
 * @param source Scoped source value; nullish sources supply undefined.
 * @param source_key Static source property name; its getter is read once.
 */
void lab_dom_property_copy(int target, const char *target_key, int source, const char *source_key);

/** @brief Compare property values using browser strict equality, reading the left getter first.
 * @param left Scoped left value; nullish values supply undefined.
 * @param left_key Static left property name.
 * @param right Scoped right value; nullish values supply undefined.
 * @param right_key Static right property name.
 * @return Non-zero when the property values are strictly equal, including undefined versus undefined.
 */
int lab_dom_properties_equal(int left, const char *left_key, int right, const char *right_key);

/** @brief Set an own data property using an opaque key. @param object Scoped object.
 * @param key Scoped string. @param value Scoped value, copied by reference. */
void lab_dom_key_set(int object, int key, int value);

/** @brief Return a scoped value from labDOM.call after releasing handles. @param value Result handle.
 * The host retains the value, not the handle, including through deferred event effects. */
void lab_dom_return(int value);

/** @brief Release temporary handles without removing their DOM nodes. @param mark Previously obtained boundary. */
void lab_dom_release(unsigned mark);

/** @brief Use browser Unicode services for case/accent-insensitive search. @param text Scoped string handle.
 * @return Scoped trimmed, lower-case NFD text without combining accents. */
int lab_dom_normalize(int text);

/** @brief Search already normalised opaque strings. @param text Haystack. @param query Needle.
 * @return Whether the browser string contains the query, including an empty query. */
int lab_dom_contains(int text, int query);

/** @brief Borrow the native select's chosen option. @param select Scoped select. @return Option handle or zero. */
int lab_dom_selected(int select);

/** @brief Borrow the browser's focused element. @return Scoped node or zero. */
int lab_dom_active(void);

/** @brief Inspect native disabled state, including optgroups. @param node Scoped node. @return Disabled flag. */
int lab_dom_is_disabled(int node);

/** @brief Insert a new wrapper before a node. @param node Existing node. @param shell Detached wrapper. */
void lab_dom_wrap(int node, int shell);

/** @brief Queue an effect after the synchronous call releases handles. @param node Scoped node.
 * @param action Focus 0, scroll into view 1, bubbling change 2, select text 3, blur 4.
 * Failed calls discard their queued effects. */
void lab_dom_effect(int node, unsigned action);

/** @brief Copy a host string handle. @param text NUL-terminated UTF-8 text. @return Scoped string handle. */
int lab_dom_string(const char *text);

/** @brief Measure a host string. @param value Scoped handle. @return UTF-16 length; zero for absent strings. */
unsigned lab_dom_length(int value);

/** @brief Compare opaque strings. @param left First handle. @param right Second handle. @return Exact equality. */
int lab_dom_equal(int left, int right);

/** @brief Compare browser strings lexically. @param left First handle. @param right Second handle.
 * @return Negative, zero or positive according to UTF-16 code-unit ordering. */
int lab_dom_compare(int left, int right);

/** @brief Apply browser Unicode text operations. @param text String handle.
 * @param kind Trim 0, trim and lower-case 1, trim and collapse whitespace 2, trim trailing whitespace 3.
 * @return Scoped string handle. */
int lab_dom_clean(int text, unsigned kind);

/** @brief Test attribute presence, including empty values. @param node Node. @param key Static attribute name.
 * @return Whether the attribute is present. */
int lab_dom_has(int node, const char *key);

/** @brief Remove an attribute. @param node Node, or zero for no action. @param key Static attribute name. */
void lab_dom_remove(int node, const char *key);

/** @brief Convert a DOM numeric value. @param value Scoped string handle. @return Browser number, or zero. */
double lab_dom_number(int value);

/** @brief Format a CSS number. @param value Number. @param prefix Literal prefix. @param suffix Literal unit.
 * @return Scoped browser string. */
int lab_dom_format(double value, const char *prefix, const char *suffix);

/** @brief Join opaque text. @param left First handle. @param right Second handle. @param suffix Literal suffix.
 * @return Scoped string. */
int lab_dom_join(int left, int right, const char *suffix);

/** @brief Find one DOM node. @param parent Root handle, or zero for document. @param selector Static selector.
 * @return Scoped node handle, or zero. */
int lab_dom_query(int parent, const char *selector);

/** @brief Snapshot matching nodes. @param parent Root or zero. @param selector Static selector.
 * @return Scoped NodeList handle. */
int lab_dom_all(int parent, const char *selector);

/** @brief Count a node snapshot. @param list Scoped NodeList. @return Number of nodes. */
unsigned lab_dom_count(int list);

/** @brief Borrow a snapshot member. @param list Scoped NodeList. @param index Zero-based offset.
 * @return Scoped node, or zero. */
int lab_dom_item(int list, unsigned index);

/** @brief Test whether an array index exists, distinguishing holes from explicit null entries.
 * @param array Scoped array.
 * @param index Zero-based index.
 * @return Non-zero for a present property, including an inherited index.
 */
int lab_dom_index_present(int array, unsigned index);

/** @brief Find a containing node. @param node Scoped node. @param selector Static selector.
 * @return Scoped closest ancestor/self, or zero. */
int lab_dom_closest(int node, const char *selector);

/** @brief Create a DOM element. @param tag Static tag. @return Scoped node. */
int lab_dom_create(const char *tag);

/** @brief Append a node. @param parent Parent handle. @param child Child handle. */
void lab_dom_append(int parent, int child);

/** @brief Insert a node as the first child. @param parent Parent node. @param child Child node. */
void lab_dom_prepend(int parent, int child);

/** @brief Read browser text. @param node Node. @param kind Text 0, HTML 1, attribute 2, data 3, inline CSS 4, value 5.
 * @param key Static field name, unused for text/HTML. @return Scoped string; absent values become empty. */
int lab_dom_read(int node, unsigned kind, const char *key);

/** @brief Write browser text. @param node Node. @param kind As for lab_dom_read. @param key Static field name.
 * @param value Scoped string. HTML must be supplied by native presentation only. */
void lab_dom_write(int node, unsigned kind, const char *key, int value);

/** @brief Toggle a CSS class. @param node Node. @param name Static class name. @param enabled Desired state. */
void lab_dom_class(int node, const char *name, int enabled);

/** @brief Query a CSS class. @param node Node. @param name Static class name. @return Whether present. */
int lab_dom_has_class(int node, const char *name);

/** @brief Set disabled state. @param node Control. @param disabled Desired state. */
void lab_dom_disabled(int node, int disabled);

/** @brief Measure browser geometry. @param node Node. @param kind Client width 0, SVG absolute width/height 1/2,
 * rectangle width/height 3/4, SVG view-box width/height 5/6, rectangle left/top/bottom 7/8/9,
 * client height 10, scroll height 11, connected and visible layout flag 12.
 * @return Browser measurement, or zero for an absent node. */
double lab_dom_measure(int node, unsigned kind);

/** @brief Read a computed CSS number. @param node Node. @param key Static property name. @return Number or zero. */
double lab_dom_css(int node, const char *key);

/** @brief Resolve a registered card. @param node Node. @return Stable card index or -1. */
int lab_dom_card_id(int node);

/** @brief Schedule browser animation work. @param action Responsive fit 0, card zoom 1.
 * @param card Scoped card to retain only until its callback runs. */
void lab_dom_schedule(unsigned action, int card);

/**
 * @brief Apply the browser's integer-prefix conversion to a value.
 * @param value Borrowed scoped value handle.
 * @param radix Numeric radix, normally ten.
 * @return Parsed number, or NaN when no integer prefix is present.
 */
double lab_dom_parse_int(int value, unsigned radix);

/**
 * @brief Subscribe a browser target to a named native dispatcher with a stable browser context.
 * @param node Scoped EventTarget handle; zero is ignored.
 * @param type Static DOM event name.
 * @param entry Static exported dispatcher name, accepting action, event and context handles.
 * @param action Dispatcher-specific action index.
 * @param context Scoped browser object retained by the listener as a value, never as a handle.
 * @param passive Non-zero requests a passive listener.
 * @details Repeated registration of the same target, event, entry, action and context is ignored.
 * Dispatch returns an event plan; browser services run only after its synchronous handle scope ends.
 */
void lab_dom_subscribe(int node, const char *type, const char *entry, unsigned action, int context, unsigned passive);

/**
 * @brief Check browser node containment, including equality.
 * @param parent Scoped parent node, or zero.
 * @param child Scoped child node, or zero.
 * @return One when both nodes exist and parent contains child; zero otherwise.
 */
int lab_dom_node_contains(int parent, int child);

/**
 * @brief Resolve an exact document ID supplied as a browser string.
 * @param id Scoped string handle; no selector syntax is interpreted.
 * @return Scoped matching node handle, or zero when absent.
 */
int lab_dom_value_id(int id);

/**
 * @brief Copy an opaque browser string to a bounded UTF-8 buffer.
 * @param value Scoped string handle.
 * @param target Writable WebAssembly byte buffer.
 * @param capacity Maximum number of bytes to copy.
 * @return Byte count, or -1 for overflow or an unpaired Unicode surrogate; no partial copy is published.
 */
int lab_dom_utf8_copy(int value, unsigned char *target, unsigned capacity);

/**
 * @brief Wrap length-delimited UTF-8 bytes in a scoped browser string.
 * @param source Borrowed WebAssembly bytes; embedded NUL bytes are preserved.
 * @param length Byte count.
 * @return Scoped string handle. Invalid UTF-8 raises a host exception.
 */
int lab_dom_utf8_string(const unsigned char *source, unsigned length);

#endif
