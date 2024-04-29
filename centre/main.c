#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 256

typedef struct {
    int id;
    int sock;
    int *msgid;
    char *message_queue;
    sem_t *mutex_message_queue;
    void (*send_message)(char *xhats);
} handle_client_args;

void *handle_client(void *arg) {
    handle_client_args *args = (handle_client_args *)arg;
    int id = args->id;
    int sock = args->sock;
    int *msgid = args->msgid;
    char *message_queue = args->message_queue;
    sem_t *mutex_message_queue = args->mutex_message_queue;

    char buffer[BUFFER_SIZE];
    char *message;
    char *response;
    char n_bytes;

    response = malloc(BUFFER_SIZE * sizeof(char));

    printf("communication\tclient-%d connected\n", id);

    while (true) {
        // Receive the message from the client
        n_bytes = read(sock, buffer, BUFFER_SIZE);

        // Check if the client is disconnected
        if (n_bytes == 0) {
            printf("communication\tclient-%d disconnected\n", id);
            break;
        }

        message = malloc(n_bytes * sizeof(char));
        memcpy(message, buffer, n_bytes);
        printf("communication\t <-- %s client-%d\n", message, id);

        // Send the message to the message queue
        sem_wait(mutex_message_queue);
        strcpy(message_queue, message);
        if (msgsnd(*msgid, message_queue, sizeof(message_queue), 0) == -1) {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }

        // Receive the response from the message queue
        if (msgrcv(*msgid, message_queue, sizeof(message_queue), 0, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }
        strcpy(response, message_queue);
        sem_post(mutex_message_queue);

        // Send the response to the client
        write(sock, response, strlen(response) + 1);
        printf("communication\t%s --> client-%d\n", response, id);

        free(message);
    }
    free(response);
    pthread_exit(NULL);
}

void communicaiton(int *sock_server, int *sock_clients, int *n_socks, int port,
                   int *msgid, char *message_queue,
                   sem_t *mutex_message_queue) {
    struct sockaddr_in addr_client;
    struct sockaddr_in addr_server;
    int addr_size;

    // Initialize the server address
    memset((char *)&addr_server, 0, sizeof(addr_server));
    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = INADDR_ANY;
    addr_server.sin_port = htons(port);

    // Bind the socket to the server address
    if (bind(*sock_server, (struct sockaddr *)&addr_server,
             sizeof(addr_server)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(*sock_server, 5) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    *n_socks = 0;
    while (true) {
        // Accept the incoming connection
        addr_size = sizeof(addr_client);
        sock_clients[*n_socks] =
            accept(*sock_server, (struct sockaddr *)&addr_client, &addr_size);
        if (sock_clients[*n_socks] == -1) {
            perror("accept");
            continue;
        }

        // Create a new thread to handle the client
        pthread_t client_thread;
        handle_client_args *args = malloc(sizeof(handle_client_args));
        args->id = *n_socks;
        args->sock = sock_clients[*n_socks];
        args->msgid = msgid;
        args->message_queue = message_queue;
        args->mutex_message_queue = mutex_message_queue;
        if (pthread_create(&client_thread, NULL, handle_client, (void *)args) !=
            0) {
            free(args);
            perror("pthread_create");
            continue;
        }

        (*n_socks)++;

        pthread_detach(client_thread);
    }
}

void gestion_requete(int *sock, char *ip, int port, int *msgid,
                     char *message_queue) {
    struct hostent *server_host;
    static struct sockaddr_in addr_server;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE];
    char *message;
    char *response;
    char n_bytes;

    // Initialize the client address
    server_host = gethostbyname(ip);
    if (server_host == NULL) {
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }
    bzero(&addr_server, sizeof(struct sockaddr_in));
    addr_server.sin_family = AF_INET;
    addr_server.sin_port = htons(port);
    memcpy(&addr_server.sin_addr.s_addr, server_host->h_addr_list[0],
           server_host->h_length);

    while (true) {
        // Recieve the message from the message queue
        if (msgrcv(*msgid, message_queue, sizeof(message_queue), 0, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }
        message = message_queue;

        // Send the message to the client RMI
        n_bytes =
            sendto(*sock, message, strlen(message) + 1, 0,
                   (struct sockaddr *)&addr_server, sizeof(struct sockaddr_in));
        if (n_bytes == -1) {
            perror("sendto");
            exit(EXIT_FAILURE);
        }
        printf("gestion-requete\t %s --> client-rmi\n", message);

        // Receive the response from the client RMI
        addr_size = sizeof(struct sockaddr_in);
        n_bytes = recvfrom(*sock, buffer, BUFFER_SIZE, 0,
                           (struct sockaddr *)&addr_server, &addr_size);
        if (n_bytes == -1) {
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }
        response = malloc(n_bytes * sizeof(char));
        memcpy(response, buffer, n_bytes);
        printf("gestion-requete\t <-- %s client-rmi\n", response);

        // Send the response to the message queue
        strcpy(message_queue, response);
        if (msgsnd(*msgid, message_queue, sizeof(message_queue), 0) == -1) {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }

        free(response);
    }
}

void initialize_message_queue(int *msgid) {
    key_t key = ftok("/tmp", 'a');
    if (key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }
    *msgid = msgget(key, 0666 | IPC_CREAT);
    if (*msgid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr,
                "Usage: %s <client_rmi_ip> <client_rmi_port> "
                "<communication_port>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    char *client_rmi_ip = argv[1];
    int client_rmi_port = atoi(argv[2]);
    int communication_port = atoi(argv[3]);

    int communication_sock;
    int *communication_sock_clients = malloc(5 * sizeof(int));
    int n_client_socks;
    int request_sock;

    sem_t mutex_message_queue;
    sem_init(&mutex_message_queue, 0, 1);

    // Initialize the communication sockets
    communication_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (communication_sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    request_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (request_sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Initialize the file message
    int msgid;
    initialize_message_queue(&msgid);
    char *message_queue = malloc(BUFFER_SIZE * sizeof(char));

    char ip[INET_ADDRSTRLEN];
    struct hostent *host;
    char hostname[1024];
    gethostname(hostname, 1024);
    host = gethostbyname(hostname);
    strcpy(ip, inet_ntoa(*(struct in_addr *)host->h_addr_list[0]));

    system("clear");
    printf("%s:%d\tclient-rmi\n", client_rmi_ip, client_rmi_port);
    printf("%s:%d\tcommunication\n", ip, communication_port);
    printf("\n");

    pid_t pid_communication = fork();
    if (pid_communication == 0) {
        communicaiton(&communication_sock, communication_sock_clients,
                      &n_client_socks, communication_port, &msgid,
                      message_queue, &mutex_message_queue);
    } else if (pid_communication == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    pid_t pid_request = fork();
    if (pid_request == 0) {
        gestion_requete(&request_sock, client_rmi_ip, client_rmi_port, &msgid,
                        message_queue);
    } else if (pid_request == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    printf("Press enter to quit\n\n");
    getchar();

    // Arrêter les processus fils
    kill(pid_communication, SIGKILL);
    kill(pid_request, SIGKILL);

    close(communication_sock);
    for (int i = 0; i < n_client_socks; i++)
        close(communication_sock_clients[i]);
    close(request_sock);

    // Supprimer la file de messages
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(EXIT_FAILURE);
    }
    free(message_queue);

    exit(EXIT_SUCCESS);
}