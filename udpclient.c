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
	//we should create a MSG_FILE request and send it to server on listing port
	//we should use timer to resend the msg till we get a rebound port

	ssize_t n;
        struct iovec iovsend[2], iovrecv[1];
        struct hdr recvhdr;
	char buf[MAXLINE];
	
	bzero(&msgsend, sizeof(msgsend));
        bzero(&msgrecv, sizeof(msgrecv));
        
        if (rttinit == 0)
        {
                rtt_init(&rttinfo);
                rttinit = 1;
                rtt_d_flag = 1;
        }

	conn->header.seq++;
        conn->header.msgType = MSG_FILE;
 
        msgsend.msg_iov = iovsend;
        msgsend.msg_iovlen = 2;
        iovsend[0].iov_base = &conn->header;
        iovsend[0].iov_len = sizeof(struct hdr);
        iovsend[1].iov_base = conn->fileName;
        iovsend[1].iov_len = sizeof(conn->fileName);   
        
        iovrecv[0].iov_base = &recvhdr;
        iovrecv[0].iov_len = sizeof(struct hdr);
        iovrecv[1].iov_base = buf;
        iovrecv[1].iov_len = sizeof(buf);

        Signal(SIGALRM, sig_alrm);
        rtt_newpack(&rttinfo);

sendagain:
        conn->header.ts = rtt_ts(&rttinfo);
        if(sendmsg(conn->sockfd, &msgsend, 0) == -1)
                printf("error requestRebind:sendmsg() errno:%d\n", errno);

	alarm(rtt_start(&rttinfo));
        
        if (sigsetjmp(jmpbuf, 1) != 0)
        {
                if (rtt_timeout(&rttinfo) < 0)
                {
                        err_msg("dg_send_recv: no response from client, giving up");
                        rttinit = 0;        /* reinit in case we're called again */
                        errno = ETIMEDOUT;
                        return (-1);
                }
                printf("requestRebind:Retransmitting. rtt_rto:%d\n", rttinfo.rtt_rto);
                goto sendagain;
        }
	
	printf("waiting for the reconnect port from server...\n");

	do      
        { 
                if((n = recvmsg(conn->sockfd, &msgrecv, 0)) == -1)
			printf("error requestRebind:recvmsg() errno:%d\n", errno);

        }while (n < sizeof(struct hdr) || recvhdr.seq != conn->header.seq || recvhdr.msgType != MSG_PORT);
	
	alarm(0);
        rtt_stop(&rttinfo, rtt_ts(&rttinfo) - recvhdr.ts);

	printf("Read from listening port: %s\n", buf);
	struct sockaddr_in sockAddr;
	socklen_t len = sizeof(sockAddr);
        if (getpeername(conn->sockfd, (struct sockaddr *)&sockAddr, &len) == -1)
        {
                printf("error requestRebind:getpeername() errno:%d\n", errno);
                return -1;
        }

	sockAddr.sin_port = htons(atoi(buf));
	if((n = connect(conn->sockfd,(SA *)&sockAddr, sizeof(sockAddr))) == -1)
        {
		printf("error requestRebind:connect() errno:%d\n", errno);
		return -1;
	}
	printf("Port rebound successful.\n");
	return 1;
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

/*	
	memset(buf, 0, sizeof(buf));
	sprintf(buf,"test.txt");
        if(write(sockfd, buf, strlen(buf))<0)
        	err_quit("error write() errno:%d\n", errno);
                	
	fd_set  rset;
	FD_ZERO(&rset);
	for ( ; ; )
        {
		FD_SET(sockfd, &rset);
		printf("waiting for the reconnect port from server...\n");
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
				bzero(buf, sizeof(buf));
				n = dg_recv_send(sockfd, buf, sizeof(buf));
				printf("Read from listening port: %s\n", buf);

				memset(buf, 0, sizeof(buf));
				bzero(&sockAddr, sizeof(sockAddr));
				if(recvfrom(sockfd, buf, MAXLINE, 0, (struct sockaddr *)&sockAddr, &len)!=0)
        			{
					printf("read from listening port:%s\n", buf);
					sockAddr.sin_port = htons(atoi(buf));
					printf("Reconnecting server at port:%d\n", atoi(buf));
					if((n = connect(sockfd,(SA *)&sockAddr, sizeof(sockAddr))) == -1)
                				err_quit("error connect() errno:%d\n", errno);
					while(1)
					{
						printf("enter string to send:");
						gets(buf);
						if(strcmp(buf, "exit") == 0)
							exit(0);
				                if(write(sockfd, buf, strlen(buf))<0)
                                			err_quit("error write() errno:%d\n", errno);
					}
				}
			}
		}
	}	
}
*/

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
	
	if(requestRebind(conn) == -1)
		err_quit("Unable to request rebind port from server.\n");	
}
