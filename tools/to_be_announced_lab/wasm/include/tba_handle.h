/**
 * @file tba_handle.h
 * @brief Borrowed JavaScript-value references for the private Lab bridge.
 *
 * The _jsref suffix denotes an opaque integer reference to a JavaScript value.
 * These integer aliases identify values in host-owned tables during one
 * synchronous call. They are not linear-memory pointers and cannot be retained
 * between calls. The DOM and codec tables are independent; C aliases describe
 * these roles but do not enforce distinct integer types. This header is owned
 * by the private Lab and has no dependency on another application's bridge.
 */
#ifndef TBA_HANDLE_H
#define TBA_HANDLE_H

/** @brief Scoped DOM-bridge value; zero denotes absence where documented. */
typedef int browser_jsref;

/** @brief Scoped browser node or event target. */
typedef browser_jsref node_jsref;

/** @brief Scoped browser event. */
typedef browser_jsref event_jsref;

/** @brief Scoped codec-table value; codec APIs document their -1 error sentinel. */
typedef int codec_jsref;

#endif
