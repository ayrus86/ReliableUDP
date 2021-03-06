#include "udp.h" 
#include "unprtt.h" 
#include <setjmp.h>

#define RTT_DEBUG

static void sig_alrm(int signo);
static sigjmp_buf jmpbuf;

static void sig_alrm(int signo)
{
	siglongjmp(jmpbuf, 1);
}


int udp_recv(int sockfd, struct packet_t* packet, struct sockaddr* sockAddr)
{
	int len = sizeof(*sockAddr);
	bzero(sockAddr, len);
	if(recvfrom(sockfd, packet, sizeof(*packet), 0, sockAddr, &len) != -1)
	{
		return 1;
	}
	printf("error udp_recv:recvfrom errno:%s\n", strerror(errno));
	return -1;
}

int udp_send(int sockfd, struct packet_t* packet, struct sockaddr* sockAddr)
{
	int len = sockAddr == NULL? 0 : sizeof(*sockAddr);

	if(sendto(sockfd, packet, sizeof(*packet), 0, sockAddr, len) > 0)
        {
                return 1;
        }
	printf("error udp_send:sendto errno:%s\n", strerror(errno));
        return -1; 
	
}


int deQueue(struct packet_t* packet)  
{
	pthread_mutex_lock(&queMutex);
        if(queueCapacity == queueSize) //read everything nothing to read;
        {
		pthread_mutex_unlock(&queMutex);
	        return -1;
        }
	memcpy(packet, &queue[tail], sizeof(struct packet_t));
        bzero(&queue[tail], sizeof(struct packet_t));
        queueCapacity++;
        tail = (tail+1) % queueSize;
	pthread_mutex_unlock(&queMutex);
        return 1;
}
                
int peekQueueTail(struct packet_t* packet)
{
	pthread_mutex_lock(&queMutex);
        if(queueCapacity == queueSize)
	{
		pthread_mutex_unlock(&queMutex);
                return -1;
        }

	memcpy(packet, &queue[tail], sizeof(struct packet_t));
	pthread_mutex_unlock(&queMutex);
	return 1;
}

int peekQueueHead(struct packet_t* packet)
{
	pthread_mutex_lock(&queMutex);
        if(queueCapacity == 0)
        {
		pthread_mutex_unlock(&queMutex);
	        return -1;
	}
       
        memcpy(packet, &queue[head], sizeof(struct packet_t));
        pthread_mutex_unlock(&queMutex);
	return 1;
}

int enQueue(struct packet_t* packet)
{
	pthread_mutex_lock(&queMutex);
        int n;
        if(queueCapacity == 0)
        {
//		printf("queue locked. head:%d tail:%d\n", head, tail);
		pthread_mutex_unlock(&queMutex);
	        return -1;
        }
	if(queueCapacity == queueSize)
        {
                memcpy(&queue[head], packet, sizeof(struct packet_t));
                queueCapacity--;
		pthread_mutex_unlock(&queMutex);
                return 1;
        }
        else if((n = packet->seq - queue[head].seq) == 1)
        {
                head = (head+1)%queueSize;
                memcpy(&queue[head], packet, sizeof(struct packet_t));
                queueCapacity--;
                
                while((head+1)%queueSize!=tail && queue[(head+1)%queueSize].seq!=0)
                {
//			printf("-------moving tail :%d head:%d queue[head+1].seq:%d-------\n", tail, head, queue[(head+1)%queueSize].seq);
                        head = (head+1)%queueSize;
                        queueCapacity--;
                }
		
		pthread_mutex_unlock(&queMutex);
		return 1;
        }
        else if(n > 0 && n <= queueCapacity)
        {
                memcpy(&queue[(head+n)%queueSize], packet, sizeof(struct packet_t));
		pthread_mutex_unlock(&queMutex);
                return -1;
        }
                
	pthread_mutex_unlock(&queMutex);
        return -1;
}

