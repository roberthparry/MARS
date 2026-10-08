/**
 * @file checks_process_fixture.c
 * @brief Native fork fixture retaining capture descriptors after its leader exits.
 *
 * Used only by the sequential process-group cleanup regression. Argument count
 * selects successful exit, failed exit, timeout, invalid UTF-8 capture or parent
 * interruption (five arguments for SIGINT, six for SIGTERM). A pipe
 * handshake ensures the descendant has written to stderr before the leader prints
 * its PID. The descendant retains both capture streams until explicitly killed.
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

/* Create one persistent descendant and expose its PID to the owning test. */
int main(int argc, char **argv)
{
    (void)argv;
    int ready[2];
    if (pipe(ready))
        return 90;
    pid_t descendant = fork();
    if (descendant < 0) {
        close(ready[0]);
        close(ready[1]);
        return 91;
    }
    if (!descendant) {
        close(ready[0]);
        if (fputs("descendant retains capture streams\n", stderr) < 0 || fflush(stderr))
            _exit(92);
        ssize_t written;
        do {
            written = write(ready[1], "R", 1);
        } while (written < 0 && errno == EINTR);
        close(ready[1]);
        if (written != 1)
            _exit(93);
        for (;;)
            pause();
    }
    close(ready[1]);
    char marker;
    ssize_t received;
    do {
        received = read(ready[0], &marker, 1);
    } while (received < 0 && errno == EINTR);
    close(ready[0]);
    if (received != 1 || printf("%ld\n", (long)descendant) < 0 || fflush(stdout)) {
        kill(descendant, SIGKILL);
        while (waitpid(descendant, NULL, 0) < 0 && errno == EINTR) {
        }
        return 94;
    }
    if (argc == 5 || argc == 6)
        kill(getppid(), argc == 5 ? SIGINT : SIGTERM);
    if (argc == 3 || argc == 5 || argc == 6)
        for (;;)
            pause();
    if (argc == 4 && (fputc(0xff, stderr) == EOF || fflush(stderr)))
        return 95;
    return argc == 2 ? 7 : 0;
}
