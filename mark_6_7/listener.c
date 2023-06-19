#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <time.h>

#define BUFFER_SIZE 256


void sys_err(char *msg) {
    puts(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int clientSocket;
    struct sockaddr_in serverAddr;
    char buffer[BUFFER_SIZE];
    int recvMsgSize;

    if (argc != 3) {
        printf("Usage: <%s> <server ip> <server port>\n", argv[0]);
        exit(1);
    }

    // Создание клиентского сокета
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        perror("Ошибка при создании сокета");
        exit(1);
    }

    // Настройка серверного адреса
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(argv[2]));  // Порт, указанный в аргументах
    serverAddr.sin_addr.s_addr = inet_addr(argv[1]);  // Адрес сервера, указанный в аргументах

    // Подключение к серверу
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Ошибка при подключении к серверу");
        exit(1);
    }
    pid_t childPid1, childPid2;
    childPid1 = fork();
    if (childPid1 < 0) {
        perror("Error while creating a new process\n");
    } else if (childPid1 == 0) {
        while (1) {
            memset(buffer, 0, sizeof(buffer));
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            printf("%s\n", buffer);
            if (bytesReceived < 0) {
                perror("Ошибка при получении сообщения от сервера");
                exit(1);
            }
            // Проверка на завершение
            if (strcmp(buffer, "The End") == 0) {
                break;
            }
            sleep(1);
        }
        exit(1);
    }
    for (int i = 0; i < 2; ++i) {
        int status;
        waitpid(-1, &status, 0);
    }
    close(clientSocket);
    return 0;
}
