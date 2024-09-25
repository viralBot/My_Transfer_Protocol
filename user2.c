#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <arpa/inet.h>
#include "msocket.h"

int main(int argc, char *argv[])
{
    if (argc != 6 || strcmp(argv[2], argv[4]) == 0 || strncmp(argv[5] + strlen(argv[5]) - 4, ".txt", 4) != 0)
    {
        printf("Usage: %s <ip2> <port2> <ip1> <port1> <filename.txt>\n", argv[0]);
        exit(1);
    }

    char *ip2 = argv[1];
    int port2 = atoi(argv[2]);
    char *ip1 = argv[3];
    int port1 = atoi(argv[4]);
    char *filename = argv[5];

    key_t key = ftok("initmsocket.c", 100);
    int usermut = semget(key, 1, IPC_CREAT | 0666);
    semctl(usermut, 0, SETVAL, 0);

    struct sembuf vop;
    vop.sem_num = 0;
    vop.sem_flg = 0;
    vop.sem_op = 1;

    sleep(2);

    int sockfd = m_socket(AF_INET, SOCK_MTP, 0, getpid());
    if (sockfd < 0)
    {
        perror("m_socket");
        exit(1);
    }
    printf("Socket created\n");

    if (m_bind(sockfd, ip2, port2, ip1, port1) == -1)
    {
        perror("m_bind");
        exit(1);
    }
    printf("Bind done\n");

    semop(usermut, &vop, 1);

    FILE *file;
    char *newfilename;
    newfilename = (char *)malloc(strlen(filename) + 5);
    strcpy(newfilename, filename);
    strcat(newfilename, ".new");
    file = fopen(newfilename, "w");

    char buf[1025];
    int bytes_read;

    int cn = 0;
    while (1)
    {
        printf("Sleeping...\n");
        sleep(2);
        // memset(buf, '\0', strlen(buf));
        if ((bytes_read = m_recvfrom(sockfd, buf, sizeof(buf) - 1, 0)))
        {
            if (bytes_read < 0)
            {
                printf("Message not found...\n");
                continue;
            }
            printf("%d bytes read...\n", bytes_read);
            printf("Last letter: %c\n", buf[bytes_read - 1]);
            buf[bytes_read] = '\0';

            int gotlast = 0;
            if (buf[bytes_read - 1] == '.')
            {
                gotlast = 1;
                buf[bytes_read - 1] = '\0';
            }
            int ret = fprintf(file, "%s", buf);
            if (ret < 0)
            {
                perror("fprintf");
                exit(1);
            }
            cn++;
            printf("cn: %d\n", cn);
            if (gotlast)
            {
                break;
            }
        }
        // else
        // {
        //     break;
        // }
    }

    printf("Data transfer complete\n");

    fclose(file);

    return 0;
}
