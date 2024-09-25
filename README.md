# MTP Socket Implementation

This repository contains an implementation of the MTP (Message Transfer Protocol) socket over UDP. It uses Selective Repeat ARQ for reliable message transmission. Below is a detailed explanation of the code structure and its working.

## Files and Their Purpose

### 1. **msocket.h**
Contains definitions for all structs, macros, and functions used in the implementation.

#### Structs and Their Members

- **Window**
  - `window_size`: Maximum number of extra messages that can be sent without receiving ACK (for sender window) or expected to be received (for receiver window).
  - `seqnum`: Array of sequence numbers of messages sent but not yet acknowledged (for sender window) or expected to be received next (for receiver window).
  - `seqnumLen`: Number of messages in the window.
  - `lastseqnum`: Sequence number of the last in-order message acknowledged (for sender) or received (for receiver).
  - `highestseqnum`: Highest sequence number message received so far in a cyclic manner.

- **bufItem**
  - `message`: Char array storing the message content.
  - `time`: Time when the message was last sent.
  - `seq_num`: Sequence number of the message (0 to 15).
  - `free`: Boolean indicating whether this buffer slot is free (`1` if free, `0` otherwise).

- **socketStruct**
  - `free`: Boolean indicating whether this socket table slot is free (`1` if free, `0` otherwise).
  - `pid`: Process ID of the process that created the socket.
  - `sockfd`: UDP socket ID.
  - `ip`: IP of the connected socket.
  - `port`: Port number of the connected socket.
  - `sendBuf`: Sender side buffer (array of `bufItem` of size 10).
  - `recvBuf`: Receiver side buffer (array of `bufItem` of size 5).
  - `swnd`: Sender side window.
  - `rwnd`: Receiver side window.
  - `lastseqnum`: Sequence number of the last in-order message extracted from the receiver buffer.
  - `currseqnum`: Sequence number of the last message put in the sender window.
  - `nospace`: Boolean indicating whether the receive buffer is full (`1` if full, `0` otherwise).

- **socketInfo**
  - `sockfd`
  - `ip`
  - `port`
  - `error`

#### Macros

- `SOCK_MTP`: Custom socket type for MTP sockets.
- `MAX_SOCKETS`: Maximum number of active MTP sockets.
- `SEND_BUF_SIZE`: Sender buffer size.
- `RECV_BUF_SIZE`: Receiver buffer size.
- `T`: Message timeout period.
- `DROP_PROBABILITY`: Probability that a received message will be dropped.

### 2. **msocket.c**
Contains implementations of the MTP socket function calls and the drop probability function.

#### Functions

- `int m_socket(int domain, int type, int protocol, pid_t pid)`: Initializes socket entries in the shared memory table.
- `int m_bind(int sockid, char *source_IP, int source_port, char *dest_IP, int dest_port)`: Binds socket IP and port information in the shared memory table.
- `int m_recvfrom(int m_socket, char *buf, int len, int flags)`: Receives messages in the appropriate buffer slot.
- `int m_sendto(int sockid, char *buf, int len, int flags, char *dest_IP, int dest_port)`: Sends messages from the sender buffer.
- `int dropMessage(float p)`: Generates a random number and drops messages based on the drop probability.

### 3. **initmsocket.c**
Contains implementations of the receiver, sender, and garbage threads.

#### Functions

- **main**: Initializes shared memory and semaphores, and creates threads for receiver, sender, and garbage collector.
- **receiver**: Monitors all sockets for incoming messages and processes them.
- **sender**: Periodically checks for messages that need to be resent or sent.
- **garbage**: Cleans up socket entries in case of process termination.
- **terminate**: Custom signal handler for collecting stats on messages and ACKs sent.

### 4. **user1.c**
Sends data in chunks of 1024 characters. Retries sending if the send buffer is full and signals the end of the file with a `.` message.

### 5. **user2.c**
Receives data in chunks of 1024 characters. Retries if messages are not in the buffer and terminates upon receiving a `.` message.

## Results

| Drop Probability | Number of Messages Sent | Average Rate | Number of ACKs Sent |
|------------------|-------------------------|--------------|---------------------|
| 0.05             | 38                      | 1.73         | 32                  |
| 0.10             | 45                      | 2.05         | 35                  |
| 0.15             | 44                      | 2.00         | 33                  |
| 0.20             | 42                      | 1.91         | 26                  |
| 0.25             | 49                      | 2.23         | 31                  |
| 0.30             | 55                      | 2.50         | 42                  |
| 0.35             | 58                      | 2.64         | 39                  |
| 0.40             | 65                      | 2.95         | 44                  |
| 0.45             | 77                      | 3.50         | 47                  |
| 0.50             | 95                      | 4.32         | 57                  |
