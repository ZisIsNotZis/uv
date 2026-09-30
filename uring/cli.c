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

int main(int argc, char *argv[]) {
    struct io_uring ring;
    int tmp = io_uring_queue_init(512, &ring, 0), n = 0, t = time(NULL);
    if (tmp < 0) {
        printf("ring fail: %s\n", strerror(-tmp));
        exit(1);
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(8000)
    };
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) < 0) {
        perror("addr fail");
        exit(1);
    }
    for (tmp=0; tmp<512; ++tmp) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (sqe == NULL) {
            printf("ring full");
            exit(1);
        }
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            perror("sock fail");
            exit(1);
        }
        io_uring_prep_connect(sqe, fd, (struct sockaddr*)&addr, sizeof(addr));
    }
    while (1) {
        io_uring_submit_and_wait(&ring, 1);
        struct io_uring_cqe *cqe;
        unsigned count = 0;
        io_uring_for_each_cqe(&ring, tmp, cqe) {
            ++count;
            if (cqe->res < 0) {
                printf("connect fail: %s\n", strerror(-cqe->res));
                continue;
            }
            struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
            if (sqe == NULL) {
                printf("ring full");
                exit(1);
            }
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0) {
                perror("sock fail");
                exit(1);
            }
            io_uring_prep_connect(sqe, fd, (struct sockaddr*)&addr, sizeof(addr));
        }
        io_uring_cq_advance(&ring, count);
        n += count;
        if (time(NULL) != t) {
            printf("%d\r", ++n);
            fflush(stdout);
            t = time(NULL);
        }
    }
}
