/**
 * @file lab_worker_internal.h
 * @brief Calculation entry points linked into the MARS Lab executable.
 *
 * Private to the worker module and its dispatcher. Each entry point runs once
 * in a fresh child process; callers must use lab_process.h for bounded execution.
 * These routines may initialise process-wide mathematical state and are not
 * re-entrant services for the listener or request processes.
 */
#ifndef MARS_LAB_WORKER_INTERNAL_H
#define MARS_LAB_WORKER_INTERNAL_H

#if !defined(MARS_LAB_WORKER_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_LAB_WORKER_INTERNAL_ACCESS."
#endif

#include "number-t.h"
#include "string-t.h"
#include "expression-t.h"

/** @brief Emit an in-memory real curve SVG for a selected variable and mathematical bound texts. */
int lab_worker_plot(const expr_t *expression, const expr_t *variable, const char *lower, const char *upper,
                    const char *scale, const char *y_lower, const char *y_upper);

/**
 * @brief Formats a numerical card value with decimal or Unicode power-of-ten notation.
 * @param value Borrowed value; nonzero imaginary components are retained.
 * @param precision Significant decimal digits, or non-positive to use the current default.
 * @return Caller-owned text, released with string_free, or NULL on allocation failure.
 */
string_t *lab_worker_number_text(number_t value, int precision);

/**
 * @brief Runs the built-in expression calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_expression(int argc, char **argv);

/**
 * @brief Runs the built-in equation calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_equation(int argc, char **argv);

/**
 * @brief Runs the built-in diffequation calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_diffequation(int argc, char **argv);

/**
 * @brief Runs the built-in matrix calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_matrix(int argc, char **argv);

/**
 * @brief Runs the built-in integrator calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_integrator(int argc, char **argv);

/**
 * @brief Runs the built-in datetime calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_datetime(int argc, char **argv);

/**
 * @brief Runs the built-in almanac calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_almanac(int argc, char **argv);

/**
 * @brief Runs the built-in almanac event calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_almanac_event(int argc, char **argv);

/**
 * @brief Runs the built-in holiday calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_holiday(int argc, char **argv);

/**
 * @brief Runs the built-in ophelia calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_ophelia(int argc, char **argv);

#endif
