#include <stdio.h>
#include "unpifiplus.h"

#define DEBUG 1

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

void handleNewConnection(int index, struct bind_info* interfaces, int numInterfaces)
{
      	int i;
	for(i=0;i<numInterfaces;i++)
		if(i!=index)
			close(interfaces[i].sockfd);
	char    buf[MAXLINE];
	char    fileName[MAXLINE];
	char    clientIp[INET_ADDRSTRLEN];
        struct sockaddr_in sockAddr;
        socklen_t len = sizeof(sockAddr);
	int clientPort, serverPort;

	bzero(&sockAddr, sizeof(sockAddr));
	if(recvfrom(interfaces[index].sockfd, fileName, MAXLINE, 0, (struct sockaddr *)&sockAddr, &len)!=0)
        {
        	memset(buf, 0, MAXLINE);
        	memset(clientIp, 0, INET_ADDRSTRLEN);

		if(inet_ntop(AF_INET, &(((struct sockaddr_in*)&sockAddr)->sin_addr), clientIp, sizeof(clientIp)) == NULL)
                         err_quit("error inet_ntop() errno:%d\n", errno);
                else
		{
			clientPort = ntohs(sockAddr.sin_port);
                        printf("From:%s ", clientIp);
                        printf("Port:%d ", clientPort);
                	printf("Data:%s\n", fileName);
		}
        }
	else
		err_quit("error recvfrom() errno:%d\n", errno);

	int connectionfd,n;
	const int on = 1;
	struct sockaddr_in *serverAddr;
	
	if((connectionfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
                err_quit("error socket() errno:%d\n", errno);	  
	
	if((n = setsockopt(connectionfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) == -1)
                err_quit("error setsockopt() errno:%d\n", errno);

	serverAddr = (struct sockaddr_in *) interfaces[index].bind_ipaddr;
        serverAddr->sin_family = AF_INET;
        serverAddr->sin_port = htons(0);
        
	if((n = bind(connectionfd, (SA *) serverAddr, sizeof(*serverAddr))) == -1)
                err_quit("error bind() errno:%d\n", errno);
		
	bzero(&sockAddr, sizeof(sockAddr));
        len = sizeof(sockAddr);
        memset(buf, 0, sizeof(buf));
        if (getsockname(connectionfd, (struct sockaddr *)&sockAddr, &len) == -1)
        	err_quit("error getsockname() errno:%d\n", errno);
                        
	printf("Server(child) IP:%s Port:%d\n", inet_ntop(AF_INET, &sockAddr.sin_addr, buf, sizeof(buf)),ntohs(sockAddr.sin_port));  
	serverPort = ntohs(sockAddr.sin_port);
	
	
	bzero(&sockAddr, sizeof(sockAddr));
        sockAddr.sin_family = AF_INET;
        sockAddr.sin_port = htons(clientPort);
        if((n=inet_pton(AF_INET, clientIp, &sockAddr.sin_addr))!=1)
                err_quit("error inet_pton() n:%d errno:%d\n", n, errno);
 	
	if((n = connect(connectionfd,(SA *)&sockAddr, sizeof(sockAddr))) == -1)   
        	err_quit("error connect() errno:%d\n", errno);
		
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%d", serverPort);
        if(sendto(interfaces[index].sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&sockAddr,sizeof(sockAddr))<0)
        	err_quit("error sendto() errno:%d\n", errno);

	fd_set  rset;
        FD_ZERO(&rset);
        for ( ; ; )
        {
         	FD_SET(connectionfd, &rset);
		if ( (n = select(connectionfd+1, &rset, NULL, NULL, NULL)) < 0)
                {
                	if (errno == EINTR)
                        	continue;
                        else
                        	 err_quit("error select() errno:%d\n", errno);
                }
                else
                {
                	if (FD_ISSET(connectionfd, &rset))
                        {
				bzero(&sockAddr, sizeof(sockAddr));
        			memset(buf, 0, MAXLINE);

				if(recvfrom(connectionfd, buf, MAXLINE, 0, (struct sockaddr *)&sockAddr, &len)!=0)
        			{
					printf("read on connction port:%s\n", buf);
        			}
			}               
		}
	}
	exit(0);
}

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
					pid_t   pid;	
					if ( (pid = fork())  < 0)
						err_quit("error fork() errno:%d\n", errno);
					else if (pid >  0) 	/* child */
					{  
						printf("forked new server(child) with pid:%d\n", pid);
					}
					else
					{
						handleNewConnection(i, interfaces, numInterfaces);
					}
				}		
 			}
		}
	}
}
