/**
 * @file tba_dom.h
 * @brief Scoped browser capabilities for the forecasting WebAssembly application.
 *
 * Handles are valid only for one synchronous native entry point. Long-lived
 * values belong to the browser context, never to retained numeric handles.
 * Business decisions remain in C; imports expose only browser/value primitives.
 */
#ifndef TBA_DOM_H
#define TBA_DOM_H

/**
 * @brief Borrow the browser-owned application context.
 * @return Scoped context handle.
 */
int tba_dom_root(void);

/**
 * @brief Find a DOM element by exact ID.
 * @param id Borrowed static UTF-8 ID.
 * @return Scoped node or zero.
 */
int tba_dom_id(const char *id);

/**
 * @brief Find a descendant or document node.
 * @param node Root or zero for document.
 * @param selector Static CSS selector.
 * @return Scoped node or zero.
 */
int tba_dom_query(int node, const char *selector);

/**
 * @brief Snapshot matching descendants.
 * @param node Root or zero for document.
 * @param selector Static selector.
 * @return Scoped list.
 */
int tba_dom_all(int node, const char *selector);

/**
 * @brief Read a property.
 * @param object Scoped value.
 * @param key Static property name.
 * @return Scoped value or zero.
 */
int tba_dom_get(int object, const char *key);

/**
 * @brief Read a property using opaque text.
 * @param object Scoped value.
 * @param key Scoped string.
 * @return Scoped value.
 */
int tba_dom_key_get(int object, int key);

/**
 * @brief Store an own data property.
 * @param object Scoped object.
 * @param key Static name.
 * @param value Scoped value.
 */
void tba_dom_set(int object, const char *key, int value);

/**
 * @brief Store an own data property with an opaque key.
 * @param object Scoped object.
 * @param key Scoped name.
 * @param value Scoped value.
 */
void tba_dom_key_set(int object, int key, int value);

/**
 * @brief Read an indexed value.
 * @param list Scoped array or collection.
 * @param index Zero-based position.
 * @return Scoped value.
 */
int tba_dom_item(int list, unsigned index);

/**
 * @brief Count array or collection members.
 * @param list Scoped array or collection.
 * @return Member count.
 */
unsigned tba_dom_count(int list);

/**
 * @brief Create a temporary container.
 * @param array Non-zero for an array; zero for a null-prototype object.
 * @return Scoped container.
 */
int tba_dom_object(unsigned array);

/**
 * @brief Append to a temporary array.
 * @param array Scoped array.
 * @param value Scoped value.
 */
void tba_dom_push(int array, int value);

/**
 * @brief List enumerable property names.
 * @param object Scoped object.
 * @return Scoped key array.
 */
int tba_dom_keys(int object);

/**
 * @brief Copy a static UTF-8 literal.
 * @param text NUL-terminated text.
 * @return Scoped string.
 */
int tba_dom_string(const char *text);

/**
 * @brief Convert a value to text without changing its meaning.
 * @param value Scoped value; absent becomes empty.
 * @return Scoped string.
 */
int tba_dom_text(int value);

/** @brief Count browser string code units. @param text Scoped text. @return Length. */
unsigned tba_dom_length(int text);

/** @brief Read a browser string code unit. @param text Scoped text. @param index Position. @return Code unit or zero. */
unsigned tba_dom_char(int text, unsigned index);

/** @brief Slice opaque text. @param text Scoped text. @param begin Start. @param end Exclusive end. @return Scoped text. */
int tba_dom_slice(int text, unsigned begin, unsigned end);

/**
 * @brief Trim Unicode text and optionally lowercase.
 * @param value Scoped text.
 * @param lower Non-zero to lowercase.
 * @return Scoped string.
 */
int tba_dom_clean(int value, unsigned lower);

/**
 * @brief Split opaque text at a literal separator.
 * @param text Scoped string.
 * @param separator Static separator.
 * @return Scoped array.
 */
int tba_dom_split(int text, const char *separator);

/**
 * @brief Join opaque text fragments.
 * @param array Scoped array.
 * @param separator Static separator.
 * @return Scoped string.
 */
int tba_dom_join(int array, const char *separator);

/**
 * @brief Compare opaque values strictly.
 * @param left First scoped value.
 * @param right Second scoped value.
 * @return Equality flag.
 */
int tba_dom_equal(int left, int right);

/**
 * @brief Read browser truthiness.
 * @param value Scoped value.
 * @return Boolean coercion.
 */
int tba_dom_truth(int value);

/**
 * @brief Classify a scoped value.
 * @param value Scoped value.
 * @return Null 0, boolean 1, number 2, string 3, array 4, object 5.
 */
unsigned tba_dom_type(int value);

/**
 * @brief Convert an opaque numeric control value.
 * @param value Scoped value.
 * @return Number, including NaN.
 */
double tba_dom_number(int value);

/**
 * @brief Wrap a numeric value.
 * @param value Number.
 * @return Scoped number.
 */
int tba_dom_numeric(double value);

/**
 * @brief Format numeric presentation using browser formatting.
 * @param value Number.
 * @param digits Fixed decimals, -1 for plain, or -(decimals + 2) for localised text.
 * @return Scoped string.
 */
int tba_dom_format(double value, int digits);

/**
 * @brief Escape text for insertion into HTML or an attribute.
 * @param text Scoped text.
 * @return Scoped escaped string.
 */
int tba_dom_escape(int text);

/**
 * @brief Read browser text.
 * @param node Scoped node.
 * @param kind Text 0, HTML 1, attribute 2, dataset 3, CSS 4, value 5.
 * @param key Static field name.
 * @return Scoped string.
 */
int tba_dom_read(int node, unsigned kind, const char *key);

/**
 * @brief Write browser text.
 * @param node Scoped node.
 * @param kind As for tba_dom_read.
 * @param key Static field.
 * @param value Scoped text.
 */
void tba_dom_write(int node, unsigned kind, const char *key, int value);

/**
 * @brief Set a boolean DOM property.
 * @param node Scoped node.
 * @param key Static property.
 * @param flag Desired value.
 */
void tba_dom_flag(int node, const char *key, int flag);

/**
 * @brief Toggle a CSS class.
 * @param node Scoped node.
 * @param name Static class.
 * @param enabled Desired state.
 */
void tba_dom_class(int node, const char *name, int enabled);

/**
 * @brief Query a CSS class.
 * @param node Scoped node.
 * @param name Static class.
 * @return Membership flag.
 */
int tba_dom_has_class(int node, const char *name);

/**
 * @brief Find an ancestor matching a selector.
 * @param node Scoped node.
 * @param selector Static selector.
 * @return Scoped node.
 */
int tba_dom_closest(int node, const char *selector);

/**
 * @brief Prevent the default browser event action.
 * @param event Scoped event.
 */
void tba_dom_prevent(int event);

/**
 * @brief Register native event dispatch.
 * @param node Scoped target.
 * @param type Static event name.
 * @param action Native action index.
 */
void tba_dom_listen(int node, const char *type, unsigned action);

/**
 * @brief Replace an action's timer after returning from C.
 * @param action Native action.
 * @param delay Milliseconds, or negative to cancel.
 */
void tba_dom_timer(unsigned action, int delay);

/**
 * @brief Queue a browser request and deliver its completion to C.
 * @param url Scoped URL.
 * @param kind GET 0, Protobuf POST 1, local file read 2, encrypted stream 3.
 * @param body Scoped payload or File.
 * @param headers Scoped headers.
 * @param action Native completion index.
 * @param serial Request generation.
 */
void tba_dom_request(int url, unsigned kind, int body, int headers, unsigned action, unsigned serial);

/**
 * @brief Download native-selected content using a Blob.
 * @param filename Scoped filename.
 * @param text Scoped contents.
 * @param mime Static MIME type.
 */
void tba_dom_download(int filename, int text, const char *mime);

/**
 * @brief Copy text and dispatch its completion.
 * @param text Scoped text.
 * @param action Native completion index.
 */
void tba_dom_clipboard(int text, unsigned action);

/**
 * @brief Read element geometry.
 * @param node Scoped element.
 * @param kind Left 0, top 1, width 2, height 3.
 * @return CSS pixel measurement.
 */
double tba_dom_measure(int node, unsigned kind);

/**
 * @brief Transform a pointer position into SVG coordinates.
 * @param svg Scoped SVG.
 * @param event Scoped mouse event.
 * @param axis X 0 or Y 1.
 * @return Coordinate or NaN.
 */
double tba_dom_cursor(int svg, int event, unsigned axis);

/**
 * @brief Use browser date formatting without forecast arithmetic.
 * @param value Scoped ISO text for mode 0, YYYYMMDD for 1/2, timestamp for 3.
 * @param mode Parse to YYYYMMDD 0, ISO 1, UK 2, timestamp to UK 3.
 * @return Scoped date number or text; invalid input gives zero or empty.
 */
int tba_dom_date(int value, unsigned mode);

/**
 * @brief Mark a temporary handle scope.
 * @return Scope boundary.
 */
unsigned tba_dom_mark(void);

/**
 * @brief Release temporary handles, retaining browser-owned values.
 * @param mark Previous scope boundary.
 */
void tba_dom_release(unsigned mark);
#endif
