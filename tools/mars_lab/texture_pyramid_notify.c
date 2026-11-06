/**
 * @file texture_pyramid_notify.c
 * @brief Optional, shell-free desktop progress reports for offline photographic preparation.
 *
 * Waits for the short notification child and tolerates missing desktop services. Kept separate from image
 * preparation because the POSIX signal-stack type and the library array-stack type have different meanings.
 */
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>

#define MARS_TEXTURE_PYRAMID_INTERNAL_ACCESS
#include "texture_pyramid.h"

extern char **environ;

/* Keep optional desktop reporting outside the preparation success/failure path. */
void texture_pyramid_notify(bool enabled, const char *message)
{
    if (!enabled || !message) return;
    char *const args[] = {"notify-send", "--app-name=MARS", "MARS photographic blocks", (char *)message, NULL};
    pid_t child;
    if (!posix_spawnp(&child, args[0], NULL, NULL, args, environ))
        while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {}
}
