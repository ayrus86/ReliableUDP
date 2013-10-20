#include "udp.h"
#include    "unprtt.h"
#include    <setjmp.h>

#define RTT_DEBUG

static struct rtt_info rttinfo;
static int rttinit = 0;
static void sig_alrm(int signo);
static sigjmp_buf jmpbuf;

static void sig_alrm(int signo)
{
	siglongjmp(jmpbuf, 1);
}

int udp_recv(int sockfd, struct packet_t* packet, struct sockaddr* sockAddr)
{
	int len = sizeof(*sockAddr);
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

/*
ssize_t dg_send_recv(int fd, const void *outbuff, size_t outbytes, void *inbuff, size_t inbytes, const SA *destaddr, socklen_t destlen)
{
	ssize_t n;
	struct iovec iovsend[2], iovrecv[2];

	struct hdr sendhdr, recvhdr;

	if (rttinit == 0) 
	{
		rtt_init(&rttinfo);
		rttinit = 1;
		rtt_d_flag = 1;
	}

	sendhdr.seq++;
	msgsend.msg_name = destaddr;
	msgsend.msg_namelen = destlen;
	msgsend.msg_iov = iovsend;
	msgsend.msg_iovlen = 2;
	
	iovsend[0].iov_base = &sendhdr;
	iovsend[0].iov_len = sizeof(struct hdr);
	iovsend[1].iov_base = outbuff;
	iovsend[1].iov_len = outbytes;
	printf("sending msg:%s\n", outbuff);


	msgrecv.msg_name = NULL;
	msgrecv.msg_namelen = 0;
	msgrecv.msg_iov = iovrecv;
	msgrecv.msg_iovlen = 2;

	iovrecv[0].iov_base = &recvhdr;
	iovrecv[0].iov_len = sizeof(struct hdr);
	iovrecv[1].iov_base = inbuff;
	iovrecv[1].iov_len = inbytes;
	
	Signal(SIGALRM, sig_alrm);
	rtt_newpack(&rttinfo);

sendagain:
	sendhdr.ts = rtt_ts(&rttinfo);
	Sendmsg(fd, &msgsend, 0);
	
	alarm(rtt_start(&rttinfo));
	
	if (sigsetjmp(jmpbuf, 1) != 0)
	{
		if (rtt_timeout(&rttinfo) < 0)
		{
			err_msg("dg_send_recv: no response from client, giving up");
			rttinit = 0;
			errno = ETIMEDOUT;
			return (-1);
		}
		printf("Retransmitting. rtt_rto:%d\n", rttinfo.rtt_rto);
		goto sendagain;
	}

	do
	{
		n = Recvmsg(fd, &msgrecv, 0);
	}while (n < sizeof(struct hdr) || recvhdr.seq != sendhdr.seq);
	
	alarm(0);
	rtt_stop(&rttinfo, rtt_ts(&rttinfo) - recvhdr.ts);
	return (n - sizeof(struct hdr)); 
}
*/
