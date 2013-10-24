#include <stdio.h>
#include <setjmp.h>
#include <math.h>
#include "unp.h"
#include "udp.h"
#include "unpifiplus.h"
#include "unprtt.h"

#define DEBUG 1

struct client_config_t{
	char serverIp[INET_ADDRSTRLEN];
	int serverPort;
	char fileName[25];
	int winSize;
	int seed;
	double lossProb;
	int mean;
};

struct client_config_t* clientConfig;

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

/*static void sig_alrm(int signo)
{
        siglongjmp(jmpbuf, 1);
} */

int printQueue()
{
	int ret = 0;
	struct packet_t packet;
	while(1)
	{
		sleep( -1 * clientConfig->mean * log(drand48()));
		if(deQueue(&packet)!=-1)
		{
			if(packet.msgType == MSG_EOF)
			{
				printf("read end of file.seq:%d\n", packet.seq);
				ret = 1;
				pthread_exit(&ret);
			}
			printf("Read seq:%d msg:%s\n", packet.seq, packet.msg);
			bzero(&packet, sizeof(packet));
		}
		else
		{
			printf("queueCapacity:%d head:%d tail:%d tail->seq:%d head->seq:%d\n", queueCapacity, head, tail, queue[tail].seq, queue[head].seq);
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
        					printf("Port rebound successful. Sending ACK to finish 3-Way handshake.\n");
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
	
/*	if(peekQueueHead(packet)==-1)
		return -1;*/	
	
	packet->seq = conn->seq;
	packet->ws = queueCapacity;
        packet->msgType = MSG_ACK;
	

	int eof = 0;
	struct timeval timeout;
	fd_set  rset;
        FD_ZERO(&rset);
	srand(clientConfig->seed);
	
	for(;;)
        {

sendagain:
	    	timeout.tv_usec = 5;
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
					if(recvPacket->msgType == MSG_DATA && recvPacket->seq >= packet->seq)
                                        {
						
						if(clientConfig->lossProb > ((double) drand48())) //(double)RAND_MAX))
                                        	{
                                                	printf("dropping packet seq:%d msgType:%d\n", recvPacket->seq, recvPacket->msgType);
                                                	goto sendagain;
                                        	}
						
						if(enQueue(recvPacket) != -1)
						{
							bzero(packet, sizeof(struct packet_t));
							packet->seq = queue[head].seq+1;
							packet->ws = queueCapacity;
							packet->ts = recvPacket->ts;
							packet->msgType = MSG_ACK;
							printf("sending ACK:%d recv->seq:%d ws:%d\n", packet->seq, recvPacket->seq, packet->ws);
						}
						free(recvPacket);
						udp_send(conn->sockfd, packet, NULL);
					}
					else if(recvPacket->seq < queue[head].seq)
					{
						free(recvPacket);
						continue; //received a old duplicate packet. ignore it
					}
					else if(recvPacket->msgType == MSG_EOF && recvPacket->seq >= queue[head].seq)
					{
						int n = 0;
						if((eof!= 1) && (enQueue(recvPacket) != -1))
                                                {
							//printf("inside eof if. recv->seq:%d\n",recvPacket->seq);							
							bzero(packet, sizeof(struct packet_t));	
							packet->msgType = MSG_EOF;
							packet->ws = queueCapacity;
							packet->ts = recvPacket->ts;
							packet->seq = queue[head].seq+1;
							eof = 1;
							free(recvPacket);
 							udp_send(conn->sockfd, packet, NULL);
						}
					}
				}
			}
			else
			{
				if(eof == 1)
				{
					printf("Finished WAIT_TIME. breaking.\n");
					break;
				}
				printf("recvFile: timeout. sending ack seq:%d msgType:%d\n", packet->seq, packet->msgType);
				udp_send(conn->sockfd, packet, NULL);
			}
		}
	}

	free(packet);
	int* retval;
	pthread_join(tid, (void *)&retval);
	if(*retval == 1)
	{
		printf("finished reading file.\n");
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
	printf("Client IP:%s Port:%d\n", conn->clientIp, conn->clientPort);

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
	printf("Server IP:%s Port:%d\n", conn->serverIp, conn->serverPort);
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
	inet_ntop(AF_INET, &((struct sockaddr_in *)(interfaces[1].bind_ipaddr))->sin_addr, conn->clientIp, sizeof(conn->clientIp));
	strcpy(conn->serverIp, clientConfig->serverIp);
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
