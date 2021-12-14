#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pty.h>
#include "tmt.h"
#include <sys/ioctl.h>

struct tailer
{
    FILE *output;
};

static void callback(tmt_msg_t m, TMT *vt, const void *a, void *p)
{
    /* grab a pointer to the virtual screen */
    const TMTSCREEN *s = tmt_screen(vt);
    const TMTPOINT *c = tmt_cursor(vt);
    switch (m) {
        case TMT_MSG_UPDATE:

            tmt_clean(vt);
            break;
    }
}

int main(int argc, char **argv)
{
    char **child_options;
    int pid, o, termfd;
    char s[100];
    int sk[2];
    int sk2[2];
    struct tailer instance;

    while((o = getopt(argc, argv, "hf:")) != -1) {
        switch(o) {
            case 'f':
                fprintf(stderr, "Option f: %s\n", optarg);
                break;
        }
    }
    child_options = argv + optind;

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sk) < 0)
    {
        perror("socketpair");
        exit(EXIT_FAILURE);
    }
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sk2) < 0)
    {
        perror("socketpair");
        exit(EXIT_FAILURE);
    }


    switch((pid = fork())) {
        case -1:/* Error */
            fprintf(stderr, "Unable to fork()\n");
            break;

        case 0: /* Child */
            dup2(sk[0], 0);
            dup2(sk2[0], 1);
            /* Execute child, passing the options */
            execvp(child_options[0], child_options);

            perror("execvp");
            fprintf(stderr, "Cannot start child process\n");
            exit(EXIT_FAILURE);
            break;

        default: /* Parent */
            fprintf(stderr, "Successfully started the child process\n");
            break;
    }

    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);

    printf ("lines %d\n", w.ws_row);
    printf ("columns %d\n", w.ws_col);

    TMT *vt = tmt_open(w.ws_row, w.ws_col, callback, &instance, NULL);

    instance.output = fopen("log.log", "w");

    struct termios old_tio, new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON|ECHO);
    tcsetattr(0, TCSANOW, &new_tio);

    fcntl(0, F_SETFL, O_NONBLOCK);
    fcntl(sk2[1], F_SETFL, O_NONBLOCK);


    while(1) {
        fd_set fds;
        struct timeval tv = {.tv_sec = 0, .tv_usec = 500000};

        FD_ZERO(&fds);
        FD_SET(sk2[1], &fds);
        FD_SET(0, &fds);
        select(sk2[1]+1, &fds, NULL, NULL, &tv);
        if(FD_ISSET(sk2[1], &fds)) {
            int n;
            char buf[1024];
            while((n = read(sk2[1], buf, sizeof(buf))) > 0) {
                write(1, buf, n);
                tmt_write(vt, buf, n);
            }
        }
        if(FD_ISSET(0, &fds)) {
            int n;
            char buf[1024];
            while((n = read(0, buf, sizeof(buf))) > 0) {
                write(sk[1], buf, n);
                write(1, buf, n);
            }
        }
    }

    fprintf(stderr, "EOF, exit\n");
}
