#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "msocket.h"
#include <errno.h>

#define wait(s) semop(s, &pop, 1)
#define signal(s) semop(s, &vop, 1)

int m_socket(int domain, int type, int protocol, pid_t pid)
{
    if (type != SOCK_MTP)
    {
        errno = EINVAL;
        return -1;
    }
    socketStruct *SM;
    socketInfo *SOCK_INFO;
    int semid1, semid2, mutid1, mutid2;
    struct sembuf pop, vop;

    key_t key1 = ftok("initmsocket.c", 1);
    key_t key2 = ftok("initmsocket.c", 2);
    int shmid1 = shmget(key1, MAX_SOCKETS * sizeof(socketStruct), 0666);
    int shmid2 = shmget(key2, sizeof(socketInfo), 0666);
    if (shmid1 < 0 || shmid2 < 0)
    {
        errno = ECONNREFUSED;
        return -1;
    }
    SM = (socketStruct *)shmat(shmid1, NULL, 0);
    SOCK_INFO = (socketInfo *)shmat(shmid2, NULL, 0);

    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;
    key_t key3 = ftok("initmsocket.c", 3);
    key_t key4 = ftok("initmsocket.c", 4);
    key_t key5 = ftok("initmsocket.c", 5);
    key_t key6 = ftok("initmsocket.c", 6);

    semid1 = semget(key3, 1, 0666);
    semid2 = semget(key4, 1, 0666);
    mutid1 = semget(key5, 1, 0666);
    mutid2 = semget(key6, 1, 0666);
    if (semid1 < 0 || semid2 < 0 || mutid1 < 0 || mutid2 < 0)
    {
        errno = ECONNREFUSED;
        return -1;
    }

    wait(mutid2);

    int i, available_index = -1;
    wait(mutid1);
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (SM[i].free == 1)
        {
            SM[i].free = 0;
            SM[i].pid = pid;
            SM[i].currseqnum = 0;
            SM[i].lastseqnum = -1;
            available_index = i;
            SM[i].swnd.seqnumLen = 0;
            SM[i].rwnd.seqnumLen = RECV_BUF_SIZE;
            SM[i].swnd.lastseqnum = -1;
            SM[i].rwnd.lastseqnum = -1;
            SM[i].rwnd.highestseqnum = -1;
            SM[i].rwnd.window_size = RECV_BUF_SIZE;
            SM[i].swnd.window_size = RECV_BUF_SIZE;
            for (int j = 0; j < RECV_BUF_SIZE; j++)
            {
                SM[i].recvBuf[j].free = 1;
                SM[i].recvBuf[j].seqnum = -1;
                SM[i].rwnd.seqnum[j] = j;
            }
            for (int j = 0; j < SEND_BUF_SIZE; j++)
            {
                SM[i].sendBuf[j].free = 1;
                SM[i].sendBuf[j].seqnum = -1;
            }

            break;
        }
    }
    // signal(mutid1);
    if (available_index == -1)
    {
        errno = EMFILE;
        signal(mutid1);
        return -1;
    }
    // wait(mutid1);
    SOCK_INFO->sockfd = 0;
    SOCK_INFO->port = 0;
    SOCK_INFO->ip[0] = '\0';
    // memset(SOCK_INFO->ip, '\0', strlen(SOCK_INFO->ip));
    SOCK_INFO->error = 0;
    signal(mutid1);

    signal(semid1);
    wait(semid2);

    wait(mutid1);
    if (SOCK_INFO->sockfd == -1)
    {
        errno = SOCK_INFO->error;
        SOCK_INFO->sockfd = 0;
        SOCK_INFO->ip[0] = '\0';
        SOCK_INFO->port = 0;
        SOCK_INFO->error = 0;
        signal(mutid1);
        return errno;
    }
    SM[available_index].sockfd = SOCK_INFO->sockfd;
    SOCK_INFO->sockfd = 0;
    SOCK_INFO->ip[0] = '\0';
    SOCK_INFO->port = 0;
    SOCK_INFO->error = 0;
    signal(mutid1);

    signal(mutid2);
    printf("Socket at %d\n", SM[available_index].sockfd);
    return SM[available_index].sockfd;
}

int m_bind(int sockid, char *source_IP, int source_port, char *dest_IP, int dest_port)
{
    socketStruct *SM;
    socketInfo *SOCK_INFO;
    int semid1, semid2, mutid1, mutid2;
    struct sembuf pop, vop;

    key_t key1 = ftok("initmsocket.c", 1);
    key_t key2 = ftok("initmsocket.c", 2);
    int shmid1 = shmget(key1, MAX_SOCKETS * sizeof(socketStruct), 0666);
    int shmid2 = shmget(key2, sizeof(socketInfo), 0666);
    if (shmid1 < 0 || shmid2 < 0)
    {
        errno = ECONNREFUSED;
        return -1;
    }
    SM = (socketStruct *)shmat(shmid1, NULL, 0);
    SOCK_INFO = (socketInfo *)shmat(shmid2, NULL, 0);

    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;
    key_t key3 = ftok("initmsocket.c", 3);
    key_t key4 = ftok("initmsocket.c", 4);
    key_t key5 = ftok("initmsocket.c", 5);
    key_t key6 = ftok("initmsocket.c", 6);

    semid1 = semget(key3, 1, 0666);
    semid2 = semget(key4, 1, 0666);
    mutid1 = semget(key5, 1, 0666);
    mutid2 = semget(key6, 1, 0666);
    if (semid1 < 0 || semid2 < 0 || mutid1 < 0 || mutid2 < 0)
    {
        errno = ECONNREFUSED;
        return -1;
    }

    wait(mutid2);

    int i;
    wait(mutid1);
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (SM[i].sockfd == sockid)
        {
            break;
        }
    }
    if (i == MAX_SOCKETS)
    {
        errno = EBADF;
        signal(mutid1);
        return -1;
    }

    // wait(mutid1);
    SOCK_INFO->sockfd = sockid;
    memset(SOCK_INFO->ip, '\0', 16);
    strcpy(SOCK_INFO->ip, source_IP);
    SOCK_INFO->port = source_port;
    signal(mutid1);

    signal(semid1);
    wait(semid2);

    wait(mutid1);
    if (SOCK_INFO->sockfd == -1)
    {
        errno = SOCK_INFO->error;
        SOCK_INFO->sockfd = 0;
        memset(SOCK_INFO->ip, '\0', 16);
        SOCK_INFO->port = 0;
        SOCK_INFO->error = 0;
        signal(mutid1);
        return errno;
    }
    SOCK_INFO->sockfd = 0;
    SOCK_INFO->port = 0;
    SOCK_INFO->error = 0;
    memset(SOCK_INFO->ip, '\0', 16);
    memset(SM[i].ip, '\0', 16);
    strcpy(SM[i].ip, dest_IP);
    SM[i].port = dest_port;
    signal(mutid1);

    signal(mutid2);
    return 0;
}

int m_recvfrom(int m_socket, char *buf, int len, int flags)
{
    socketStruct *SM;

    int mutid1;
    struct sembuf pop, vop;

    key_t key1 = ftok("initmsocket.c", 1);
    key_t key5 = ftok("initmsocket.c", 5);
    int shmid1 = shmget(key1, MAX_SOCKETS * sizeof(socketStruct), 0666);
    if (shmid1 < 0)
    {
        errno = ECONNREFUSED;
        return -1;
    }
    SM = (socketStruct *)shmat(shmid1, NULL, 0);

    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;
    mutid1 = semget(key5, 1, 0666);
    if (mutid1 < 0)
    {
        errno = ECONNREFUSED;
        return -1;
    }
    int i;
    wait(mutid1);
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (SM[i].sockfd == m_socket)
        {
            break;
        }
    }
    if (i == MAX_SOCKETS)
    {
        errno = EBADF;
        signal(mutid1);
        return -1;
    }
    // wait(mutid1);
    int lookfor = (SM[i].lastseqnum + 1) % 16;
    int available = -1;
    int buflen = 0;
    for (int j = 0; j < RECV_BUF_SIZE; j++)
    {
        if (!SM[i].recvBuf[j].free)
        {
            buflen++;
        }
    }
    for (int j = 0; j < RECV_BUF_SIZE; j++)
    {
        if (!SM[i].recvBuf[j].free && (SM[i].recvBuf[j].seqnum == lookfor))
        {
            available = j;
            break;
        }
    }
    printf("buf: ");
    for (int j = 0; j < RECV_BUF_SIZE; j++)
    {
        if (!SM[i].recvBuf[j].free)
            printf("%d ", SM[i].recvBuf[j].seqnum);
    }
    printf(", ");
    printf("lookfor: %d, m_recvfrom: %d,, msg_found: %d buf: ", lookfor, available, SM[i].recvBuf[available].seqnum);
    for (int j = 0; j < RECV_BUF_SIZE; j++)
    {
        if (!SM[i].recvBuf[j].free)
            printf("%d ", SM[i].recvBuf[j].seqnum);
    }
    if (available == -1)
    {
        errno = EAGAIN;
        signal(mutid1);
        return -1;
    }
    memset(buf, '\0', strlen(buf));
    strcpy(buf, SM[i].recvBuf[available].message);
    SM[i].recvBuf[available].free = 1;
    SM[i].lastseqnum = (SM[i].lastseqnum + 1) % 16;
    len = strlen(buf);
    signal(mutid1);
    return len;
}

int m_sendto(int sockid, char *buf, int len, int flags, char *dest_IP, int dest_port)
{
    socketStruct *SM;

    key_t key1 = ftok("initmsocket.c", 1);
    int shmid = shmget(key1, MAX_SOCKETS * sizeof(socketStruct), 0666);
    if (shmid < 0)
    {
        perror("shmget");
        return -1;
    }
    SM = (socketStruct *)shmat(shmid, NULL, 0);

    key_t key5 = ftok("initmsocket.c", 5);
    int mutid1 = semget(key5, 1, 0666);
    struct sembuf pop, vop;
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;
    wait(mutid1);
    int i;
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (SM[i].sockfd == sockid)
        {
            break;
        }
    }
    if (strcmp(SM[i].ip, dest_IP) || SM[i].port != dest_port)
    {
        errno = ENOTCONN;
        signal(mutid1);
        return -1;
    }
    int available = -1;
    int full = 1;
    for (int j = 0; j < RECV_BUF_SIZE; j++)
    {
        if (SM[i].sendBuf[j].free)
        {
            available = j;
            full = 0;
            break;
        }
    }
    if (full)
    {
        errno = ENOBUFS;
        signal(mutid1);
        return -1;
    }
    char display[1025];
    strcpy(display, buf);
    display[10] = '\0';
    printf("m_sendto: %s\n", display);
    memset(SM[i].sendBuf[available].message, '\0', strlen(SM[i].sendBuf[available].message));
    strcpy(SM[i].sendBuf[available].message, buf);
    SM[i].sendBuf[available].seqnum = SM[i].currseqnum;
    SM[i].currseqnum++;
    SM[i].currseqnum %= 16;
    SM[i].sendBuf[available].free = 0;

    signal(mutid1);
    return 0;
}

int m_close(int m_socket)
{
    socketStruct *SM;
    key_t key1 = ftok("initmsocket.c", 1);
    int shmid = shmget(key1, MAX_SOCKETS * sizeof(socketStruct), 0666);
    if (shmid < 0)
    {
        perror("shmget");
        return -1;
    }
    SM = (socketStruct *)shmat(shmid, NULL, 0);

    key_t key5 = ftok("initmsocket.c", 5);
    int mutid1 = semget(key5, 1, 0666);
    struct sembuf pop, vop;
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;

    wait(mutid1);
    int i;
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (SM[i].sockfd == m_socket)
        {
            break;
        }
    }
    if (i == MAX_SOCKETS)
    {
        errno = EBADF;
        signal(mutid1);
        return -1;
    }
    SM[i].free = 1;
    signal(mutid1);
    return 0;
}

int dropMessage(float p)
{
    float random_num = (float)rand() / RAND_MAX;

    if (random_num < p)
    {
        return 1; // Message dropped
    }
    else
    {
        return 0; // Message not dropped
    }
}
