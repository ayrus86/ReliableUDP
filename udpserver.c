#include <stdio.h>
#include "unpifiplus.h"
#include "udp.h"
#include "unprtt.h"

#define DEBUG 1

static struct msghdr msgsend, msgrecv;


struct server_config_t{
        int serverPort;
        int winSize;
};

struct server_config_t* serverConfig;

struct packet_t* queue;
struct rtt_info rttinfo;
int rttinit = 0;
float threshold;

void signalHandler(int signal)
{                               
        pid_t     pid;
        int      stat;           
        while ( (pid = waitpid(-1, &stat, WNOHANG)) > 0)
        {         

		struct connection* cur = connections;
		while(cur != NULL)
		{
			if(cur->pid != pid)
				cur=cur->next;
			else
			{
				if(cur->prev!=NULL)
					cur->prev->next = cur->next;
				if(cur->next!=NULL)
					cur->next->prev = cur->prev;
			
				if(connections == cur)
				{
					connections = connections->next;
				}
			
				close(cur->sockfd);
				free(cur);
				printf("Succesfully released the resources of server(child):%d\n", pid);
				return;
			}
		}
	}
	
        return;                  
}

void readConfig()
{
        FILE *fp;
	char line[80];
        fp = fopen("server.in", "r");
        if (fp == NULL)
        {
                printf("Can't open server.in file\n");
                exit(1);
        }

	serverConfig->serverPort = atoi(fgets(line, 80, fp));
	memset(line, 0, 80);
	serverConfig->winSize = atoi(fgets(line, 80, fp));
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

struct connection* is_dup_connection(struct sockaddr_in* clientAddr, char* serverIp, char* fileName)
{
	struct connection* cur = connections;
	struct connection* prev = connections;
	char clientIp[INET_ADDRSTRLEN];
	int clientPort;

	inet_ntop(AF_INET, &clientAddr->sin_addr, clientIp, sizeof(clientIp));
	clientPort = ntohs(clientAddr->sin_port);

	while(cur!=NULL)
	{
		if((strcmp(clientIp, cur->clientIp)==0) && (clientPort == cur->clientPort) && (strcmp(serverIp, cur->serverIp)==0) && (strcmp(fileName, cur->fileName)==0))
			return cur;
		
		prev = cur;
		cur = cur->next;
	}
	
	struct connection* newConn = (struct connection*)malloc(sizeof(struct connection));
	strcpy(newConn->clientIp, clientIp);
	strcpy(newConn->serverIp, serverIp);
	strcpy(newConn->fileName, fileName);	
	newConn->clientPort = clientPort;
	newConn->pid = -1;
	
	if(connections == NULL)
		connections = newConn;
	else
	{
		prev->next = newConn;
		newConn->prev = prev;
		newConn->next = NULL;
	}

	return newConn;
}

int createNewConnection(struct connection* conn)
{
	char    buf[MAXLINE];
        struct sockaddr_in sockAddr;
        socklen_t len;
	int sockfd,n;
	const int on = 1;
	struct sockaddr_in serverAddr;
	
	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
                printf("error createNewConnection:socket() errno:%d\n", errno);
		return -1;	  
	}

	if((n = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) == -1)
        {
	        printf("error createNewConnection:setsockopt() errno:%d\n", errno);
	}


        inet_pton(AF_INET, conn->serverIp, &serverAddr.sin_addr);
	serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(0);
        
	if((n = bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr))) == -1)
        {
	        printf("error createNewConnection:bind() errno:%d\n", errno);
		return -1;
	}		

	bzero(&sockAddr, sizeof(sockAddr));
        len = sizeof(sockAddr);
        bzero(buf, sizeof(buf));
        if (getsockname(sockfd, (struct sockaddr *)&sockAddr, &len) == -1)
        {
		printf("error createNewConnection:getsockname() errno:%d\n", errno);
        	return -1;
	}                
	conn->serverPort = ntohs(sockAddr.sin_port);
	printf("Server(child) IP:%s Port:%d\n", inet_ntop(AF_INET, &sockAddr.sin_addr, buf, sizeof(buf)), conn->serverPort);  
	
	bzero(&sockAddr, sizeof(sockAddr));
        sockAddr.sin_family = AF_INET;
        sockAddr.sin_port = htons(conn->clientPort);
        if((n=inet_pton(AF_INET, conn->clientIp, &sockAddr.sin_addr))!=1)
	{
		printf("error createNewConnection:inet_pton() errno:%d\n", errno);
		return -1;
 	}

	if((n = connect(sockfd,(SA *)&sockAddr, sizeof(sockAddr))) == -1)
	{
        	printf("error createNewConnection:connect() errno:%d\n", errno);
		return -1;
	}
	conn->sockfd = sockfd;
	return 1;	
}

int sendFile(struct connection* conn)
{
	queueCapacity = serverConfig->winSize;
        queueSize = serverConfig->winSize;
	queue = (struct packet_t*) malloc(serverConfig->winSize * sizeof(struct packet_t));
	bzero(queue, sizeof(serverConfig->winSize * sizeof(struct packet_t)));

	struct packet_t* packet = (struct packet_t*) malloc(sizeof(struct packet_t));
	struct timeval timeout;
	struct packet_t* recvPacket;
	
	FILE *fp = fopen(conn->fileName, "r");	
	fd_set  rset;
        FD_ZERO(&rset);
	int sentIndex = -1;
	int clientws = -1;

	while(!feof(fp) && peekQueueHead(packet)!=-1)
        {
        	bzero(packet->msg, sizeof(packet->msg));
                packet->msgType = MSG_DATA; 
                packet->seq = conn->seq++;
                fgets(packet->msg,512,fp);
                enQueue(packet);
        }

        bzero(packet, sizeof(packet));
	if(peekQueueTail(packet)==-1)
	{
		printf("Nothing to send.\n");
                return 1;
	}

	int eof = 0;
	float congWnd = 1;
	int lastack = packet->seq-1;
	int lastackcount = 0;
	
	if (rttinit == 0)
        {
                rtt_init(&rttinfo);
                rttinit = 1;
                rtt_d_flag = 1;
        }
        
        rtt_newpack(&rttinfo);
	packet->ts = rtt_ts(&rttinfo);
	udp_send(conn->sockfd, packet, NULL);
	
	for(;;)
	{
		if(rttinit == 1)
		{
			packet->ts = rtt_ts(&rttinfo);
			timeout.tv_sec = rtt_start(&rttinfo)/1000000;
                	timeout.tv_usec = rtt_start(&rttinfo)%1000000;
		}
	
		FD_SET(conn->sockfd, &rset);
                if (select(conn->sockfd+1, &rset, NULL, NULL, &timeout) < 0)
                {
                        if (errno == EINTR)
                                continue;
                        else
                        {
                                printf("error sendFile:select() errno:%s\n", strerror(errno));
                                return -1;
                        }
                }
		else
                {
                        if (FD_ISSET(conn->sockfd, &rset))
                        {
				recvPacket = (struct packet_t*)malloc(sizeof(struct packet_t));
                                struct sockaddr_in sockAddr;
                                bzero(&sockAddr, sizeof(struct sockaddr_in));
				struct packet_t tempPacket;
				bzero(&tempPacket, sizeof(struct packet_t));
				if(udp_recv(conn->sockfd, recvPacket, (SA*) &sockAddr) == 1)
                                {
					if(recvPacket->msgType == MSG_ACK && peekQueueTail(&tempPacket)!= -1 && recvPacket->seq >= tempPacket.seq)
                                        {
						while(peekQueueTail(&tempPacket) != -1 && recvPacket->seq > tempPacket.seq)
						{
							deQueue(&tempPacket);
							
							if(tail == sentIndex)
								sentIndex = -1;

							if(congWnd < threshold)
								congWnd += 1;
							else
							{
								congWnd = congWnd + 1/congWnd;
								threshold = congWnd;
							}
							//if(tempPacket.seq == recvPacket->seq-1)
							//	rtt_stop(&rttinfo, (rtt_ts(&rttinfo) - recvPacket->ts)*1000000);
								
						}
						
						while(!feof(fp) && peekQueueHead(&tempPacket)!=-1)
        					{
                					bzero(&tempPacket, sizeof(struct packet_t));
							tempPacket.msgType = MSG_DATA;
                					tempPacket.seq = conn->seq++;
                					fgets(tempPacket.msg,512,fp);
							enQueue(&tempPacket);
        					}

						if(feof(fp) &&  eof!=1 && peekQueueHead(&tempPacket)!=-1)
        					{
							bzero(packet, sizeof(packet));
                                                        packet->msgType = MSG_EOF;
                                                        packet->seq = conn->seq++;
							printf("Finished reading file. Enqueuing EOF packet. seq:%d\n", packet->seq);
							if(enQueue(packet) == -1)
								printf("Failed to enqueue EOF.\n");
                                                        eof = 1;
        					}
						if(peekQueueTail(packet)!= -1)
						{
							int reqPaks, i;
								
							if(sentIndex == -1)
								sentIndex = tail;
							else if(sentIndex < tail)
								sentIndex += queueSize;

							clientws = recvPacket->ws;
							reqPaks = recvPacket->ws <= (queueSize-queueCapacity) ? recvPacket->ws : queueSize-queueCapacity;
							reqPaks = congWnd < reqPaks ? congWnd : reqPaks;
							reqPaks = tail+reqPaks-1;
							
							printf("window data congWnd:%f threshold:%f\n", congWnd, threshold);

							if(reqPaks<0 || reqPaks < sentIndex || recvPacket->ws == 0)
							{
								sentIndex = sentIndex%queueSize;
								continue;
							}


							if (rttinit == 0)   
        						{
								rtt_init(&rttinfo);
                						rttinit = 1;
                						rtt_d_flag = 1;
        						}
                
							rtt_newpack(&rttinfo);
							for(i = sentIndex; i <= reqPaks ; i++)
                                                	{
                                                        	bzero(&tempPacket, sizeof(struct packet_t));
                                                        	memcpy(&tempPacket, &queue[i%queueSize], sizeof(struct packet_t));
                                                        	printf("Window data Seq:%d\n", tempPacket.seq);
                                                        	udp_send(conn->sockfd, &tempPacket, NULL);
                                                	}
							sentIndex = (reqPaks+1)%queueSize;
						}
					}
					else if(recvPacket->msgType == MSG_EOF && eof == 1 && recvPacket->seq == queue[tail].seq+1)
					{
						printf("Got ack for EOF. Exiting Server(child).\n");
						break;
					}
				}
				else
				{
					printf("recv didnt return 1.\n");
				} 	
			}
			else
			{
				if(clientws == 0)
				{
					rttinit = 0;
					timeout.tv_sec = 10;
                        		timeout.tv_usec = 0;
					struct packet_t tempPacket;
	                                bzero(&tempPacket, sizeof(struct packet_t));
					tempPacket.msgType = MSG_PROBE;
					printf("sending probe packet.\n");				
					udp_send(conn->sockfd, &tempPacket, NULL);
				}
				else
				{
					if(rtt_timeout(&rttinfo))
                                	{
                                        	rttinit = 0;
                                        	errno = ETIMEDOUT;
                                        	printf("error sendFile:rtt_timeout(). errno:%s\n",strerror(errno));
                                        	return -1;
                                	}

					threshold = congWnd/2;
					congWnd = 1;

					if(threshold == 0)
						threshold = 1;
					printf("timeout congWnd:%f threshold:%f\n", congWnd, threshold);

					printf("sendFile: timeout. Resending Seq:%d msgType:%d rtt_rto:%d\n", packet->seq, packet->msgType, rttinfo.rtt_rto);
					udp_send(conn->sockfd, packet, NULL);
				}
			}
		}
		
	}
        free(packet);
	fclose(fp);
	printf("Successfully finished sending file.\n");
}

int sendRebindPort(int sockfd, struct sockaddr* sockAddr, struct connection* conn)
{
	struct packet_t* packet = (struct packet_t*) malloc(sizeof(struct packet_t));
        bzero(packet, sizeof(struct packet_t));

	packet->seq = conn->seq;
        packet->msgType = MSG_ACK;
        sprintf(packet->msg,"%d", conn->serverPort);

 	struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

	fd_set  rset;	
        FD_ZERO(&rset);
        for ( ; ; )
        {
		printf("sending for sendRebindPort seq:%d\n", packet->seq);
		udp_send(sockfd, packet, sockAddr);
         	FD_SET(conn->sockfd, &rset);
		if (select(conn->sockfd+1, &rset, NULL, NULL, &timeout) < 0)
                {
                	if (errno == EINTR)
                        	continue;
                        else
			{
                        	printf("error sendRebindPort:select() errno:%d\n", errno);
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
					if(recvPacket->msgType == MSG_ACK && recvPacket->seq == conn->seq+1)
					{
						conn->seq = recvPacket->seq;
						threshold = recvPacket->ws / 2;
						printf("Got ACK on rebound port. 3-way Handshake successful. seq:%d\n", conn->seq);
						free(recvPacket);
						return 1;
					}
				}
			}
			else
				printf("sendRebindPort: timed out. resending seq:%d\n", conn->seq);               
		}
	}
	return -1;
}


int main(int argc, char* argv)
{
	serverConfig = (struct server_config_t*) malloc(sizeof(struct server_config_t));
	readConfig();
	

#ifdef DEBUG
	printf("Port:%d\n", serverConfig->serverPort);
	printf("maxWinSize:%d\n", serverConfig->winSize);
#endif

	int sockfd;
	const int on = 1;
	int n, numInterfaces;
	struct sockaddr_in *sa;
	struct bind_info* interfaces;

	getInterfaces(&interfaces, &numInterfaces);
	
	int     maxfdp=0;
        fd_set  rset;

#ifdef DEBUG
	printf("numInterfaces:%d\n", numInterfaces);		
#endif
	int i;
	for(i=0;i < numInterfaces;i++) 
	{
		char    buff[MAXLINE];
                printf("IP addr:%s\n", inet_ntop(AF_INET, &(((struct sockaddr_in*)interfaces[i].bind_ipaddr)->sin_addr) , buff, sizeof(buff)));
                printf("N/W Mask:%s\n", inet_ntop(AF_INET, &(((struct sockaddr_in*)interfaces[i].bind_ntmaddr)->sin_addr), buff, sizeof(buff)));
                printf("Subnet addr:%s\n\n", inet_ntop(AF_INET, interfaces[i].bind_subaddr, buff, sizeof(buff)));
		
		if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
			err_quit("error socket() errno:%d\n", errno);

		if((n = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) == -1)
			 err_quit("error setsockopt() errno:%d\n", errno);

		sa = (struct sockaddr_in *) interfaces[i].bind_ipaddr;	
                sa->sin_family = AF_INET;
                sa->sin_port = htons(serverConfig->serverPort);

		if((n = bind(sockfd, (SA *) sa, sizeof(*sa))) == -1)
			err_quit("error bind() errno:%d\n", errno);

		interfaces[i].sockfd = sockfd;

		if(maxfdp < sockfd)
			maxfdp = sockfd;
	}
	
	signal(SIGCHLD,signalHandler);

	FD_ZERO(&rset);
	maxfdp++;

	for ( ; ; )
        {
		for (i = 0; i < numInterfaces; i++)
        		FD_SET(interfaces[i].sockfd, &rset);
		
		printf("waiting for connections...\n");
		if ( (n = select(maxfdp, &rset, NULL, NULL, NULL)) < 0)
                {
                	if (errno == EINTR)
                        	continue;
                        else
                        	err_quit("error select() errno:%d\n", errno);
                }  
		else
		{
			for (i = 0; i < numInterfaces; i++)
			{
				if (FD_ISSET(interfaces[i].sockfd, &rset))
				{
					struct packet_t* recvPacket = (struct packet_t*) malloc(sizeof(struct packet_t));
        				bzero(recvPacket, sizeof(struct packet_t));
					struct sockaddr_in sockAddr;
					socklen_t len = sizeof(sockAddr);		
					if(udp_recv(interfaces[i].sockfd, recvPacket, (SA*) &sockAddr) == 1)
					{
						printf("read fileName:%s msgType:%d seq:%d\n", recvPacket->msg, recvPacket->msgType, recvPacket->seq);
						char serverIp[INET_ADDRSTRLEN];
						inet_ntop(AF_INET, &((struct sockaddr_in *)(interfaces[i].bind_ipaddr))->sin_addr, serverIp, sizeof(serverIp));
						struct connection* conn = is_dup_connection(&sockAddr, serverIp, recvPacket->msg);
						conn->seq = recvPacket->seq;
						if(conn->pid == -1 && recvPacket->msgType == MSG_SYN)
						{
							printf("From:%s ", conn->clientIp);
                        				printf("Port:%d ", conn->clientPort);
                        				printf("Data:%s\n", recvPacket->msg);
							pid_t   pid;	

							if ( (pid = fork())  < 0)
								err_quit("error fork() errno:%d\n", errno);
							else if (pid >  0) 	/* child */
							{
								conn->pid = pid;  
								printf("forked new server(child) with pid:%d\n", pid);
							}
							else
							{
								int j;
								for(j=0;j<numInterfaces;j++)
                							if(i!=j)   
                        							close(interfaces[j].sockfd);
								
								if(createNewConnection(conn) == -1)
									err_quit("error createNewConnection().\n");
								
								//if we reach here we have both listenfd and connfd setup for client.
								//now send the serverport to client and let it rebind.
								if(sendRebindPort(interfaces[i].sockfd, (SA*)&sockAddr, conn) == -1)
									err_quit("error sendRebindPort().\n");
		
								close(interfaces[i].sockfd);
								if(sendFile(conn) == -1)
									err_quit("error sendFile().\n");
								exit(0);
							}
						}
						else if(recvPacket->msgType == MSG_SYN)
						{
							printf("Duplicate request, Sending on both listening and connection fd.\n");
							
							struct packet_t* packet = (struct packet_t*) malloc(sizeof(struct packet_t));
						        bzero(packet, sizeof(struct packet_t));
                                
        						packet->seq = conn->seq;
        						packet->msgType = MSG_ACK;
        						sprintf(packet->msg,"%d", conn->serverPort);

							if(udp_send(interfaces[i].sockfd, packet,(SA *) &sockAddr) == -1)
                                                                        err_quit("error sending the binding port number.\n");
							
							if(udp_send(conn->sockfd, packet,(SA *) &sockAddr) == -1)
                                                                        err_quit("error sending the binding port number.\n");
						}
					}
				}		
 			}
		}
	}
}
