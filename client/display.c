#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 256
#define PIPE_PATH "./pipe"

int main() {
    char buffer[BUFFER_SIZE];
    int fd;

    fd = open(PIPE_PATH, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    while (1) {
        ssize_t num_read = read(fd, buffer, BUFFER_SIZE);
        if (num_read > 0) {
            buffer[num_read] = '\0';
            if (buffer[0] == '\0')
                continue;
            printf("%s\n", buffer);
        } else if (num_read == 0) {
            break;
        } else {
            perror("read");
            break;
        }
        fflush(stdout);
    }

    close(fd);
    return 0;
}