#include <stdio.h>
#include <setjmp.h>
#include "unp.h"
#include "unpifiplus.h"
#include "udp.h"
#include "unprtt.h"

#define DEBUG 1

static struct rtt_info rttinfo;
static int rttinit = 0;
static void sig_alrm(int signo);
static sigjmp_buf jmpbuf;

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

static void sig_alrm(int signo)
{
        siglongjmp(jmpbuf, 1);
} 

int requestRebind(struct connection* conn)
{
	//we should create a MSG_SYN request and send it to server on listing port
	//we should use timer to resend the msg till we get a rebound port

	struct packet_t* packet = (struct packet_t*) malloc(sizeof(struct packet_t));
	bzero(packet, sizeof(struct packet_t));
	
	packet->seq = 1001;
	packet->msgType = MSG_SYN;
	strcpy(packet->msg, "test.txt");
	
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
				printf("client calling udp_recv.\n");
                                if(udp_recv(conn->sockfd, recvPacket, (SA*) &sockAddr) == 1)
                                {
                                        if(recvPacket->msgType == MSG_ACK && recvPacket->seq == 1001)
					{
						printf("Received new binding port. Rebinding to port.\n");
        					sockAddr.sin_port = htons(atoi(recvPacket->msg));
        					if(connect(conn->sockfd,(SA *)&sockAddr, sizeof(sockAddr)) == -1)
        					{       
                					printf("error requestRebind:connect() errno:%d\n", errno);
                					return -1;
        					}
						free(recvPacket);
        					printf("Port rebound successful.\n");
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
	bzero(packet, sizeof(struct packet_t));
                                                        
        packet->seq = 1002;
        packet->msgType = MSG_ACK;
	udp_send(conn->sockfd, packet, NULL);
	free(packet);
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
        sockAddr.sin_family = AF_INET;  
        sockAddr.sin_port = htons(conn->serverPort);
        if((n=inet_pton(AF_INET, conn->serverIp, &sockAddr.sin_addr))!=1)
        {
                printf("error createNewConnection:inet_pton() errno:%d\n", errno);
                return -1;
        }       

	if((n = connect(sockfd,(SA *)&sockAddr, sizeof(sockAddr))) == -1)
        {
                printf("error createNewConnection:connect() errno:%d\n", errno);
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
        char *serverIp;
        int serverPort;
        char    buff[MAXLINE];        

        readConfig(&serverIp, &serverPort);
	                
#ifdef DEBUG
        printf("Server Ip:%s\n", serverIp);
        printf("Port:%d\n", serverPort);
#endif 
 
        getInterfaces(&interfaces, &numInterfaces);

	for(i=0;i < numInterfaces;i++)
        {   
                printf("IP addr:%s\n", inet_ntop(AF_INET, &(((struct sockaddr_in*)interfaces[i].bind_ipaddr)->sin_addr) , buff, sizeof(buff)));
                printf("N/W Mask:%s\n", inet_ntop(AF_INET, &(((struct sockaddr_in*)interfaces[i].bind_ntmaddr)->sin_addr), buff, sizeof(buff)));
                printf("Subnet addr:%s\n\n", inet_ntop(AF_INET, interfaces[i].bind_subaddr, buff, sizeof(buff)));
	}

	struct connection* conn = (struct connection*) malloc(sizeof(struct connection));
	inet_ntop(AF_INET, &((struct sockaddr_in *)(interfaces[0].bind_ipaddr))->sin_addr, conn->clientIp, sizeof(conn->clientIp));
	strcpy(conn->serverIp, serverIp);
	conn->serverPort = serverPort;	

	if(createNewConnection(conn)==-1)
		err_quit("Unable to connect to server.\n");

	printf("Requesting rebound port.\n");	
	if(requestRebind(conn) == -1)
		err_quit("Unable to request rebind port from server.\n");
}
