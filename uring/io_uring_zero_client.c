#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "liburing.h"
#define env(a, b) getenv(a) == NULL ? b : atoi(getenv(a))

int main(int argc, char *argv[]) {
    int tmp, fd, len = env("BUF", 524288);
    char *buf = malloc(len);
    struct io_uring ring;
    if ((tmp = io_uring_queue_init(env("RING", 256), &ring, 0)) < 0) {
        printf("ring fail: %s\n", strerror(-tmp));
        exit(1);
    }
    struct io_uring_sqe *sqe;
    for(--argc; argc; argc-=3) {
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(atoi(argv[argc-1]))
        };
        if (inet_pton(AF_INET, argv[argc-2], &addr.sin_addr) <= 0) {
            printf("addr fail: %s\n", argv[argc-2]);
            exit(1);
        }
        for(tmp=0; tmp<atoi(argv[argc]); tmp++) {
            fd = socket(AF_INET, SOCK_STREAM, 0);
            if ((sqe = io_uring_get_sqe(&ring)) == NULL) {
                printf("net block\n");
                exit(1);
            }
            io_uring_prep_connect(sqe, fd, (struct sockaddr*)&addr, sizeof(addr));
            sqe->user_data = fd | 65536;
        }
    }
    unsigned long T = time(NULL), N = 0;
    while (true) {
        io_uring_submit_and_wait(&ring, 1);
        unsigned count = 0;
        struct io_uring_cqe *cqe;
        io_uring_for_each_cqe(&ring, tmp, cqe) {
            ++count;
            if (cqe->user_data & 65536) {
                fd = cqe->user_data & 65535;
            } else {
                fd = cqe->user_data;
                N += cqe->res;
            }
            if (cqe->res < 0) {
                printf("net fail: %s\n", strerror(-cqe->res));
                close(fd);
                continue;
            }
            sqe = io_uring_get_sqe(&ring);
            io_uring_prep_recv(sqe, fd, buf, len, 0);
            sqe->user_data = fd;
        }
        io_uring_cq_advance(&ring, count);
        if (time(NULL) != T) {
            printf("%ld\n", (N >> 20) / (time(NULL) - T));
            T = time(NULL);
            N = 0;
        }
    }
}
