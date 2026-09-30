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

int main(int argc, char *argv[]) {
    int fd = socket(AF_INET, SOCK_STREAM, 0), tmp = 1, t = time(NULL);
    if (fd < 0) {
        perror("sock fail");
        exit(1);
    }
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &tmp, sizeof(tmp))) {
        perror("reuse fail");
        exit(1);
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(8000),
        .sin_addr.s_addr = INADDR_ANY
    };
    socklen_t len = sizeof(addr);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind fail");
        exit(1);
    }
    if (listen(fd, 0) < 0) {
        perror("listen fail");
        exit(1);
    }
    while (1) {
        if (accept(fd, (struct sockaddr*)&addr, &len) < 0) {
            perror("accept fail");
            exit(1);
        }
        ++tmp;
        if (time(NULL) != t) {
            printf("%d\r", tmp);
            fflush(stdout);
            t = time(NULL);
        }
    }
}
