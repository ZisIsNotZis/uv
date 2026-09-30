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

char *buf = "HTTP/1.0 200\n\nhello";

int main(int argc, char *argv[]) {
    int buflen = strlen(buf);
    int fd = socket(AF_INET, SOCK_STREAM, 0), tmp = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &tmp, sizeof(tmp))) {
        perror("reuse fail");
        exit(1);
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(env("PORT", 8000)),
        .sin_addr.s_addr = INADDR_ANY
    };
    socklen_t addrlen = sizeof(addr);
    if (bind(fd, (struct sockaddr*)&addr, addrlen) < 0) {
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
    io_uring_prep_accept(sqe, fd, (struct sockaddr*)&addr, &addrlen, 0);
    bool block = false;
    while (true) {
        io_uring_submit_and_wait(&ring, 1);
        unsigned count = 0;
        struct io_uring_cqe *cqe;
        io_uring_for_each_cqe(&ring, tmp, cqe) {
            ++count;
            if (cqe->user_data) {
                close(cqe->user_data);
            } else {
                if (cqe->res < 0) {
                    printf("ring fail: %s\n", strerror(-cqe->res));
                    exit(1);
                }
                if ((sqe = io_uring_get_sqe(&ring)) == NULL) {
                    close(cqe->res);
                    printf("ring block\n");
                    block = true;
                    continue;
                }
                io_uring_prep_accept(sqe, fd, (struct sockaddr*)&addr, &addrlen, 0);
                if ((sqe = io_uring_get_sqe(&ring)) == NULL) {
                    close(cqe->res);
                    continue;
                }
                io_uring_prep_send(sqe, cqe->res, buf, buflen, 0);
                sqe->user_data = cqe->res;
            }
            if (block && (sqe = io_uring_get_sqe(&ring)) != NULL) {
                printf("ring resume\n");
                block = false;
                io_uring_prep_accept(sqe, fd, (struct sockaddr*)&addr, &addrlen, 0);
            }
        }
        io_uring_cq_advance(&ring, count);
    }
}
