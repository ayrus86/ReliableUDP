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

	if (rttinit == 0)
        {
                rtt_init(&rttinfo);
                rttinit = 1;
                rtt_d_flag = 1;
        }
        
        rtt_newpack(&rttinfo);
	
	for(;;)
	{
		packet->ts = rtt_ts(&rttinfo);
		timeout.tv_usec = rtt_start(&rttinfo)/1000000;
                timeout.tv_usec = rtt_start(&rttinfo)%1000000;
		udp_send(conn->sockfd, packet, NULL);
		FD_SET(conn->sockfd, &rset);
                if (select(conn->sockfd+1, &rset, NULL, NULL, &timeout) < 0)
                {
                        if (errno == EINTR)
                                continue;
                        else
                        {
                                printf("error sendFile:select() errno:%d\n", errno);
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
					if(recvPacket->msgType == MSG_ACK && peekQueueTail(&tempPacket)!= -1 && recvPacket->seq >= tempPacket.seq+1)
                                        {
						while(peekQueueTail(&tempPacket) != -1 && recvPacket->seq > tempPacket.seq)
							deQueue(&tempPacket);
						
						while(!feof(fp) && peekQueueHead(&tempPacket)!=-1)
        					{
                					bzero(&tempPacket, sizeof(struct packet_t));
							tempPacket.msgType = MSG_DATA;
                					tempPacket.seq = conn->seq++;
                					fgets(tempPacket.msg,512,fp);
							enQueue(&tempPacket);
        					}

						if(eof!=1 && peekQueueTail(packet)== -1 && peekQueueHead(&tempPacket)!=-1)
        					{
							printf("Finished Reading file. Sending EOF. Seq:%d\n", conn->seq);
							bzero(packet, sizeof(packet));
                                                        packet->msgType = MSG_EOF;
							//packet->ts = recvPacket->ts;
                                                        packet->seq = conn->seq++;
							enQueue(packet);
                                                        eof = 1;
							continue;
        					}
						else
						{
						/*	if(eof!=1)
							{
								int i, k;
								k = tail + 
                                                		for(i = tail, k = recvPacket->ws; i != head && k>=0 ; i = (i+1)%queueSize, k--)
                                                		{
                                                        		bzero(&tempPacket, sizeof(struct packet_t));
                                                        		memcpy(&tempPacket, &queue[i], sizeof(struct packet_t));
                                                        		//printf("Sending data Seq:%d msg:%s\n", tempPacket.seq, tempPacket.msg);
                                                        		udp_send(conn->sockfd, &tempPacket, NULL);
                                                		}
							}*/
						}
						rtt_newpack(&rttinfo);
						rtt_init(&rttinfo);
						//printf("Sending data Seq:%d msg:%s\n", packet->seq, packet->msg);
					}
					else if(recvPacket->msgType == MSG_EOF && eof == 1 && queue[tail].seq+1)
						break;
				} 	
			}
			else
			{
				if(rtt_timeout(&rttinfo))
                                {
                                        rttinit = 0;
                                        errno = ETIMEDOUT;
                                        printf("error recvFile:rtt_timeout(). errno:%s\n",strerror(errno));
                                        return -1;
                                }
				printf("sendFile: timeout. Resending Seq:%d msgType:%d rtt_rto:%d\n", packet->seq, packet->msgType, rttinfo.rtt_rto);
			}
		}
		
	}
	rtt_stop(&rttinfo, (rtt_ts(&rttinfo) - recvPacket->ts)*1000000);
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
