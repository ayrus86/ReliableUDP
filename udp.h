#include "unp.h"

#define MSG_PORT 1
#define MSG_FILE 2
#define MSG_ACK 3
#define MSG_CLOSE 4

struct hdr{
        int msgType;
        uint32_t seq;
        uint32_t ts;
};

struct connection{
	int pid;
	int sockfd;
	char clientIp[INET_ADDRSTRLEN];
	int clientPort;
	char serverIp[INET_ADDRSTRLEN];
	int serverPort;
	char fileName[25];
	struct hdr header;
	struct connection* next;
  	struct connection* prev;
};


//struct hdr sendhdr, recvhdr;
struct connection* connections;
static struct msghdr msgsend, msgrecv; 

