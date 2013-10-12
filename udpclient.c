#include <stdio.h>
#include "unpifiplus.h"

#define DEBUG 1
void readConfig(char** serverIp, int* port)
{
        FILE *fp;
        char line[80];
        fp = fopen("client.in", "r");
        if (fp == NULL)
        {
                printf("Can't open client.in file\n");
                exit(1);
        }

        memset(line, 0, 80);
	fgets(line,80,fp);
	strtok(line, "\n");
	*serverIp = (char *) malloc(INET_ADDRSTRLEN);
	strcpy(*serverIp, line);
        memset(line, 0, 80);
        *port = atoi(fgets(line, 80, fp));
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

int main(int argc, char* argv)
{
	int i, n, numInterfaces;
	struct bind_info* interfaces;  
        char *serverIp;
        int port;
        char    buff[MAXLINE];        

        readConfig(&serverIp, &port);
	                
#ifdef DEBUG
        printf("Server Ip:%s\n", serverIp);
        printf("Port:%d\n", port);
#endif 
 
        getInterfaces(&interfaces, &numInterfaces);

	for(i=0;i < numInterfaces;i++)
        {   
                printf("IP addr:%s\n", inet_ntop(AF_INET, &(((struct sockaddr_in*)interfaces[i].bind_ipaddr)->sin_addr) , buff, sizeof(buff)));
                printf("N/W Mask:%s\n", inet_ntop(AF_INET, &(((struct sockaddr_in*)interfaces[i].bind_ntmaddr)->sin_addr), buff, sizeof(buff)));
                printf("Subnet addr:%s\n\n", inet_ntop(AF_INET, interfaces[i].bind_subaddr, buff, sizeof(buff)));
	}

	struct sockaddr_in sockAddr;
        struct sockaddr_in *clientAddr; 
	int sockfd;

	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        	err_quit("error socket() errno:%d\n", errno);

	clientAddr = (struct sockaddr_in *) interfaces[0].bind_ipaddr;
	clientAddr->sin_family = AF_INET;
        clientAddr->sin_port = htons(0);
	
	if((n = bind(sockfd, (SA *) clientAddr, sizeof(*clientAddr))) == -1)
        	err_quit("error bind() errno:%d\n", errno);
	else
	{
		bzero(&sockAddr, sizeof(sockAddr));
		socklen_t len = sizeof(sockAddr);
		memset(buff, 0, sizeof(buff));
		if (getsockname(sockfd, (struct sockaddr *)&sockAddr, &len) == -1)
    			err_quit("error getsockname() errno:%d\n", errno);
		else
    			printf("Client IP:%s Port:%d\n", inet_ntop(AF_INET, &sockAddr.sin_addr, buff, sizeof(buff)),ntohs(sockAddr.sin_port));
	}

	bzero(&sockAddr, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(port);
	
	if((n=inet_pton(AF_INET, serverIp, &sockAddr.sin_addr))!=1)
		 err_quit("error inet_pton() n:%d errno:%d\n", n, errno);

	if((n = connect(sockfd,(SA *)&sockAddr, sizeof(sockAddr))) == -1)
        	err_quit("error connect() errno:%d\n", errno);
	else
	{
		bzero(&sockAddr, sizeof(sockAddr));
                socklen_t len = sizeof(sockAddr);
                memset(buff, 0, sizeof(buff));
                if (getpeername(sockfd, (struct sockaddr *)&sockAddr, &len) == -1)
                        err_quit("error getpeername() errno:%d\n", errno);
                else
		{
                        printf("Server IP:%s Port:%d\n", inet_ntop(AF_INET, &sockAddr.sin_addr, buff, sizeof(buff)),ntohs(sockAddr.sin_port));
			//if(sendto(sockfd, "test.txt", 9, 0, (struct sockaddr *)&sockAddr, len)==-1)
			if(write(sockfd, "test.txt", 9)<0)
				err_quit("error sendto() errno:%d\n", errno);
		}
	}	
}
