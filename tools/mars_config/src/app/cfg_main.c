/**
 * @file cfg_main.c
 * @brief Executable entry point for native configuration commands.
 *
 * Delegates argument validation and startup to the shared application dispatcher.
 */
#include "cfg_command.h"

/* Execute the requested command and preserve its exit status. */
int main(int argc, char **argv)
{
    return cfg_command_run(argc, argv);
}
