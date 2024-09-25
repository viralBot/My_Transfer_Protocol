#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include "msocket.h"

#define wait(s) semop(s, &pop, 1)
#define sig(s) semop(s, &vop, 1)

static int msgsends = 0;
static int acksends = 0;

void *receiver(void *arg)
{
    fd_set readfds;
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    int key1, key5;
    key1 = ftok("initmsocket.c", 1);
    int shmid = shmget(key1, MAX_SOCKETS * sizeof(socketStruct), 0777);
    socketStruct *SM = (socketStruct *)shmat(shmid, 0, 0);

    key5 = ftok("initmsocket.c", 5);
    int mutid1 = semget(key5, 1, 0777);
    struct sembuf pop, vop;
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;

    while (1)
    {
        FD_ZERO(&readfds);
        int maxfd = 0;
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            if (SM[i].free == 0)
            {
                FD_SET(SM[i].sockfd, &readfds);
                maxfd = SM[i].sockfd > maxfd ? SM[i].sockfd : maxfd;
            }
        }
        int ret = select(maxfd + 1, &readfds, NULL, NULL, &timeout);
        if (ret > 0)
        {
            wait(mutid1);
            for (int i = 0; i < MAX_SOCKETS; i++)
            {
                if (FD_ISSET(SM[i].sockfd, &readfds))
                {
                    // receive
                    char msg[1030];

                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    recvfrom(SM[i].sockfd, msg, sizeof(msg), 0, (struct sockaddr *)&client_addr, &client_len);

                    char *textData;
                    int type, seqnum;
                    sscanf(msg, "%d\n%2d\n", &type, &seqnum);

                    textData = strchr(msg, '\n');
                    textData = strchr(textData + 1, '\n');
                    textData++;

                    if (dropMessage(DROP_PROBABILITY))
                    {
                        if (type == 0)
                        {
                            printf("\033[0;34mDropped MSG: %d %d %d\n", type, seqnum, (int)strlen(textData));
                        }
                        else
                        {
                            printf("\033[0;34m\t\tDropped ACK: %d %d %d\n", type, seqnum, (int)atoi(textData));
                        }
                        break;
                    }

                    if (SM[i].nospace)
                    {
                        printf("\033[0;34mNospace in recBuf, continuing...\n");
                        printf("recbuf: ");
                        for (int j = 0; j < RECV_BUF_SIZE; j++)
                        {
                            if (!SM[i].recvBuf[j].free)
                            {
                                printf("%d ", SM[i].recvBuf[j].seqnum);
                            }
                        }
                        continue;
                    }

                    if (type == 0) // message type
                    {
                        char display[1031];
                        strcpy(display, textData);
                        display[10] = '\0';
                        printf("\033[0;34mReceived MSG: %d %d %d %s\n", type, seqnum, (int)strlen(textData), display);
                        // if (SM[i].rwnd.seqnumLen == 0)
                        // {
                        //     perror("rwnd size 0");
                        //     exit(1);
                        // }
                        int pos = -1;
                        for (int j = 0; j < SM[i].rwnd.seqnumLen; j++)
                        {
                            if (SM[i].rwnd.seqnum[j] == seqnum)
                            {
                                pos = j;
                                break;
                            }
                        }
                        // printf("pos: %d %d\n", pos, SM[i].rwnd.seqnumLen);
                        if (pos != -1) // if message in rwnd
                        {
                            // update rwnd
                            SM[i].rwnd.seqnumLen--;
                            if (seqnum > SM[i].rwnd.highestseqnum && seqnum <= SM[i].rwnd.highestseqnum + RECV_BUF_SIZE - 1)
                            {
                                SM[i].rwnd.highestseqnum = seqnum;
                            }
                            else if (seqnum < SM[i].rwnd.highestseqnum && (SM[i].rwnd.highestseqnum - seqnum > RECV_BUF_SIZE) && seqnum <= (SM[i].rwnd.highestseqnum + RECV_BUF_SIZE + 1) % 16)
                            {
                                SM[i].rwnd.highestseqnum = seqnum;
                            }
                            for (int j = pos; j < SM[i].rwnd.seqnumLen; j++)
                            {
                                SM[i].rwnd.seqnum[j] = SM[i].rwnd.seqnum[j + 1];
                            }
                            printf("\033[0;34mrwnd: ");
                            for (int j = 0; j < SM[i].rwnd.seqnumLen; j++)
                            {
                                printf("%d ", SM[i].rwnd.seqnum[j]);
                            }
                            printf("\n");
                            printf("highest seq num: %d\n", SM[i].rwnd.highestseqnum);

                            // changing window_length
                            if (SM[i].rwnd.seqnumLen)
                            {
                                SM[i].rwnd.lastseqnum = (SM[i].rwnd.seqnum[0] - 1 + 16) % 16;
                                SM[i].rwnd.window_size = (SM[i].rwnd.seqnum[SM[i].rwnd.seqnumLen - 1] - SM[i].rwnd.seqnum[0] + 16) % 16 + 1;
                            }
                            else
                            {
                                SM[i].rwnd.lastseqnum = SM[i].rwnd.highestseqnum;
                                SM[i].rwnd.window_size = RECV_BUF_SIZE;
                                SM[i].rwnd.seqnumLen = RECV_BUF_SIZE;
                                printf("\033[0;34mNew rwnd: ");
                                for (int j = 0; j < SM[i].rwnd.seqnumLen; j++)
                                {
                                    SM[i].rwnd.seqnum[j] = (SM[i].rwnd.lastseqnum + 1 + j) % 16;
                                    printf("%d ", SM[i].rwnd.seqnum[j]);
                                }
                                printf("\n");
                            }

                            // store message in recvBuf
                            int available = -1;
                            for (int j = 0; j < RECV_BUF_SIZE; j++)
                            {
                                if (SM[i].recvBuf[j].free)
                                {
                                    available = j;
                                    break;
                                }
                            }
                            memset(SM[i].recvBuf[available].message, '\0', strlen(SM[i].recvBuf[available].message));
                            strcpy(SM[i].recvBuf[available].message, textData);
                            SM[i].recvBuf[available].seqnum = seqnum;
                            SM[i].recvBuf[available].free = 0;
                            printf("\033[0;34mrecBuf: ");
                            for (int j = 0; j < RECV_BUF_SIZE; j++)
                            {
                                if (!SM[i].recvBuf[j].free)
                                {
                                    printf("%d ", SM[i].recvBuf[j].seqnum);
                                }
                            }
                            printf("\n");
                            // printf("\033[0;34mRecvBuf msg len %d at %d\n", (int)strlen(textData), available);
                            // for (int i1 = 0; i1 < 10; i1++)
                            // {
                            //     printf("%c", SM[i].recvBuf[available].message[i1]);
                            // }
                            // printf("\n");

                            // check if buffer full
                            int full = 1;
                            for (int j = 0; j < RECV_BUF_SIZE; j++)
                            {
                                if (SM[i].recvBuf[j].free)
                                {
                                    full = 0;
                                    break;
                                }
                            }
                            if (full)
                            {
                                SM[i].nospace = 1;
                            }

                            // send ACK
                            char ack[10];
                            sprintf(ack, "%d\n%d\n%d", 1, SM[i].rwnd.lastseqnum, SM[i].rwnd.window_size);
                            sendto(SM[i].sockfd, ack, sizeof(ack), 0, (const struct sockaddr *)&client_addr, client_len);
                            printf("\033[0;31mACK %d sent %d %d %d\n", seqnum, 1, SM[i].rwnd.lastseqnum, SM[i].rwnd.window_size);
                            acksends++;
                        }
                        else
                        { // if message not in rwnd
                            printf("\033[0;34mMessage not in rwnd\n");
                            // send ACK
                            char ack[10];
                            sprintf(ack, "%d\n%d\n%d", 1, SM[i].rwnd.lastseqnum, SM[i].rwnd.window_size);
                            sendto(SM[i].sockfd, ack, sizeof(ack), 0, (const struct sockaddr *)&client_addr, client_len);
                            printf("\033[0;31mACK %d sent %d %d %d\n", seqnum, 1, SM[i].rwnd.lastseqnum, SM[i].rwnd.window_size);
                            acksends++;
                        }
                    }
                    else // ACK message
                    {
                        printf("\033[0;34m\t\tReceived ACK: %d %d %d\n", type, seqnum, (int)atoi(textData));
                        // if (SM[i].swnd.seqnumLen == 0)
                        // {
                        //     perror("swnd size 0");
                        //     exit(1);
                        // }
                        int pos = -1;
                        for (int j = 0; j < SM[i].swnd.seqnumLen; j++)
                        {
                            if (SM[i].swnd.seqnum[j] == seqnum)
                            {
                                pos = j;
                                break;
                            }
                        }
                        if (pos != -1) // proper ACK
                        {
                            printf("\t\tProper ACK\n");
                            SM[i].swnd.seqnumLen -= pos + 1;
                            for (int j = 0; j < SM[i].swnd.seqnumLen; j++)
                            {
                                SM[i].swnd.seqnum[j] = SM[i].swnd.seqnum[j + pos + 1];
                            }
                            SM[i].swnd.lastseqnum = seqnum;

                            // update window size
                            SM[i].swnd.window_size = seqnum + atoi(textData) - SM[i].swnd.lastseqnum;

                            // remove from buffer
                            for (int j = 0; j < SEND_BUF_SIZE; j++)
                            {
                                int present = 0;
                                // check if message in buffer is not in swnd
                                for (int k = 0; k < SM[i].swnd.seqnumLen; k++)
                                {
                                    if (SM[i].swnd.seqnum[k] == SM[i].sendBuf[j].seqnum)
                                    {
                                        present = 1;
                                        break;
                                    }
                                }
                                if (!present)
                                {
                                    memset(SM[i].sendBuf[j].message, '\0', strlen(SM[i].sendBuf[j].message));
                                    SM[i].sendBuf[j].free = 1;
                                }
                            }
                        }
                        else
                        {
                            printf("\t\tDup ACK %d %d %d\n", seqnum, atoi(textData), SM[i].swnd.lastseqnum);
                            // update window size
                            SM[i].swnd.window_size = seqnum + atoi(textData) - SM[i].swnd.lastseqnum;
                        }
                    }
                }
            }
            sig(mutid1);
        }
        else // timeout
        {
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            wait(mutid1);
            for (int i = 0; i < MAX_SOCKETS; i++)
            {
                if (SM[i].free == 0)
                {
                    // updating nospace flag
                    if (SM[i].nospace)
                    {
                        for (int j = 0; j < RECV_BUF_SIZE; j++)
                        {
                            if (SM[i].recvBuf[j].free)
                            {
                                SM[i].nospace = 0;
                                break;
                            }
                        }
                    }

                    // sending duplicate ACK
                    if (SM[i].rwnd.lastseqnum >= 0)
                    {
                        char ack[10];
                        sprintf(ack, "%d\n%d\n%d", 1, SM[i].rwnd.lastseqnum, SM[i].rwnd.window_size);
                        struct sockaddr_in client_addr;
                        client_addr.sin_family = AF_INET;
                        client_addr.sin_port = htons(SM[i].port);
                        inet_aton(SM[i].ip, &client_addr.sin_addr);
                        sendto(SM[i].sockfd, ack, sizeof(ack), 0, (const struct sockaddr *)&client_addr, sizeof(client_addr));
                        printf("dup ACK %d sent\n", SM[i].rwnd.lastseqnum);
                        acksends++;
                    }
                }
            }
            sig(mutid1);
        }
    }

    pthread_exit(NULL);
}

void *sender(void *arg)
{
    int key1, key5;
    key1 = ftok("initmsocket.c", 1);
    int shmid = shmget(key1, MAX_SOCKETS * sizeof(socketStruct), 0777);
    socketStruct *SM = (socketStruct *)shmat(shmid, 0, 0);

    key5 = ftok("initmsocket.c", 5);
    int mutid1 = semget(key5, 1, 0777);
    struct sembuf pop, vop;
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;

    while (1)
    {
        sleep(2);
        wait(mutid1);
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            // resend
            if (SM[i].free)
                continue;
            int available = -1;
            for (int j = 0; j < RECV_BUF_SIZE; j++)
            {
                if (!SM[i].sendBuf[j].free)
                {
                    available = j;
                    break;
                }
            }
            if (available == -1)
            {
                continue;
            }
            int max_time = 0;
            for (int j = 0; j < RECV_BUF_SIZE; j++)
            {
                if (SM[i].sendBuf[j].free == 0)
                {
                    max_time = ((max_time > SM[i].sendBuf[j].time) ? max_time : SM[i].sendBuf[j].time);
                }
            }
            // if curr - maxT > T, resend all of swnd
            if (time(NULL) - max_time > T)
            {
                for (int j = 0; j < SM[i].swnd.seqnumLen; j++)
                {
                    int pos = -1;
                    for (int k = 0; k < SEND_BUF_SIZE; k++)
                    {
                        if (SM[i].sendBuf[k].seqnum == SM[i].swnd.seqnum[j])
                        {
                            pos = k;
                            break;
                        }
                    }

                    char msg[1031];
                    sprintf(msg, "%d\n%d\n%s", 0, SM[i].swnd.seqnum[j], SM[i].sendBuf[pos].message);

                    struct sockaddr_in client_addr;
                    client_addr.sin_family = AF_INET;
                    client_addr.sin_port = htons(SM[i].port);
                    inet_aton(SM[i].ip, &client_addr.sin_addr);
                    sendto(SM[i].sockfd, (const char *)msg, sizeof(msg), 0, (const struct sockaddr *)&client_addr, sizeof(client_addr));
                    char display[1031];
                    strcpy(display, SM[i].sendBuf[pos].message);
                    display[10] = '\0';
                    printf("\033[0;31m\t\tResend send MSG: %d %d %d %s\n", 0, SM[i].swnd.seqnum[j], (int)strlen(SM[i].sendBuf[pos].message), display);
                    SM[i].sendBuf[j].time = time(NULL);
                    msgsends++;
                }
            }

            // send new
            for (int j = 0; j < SEND_BUF_SIZE; j++)
            {
                if (SM[i].sendBuf[j].free)
                    continue;
                // check if message not in swnd
                int present = 0;
                for (int k = 0; k < SM[i].swnd.seqnumLen; k++)
                {
                    if (SM[i].sendBuf[j].seqnum == SM[i].swnd.seqnum[k])
                    {
                        present = 1;
                        break;
                    }
                }
                if (present == 0)
                {
                    char msg[1031];
                    sprintf(msg, "%d\n%d\n%s", 0, SM[i].sendBuf[j].seqnum, SM[i].sendBuf[j].message);
                    struct sockaddr_in client_addr;
                    client_addr.sin_family = AF_INET;
                    client_addr.sin_port = htons(SM[i].port);
                    inet_aton(SM[i].ip, &client_addr.sin_addr);
                    sendto(SM[i].sockfd, (const char *)msg, sizeof(msg), 0, (const struct sockaddr *)&client_addr, sizeof(client_addr));
                    printf("\033[0;31m\t\tSend MSG: %d %d %d\n", 0, SM[i].sendBuf[j].seqnum, (int)strlen(SM[i].sendBuf[j].message));
                    SM[i].sendBuf[j].time = time(NULL);
                    msgsends++;

                    // update swnd
                    SM[i].swnd.seqnum[SM[i].swnd.seqnumLen] = SM[i].sendBuf[j].seqnum;
                    SM[i].swnd.seqnumLen++;
                }
            }
            printf("\033[0;31m\t\tSendBUF: ");
            for (int j = 0; j < SEND_BUF_SIZE; j++)
            {
                if (!SM[i].sendBuf[j].free)
                {
                    printf("%d ", SM[i].sendBuf[j].seqnum);
                }
            }
            printf("\n\033[0;31m\t\tswnd: ");
            for (int j = 0; j < SM[i].swnd.seqnumLen; j++)
            {
                printf("%d ", SM[i].swnd.seqnum[j]);
            }
            printf("\n");
        }
        sig(mutid1);
    }
    pthread_exit(NULL);
}

void *garbage(void *arg)
{
    int key1, key5;
    key1 = ftok("initmsocket.c", 1);
    int shmid = shmget(key1, MAX_SOCKETS * sizeof(socketStruct), 0777);
    socketStruct *SM = (socketStruct *)shmat(shmid, 0, 0);

    key5 = ftok("initmsocket.c", 5);
    int mutid1 = semget(key5, 1, 0777);
    struct sembuf pop, vop;
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;

    while (1)
    {
        printf("\033[0;32m\tGarbage collector running...\n");
        sleep(5);
        wait(mutid1);
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            int recBufEmpty = 1;
            int sendBufEmpty = 1;
            for (int j = 0; j < RECV_BUF_SIZE; j++)
            {
                if (!SM[i].recvBuf[j].free)
                {
                    recBufEmpty = 0;
                    break;
                }
            }
            for (int j = 0; j < SEND_BUF_SIZE; j++)
            {
                if (!SM[i].sendBuf[j].free)
                {
                    sendBufEmpty = 0;
                    break;
                }
            }
            if (SM[i].free == 0)
            {
                printf("\033[0;32m\tChecking socket %d\n", SM[i].sockfd);
                if (!recBufEmpty || !sendBufEmpty)
                {
                    continue;
                }
                if (kill(SM[i].pid, 0) < 0 && errno == ESRCH)
                {
                    printf("\033[0;32m\tClosing socket %d\n", SM[i].sockfd);
                    close(SM[i].sockfd);
                    SM[i].free = 1;
                }
                else if (kill(SM[i].pid, 0) < 0)
                {
                    perror("kill");
                    sig(mutid1);
                    exit(1);
                }
            }
        }
        sig(mutid1);
    }
}

void terminate()
{
    printf("\033[0;31m");
    printf("Total messages sent: %d\n", msgsends);
    printf("Total ACKs sent: %d\n", acksends);
    printf("\033[0m");
    exit(0);
}

int main()
{
    signal(SIGINT, terminate);

    int shmid1, shmid2;
    socketStruct *SM;
    socketInfo *SOCK_INFO;
    key_t key1, key2;

    key1 = ftok("initmsocket.c", 1);
    key2 = ftok("initmsocket.c", 2);
    shmid1 = shmget(key1, MAX_SOCKETS * sizeof(socketStruct), IPC_CREAT | 0777);
    shmid2 = shmget(key2, sizeof(socketInfo), IPC_CREAT | 0777);
    if (shmid1 == -1)
    {
        perror("shmget1");
        exit(1);
    }
    if (shmid2 == -1)
    {
        perror("shmget2");
        exit(1);
    }
    SM = (socketStruct *)shmat(shmid1, 0, 0);
    SOCK_INFO = (socketInfo *)shmat(shmid2, 0, 0);
    if (SM == (void *)-1)
    {
        perror("shmat1");
        exit(1);
    }
    if (SOCK_INFO == (void *)-1)
    {
        perror("shmat2");
        exit(1);
    }
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        SM[i].free = 1;
    }
    SOCK_INFO->sockfd = 0;
    SOCK_INFO->port = 0;
    memset(SOCK_INFO->ip, '\0', 16);
    SOCK_INFO->error = 0;

    int semid1, semid2, mutid1, mutid2;
    key_t key3, key4, key5, key6;
    struct sembuf pop, vop;

    key3 = ftok("initmsocket.c", 3);
    key4 = ftok("initmsocket.c", 4);
    key5 = ftok("initmsocket.c", 5);
    key6 = ftok("initmsocket.c", 6);
    semid1 = semget(key3, 1, IPC_CREAT | 0777);
    semid2 = semget(key4, 1, IPC_CREAT | 0777);
    mutid1 = semget(key5, 1, IPC_CREAT | 0777);
    mutid2 = semget(key6, 1, IPC_CREAT | 0777);
    semctl(semid1, 0, SETVAL, 0);
    semctl(semid2, 0, SETVAL, 0);
    semctl(mutid1, 0, SETVAL, 1);
    semctl(mutid2, 0, SETVAL, 1);
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;

    pthread_t thread1, thread2, thread3;
    pthread_create(&thread1, NULL, receiver, NULL);
    pthread_create(&thread2, NULL, sender, NULL);
    pthread_create(&thread3, NULL, garbage, NULL);

    // everything else

    while (1)
    {
        wait(semid1);
        wait(mutid1);
        if (SOCK_INFO->sockfd == 0 && SOCK_INFO->port == 0 && strlen(SOCK_INFO->ip) == 0)
        { // m_socket() call
            int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd == -1)
                SOCK_INFO->error = errno;
            SOCK_INFO->sockfd = sockfd;
            sig(mutid1);
            sig(semid2);
        }
        else
        { // m_bind() call
            struct sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(SOCK_INFO->port);
            inet_aton(SOCK_INFO->ip, &server_addr.sin_addr);

            if (bind(SOCK_INFO->sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
            {
                SOCK_INFO->sockfd = -1;
                SOCK_INFO->error = errno;
            }
            sig(mutid1);
            sig(semid2);
        }
    }

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    return 0;
}
