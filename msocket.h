#ifndef MSOCKET_H
#define MSOCKET_H

#define SOCK_MTP 0
#define MAX_SOCKETS 25

#define SEND_BUF_SIZE 10
#define RECV_BUF_SIZE 5

#define T 5
#define DROP_PROBABILITY 0.1

typedef struct window
{
    int window_size;
    int seqnum[SEND_BUF_SIZE];
    int seqnumLen;
    int lastseqnum;
    int highestseqnum;
} window;

typedef struct bufItem
{
    char message[1025];
    time_t time;
    int seqnum;
    int free;
} bufItem;

typedef struct socketStruct
{
    int free;
    int pid;
    int sockfd;
    char ip[16];
    int port;
    bufItem sendBuf[SEND_BUF_SIZE];
    bufItem recvBuf[RECV_BUF_SIZE];
    window swnd;
    window rwnd;
    int lastseqnum;
    int currseqnum;
    int nospace;
} socketStruct;

typedef struct socketInfo
{
    int sockfd;
    char ip[16];
    int port;
    int error;
} socketInfo;

int m_socket(int domain, int type, int protocol);

int m_bind(int m_socket, char *source_ip, int source_port, char *dest_ip, int dest_port);

int m_sendto(int m_socket, char *, int, int, char *, int);

int m_recvfrom(int m_socket, char *buf, int len, int flags);

int m_close(int m_socket);

int dropMessage(float pr);

#endif
