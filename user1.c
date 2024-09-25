#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <arpa/inet.h>
#include "msocket.h"

int main(int argc, char *argv[])
{
    if (argc != 6 || strcmp(argv[2], argv[4]) == 0 || strncmp(argv[5] + strlen(argv[5]) - 4, ".txt", 4) != 0)
    {
        printf("Usage: %s <ip1> <port1> <ip2> <port2> <filename.txt>\n", argv[0]);
        exit(1);
    }

    char *ip1 = argv[1];
    int port1 = atoi(argv[2]);
    char *ip2 = argv[3];
    int port2 = atoi(argv[4]);
    char *filename = argv[5];

    key_t key = ftok("initmsocket.c", 100);
    int usermut = semget(key, 1, IPC_CREAT | 0666);

    struct sembuf pop;
    pop.sem_num = 0;
    pop.sem_flg = 0;
    pop.sem_op = -1;

    sleep(2);
    
    int sockfd = m_socket(AF_INET, SOCK_MTP, 0, getpid());
    if (sockfd < 0)
    {
        perror("m_socket");
        exit(1);
    }
    printf("Socket created\n");

    if (m_bind(sockfd, ip1, port1, ip2, port2) == -1)
    {
        perror("m_bind");
        exit(1);
    }
    printf("Bind done\n");

    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        perror("fopen");
        exit(1);
    }

    semop(usermut, &pop, 1);

    char buf[1025];
    int bytes_read;
    while ((bytes_read = fread(buf, 1, sizeof(buf) - 1, fp)) > 0)
    {
        sleep(1);
        buf[bytes_read] = '\0';
        while (1)
        {
            int ret = m_sendto(sockfd, buf, (int)strlen(buf), 0, ip2, port2);
            if (ret < 0 && errno == ENOBUFS)
            {
                printf("No buffer space, tyring again...\n");
                sleep(2);
            }
            else
                break;
        }

        printf("MESSAGE: \n%d\n\n", (int)strlen(buf));
    }

    memset(buf, '\0', strlen(buf));
    strcpy(buf, ".");
    while (1)
    {
        int ret = m_sendto(sockfd, buf, (int)strlen(buf), 0, ip2, port2);
        if (ret < 0 && errno == ENOBUFS)
        {
            printf("No buffer space, tyring again...\n");
            sleep(2);
        }
        else
            break;
    }
}
