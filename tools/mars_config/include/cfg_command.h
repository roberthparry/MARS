/**
 * @file cfg_command.h
 * @brief Application command dispatch for the native configuration executable.
 *
 * The executable and isolated command regressions share argument validation and
 * startup policy. Library clients do not use this application interface.
 */
#ifndef MARS_CONFIG_COMMAND_H
#define MARS_CONFIG_COMMAND_H

/**
 * @brief Execute one native configuration command.
 * @param argc Number of command-line arguments, including the executable name.
 * @param[in] argv Borrowed argument vector, valid throughout this call; never retained.
 * @return Zero on success, one on an operational failure, two on invalid arguments.
 * @note Call once during single-threaded startup. May start an owned settings helper,
 * prompt on a terminal and change configuration or files selected by the command.
 * No returned allocation requires release.
 */
int cfg_command_run(int argc, char **argv);

#endif
