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
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 256
#define MSG_PATH "msg"
#define SHM_PATH "shm"

typedef struct {
    char *username;
    int sock;
} user_t;

typedef struct {
    user_t *users;
    unsigned int size;
} users_list_t;

typedef struct {
    int id;
    int *sock;
    sem_t *mutex_client;
} handle_client_args;

volatile sig_atomic_t running = 1;

int sock_communication;
int sock_gestion_requete;

void sigpipe_handler() { running = 0; }

void broadcast(char *message) {
    key_t shm_key;
    int shm_id;
    users_list_t *users_list;

    // Get the shared memory
    shm_key = ftok(SHM_PATH, 0);
    if (shm_key == -1) {
        perror("broadcast:ftok");
        exit(EXIT_FAILURE);
    }
    shm_id = shmget(shm_key, sizeof(users_list), 0);
    if (shm_id == -1) {
        perror("broadcast:shmget");
        exit(EXIT_FAILURE);
    }

    // Get the users list
    users_list = shmat(shm_id, NULL, 0);
    if (users_list == (void *)-1) {
        perror("broadcast:shmat");
        exit(EXIT_FAILURE);
    }

    // Send the message to all the clients
    printf("communication: --> %s clients\n", message);
    for (int i = 0; i < users_list->size; i++) {
        write(users_list->users[i].sock, message, strlen(message) + 1);
    }
}

void *handle_client(void *arg) {
    handle_client_args *args = (handle_client_args *)arg;
    int id = args->id;
    int *sock = args->sock;
    sem_t *mutex_client = args->mutex_client;

    char buffer[BUFFER_SIZE];
    char *message;
    char *response;
    char n_bytes;

    key_t msg_key;
    int msg_id;

    // Get the message queue
    msg_key = ftok(MSG_PATH, 0);
    if (msg_key == -1) {
        perror("communication:ftok");
        exit(EXIT_FAILURE);
    }
    msg_id = msgget(msg_key, 0);
    if (msg_id == -1) {
        perror("communication:msgget");
        exit(EXIT_FAILURE);
    }

    printf("communication: client-%d connected\n", id);

    while (running) {
        // Receive the message from the client
        n_bytes = read(*sock, buffer, BUFFER_SIZE);
        // Check if the client is disready
        if (n_bytes == 0) {
            printf("communication: client-%d disconnected\n", id);
            break;
        }
        printf("\n");
        printf("communication: <-- %s client-%d\n", buffer, id);
        message = malloc(n_bytes * sizeof(char));
        strcpy(message, buffer);

        if (strncmp(message, "message:", 8) == 0) {
            broadcast(message);
            free(message);
            continue;
        }

        // Send the message to the message queue
        sem_wait(mutex_client);
        if (msgsnd(msg_id, message, sizeof(message), 0) == -1) {
            perror("handle_client:msgsnd");
            sem_post(mutex_client);
            continue;
        }
        // Receive the response from the message queue
        response = malloc(BUFFER_SIZE * sizeof(char));
        if (msgrcv(msg_id, response, sizeof(response), 0, 0) == -1) {
            perror("handle_client:msgrcv");
            sem_post(mutex_client);
            continue;
        }
        sem_post(mutex_client);

        // Send the response to the client
        printf("communication: %s --> client-%d\n", response, id);
        write(*sock, response, strlen(response) + 1);

        free(message);
        free(response);
    }

    close(*sock);
    free(args);
}

void communicaiton(int port) {
    struct sockaddr_in addr_client;
    struct sockaddr_in addr_server;
    int addr_size;

    sem_t mutex_client;

    int *sock_clients = NULL;
    pthread_t *client_threads = NULL;

    key_t msg_key;
    key_t shm_key;
    int msg_id;
    int shm_id;

    // Get the message queue
    msg_key = ftok(MSG_PATH, 0);
    if (msg_key == -1) {
        perror("communication:ftok");
        exit(EXIT_FAILURE);
    }
    msg_id = msgget(msg_key, 0);
    if (msg_id == -1) {
        perror("communication:msgget");
        exit(EXIT_FAILURE);
    }

    // Get the shared memory
    shm_key = ftok(SHM_PATH, 0);
    if (shm_key == -1) {
        perror("handle_client:ftok");
        exit(EXIT_FAILURE);
    }
    shm_id = shmget(shm_key, sizeof(users_list_t), 0);
    if (shm_id == -1) {
        perror("handle_client:shmget");
        exit(EXIT_FAILURE);
    }

    // Get the users list
    users_list_t *users_list = shmat(shm_id, NULL, 0);
    if (users_list == (void *)-1) {
        perror("handle_client:shmat");
        exit(EXIT_FAILURE);
    }

    // Initialize the semaphore
    sem_init(&mutex_client, 0, 1);

    // Initialize the sockets
    sock_communication = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_communication == -1) {
        perror("communicaiton:socket");
        exit(EXIT_FAILURE);
    }

    // Initialize the server address
    memset((char *)&addr_server, 0, sizeof(addr_server));
    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = INADDR_ANY;
    addr_server.sin_port = htons(port);

    // Bind the socket to the server address
    if (bind(sock_communication, (struct sockaddr *)&addr_server,
             sizeof(addr_server)) == -1) {
        perror("communicaiton:bind");
        close(sock_communication);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(sock_communication, 5) == -1) {
        perror("communicaiton:listen");
        close(sock_communication);
        exit(EXIT_FAILURE);
    }

    printf("communication: ready\n");

    int n_clients = 0;
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
        sock_clients[n_clients] = accept(
            sock_communication, (struct sockaddr *)&addr_client, &addr_size);
        if (sock_clients[n_clients] == -1) {
            perror("communicaiton:accept");
            continue;
        }

        // Create a new thread to handle the client
        handle_client_args *args = malloc(sizeof(handle_client_args));
        args->id = n_clients;
        args->sock = &sock_clients[n_clients];
        args->mutex_client = &mutex_client;
        if (pthread_create(&client_threads[n_clients], NULL, handle_client,
                           (void *)args) != 0) {
            perror("communicaiton:pthread_create");
            continue;
        }
        pthread_detach(client_threads[n_clients]);

        users_list->users =
            realloc(users_list->users, (users_list->size + 1) * sizeof(user_t));
        users_list->users[users_list->size].username = "client";
        users_list->users[users_list->size].sock = sock_clients[n_clients];
        users_list->size++;

        n_clients++;
    }

    // Wait for the client threads
    for (int i = 0; i < n_clients; i++) {
        pthread_join(client_threads[i], NULL);
    }

    // Close the sockets
    close(sock_communication);

    // Destroy the semaphore
    sem_destroy(&mutex_client);

    // Free the memory
    free(sock_clients);
    free(client_threads);

    exit(EXIT_SUCCESS);
}

void gestion_requete(char *ip, int port) {
    struct hostent *server_host;
    static struct sockaddr_in addr_server;
    socklen_t addr_size;

    key_t msg_key;
    int msg_id;

    char buffer[BUFFER_SIZE];
    char *message;
    char *response;
    char n_bytes;

    // Get the message queue
    msg_key = ftok(MSG_PATH, 0);
    if (msg_key == -1) {
        perror("gestion_requete:ftok");
        exit(EXIT_FAILURE);
    }
    msg_id = msgget(msg_key, 0);
    if (msg_id == -1) {
        perror("gestion_requete:msgget");
        exit(EXIT_FAILURE);
    }

    // Initialize the socket
    sock_gestion_requete = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_gestion_requete == -1) {
        perror("gestion_requete:socket");
        exit(EXIT_FAILURE);
    }
    // Initialize the client address
    server_host = gethostbyname(ip);
    if (server_host == NULL) {
        perror("gestion_requete:gethostbyname");
        exit(EXIT_FAILURE);
    }
    bzero(&addr_server, sizeof(struct sockaddr_in));
    addr_server.sin_family = AF_INET;
    addr_server.sin_port = htons(port);
    memcpy(&addr_server.sin_addr.s_addr, server_host->h_addr_list[0],
           server_host->h_length);

    printf("gestion-requete: ready\n");

    while (running) {
        // Receive the message from the message queue
        if (msgrcv(msg_id, buffer, sizeof(buffer), 0, 0) == -1) {
            perror("gestion_requete:msgrcv");
            continue;
        }
        message = malloc(strlen(buffer) * sizeof(char));
        strcpy(message, buffer);
        memset(buffer, 0, BUFFER_SIZE);
        printf("gestion-requete: <-- %s communication\n", message);

        // Send the message to the client RMI
        if (sendto(sock_gestion_requete, message, strlen(message), 0,
                   (struct sockaddr *)&addr_server,
                   sizeof(addr_server)) == -1) {
            perror("gestion_requete:sendto");
            continue;
        }

        // Receive the response from the client RMI
        n_bytes = recvfrom(sock_gestion_requete, buffer, BUFFER_SIZE, 0,
                           (struct sockaddr *)&addr_server, &addr_size);
        if (n_bytes == -1) {
            perror("gestion_requete:recvfrom");
            continue;
        }
        response = malloc(n_bytes * sizeof(char));
        strcpy(response, buffer);
        memset(buffer, 0, BUFFER_SIZE);

        // Send the response to the message queue
        printf("gestion-requete: %s --> communication\n", response);
        if (msgsnd(msg_id, response, sizeof(response), 0) == -1) {
            perror("gestion_requete:msgsnd");
            continue;
        }

        free(response);
        free(message);
    }

    // Close the socket
    close(sock_gestion_requete);

    exit(EXIT_SUCCESS);
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

    pid_t pid_communication;
    pid_t pid_gestion_requete;
    pid_t pid_client_rmi;

    users_list_t users_list;
    users_list.size = 0;

    key_t msg_key;
    key_t shm_key;
    int msg_id;
    int shm_id;

    // Initialize the file message
    system("touch msg");
    msg_key = ftok(MSG_PATH, 0);
    if (msg_key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }
    // Create the message queue
    msg_id = msgget(msg_key, IPC_CREAT | 0666);
    if (msg_id == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    // Initialize the shared memory
    system("touch shm");
    shm_key = ftok(SHM_PATH, 0);
    if (shm_key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }
    // Create the shared memory
    shm_id = shmget(shm_key, sizeof(users_list), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Launch the processes
    pid_communication = fork();
    if (pid_communication == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid_communication == 0) {
        communicaiton(communication_port);
    }
    pid_gestion_requete = fork();
    if (pid_gestion_requete == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid_gestion_requete == 0) {
        gestion_requete("localhost", client_rmi_port);
    }
    pid_client_rmi = fork();
    if (pid_client_rmi == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid_client_rmi == 0) {
        execlp("java", "java", "-jar", "client-rmi/client-rmi.jar",
               gestion_compte_ip, argv[2], argv[3], NULL);
    }

    // Wait for signal
    while (running)
        ;

    // Kill the child processes
    kill(pid_communication, SIGKILL);
    kill(pid_gestion_requete, SIGKILL);
    kill(pid_client_rmi, SIGKILL);

    // Wait for the child processes
    waitpid(pid_communication, NULL, 0);
    waitpid(pid_gestion_requete, NULL, 0);
    waitpid(pid_client_rmi, NULL, 0);

    // Remove the message queue and the shared memory
    msgctl(msg_id, IPC_RMID, NULL);
    shmctl(shm_id, IPC_RMID, NULL);

    exit(EXIT_SUCCESS);
}