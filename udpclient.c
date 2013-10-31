#include <stdio.h>
#include <setjmp.h>
#include <math.h>
#include "unp.h"
#include "udp.h"
#include "unpifiplus.h"
#include "unprtt.h"

//#define DEBUG 1

struct client_config_t{
	char serverIp[INET_ADDRSTRLEN];
	int serverPort;
	char fileName[25];
	int winSize;
	int seed;
	double lossProb;
	int mean;
	int islocal;
};

struct client_config_t* clientConfig;

int decide_if(struct bind_info* interfaces, int numIfs, char *server) {
  unsigned char mask[32];
  unsigned char cli[32];
  unsigned char serv[32];
  char buff[MAXLINE];
  char pmask[16];
  char pcli[16];
  char pserv[16];
  int x = 0;
  int i = 0;

  //check if the client and server are on the same host
  for(i; i < numIfs; i++){
    char* iptmp = inet_ntop(AF_INET, &(((struct sockaddr_in*)interfaces[i].bind_ipaddr)->sin_addr), buff, sizeof(buff));

    if(strcmp(iptmp, server) == 0) {
      printf("Both server and client are on same host. Using loopback address\n");
      clientConfig->islocal = 1;
	 return 0;
     
    }
  }


  //otherwise, check if they're local
  strcpy(pserv, server);
  ip_bit_array(pserv, serv);


  for(x; x < numIfs; x++){
    strcpy(pmask,inet_ntop(AF_INET, &(((struct sockaddr_in*)interfaces[x].bind_ntmaddr)->sin_addr), buff, sizeof(buff)));
    strcpy(pcli,inet_ntop(AF_INET, &(((struct sockaddr_in*)interfaces[x].bind_ipaddr)->sin_addr), buff, sizeof(buff)));
    
    ip_bit_array(pmask, mask);
    
    ip_bit_array(pcli, cli);

    int y = 0;
    for(y; y < 32; y++){
      if(mask[y] == 0){
	return x;
      }
      else {
	if(cli[y] != serv[y]){
	  //not matching when they should
	  break;
	}
      }
    }
  }

  //if we're here we know it's not itself or a local address
  return (numIfs -1);

}

int ip_bit_array(char* ip, unsigned char bits[32]) {
  unsigned char to_convert[4];
  int x = 0;
  int status;

  status = inet_pton(AF_INET, ip, to_convert);

  for(x; x < 4; x++){
    int y = 0;
    int z = 7;
    for(y; y < 8; y++){
      bits[(x*8)+z] = (to_convert[x] >> y) & 1;
      z--;
    }
    
  }
  
}

void readConfig()
{
        FILE *fp;
        char line[80];
        fp = fopen("client.in", "r");
        if (fp == NULL)
        {
                printf("Can't open client.in file\n");
                exit(1);
        }
        
	bzero(line, 80);
	fgets(line, 80, fp);
	strtok(line, "\n");
	strcpy(clientConfig->serverIp, line);
	
	bzero(line, 80);
	clientConfig->serverPort = atoi(fgets(line, 80, fp));
	
	bzero(line, 80);
	fgets(line, 80, fp);
	strtok(line, "\n");
	strcpy(clientConfig->fileName, line);

	bzero(line, 80);
	clientConfig->winSize = atoi(fgets(line, 80, fp));
	
	bzero(line, 80);
	clientConfig->seed = atoi(fgets(line, 80, fp));

	bzero(line, 80);
	clientConfig->lossProb = atof(fgets(line, 80, fp));

	bzero(line, 80);
	clientConfig->mean = atoi(fgets(line, 80, fp));

	/*
	*serverIp = (char *) malloc(INET_ADDRSTRLEN);
	strcpy(*serverIp, line);
        memset(line, 0, 80);
        *port = atoi(fgets(line, 80, fp));*/
        
	fclose(fp);
}

void getInterfaces(struct bind_info** addrInterfaces, int* numInterfaces)
{
        struct ifi_info *ifi, *ifihead;
        int n;
        
        for (ifihead = ifi = get_ifi_info_plus(AF_INET, 1), n=0; ifi != NULL; ifi = ifi->ifi_next, n++)
        {
                
        }

        struct bind_info* interfaces = malloc(n * sizeof(struct bind_info));
        *numInterfaces = n; 
        
        for (ifi = ifihead, n = 0; ifi != NULL; ifi = ifi->ifi_next, n++)
        {
                interfaces[n].bind_ipaddr = (struct sockaddr*) malloc(sizeof(struct sockaddr));
                interfaces[n].bind_ntmaddr = (struct sockaddr*) malloc(sizeof(struct sockaddr));
                interfaces[n].bind_subaddr = (struct in_addr*) malloc(sizeof(struct in_addr));
        
                memcpy(interfaces[n].bind_ipaddr, ifi->ifi_addr, sizeof(struct sockaddr));
                memcpy(interfaces[n].bind_ntmaddr, ifi->ifi_ntmaddr, sizeof(struct sockaddr));
		interfaces[n].bind_subaddr->s_addr = ((struct sockaddr_in*)ifi->ifi_ntmaddr)->sin_addr.s_addr & ((struct sockaddr_in*)ifi->ifi_addr)->sin_addr.s_addr;
        }
        *addrInterfaces = interfaces;
}

int printQueue()
{
	int ret = 0;
	struct packet_t packet;
	while(1)
	{
		float val = log(drand48());
		sleep( -1 * clientConfig->mean * val);
		if(deQueue(&packet)!=-1)
		{
			if(packet.msgType == MSG_EOF)
			{
				printf("Read EOF. Seq:%d\n", packet.seq);
				ret = 1;
				pthread_exit(&ret);
			}
			printf("Read Seq:%d Data:%s", packet.seq, packet.msg);
			bzero(&packet, sizeof(packet));
		}
		else
		{
			//printf("queueCapacity:%d head:%d tail:%d tail->seq:%d head->seq:%d\n", queueCapacity, head, tail, queue[tail].seq, queue[head].seq);
		}
	}
}

int requestRebind(struct connection* conn)
{
	//we should create a MSG_SYN request and send it to server on listing port
	//we should use timer to resend the msg till we get a rebound port
	
	struct packet_t* packet = (struct packet_t*) malloc(sizeof(struct packet_t));
	bzero(packet, sizeof(struct packet_t));
	
	packet->seq = conn->seq;
	packet->msgType = MSG_SYN;
	packet->ws = queueCapacity;
	strcpy(packet->msg, conn->fileName);
	
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
	
	fd_set  rset;
        FD_ZERO(&rset);
        for ( ; ; )
        {
                udp_send(conn->sockfd, packet, NULL);
                FD_SET(conn->sockfd, &rset);
                if (select(conn->sockfd+1, &rset, NULL, NULL, &timeout) < 0)
                {
                        if (errno == EINTR)
                                continue;
                        else
                        {
                                printf("error requestRebind:select() errno:%d\n", errno);
                                return -1;
                        }
                }
                else
                {       
                        if (FD_ISSET(conn->sockfd, &rset))
                        {
				struct packet_t* recvPacket = (struct packet_t*)malloc(sizeof(struct packet_t));
				struct sockaddr_in sockAddr;
                                if(udp_recv(conn->sockfd, recvPacket, (SA*) &sockAddr) == 1)
                                {
                                        if(recvPacket->msgType == MSG_ACK && recvPacket->seq == conn->seq)
					{
						printf("Received new binding port. Rebinding to port:%s.\n", recvPacket->msg);
        					sockAddr.sin_port = htons(atoi(recvPacket->msg));
        					if(connect(conn->sockfd,(SA *)&sockAddr, sizeof(sockAddr)) == -1)
        					{       
                					printf("error requestRebind:connect() errno:%d\n", errno);
                					return -1;
        					}
						free(recvPacket);
						goto sendack;						
					}  
                                }
                        }   
                        else
                                printf("requestRebind: timed out. resending.\n");
		}
        }
        return -1;


sendack:
	++conn->seq;
	packet->seq = conn->seq;
	packet->ws = queueCapacity;
	packet->msgType = MSG_ACK;
	memcpy(&queue[head], packet, sizeof(struct packet_t));
	free(packet);
	return 1;
}

int recvFile(struct connection* conn)
{
	pthread_t tid;
	pthread_create(&tid, NULL, (void*)&printQueue, NULL);

	struct packet_t* packet = (struct packet_t*) malloc(sizeof(struct packet_t));
	bzero(packet, sizeof(struct packet_t));
	struct packet_t* recvPacket;
	
	packet->seq = conn->seq;
	packet->ws = queueCapacity;
        packet->msgType = MSG_ACK;
	

	int eof = 0;
	struct timeval timeout;
	fd_set  rset;
        FD_ZERO(&rset);
	srand(clientConfig->seed);
        printf("Port rebound successful. Sending ACK to finish 3-Way handshake. Next Seq:%d WS:%d\n", packet->seq, packet->ws);
 	udp_send(conn->sockfd, packet, NULL);

	for(;;)
        {

sendagain:
	    	timeout.tv_sec = 5;
                FD_SET(conn->sockfd, &rset);
                if (select(conn->sockfd+1, &rset, NULL, NULL, &timeout) < 0)
                {
			if (errno == EINTR)
                        	continue;
                        else
                        {
                                printf("error recvFile:select() errno:%s\n", strerror(errno));
                                return -1;
                        }
                }
                else
                {
			if (FD_ISSET(conn->sockfd, &rset))
                        {
			        recvPacket = (struct packet_t*)malloc(sizeof(struct packet_t));
                                struct sockaddr_in sockAddr;
				if(udp_recv(conn->sockfd, recvPacket, (SA*) &sockAddr) == 1)
                                {
					//printf("recv seq:%d. queue[head].seq=%d\n", recvPacket->seq, queue[head].seq);
					if(recvPacket->msgType == MSG_PROBE)
					{
						packet->ws = queueCapacity;
						udp_send(conn->sockfd, packet, NULL);
						free(recvPacket);
					}	
					else if(recvPacket->msgType == MSG_DATA && recvPacket->seq >= packet->seq)
                                        {
						float randProb = drand48();
						if(clientConfig->lossProb > randProb)
                                        	{
                                                	printf("Dropping Packet Seq:%d\n", recvPacket->seq);
                                                	goto sendagain;
                                        	}
						
						if(enQueue(recvPacket) != -1)
						{
							bzero(packet, sizeof(struct packet_t));
							packet->seq = queue[head].seq+1;
							packet->ws = queueCapacity;
							packet->ts = recvPacket->ts;
							packet->msgType = MSG_ACK;
							if(drand48() < clientConfig->lossProb)
							{
								printf("Dropping ACK. Seq:%d\n", packet->seq);
							}
							else
							{	
								printf("Sending ACK. Next Seq:%d WS:%d\n", packet->seq, packet->ws);
								udp_send(conn->sockfd, packet, NULL);
							}
						}
						free(recvPacket);
						
						if(queueCapacity == 0)
							printf("Receiver window is locked.\n");

					}
					else if(recvPacket->seq < packet->seq)//queue[head].seq)
					{

						//received a old duplicate packet. ignore it
						free(recvPacket);
						if(drand48() < clientConfig->lossProb)
                                                {
                                                	printf("Dropping ACK. Seq:%d\n", packet->seq);
                                                }
                                                else
                                                {
                                                	printf("Sending ACK. Next Seq:%d WS:%d\n", packet->seq, packet->ws);
                                                 	udp_send(conn->sockfd, packet, NULL);
                                                }
					}
					else if(recvPacket->msgType == MSG_EOF && recvPacket->seq >= queue[head].seq)
					{
						int n = 0;
						if((eof!= 1))
						// && (enQueue(recvPacket) != -1))
                                                {
							bzero(packet, sizeof(struct packet_t));	
							packet->msgType = MSG_EOF;
							packet->ws = queueCapacity;
							packet->ts = recvPacket->ts;
							if(drand48() < clientConfig->lossProb)
	                                                {
        	                                                printf("Received EOF. Dropping ACK. Seq:%d\n", packet->seq);
                	                                }
                        	                        else if(enQueue(recvPacket)!=-1)
                                	                {
								packet->seq = queue[head].seq+1;
                                        	                printf("Received EOF. Sending ACK. Next Seq:%d WS:%d\n", 
packet->seq, packet->ws);
                                                	        udp_send(conn->sockfd, packet, NULL);
								eof = 1;
                                                	}
							free(recvPacket);
						}
					}
				}
			}
			else
			{
				if(eof == 1)
				{
					printf("Finished TIME_WAIT. No more data to read from server.\n");
					break;
				}
				if(queueCapacity != 0)
				{
					packet->ws = queueCapacity;
					printf("Sending Window Update. Seq:%d WS:%d\n", packet->seq, packet->ws);
					udp_send(conn->sockfd, packet, NULL);
				}
			}
		}
	}

	free(packet);
	int* retval;
	pthread_join(tid, (void *)&retval);
	if(*retval == 1)
	{
		printf("Finished reading file.\n");
		return 1;
	}
	return -1;
}


int createNewConnection(struct connection* conn)
{
	struct sockaddr_in sockAddr;
	socklen_t len;
	const int on = 1;
        int sockfd, n;
	char buf[MAXLINE];
        
        if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        {
	        printf("error createNewConnection:socket() errno:%d\n", errno);
		return -1;
        }

	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) 
        {
                printf("error createNewConnection:setsockopt() errno:%d\n", errno);
		return -1;
        }

	if(clientConfig->islocal)
	{
		 if(setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on)) == -1)
        	{
                	printf("error createNewConnection:setsockopt() errno:%d\n", errno);
                	return -1;
        	}
	}

	inet_pton(AF_INET, conn->clientIp, &sockAddr.sin_addr);
        sockAddr.sin_family = AF_INET;
        sockAddr.sin_port = htons(0);

        if(bind(sockfd, (SA *) &sockAddr, sizeof(sockAddr)) == -1)
	{
                printf("error createNewConnection:bind() errno:%d\n", errno);
		return -1;
        }
	
	bzero(&sockAddr, sizeof(sockAddr));
        len = sizeof(sockAddr);
        bzero(buf, sizeof(buf)); 
	if (getsockname(sockfd, (struct sockaddr *)&sockAddr, &len) == -1)
        {
                err_quit("error createNewConnection:getsockname() errno:%d\n", errno);
                return -1;
        }
        conn->clientPort = ntohs(sockAddr.sin_port);
 	strcpy(conn->clientIp, inet_ntop(AF_INET, &sockAddr.sin_addr, buf, sizeof(buf)));
	printf("Client IP:%s Ephemeral Port:%d\n", conn->clientIp, conn->clientPort);

	bzero(&sockAddr, sizeof(sockAddr));
        bzero(buf, sizeof(buf));
	sockAddr.sin_family = AF_INET;  
        sockAddr.sin_port = htons(conn->serverPort);
	inet_pton(AF_INET, conn->serverIp, &sockAddr.sin_addr);

	if((n = connect(sockfd,(SA *)&sockAddr, sizeof(sockAddr))) == -1)
        {
                printf("error createNewConnection:connect() errno:%d desc:%s\n", errno, strerror(errno));
                return -1;
        }

	bzero(&sockAddr, sizeof(sockAddr));
        len = sizeof(sockAddr);  
        memset(buf, 0, sizeof(buf));   
        if (getpeername(sockfd, (struct sockaddr *)&sockAddr, &len) == -1)
        {
		printf("error createNewConnection:getpeername() errno:%d\n", errno);
		return -1;
        }
	conn->sockfd = sockfd;
	strcpy(conn->serverIp, inet_ntop(AF_INET, &sockAddr.sin_addr, buf, sizeof(buf)));
	conn->serverPort = ntohs(sockAddr.sin_port);
	printf("Server IP:%s Peer Port:%d\n\n", conn->serverIp, conn->serverPort);
	return 1;
}

int main(int argc, char* argv)
{
	int i, n, numInterfaces;
	struct bind_info* interfaces;  
        char    buff[MAXLINE];        
	
	clientConfig = (struct client_config_t*) malloc(sizeof(struct client_config_t));
        readConfig();
	                
#ifdef DEBUG
        printf("Server Ip:%s\n", clientConfig->serverIp);
        printf("Port:%d\n", clientConfig->serverPort);
#endif 
 
        getInterfaces(&interfaces, &numInterfaces);

	for(i=0;i < numInterfaces;i++)
        {   
                printf("IP addr:%s\n", inet_ntop(AF_INET, &(((struct sockaddr_in*)interfaces[i].bind_ipaddr)->sin_addr) , buff, sizeof(buff)));
                printf("N/W Mask:%s\n", inet_ntop(AF_INET, &(((struct sockaddr_in*)interfaces[i].bind_ntmaddr)->sin_addr), buff, sizeof(buff)));
                printf("Subnet addr:%s\n\n", inet_ntop(AF_INET, interfaces[i].bind_subaddr, buff, sizeof(buff)));
	}

	struct connection* conn = (struct connection*) malloc(sizeof(struct connection));
	i = decide_if(interfaces, numInterfaces, clientConfig->serverIp);
	if(clientConfig->islocal)
	{
		strcpy(conn->clientIp, "127.0.0.1");
		strcpy(conn->serverIp, "127.0.0.1");
	}
	else
	{
		inet_ntop(AF_INET, &((struct sockaddr_in *)(interfaces[i].bind_ipaddr))->sin_addr, conn->clientIp, sizeof(conn->clientIp));
		strcpy(conn->serverIp, clientConfig->serverIp);		
	}
	conn->serverPort = clientConfig->serverPort;	
	conn->seq = clientConfig->seed;
	strcpy(conn->fileName, clientConfig->fileName);

	queueCapacity = clientConfig->winSize;
	queueSize = clientConfig->winSize;

	if(createNewConnection(conn)==-1)
		err_quit("Unable to connect to server.\n");

	queue = (struct packet_t*) malloc(clientConfig->winSize * sizeof(struct packet_t));
	bzero(queue, sizeof(clientConfig->winSize * sizeof(struct packet_t)));

	pthread_mutex_init(&queMutex, NULL);

	printf("Requesting rebound port.\n");	
	if(requestRebind(conn) == -1)
		err_quit("Unable to request rebind port from server.\n");
	
	if(recvFile(conn) == -1)
		err_quit("Unable to read file from server.\n");
}
