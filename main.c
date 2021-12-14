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


    switch((pid = fork())) {
        case -1:/* Error */
            fprintf(stderr, "Unable to fork()\n");
            break;

        case 0: /* Child */
            /* Execute child, passing the options */
            dup2(sk[1], 1) ;
            close(sk[0]);
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
    fcntl(sk[0], F_SETFL, O_NONBLOCK);


    while(1) {
        fd_set fds;
        struct timeval tv = {.tv_sec = 0, .tv_usec = 500000};

        FD_ZERO(&fds);
        FD_SET(sk[0], &fds);
        select(sk[0]+1, &fds, NULL, NULL, &tv);
        if(FD_ISSET(sk[0], &fds)) {
            int n;
            char buf[1024];
            while((n = read(sk[0], buf, sizeof(buf))) > 0) {
                write(1, buf, n);
                tmt_write(vt, buf, n);
            }
        }
    }

    fprintf(stderr, "EOF, exit\n");
}
