#include "unp.h"
#include "unpifiplus.h"

#define MSG_SYN 1
#define MSG_PROBE 2
#define MSG_ACK 3
#define MSG_EOF 4
#define MSG_DATA 5

struct packet_t{
        int msgType;
        uint32_t seq;
	uint32_t ws;
	uint32_t ts;
	char msg[512];
};

struct connection{
	pid_t pid;
	int sockfd;
	char clientIp[INET_ADDRSTRLEN];
	int clientPort;
	char serverIp[INET_ADDRSTRLEN];
	int serverPort;
	int seq;
	char fileName[25];
	struct connection* next;
  	struct connection* prev;
};


struct connection* connections;

int udp_recv(int sockfd, struct packet_t* packet, struct sockaddr* sockAddr);
int udp_send(int sockfd, struct packet_t* packet, struct sockaddr* sockAddr);

//queue related variables and functions

int head;
int tail;
int queueCapacity, queueSize;
struct packet_t* queue;
pthread_mutex_t queMutex;


int enQueue(struct packet_t* packet);
int peekQueueTail(struct packet_t* packet);
int peekQueueHead(struct packet_t* packet);
int deQueue(struct packet_t* packet);
