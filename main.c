#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <fcntl.h>
#include <locale.h>
#include <pty.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <time.h>
#include <strings.h>
#include "tmt.h"

#define TAILER_BUFSIZE 4096
struct tailer
{
    FILE *output;
    char inbuf[TAILER_BUFSIZE];
    char outbuf[TAILER_BUFSIZE];
    unsigned int pending_inbuf;
    TMT *vt;
    char *prefix;
    bool quit, timestamp, ignore_resize;
};

struct tailer instance;

static void sig_chld()
{
    int wstat;
    pid_t	pid;

    while (true) {
        pid = wait3 (&wstat, WNOHANG, (struct rusage *)NULL );
        if (pid == 0 || pid == -1) {
            break;
        } else {
            instance.quit = true;
        }
    }
}
static void sig_winch()
{
    if(!instance.ignore_resize) {
        struct winsize wsize;
        if(ioctl(STDIN_FILENO, TIOCGWINSZ, &wsize) == 0) {
            tmt_resize(instance.vt, wsize.ws_row, wsize.ws_col);
        }
    }
}


static bool write_loop(int fd, char *buf, size_t len)
{
    while(len > 0) {
        ssize_t nwr = write(fd, buf, len);
        if(nwr <= 0) {
            return false;
        }
        len -= nwr;
        buf += nwr;
    }
    return true;
}


static void tail(struct tailer *i)
{
    TMT *vt = i->vt;
    const TMTSCREEN *s = tmt_screen(vt);
    const TMTPOINT *c = tmt_cursor(vt);
    size_t last_row = c->r;

    for(size_t row = 0; row <= last_row; row++) {
        size_t last;
        for(last = s->ncol - 1; last > 0; last--) {
            if(s->lines[row]->chars[last].c != 0x20) {
                break;
            }
        }
        if(s->lines[row]->chars[last].c != 0x20) {
            char out[MB_CUR_MAX * last + 1];
            size_t pos = 0;
            mbstate_t mbs;
            memset(&mbs, 0, sizeof(mbs));
            for(size_t col = 0; col <= last; col++) {
                size_t progress = wcrtomb(out + pos, s->lines[row]->chars[col].c, &mbs);
                if(progress != (size_t) -1) {
                    pos += progress;
                }
            }
            if(i->timestamp) {
                struct timeval tv;
                if(gettimeofday(&tv, NULL) == 0) {
                    struct tm * tm = localtime(&tv.tv_sec);
                    fprintf(i->output, "%04d-%02d-%02d %02d:%02d:%02d.%03lu ",
                            tm->tm_year+1900, tm->tm_mon, tm->tm_mday,
                            tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec/1000
                            );
                } else {
                    fprintf(i->output, "[Unknown time] ");
                }
            }
            if(i->prefix) {
                fprintf(i->output, "%s ", i->prefix);
            }
            fwrite(out, pos, 1, i->output);
            fputc('\n', i->output);
        }
    }
    fflush(i->output);
    tmt_write(vt, "\033[H\033[J", 0);
}

static void callback(tmt_msg_t m, TMT *vt, const void *a, void *p)
{
    if(m == TMT_MSG_UPDATE) {
        tmt_clean(vt);
    }
}

int main(int argc, char **argv)
{
    char **child_options;
    char *output_filename = NULL;
    bool append = false;
    int pid, o, termfd = STDIN_FILENO;
    struct termios old_tio, new_tio;
    struct winsize wsize = {.ws_row = 25, .ws_col = 80};

    memset(&instance, 0, sizeof(instance));
    setlocale(LC_ALL, "");
    ioctl(STDIN_FILENO, TIOCGWINSZ, &wsize);

    while((o = getopt(argc, argv, "thaf:p:W:H:i")) != -1) {
        switch(o) {
            case 'f':
                output_filename = strdup(optarg);
                break;
            case 'a':
                append = true;
                break;
            case 't':
                instance.timestamp = true;
                break;
            case 'p':
                instance.prefix = strdup(optarg);
                break;
            case 'W':
                wsize.ws_col = atoi(optarg);
                instance.ignore_resize = true;
                if(wsize.ws_col < 2) {
                    fprintf(stderr, "Invalid column count: %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'H':
                wsize.ws_row = atoi(optarg);
                instance.ignore_resize = true;
                if(wsize.ws_row < 2) {
                    fprintf(stderr, "Invalid row count: %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'h':
                fprintf(stderr,
                        "Usage: %s [options] [-f <output file>] [-- <command> [arguments]]\n"
                        "   -a       Append instead of creating a new file\n"
                        "   -W <col> Override terminal width\n"
                        "   -H <row> Override terminal height\n"
                        "   -i       Ignore resizes\n"
                        "   -p <pfx> Prefix all lines with the specified string\n"
                        "   -t       Add timestamp to every line\n", argv[0]);
                exit(EXIT_SUCCESS);
        }
    }
    child_options = argv + optind;
    if(output_filename) {
        instance.output = fopen(output_filename, append ? "a" : "w");
        if(!instance.output) {
            perror("fopen");
            fprintf(stderr, "Cannot open output file.\n");
            exit(EXIT_FAILURE);
        }
    } else {
        instance.output = stdout;
    }

    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON|ECHO|ISIG);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    if(child_options[0]) {
        switch((pid = forkpty(&termfd, NULL, &old_tio, &wsize))) {
            case -1:/* Error */
                fprintf(stderr, "Unable to forkpty()\n");
                break;

            case 0: /* Child */
                /* Execute child, passing the options */
                execvp(child_options[0], child_options);

                perror("execvp");
                fprintf(stderr, "Cannot start child process\n");
                exit(EXIT_FAILURE);
                break;

            default: /* Parent */
                break;
        }
    }

    signal (SIGCHLD, sig_chld);
    signal (SIGWINCH, sig_winch);

    instance.vt = tmt_open(wsize.ws_row, wsize.ws_col, callback, &instance, NULL);
    fcntl(0, F_SETFL, O_NONBLOCK);
    fcntl(termfd, F_SETFL, O_NONBLOCK);

    while(!instance.quit) {
        fd_set fds;
        struct timeval tv = {.tv_sec = 0, .tv_usec = 500000};
        int nbytes;

        FD_ZERO(&fds);
        FD_SET(termfd, &fds);
        if(termfd != 0) {
            FD_SET(0, &fds);
        }
        select(termfd+1, &fds, NULL, NULL, &tv);
        if(instance.quit) {
            break;
        }
        while((nbytes = read(termfd, instance.outbuf, sizeof(instance.outbuf))) > 0) {
            if(!write_loop(STDOUT_FILENO, instance.outbuf, nbytes)) {
                instance.quit = true;
                break;
            }
            /* chunkuj do newlines */
            int i, last = 0;
            for(i = 0; i<nbytes; i++) {
                if(instance.outbuf[i] == '\n') {
                    tmt_write(instance.vt, instance.outbuf + last, i - last + 1);
                    tail(&instance);
                    last = i + 1;
                }
            }
            fflush(instance.output);
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
                break;
            }
        }

        if(termfd != STDIN_FILENO) {
            while((nbytes = read(STDIN_FILENO, instance.inbuf, sizeof(instance.inbuf))) > 0) {
                if(!write_loop(termfd, instance.inbuf, nbytes)) {
                    instance.quit = true;
                    break;
                }
            }
            if(nbytes == 0) {
                break;
            }
            if(nbytes < 0) {
                if(errno != EAGAIN) {
                    close(termfd);
                    break;
                }
            }
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
}
