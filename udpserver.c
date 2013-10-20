#include <stdio.h>
#include "unpifiplus.h"
#include "udp.h"
#include "unprtt.h"
#define DEBUG 1

static struct msghdr msgsend, msgrecv;

void signalHandler(int signal)
{                               
        pid_t    pid;
        int      stat;           
        while ( (pid = waitpid(-1, &stat, WNOHANG)) > 0)
                 printf("Server(child) %d terminated.\n", pid);
        return;                  
}

void readConfig(int* port, int* maxWinSize)
{
        FILE *fp;
	char line[80];
        fp = fopen("server.in", "r");
        if (fp == NULL)
        {
                printf("Can't open server.in file\n");
                exit(1);
        }

	*port = atoi(fgets(line, 80, fp));
	memset(line, 0, 80);
	*maxWinSize = atoi(fgets(line, 80, fp));
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

/*
	bzero(buf, sizeof(buf));
	sprintf(buf, "%d", conn->serverPort);
        if(sendto(listenfd, buf, sizeof(buf), 0, (struct sockaddr*)&sockAddr,sizeof(sockAddr))<0)
        	err_quit("error sendto() errno:%d\n", errno);

	fd_set  rset;
        FD_ZERO(&rset);
        for ( ; ; )
        {
         	FD_SET(sockfd, &rset);
		if ( (n = select(sockfd+1, &rset, NULL, NULL, NULL)) < 0)
                {
                	if (errno == EINTR)
                        	continue;
                        else
                        	 err_quit("error select() errno:%d\n", errno);
                }
                else
                {
                	if (FD_ISSET(sockfd, &rset))
                        {
				bzero(&sockAddr, sizeof(sockAddr));
        			memset(buf, 0, MAXLINE);

				if(recvfrom(sockfd, buf, MAXLINE, 0, (struct sockaddr *)&sockAddr, &len)!=0)
        			{
					printf("read on connction port:%s\n", buf);
        			}
			}               
		}
	}
	exit(0);
}
*/


int main(int argc, char* argv)
{
	int port;
	int maxWinSize;


	readConfig(&port, &maxWinSize);


#ifdef DEBUG
	printf("Port:%d\n", port);
	printf("maxWinSize:%d\n", maxWinSize);
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
                sa->sin_port = htons(port);

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
					char    fileName[MAXLINE];
					struct sockaddr_in sockAddr;
					socklen_t len = sizeof(sockAddr);
					//write a generic recieve which will return the msgtype
					//based on the received msg type take action				
					if(recvfrom(interfaces[i].sockfd, fileName, MAXLINE, 0, (struct sockaddr *)&sockAddr, &len)!=0)
        				{
						char serverIp[INET_ADDRSTRLEN];
						inet_ntop(AF_INET, &((struct sockaddr_in *)(interfaces[i].bind_ipaddr))->sin_addr, serverIp, sizeof(serverIp));
						struct connection* conn = is_dup_connection(&sockAddr, serverIp, fileName);
						if(conn->pid == -1)
						{
							printf("From:%s ", conn->clientIp);
                        				printf("Port:%d ", conn->clientPort);
                        				printf("Data:%s\n", fileName);
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
									err_quit("error createNewConnection.\n");
								
								//if we reach here we have both listenfd and connfd setup for client.
								//now send the serverport to client and let it rebind.

								bzero(&sockAddr, sizeof(sockAddr));
								len = sizeof(sockAddr);
								inet_pton(AF_INET, conn->clientIp, &sockAddr.sin_addr);
        							sockAddr.sin_family = AF_INET;
        							sockAddr.sin_port = htons(conn->clientPort);
								char outbuf[MAXLINE];
								char inbuf[MAXLINE];
								printf("Sending serverPort: %d to Client:%s Port:%d\n", conn->serverPort, conn->clientIp, conn->clientPort);
								sprintf(outbuf, "%d", conn->serverPort);
								n = dg_send_recv(interfaces[i].sockfd, outbuf, sizeof(outbuf), inbuf, sizeof(inbuf), (struct 
sockaddr*)&sockAddr, len);
								if(n==-1)
									printf("failed to send to client\n");
								exit(0);
							}
						}
						else
							printf("Duplicate request, ignoring.\n");
					}
				}		
 			}
		}
	}
}
