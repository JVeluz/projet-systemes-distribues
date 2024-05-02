#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE 256
#define PIPE_PATH "./pipe"
#define MAX_USERNAME_LENGTH 32

typedef struct {
    char username[MAX_USERNAME_LENGTH];
} user_t;

user_t *user;

int sock;
int pipe_fd;

void write_to_pipe(char *message) {
    if (write(pipe_fd, message, strlen(message)) == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }
}

void write_to_server(char *message) {
    if (write(sock, message, strlen(message)) == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }
}
void read_from_server(char *response) {
    if (read(sock, response, BUFFER_SIZE) == -1) {
        perror("read");
        exit(EXIT_FAILURE);
    }
}

void decode_response(char *response, char *parts[3]) {
    char *part = strtok(response, ":");
    int i = 0;
    while (part != NULL) {
        parts[i++] = part;
        part = strtok(NULL, ":");
    }
}

void *handle_server_listen() {
    char server_response[BUFFER_SIZE];
    char pipe_message[BUFFER_SIZE];
    char *parts[3];
    while (true) {
        read_from_server(server_response);
        decode_response(server_response, parts);
        if (strcmp(parts[0], "broadcast") == 0) {
            sprintf(pipe_message, "%s: %s", parts[1], parts[2]);
            write_to_pipe(pipe_message);
        }
    }
}

bool login_request(char *username, char *password) {
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    sprintf(request, "login:%s:%s", username, password);
    write_to_server(request);
    read_from_server(response);

    if (strcmp(response, "success") == 0) {
        strcpy(user->username, username);
        return true;
    } else if (strcmp(response, "failed") == 0) {
        printf("Wrong username or password\n");
        return false;
    } else {
        printf("An error occurred\n");
        return false;
    }
}

bool register_request(char *username, char *password) {
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    sprintf(request, "register:%s:%s", username, password);
    write_to_server(request);
    read_from_server(response);

    if (strcmp(response, "success") == 0) {
        printf("Registered successfully\n");
        return true;
    } else if (strcmp(response, "failed") == 0) {
        printf("Username already exists\n");
        return false;
    } else {
        printf("An error occurred\n");
        return false;
    }
}

bool delete_request(char *username, char *password) {
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    sprintf(request, "delete:%s:%s", username, password);
    write_to_server(request);
    read_from_server(response);

    if (strcmp(response, "success") == 0) {
        return true;
    } else if (strcmp(response, "failed") == 0) {
        printf("Wrong username or password\n");
        return false;
    } else {
        printf("An error occurred\n");
        return false;
    }
}

void list_request() {
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    sprintf(request, "list");
    write_to_server(request);
    read_from_server(response);

    char *parts[3];
    decode_response(response, parts);
    if (strcmp(parts[0], "list") == 0) {
        printf("Online members:\n");
        char *member = strtok(parts[1], ",");
        while (member != NULL) {
            printf("%s\n", member);
            member = strtok(NULL, ",");
        }
    } else {
        printf("An error occurred\n");
    }
}

void logout_request() {
    char request[BUFFER_SIZE];
    sprintf(request, "logout:%s", user->username);
    write_to_server(request);
}

void broadcast_request(char *message) {
    char request[BUFFER_SIZE];
    sprintf(request, "broadcast:%s:%s", user->username, message);
    write_to_server(request);
}

void ask_for(char *question, char *answer) {
    printf("%s: ", question);
    fgets(answer, MAX_USERNAME_LENGTH, stdin);
    answer[strcspn(answer, "\n")] = 0;
}

void login_menu() {
    char username[MAX_USERNAME_LENGTH];
    char password[MAX_USERNAME_LENGTH];

    int option;
    bool is_logged_in = false;
    do {
        system("clear");
        printf("1. Login\n");
        printf("2. Register\n");
        printf("3. Exit\n");
        printf("\n");

        scanf("%d", &option);

        // clear input
        while ((getchar()) != '\n')
            ;

        switch (option) {
        case 1:
            ask_for("Username", username);
            ask_for("Password", password);
            is_logged_in = login_request(username, password);
            break;
        case 2:
            ask_for("Username", username);
            ask_for("Password", password);
            register_request(username, password);
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

void main_menu() {
    bool is_logged_in = true;
    char input[BUFFER_SIZE];
    do {
        system("clear");
        printf("/list\tList all online members\n");
        printf("/delete\tDelete your account\n");
        printf("/logout\tLogout\n");
        printf("\n");

        printf("%s: ", user->username);
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = 0;

        if (strncmp(input, "/list", 5) == 0) {
            list_request();
        } else if (strncmp(input, "/delete", 7) == 0) {
            char password[MAX_USERNAME_LENGTH];
            ask_for("Password", password);
            if (delete_request(user->username, password)) {
                is_logged_in = false;
            }
        } else if (strncmp(input, "/logout", 7) == 0) {
            logout_request();
            is_logged_in = false;
        } else {
            broadcast_request(input);
        }
    } while (is_logged_in);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <communication-ip> <communication-port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *communication_ip = argv[1];
    int communication_port = atoi(argv[2]);

    printf("communication\t%s:%d\n", communication_ip, communication_port);

    struct sockaddr_in server_addr;
    struct hostent *server;
    pthread_t thread_server_listen;

    // Initialize user
    user = malloc(sizeof(user_t));

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Initialize server address
    server = gethostbyname(communication_ip);
    if (server == NULL) {
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(communication_port);
    server_addr.sin_addr = *((struct in_addr *)server->h_addr_list[0]);
    memset(server_addr.sin_zero, 0, sizeof(server_addr.sin_zero));

    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
        -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    // Create pipe
    mkfifo(PIPE_PATH, 0666);
    system("gnome-terminal -- ./display.o");
    pipe_fd = open(PIPE_PATH, O_WRONLY);
    if (pipe_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    login_menu();

    // Create thread to listen to server
    if (pthread_create(&thread_server_listen, NULL, handle_server_listen,
                       NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    main_menu();

    // Close pipe
    close(pipe_fd);
    unlink(PIPE_PATH);

    // Close socket
    close(sock);

    // Free memory
    free(user);

    return 0;
}
