/**
 * @file cfg_prompt.c
 * @brief Bounded terminal input and hidden secret confirmation support.
 *
 * Restores terminal echo before returning after ordinary input errors or handled
 * termination signals. Terminal interaction is separate from file.h disk I/O.
 * Call only from the single-threaded installer before any worker is started.
 */
#include <signal.h>
#include <sodium.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "cfg_prompt.h"

static volatile sig_atomic_t cfg_prompt_interrupted;

static void cfg_prompt_interrupt(int signal_number)
{
    cfg_prompt_interrupted = signal_number;
}

/* Read one trimmed terminal line without ever printing a secret. */
string_t *cfg_prompt_read(const char *prompt, bool secret)
{
    struct termios original, hidden;
    struct sigaction old_interrupt, old_terminate, handler = {.sa_handler = cfg_prompt_interrupt};
    sigemptyset(&handler.sa_mask);
    cfg_prompt_interrupted = 0;
    if (secret && tcgetattr(STDIN_FILENO, &original))
        return NULL;
    if (secret) {
        if (sigaction(SIGINT, &handler, &old_interrupt))
            return NULL;
        if (sigaction(SIGTERM, &handler, &old_terminate)) {
            sigaction(SIGINT, &old_interrupt, NULL);
            return NULL;
        }
        hidden = original;
        hidden.c_lflag &= ~ECHO;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &hidden)) {
            sigaction(SIGINT, &old_interrupt, NULL);
            sigaction(SIGTERM, &old_terminate, NULL);
            return NULL;
        }
    }
    fputs(prompt, stdout);
    fflush(stdout);
    char bytes[4096];
    size_t used = 0;
    int ch = EOF;
    bool ok = true;
    while (!cfg_prompt_interrupted && (ch = getchar()) != '\n' && ch != EOF) {
        if (ok) {
            ok = ch != 0 && used < sizeof(bytes);
            if (ok)
                bytes[used++] = (char)ch;
        }
    }
    if (secret) {
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original))
            ok = false;
        sigaction(SIGINT, &old_interrupt, NULL);
        sigaction(SIGTERM, &old_terminate, NULL);
        putchar('\n');
    }
    if (!ok || ch == EOF || cfg_prompt_interrupted) {
        sodium_memzero(bytes, sizeof(bytes));
        return NULL;
    }
    string_t *validated = string_new();
    if (validated && string_append_utf8_exact(validated, bytes, used)) {
        string_free(validated);
        validated = NULL;
    }
    sodium_memzero(bytes, sizeof(bytes));
    string_trim(validated);
    return validated;
}
