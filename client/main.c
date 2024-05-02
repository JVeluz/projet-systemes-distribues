#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE 256
#define PIPE_PATH "./pipe"

typedef struct {
    char *username;
    char *password;
} user_t;

char *message_server(int sock, char *message) {
    char *response;
    char nb_octets;

    response = malloc(BUFFER_SIZE);
    nb_octets = write(sock, message, strlen(message) + 1);
    if (nb_octets == -1) {
        perror("write");
        sleep(3);
        exit(EXIT_FAILURE);
    }
    if (nb_octets == 0) {
        printf("Server disconnected\n");
        sleep(3);
        exit(EXIT_FAILURE);
    }
    read(sock, response, BUFFER_SIZE);
    return response;
}

bool login_user(int sock, user_t *user) {
    char buffer[BUFFER_SIZE];

    printf("Enter username: ");
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0;
    user->username = malloc(strlen(buffer) + 1);
    strcpy(user->username, buffer);

    printf("Enter password: ");
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0;
    user->password = malloc(strlen(buffer) + 1);
    strcpy(user->password, buffer);

    char message[BUFFER_SIZE];
    sprintf(message, "login:%s:%s", user->username, user->password);

    char *response = message_server(sock, message);

    if (strcmp(response, "failed") == 0) {
        printf("Invalid username or password...\n");
        sleep(3);
        return false;
    } else if (strcmp(response, "success") == 0) {
        return true;
    }
    printf("An error occurred: %s\n", response);
    sleep(3);
    return false;
}

bool register_user(int sock, user_t *user) {
    char buffer[BUFFER_SIZE];

    printf("Enter username: ");
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0;
    user->username = malloc(strlen(buffer) + 1);
    strcpy(user->username, buffer);

    printf("Enter password: ");
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0;
    user->password = malloc(strlen(buffer) + 1);
    strcpy(user->password, buffer);

    char message[BUFFER_SIZE];
    sprintf(message, "register:%s:%s", user->username, user->password);

    char *response = message_server(sock, message);

    if (strcmp(response, "failed") == 0) {
        printf("Username already exists...\n");
        sleep(3);
        return false;
    } else if (strcmp(response, "success") == 0) {
        return true;
    }
    printf("An error occurred\n");
    sleep(3);
    return false;
}

void list_users(int sock) {
    char *response = message_server(sock, "list");
    printf("Online members:\n");
    printf("%s\n", response);
    sleep(3);
}

bool delete_user(int sock, user_t *user) {
    char message[BUFFER_SIZE];
    sprintf(message, "delete:%s:%s", user->username, user->password);
    char *response = message_server(sock, message);
    if (!strcmp(response, "success") == 0) {
        printf("An error occurred\n");
        sleep(3);
        return false;
    }
    return true;
}

void login_menu(int sock, user_t *user) {
    int option;
    bool is_logged_in = false;
    do {
        system("clear");

        printf("1. Login\n");
        printf("2. Register\n");
        printf("3. Exit\n");
        printf("\n");

        scanf("%d", &option);

        // clear buffer
        while ((getchar()) != '\n')
            ;

        switch (option) {
        case 1:
            is_logged_in = login_user(sock, user);
            break;
        case 2:
            is_logged_in = register_user(sock, user);
            break;
        case 3:
            exit(EXIT_SUCCESS);
            break;
        default:
            printf("Invalid option\n");
            break;
        }

        sleep(1);
    } while (!is_logged_in);
}

void main_menu(int *pipe_fd, int sock, user_t *user) {
    bool is_logged_in = true;
    char buffer[BUFFER_SIZE];
    do {
        system("clear");
        printf("/list\tList all online members\n");
        printf("/delete\tDelete your account\n");
        printf("/logout\tLogout\n");
        printf("\n");

        printf("%s: ", user->username);
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = 0;

        char input[BUFFER_SIZE];
        strcpy(input, buffer);

        if (strncmp(input, "/list", 5) == 0) {
            list_users(sock);
        } else if (strncmp(input, "/delete", 7) == 0) {
            delete_user(sock, user);
            is_logged_in = false;
        } else if (strncmp(input, "/logout", 7) == 0) {
            is_logged_in = false;
        } else {
        }

        memset(input, 0, sizeof(input));

        sleep(1);
    } while (is_logged_in);

    printf("Exiting...\n");

    sleep(3);
}

void connect_server(int *sock, char *ip, int port) {
    struct sockaddr_in server;
    struct hostent *host;

    host = gethostbyname(ip);
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = *((in_addr_t *)host->h_addr_list[0]);
    if (connect(*sock, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <communication-ip> <communication-port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *communication_ip = argv[1];
    int communication_port = atoi(argv[2]);
    // char *communication_ip = "127.0.1.1";
    // int communication_port = 5008;

    printf("communication\t%s:%d\n", communication_ip, communication_port);

    user_t *user = malloc(sizeof(user_t));
    int pipe_fd;
    int communication_sock;

    mkfifo(PIPE_PATH, 0666);
    system("gnome-terminal -- ./display.o");
    pipe_fd = open(PIPE_PATH, O_WRONLY);
    if (pipe_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    connect_server(&communication_sock, communication_ip, communication_port);

    login_menu(communication_sock, user);

    main_menu(&pipe_fd, communication_sock, user);

    close(pipe_fd);
    unlink(PIPE_PATH);
    close(communication_sock);

    free(user);

    return 0;
}
