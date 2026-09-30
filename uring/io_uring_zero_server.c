#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "liburing.h"
#define env(a, b) getenv(a) == NULL ? b : atoi(getenv(a))

int main(int argc, char *argv[]) {
    int len = env("BUF", 524288), fd = socket(AF_INET, SOCK_STREAM, 0), tmp = 1;
    char *buf = malloc(len);
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &tmp, sizeof(tmp))) {
        perror("reuse fail");
        exit(1);
    }
    struct sockaddr_in srv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(env("PORT", 8000)),
        .sin_addr.s_addr = INADDR_ANY
    };
    socklen_t len_addr = sizeof(srv_addr);
    if (bind(fd, (struct sockaddr*)&srv_addr, len_addr) < 0) {
        perror("bind fail");
        exit(1);
    }
    if (listen(fd, 0) < 0) {
        perror("listen fail");
        exit(1);
    }

    struct io_uring ring;
    if ((tmp = io_uring_queue_init(env("RING", 256), &ring, 0)) < 0) {
        printf("ring fail: %s\n", strerror(-tmp));
        exit(1);
    }
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    struct sockaddr cli_addr;
    io_uring_prep_accept(sqe, fd, &cli_addr, &len_addr, 0);
    bool block = false;
    while (true) {
        io_uring_submit_and_wait(&ring, 1);
        unsigned head, count = 0;
        struct io_uring_cqe *cqe;
        io_uring_for_each_cqe(&ring, head, cqe) {
            ++count;
            if (cqe->user_data) {
                tmp = cqe->user_data;
                if (cqe->res < 0) {
                    close(tmp);
                    continue;
                }
            } else {
                if (cqe->res < 0) {
                    printf("net fail: %s\n", strerror(-cqe->res));
                    exit(1);
                }
                if ((sqe = io_uring_get_sqe(&ring)) == NULL) {
                    printf("net block\n");
                    block = true;
                } else {
                    io_uring_prep_accept(sqe, fd, &cli_addr, &len_addr, 0);
                }
                tmp = cqe->res;
            }
            if ((sqe = io_uring_get_sqe(&ring)) == NULL) {
                close(tmp);
                continue;
            }
            io_uring_prep_send(sqe, tmp, buf, len, 0);
            sqe->user_data = tmp;
            if (block && (sqe = io_uring_get_sqe(&ring)) != NULL) {
                printf("net resume\n");
                io_uring_prep_accept(sqe, fd, &cli_addr, &len_addr, 0);
                block = false;
            }
        }
        io_uring_cq_advance(&ring, count);
    }
}
