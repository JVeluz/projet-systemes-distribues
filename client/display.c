#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 256
#define PIPE_PATH "./pipe"

volatile bool running = true;

void sigpipe_handler() { running = false; }

int main() {
    signal(SIGINT, sigpipe_handler);

    int fd = open(PIPE_PATH, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    while (running) {
        read(fd, buffer, sizeof(buffer));
        printf("%s\n", buffer);

        if (strncmp(buffer, "/exit", 5) == 0)
            break;
    }

    close(fd);
    unlink(PIPE_PATH);

    return 0;
}