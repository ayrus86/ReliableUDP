#include <stdio.h>
#include "unpifiplus.h"

#define DEBUG 1
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
	
	FD_ZERO(&rset);
	maxfdp++;

	for ( ; ; )
        {
		for (i = 0; i < numInterfaces; i++)
        		FD_SET(interfaces[i].sockfd, &rset);
		
		Select(maxfdp, &rset, NULL, NULL, NULL);
		
		for (i = 0; i < numInterfaces; i++)
		{
			if (FD_ISSET(interfaces[i].sockfd, &rset))
			{
			}		
 		}
	}
}
