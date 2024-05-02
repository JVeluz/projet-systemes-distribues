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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 256

typedef struct {
    int id;
    int *sock;
    int *msgid;
    sem_t *mutex_message_queue;
} handle_client_args;

volatile sig_atomic_t running = 1;

void sigpipe_handler() { running = 0; }

void *broadcast(char *message, int *sock_clients, int n_clients) {
    for (int i = 0; i < n_clients; i++)
        write(sock_clients[i], message, strlen(message) + 1);
}

void *handle_client(void *arg) {
    handle_client_args *args = (handle_client_args *)arg;
    int id = args->id;
    int *sock = args->sock;
    int *msgid = args->msgid;
    sem_t *mutex_message_queue = args->mutex_message_queue;

    char buffer[BUFFER_SIZE];
    char *message_queue = malloc(BUFFER_SIZE * sizeof(char));
    char *message;
    char *response;
    char n_bytes;

    printf("communication\tclient-%d ready\n", id);

    while (running) {
        // Receive the message from the client
        n_bytes = read(*sock, buffer, BUFFER_SIZE);

        // Check if the client is disready
        if (n_bytes == 0) {
            printf("communication\tclient-%d disready\n", id);
            break;
        }

        message = malloc(n_bytes * sizeof(char));
        memcpy(message, buffer, n_bytes);
        printf("communication\t <-- %s client-%d\n", message, id);

        // broadcast(message, sock_clients, n_clients);

        // Send the message to the message queue
        sem_wait(mutex_message_queue);
        strcpy(message_queue, message);
        if (msgsnd(*msgid, message_queue, sizeof(message_queue), 0) == -1) {
            perror("handle_client:msgsnd");
            sem_post(mutex_message_queue);
            continue;
        }

        // Receive the response from the message queue
        response = malloc(BUFFER_SIZE * sizeof(char));
        if (msgrcv(*msgid, message_queue, sizeof(message_queue), 0, 0) == -1) {
            perror("handle_client:msgrcv");
            sem_post(mutex_message_queue);
            continue;
        }
        strcpy(response, message_queue);
        sem_post(mutex_message_queue);

        // Send the response to the client
        write(*sock, response, strlen(response) + 1);
        printf("communication\t%s --> client-%d\n", response, id);
        printf("\n");

        free(message);
        free(response);
    }

    close(*sock);
    free(args);
    free(message_queue);
}

void communicaiton(int port, int *msgid) {
    struct sockaddr_in addr_client;
    struct sockaddr_in addr_server;
    int addr_size;

    int sock_server;

    // Initialize the semaphore
    sem_t mutex_message_queue;
    sem_init(&mutex_message_queue, 0, 1);

    // Initialize the sockets
    sock_server = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_server == -1) {
        perror("communicaiton:socket");
        exit(EXIT_FAILURE);
    }

    // Initialize the server address
    memset((char *)&addr_server, 0, sizeof(addr_server));
    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = INADDR_ANY;
    addr_server.sin_port = htons(port);

    // Bind the socket to the server address
    if (bind(sock_server, (struct sockaddr *)&addr_server,
             sizeof(addr_server)) == -1) {
        perror("communicaiton:bind");
        close(sock_server);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(sock_server, 5) == -1) {
        perror("communicaiton:listen");
        close(sock_server);
        exit(EXIT_FAILURE);
    }

    printf("communication: ready\n");

    int n_clients = 0;
    int *sock_clients = NULL;
    pthread_t *client_threads = NULL;
    while (running) {
        client_threads =
            realloc(client_threads, (n_clients + 1) * sizeof(pthread_t));
        if (client_threads == NULL) {
            perror("communicaiton:realloc");
            break;
        }
        sock_clients = realloc(sock_clients, (n_clients + 1) * sizeof(int));
        if (sock_clients == NULL) {
            perror("communicaiton:realloc");
            break;
        }

        // Accept the client connection
        addr_size = sizeof(addr_client);
        sock_clients[n_clients] =
            accept(sock_server, (struct sockaddr *)&addr_client, &addr_size);
        if (sock_clients[n_clients] == -1) {
            perror("communicaiton:accept");
            continue;
        }

        // Create a new thread to handle the client
        handle_client_args *args = malloc(sizeof(handle_client_args));
        args->id = n_clients;
        args->sock = &sock_clients[n_clients];
        args->msgid = msgid;
        args->mutex_message_queue = &mutex_message_queue;
        if (pthread_create(&client_threads[n_clients], NULL, handle_client,
                           (void *)args) != 0) {
            perror("communicaiton:pthread_create");
            continue;
        }
        pthread_detach(client_threads[n_clients]);

        n_clients++;
    }
    free(client_threads);
    close(sock_server);

    sem_destroy(&mutex_message_queue);

    printf("communication closed properly\n");
}

void gestion_requete(char *ip, int port, int *msgid) {
    struct hostent *server_host;
    static struct sockaddr_in addr_server;
    socklen_t addr_size;

    int sock;

    char buffer[BUFFER_SIZE];
    char *message_queue = malloc(BUFFER_SIZE * sizeof(char));
    char *message;
    char *response;
    char n_bytes;

    while (running) {
        // Initialize the socket
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == -1) {
            perror("gestion_requete:socket");
            sleep(1);
            continue;
        }

        // Initialize the client address
        server_host = gethostbyname(ip);
        if (server_host == NULL) {
            perror("gestion_requete:gethostbyname");
            sleep(1);
            continue;
        }
        bzero(&addr_server, sizeof(struct sockaddr_in));
        addr_server.sin_family = AF_INET;
        addr_server.sin_port = htons(port);
        memcpy(&addr_server.sin_addr.s_addr, server_host->h_addr_list[0],
               server_host->h_length);

        break;
    }

    printf("gestion-requete: ready\n");

    while (running) {
        // Recieve the message from the message queue
        if (msgrcv(*msgid, message_queue, sizeof(message_queue), 0, 0) == -1) {
            perror("gestion_requete:msgrcv");
            continue;
        }
        message = message_queue;

        // Send the message to the client RMI
        n_bytes =
            sendto(sock, message, strlen(message) + 1, 0,
                   (struct sockaddr *)&addr_server, sizeof(struct sockaddr_in));
        if (n_bytes == -1) {
            perror("gestion_requete:sendto");
            continue;
        }
        printf("gestion-requete\t %s --> client-rmi\n", message);

        // Receive the response from the client RMI
        addr_size = sizeof(struct sockaddr_in);
        n_bytes = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                           (struct sockaddr *)&addr_server, &addr_size);
        if (n_bytes == -1) {
            perror("gestion_requete:recvfrom");
            continue;
        }
        response = malloc(n_bytes * sizeof(char));
        memcpy(response, buffer, n_bytes);
        printf("gestion-requete\t <-- %s client-rmi\n", response);

        // Send the response to the message queue
        strcpy(message_queue, response);
        if (msgsnd(*msgid, message_queue, sizeof(message_queue), 0) == -1) {
            perror("gestion_requete:msgsnd");
            continue;
        }

        free(response);
    }

    close(sock);

    free(message_queue);

    printf("gestion-requete closed properly\n");
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
    signal(SIGINT, sigpipe_handler);

    if (argc != 5) {
        fprintf(stderr,
                "Usage: %s <gestion-compte-ip> <gestion-compte-port> "
                "<client-rmi-port> <communication-port>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    char *gestion_compte_ip = argv[1];
    int gestion_compte_port = atoi(argv[2]);
    char *client_rmi_ip = "localhost";
    int client_rmi_port = atoi(argv[3]);
    char *communication_ip = "localhost";
    int communication_port = atoi(argv[4]);

    printf("%s:%d\tgestion-compte\n", gestion_compte_ip, gestion_compte_port);
    printf("%s:%d\tclient-rmi\n", client_rmi_ip, client_rmi_port);
    printf("%s:%d\tcommunication\n", communication_ip, communication_port);
    printf("\n");

    // Initialize the file message
    int msgid;
    initialize_message_queue(&msgid);

    pid_t pid_communication = fork();
    if (pid_communication == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid_communication == 0) {
        communicaiton(communication_port, &msgid);
    }

    pid_t pid_request = fork();
    if (pid_request == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid_request == 0) {
        gestion_requete("localhost", client_rmi_port, &msgid);
    }

    pid_t pid_client_rmi = fork();
    if (pid_client_rmi == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid_client_rmi == 0) {
        execlp("java", "java", "-jar", "client-rmi/client-rmi.jar",
               gestion_compte_ip, argv[2], argv[3], NULL);
    }

    while (running)
        ;

    // Attendre la fin des processus fils
    waitpid(pid_client_rmi, NULL, 0);
    waitpid(pid_request, NULL, 0);
    waitpid(pid_communication, NULL, 0);

    // Supprimer la file de messages
    msgctl(msgid, IPC_RMID, NULL);

    exit(EXIT_SUCCESS);
}