#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pty.h>
#include "tmt.h"
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define TAILER_BUFSIZE 4096
struct tailer
{
    FILE *output;
    char inbuf[TAILER_BUFSIZE];
    char outbuf[TAILER_BUFSIZE];
    unsigned int pending_inbuf;
    TMT *vt;
};

void tail(struct tailer *i)
{
    TMT *vt = i->vt;
    const TMTSCREEN *s = tmt_screen(vt);
    const TMTPOINT *c = tmt_cursor(vt);
    size_t last_row = c->r;

    for(size_t row = 0; row <= last_row; row++) {
        fprintf(i->output, "Tailing line %ld: ", row);
        for(size_t col = 0; col <= s->ncol; col++) {
            fprintf(i->output, "%lc", s->lines[row]->chars[col].c);
        }
        fprintf(i->output, "\n");
    }
    fflush(i->output);
    tmt_write(vt, "\033[H\033[J", 0);
}

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
    memset(&instance, 0, sizeof(instance));

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

    struct termios old_tio, new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON|ECHO);
    tcsetattr(0, TCSANOW, &new_tio);

    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);

    switch((pid = forkpty(&termfd, NULL, &old_tio, &w))) {
        case -1:/* Error */
            fprintf(stderr, "Unable to fork()\n");
            break;

        case 0: /* Child */
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


    printf ("lines %d\n", w.ws_row);
    printf ("columns %d\n", w.ws_col);

    instance.vt = tmt_open(w.ws_row, w.ws_col, callback, &instance, NULL);

    instance.output = fopen("log.log", "w");


    fcntl(0, F_SETFL, O_NONBLOCK);
    fcntl(termfd, F_SETFL, O_NONBLOCK);


    while(1) {
        fd_set fds;
        struct timeval tv = {.tv_sec = 0, .tv_usec = 500000};
        int nbytes;

        FD_ZERO(&fds);
        FD_SET(termfd, &fds);
        FD_SET(0, &fds);
        select(termfd+1, &fds, NULL, NULL, &tv);
        while((nbytes = read(termfd, instance.outbuf, sizeof(instance.outbuf))) > 0) {
            write(1, instance.outbuf, nbytes);
            /* chunkuj do newlines */
            int i, last = 0;
            for(i = 0; i<nbytes; i++) {
                if(instance.outbuf[i] == '\n') {
                    /* a\nbc\n
                     * i = 1   last = 0   (2 chars @ 0)  last = 2
                     * i = 4   last = 2   (3 chars @ 2)  last = 5
                     * */
                    tmt_write(instance.vt, instance.outbuf + last, i - last + 1);
                    tail(&instance);
                    last = i + 1;
                }
            }
            /* ostatni fragment */
            if(i > last) {
                tmt_write(instance.vt, instance.outbuf + last, i - last);
            }
        }
        if(nbytes == 0) {
            /* EOF */
            break;
        }
        if(nbytes < 0) {
            if(errno != EAGAIN) {
                perror("read");
            }
        }

        char buf[1024];
        while((nbytes = read(0, buf, sizeof(buf))) > 0) {
            write(termfd, buf, nbytes);
        }
        if(nbytes == 0) {
            /* EOF */
            break;
        }
        if(nbytes < 0) {
            if(errno != EAGAIN) {
                perror("read");
            }
        }
    }

    fprintf(stderr, "EOF, exit\n");
}
