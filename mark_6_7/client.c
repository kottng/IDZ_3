// shmem-out.c
// read from the shm every 1 second
#include<stdio.h>
#include<unistd.h>
#include<sys/shm.h>
#include<stdlib.h>
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

#define BUFFER_SIZE 256

void sys_err(char *msg) {
    puts(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int id = atoi(argv[4]);
    // shared memory
    char s[MAX_STRING];   // текст сообщения
    int shmid;            // идентификатор разделяемой памяти
    message_t *msg_p;     // адрес сообщения в разделяемой памяти

    // получение доступа к сегменту разделяемой памяти
    if ((shmid = shmget(SHMID + id, sizeof(message_t), PERMS | IPC_CREAT)) < 0) {
        sys_err("server: can not create shared memory segment");
    }
    printf("Shared memory %d created\n", SHMID + id);

    // подключение сегмента к адресному пространству процесса
    if ((msg_p = (message_t *) shmat(shmid, 0, 0)) == NULL) {
        sys_err("server: shared memory attach error");
    }
    printf("Shared memory pointer = %p\n", msg_p);
//    printf("type is %d\n", msg_p->type);
    msg_p->type = MSG_TYPE_STRING;


    // client
    int clientSocket;
    struct sockaddr_in serverAddr;
    char buffer[BUFFER_SIZE];
    int recvMsgSize;
    pid_t childpid1;
    pid_t childpid2;

    if (argc != 5) {
        printf("Usage: <%s> <server ip> <server port> <areas number> <id>\n", argv[0]);
        exit(1);
    }

    // Создание клиентского сокета
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        perror("Ошибка при создании сокета");
        exit(1);
    }
    int areas_number = atoi(argv[3]);

    // Настройка серверного адреса
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(argv[2]));  // Порт, указанный в аргументах
    serverAddr.sin_addr.s_addr = inet_addr(argv[1]);  // Адрес сервера, указанный в аргументах

    // Подключение к серверу
    if (connect(clientSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
        perror("Ошибка при подключении к серверу");
        exit(1);
    }
    int lands[2 * areas_number];
    printf("Подключение к серверу установлено.\n");
    ssize_t bytesRead = recv(clientSocket, lands, sizeof(lands) - 1, 0);
    printf("An array of lands received:\n");
    for (int i = 0; i < 2 * areas_number - 1; i += 2) {
        printf("land %d, status: %d\n", lands[i], lands[i + 1]);
    }
    if (bytesRead < 0) {
        perror("Ошибка при чтении данных от клиента");
        exit(1);
    }
    childpid1 = fork();
    if (childpid1 < 0) {
        perror("Error while creating a new process");
        exit(1);
    } else if (childpid1 == 0) {
        int i = 0;
        while (i < 2 * areas_number - 1 && msg_p->type != MSG_TYPE_FINISH) {
            msg_p->type = MSG_TYPE_STRING;
            unsigned int seed = (unsigned int) (getpid() + i);
            srand(seed);
            sleep(rand() % 16 + 4); // Пауза для осмотра участка
            if (lands[i + 1] == 1 && msg_p->type != MSG_TYPE_FINISH) {
                sprintf(buffer, "Treasure is found by team %d", id - 1);
                printf("%s\n", buffer);
                // Найден клад, отправка сообщения на сервер
                if (send(clientSocket, buffer, sizeof(buffer), 0) < 0) {
                    perror("Error while sending a message to the server");
                    exit(1);
                }
                msg_p->type = MSG_TYPE_FINISH;
                break;
            }
            printf("team %d did not find anything\n", id - 1);
            i += 2;
        }
        if (msg_p->type == MSG_TYPE_STRING) {
            printf("SENDING A MESSAGE\n");
            sprintf(buffer, "Nothing was found by team %d\n", id - 1);
            // Клад не найден, отправка сообщения на сервер
            if (send(clientSocket, buffer, sizeof(buffer), 0) < 0) {
                perror("Error while sending a message to the server");
                exit(1);
            }
        }
        msg_p->type = MSG_TYPE_FINISH;
        exit(1);
    }
    childpid2 = fork();
    if (childpid2 < 0) {
        perror("Error while creating a new process");
        exit(1);
    } else if (childpid2 == 0) {
        bytesRead = 0;
        memset(buffer, 0, sizeof(buffer));
        do {
            bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesRead > 1) {
                printf("a message is received\n");
                msg_p->type = MSG_TYPE_FINISH;
            }
            sleep(1);
        } while (bytesRead < 2 && msg_p->type != MSG_TYPE_FINISH);
        exit(1);
    }

    for (int i = 0; i < 2; ++i) {
        int status;
        waitpid(-1, &status, 0);
    }
    shmdt(msg_p);
    if (shmctl(shmid, IPC_RMID, (struct shmid_ds *) 0) < 0) {
        sys_err("client: shared memory remove error");
    }

    printf("Client is ended\n");
    close(clientSocket);
    return 0;
}
