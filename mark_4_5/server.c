// shmem-gen.c
// write a random number between 0 and 999 to the shm every 1 second
#include <stdio.h>
#include <unistd.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "message.h"
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/ipc.h>


#define MAX_STRING 120
#define MAX_CLIENTS 10
#define BUFFER_SIZE 256


void sys_err(char *msg) {
    puts(msg);
    exit(1);
}


int main(int argc, char *argv[]) {
    //shared memory a num of ended clients
    int shm_id;
    int *counter;
    int num;
    srand(time(NULL));
    shm_id = shmget(0x2FF, getpagesize(), 0666 | IPC_CREAT);
    printf("shm_id = %d\n", shm_id);
    if (shm_id < 0) {
        perror("shmget()");
        exit(1);
    }

    /* подключение сегмента к адресному пространству процесса */
    counter = (int *) shmat(shm_id, 0, 0);
    if (counter == NULL) {
        perror("shmat()");
        exit(2);
    }

    printf("counter = %p\n", counter);
    *counter = 0;

    // shared memory a message
    char s[MAX_STRING];   // текст сообщения
    int shmid;            // идентификатор разделяемой памяти
    message_t *msg_p;     // адрес сообщения в разделяемой памяти

    // получение доступа к сегменту разделяемой памяти
    if ((shmid = shmget(SHM_ID, sizeof(message_t), PERMS | IPC_CREAT)) < 0) {
        sys_err("server: can not create shared memory segment");
    }
    printf("Shared memory %d created\n", SHM_ID);

    // подключение сегмента к адресному пространству процесса
    if ((msg_p = (message_t *) shmat(shmid, 0, 0)) == NULL) {
        sys_err("server: shared memory attach error");
    }
    printf("Shared memory pointer = %p\n", msg_p);
    msg_p->type = MSG_TYPE_STRING;

    // creating a new server
    int serverSocket;
    struct sockaddr_in serverAddr;
    pid_t childPid, childPid2;
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen;
    char buffer[BUFFER_SIZE];
    // проверка корректности параметров

    if (argc != 4) {
        printf("Usage: <%s> <port> <areas number> <clients number>\n", argv[0]);
        exit(1);
    }
    int areas_number = atoi(argv[2]);
    int clients_number = atoi(argv[3]);
    // Создание серверного сокета
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Ошибка при создании сокета");
        exit(1);
    }
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(argv[1]));
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Привязка сокета к серверному адресу
    if (bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
        perror("Ошибка при привязке сокета сервера");
        exit(1);
    }

    // Прослушивание входящих соединений
    if (listen(serverSocket, MAX_CLIENTS) < 0) {
        perror("Ошибка при прослушивании сокета сервера");
        exit(1);
    }


    int clientSockets[MAX_CLIENTS];
    for (int i = 0; i < clients_number; ++i) {
        int newClientSocket = accept(serverSocket, (struct sockaddr *) &clientAddr, &clientAddrLen);
        if (newClientSocket < 0) {
            perror("Ошибка при принятии соединения");
            exit(1);
        }
        clientSockets[i] = newClientSocket;
        printf("client is connected %d\n", newClientSocket);

    }
    for (int i = 0; i < clients_number; ++i) {
        childPid = fork();
        if (childPid < 0) {
            perror("Ошибка при создании нового процесса");
            exit(1);
        } else if (childPid == 0) {
            unsigned int seed = (unsigned int) (getpid() + i);
            srand(seed);
            int lands[2 * areas_number];
            // i's elem is number of area, i + 1's elem is status of this area
            int j;
            int number_of_elem = 0;
            for (j = 0; j < 2 * areas_number - 1; j += 2) {
                lands[j] = number_of_elem;
                lands[j + 1] = (rand()) % 2;
//                lands[j + 1] = 0;
                ++number_of_elem;
            }
            for (int i = 0; i < 2 * areas_number - 1; i += 2) {
                printf("land %d, status: %d\n", lands[i], lands[i + 1]);
            }
            send(clientSockets[i], lands, sizeof(lands), 0);
            printf("Отправка участков для клиента %d завершена\n", i);
            int is_treasure_found = 0;
            char buffer_2[BUFFER_SIZE];
            while (msg_p->type != MSG_TYPE_FINISH && msg_p->type != MSG_TYPE_EMPTY) {
                msg_p->type = MSG_TYPE_STRING;

                memset(buffer, 0, sizeof(buffer));
                int bytesRead = recv(clientSockets[i], buffer, sizeof(buffer), 0);

                strncpy(buffer_2, buffer, 25);
                buffer_2[25] = '\0';

                if (strcmp(buffer_2, "Treasure is found by team") == 0) {
                    printf("WE HAVE A NEW WINNER\n");
                    printf("%s\n", buffer);
                    is_treasure_found = 1;
                    msg_p->type = MSG_TYPE_FINISH;
                    break;
                } else if (strcmp(buffer_2, "Nothing was found by team") == 0) {
                    *counter = *counter + 1;
                    if (*counter == clients_number) {
                        printf("NOTHING WAS FOUND\n");
                        msg_p->type = MSG_TYPE_FINISH;
                        break;
                    }
                }
                sleep(2);
            }
            if (msg_p->type != MSG_TYPE_EMPTY) {
                if (is_treasure_found) {
                    msg_p->type = MSG_TYPE_EMPTY;
                    for (j = 0; j < clients_number; ++j) {
                        printf("sending ending msg to clients\n");
                        if (send(clientSockets[j], "IT IS DONE", sizeof("IT IS DONE"), 0) < 0) {
                            perror("Ошибка при отправке сообщения на сервер");
                            exit(1);
                        }
                    }
                } else {
                    printf("Treasure was not found\n");
                    for (j = 0; j < clients_number; ++j) {
                        printf("sending msg to clients\n");
                        if (send(clientSockets[j], "IT IS DONE", sizeof("IT IS DONE"), 0) < 0) {
                            perror("Ошибка при отправке сообщения на сервер");
                            exit(1);
                        }
                    }
                }
            }
            exit(1);
        }
    }
    printf("Ending a server\n");
    for (int i = 0; i < clients_number; ++i) {
        int status;
        waitpid(-1, &status, 0);
    }

    shmdt(msg_p);
    if (shmctl(shmid, IPC_RMID, (struct shmid_ds *) 0) < 0) {
        sys_err("server: shared memory remove error");
    }
    if (shmctl(shm_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
        sys_err("server: shared memory remove error");
    }
    close(serverSocket);
    return 0;
}
