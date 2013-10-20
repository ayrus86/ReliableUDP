#include "unp.h"

#define MSG_SYN 1
#define MSG_SYNACK 2
#define MSG_ACK 3
#define MSG_EOF 4
#define MSG_DATA 5

struct packet_t{
        int msgType;
        uint32_t seq;
	uint32_t ts;
	char msg[512];
};

struct connection{
	int pid;
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
