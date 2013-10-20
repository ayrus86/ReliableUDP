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

ssize_t dg_recv_send(int fd, void *inbuff, size_t inbytes)
{
	ssize_t n;
        struct iovec iovsend[1], iovrecv[2];
	struct hdr sendhdr, recvhdr;	
	msgrecv.msg_name = NULL;
        msgrecv.msg_namelen = 0;
        msgrecv.msg_iov = iovrecv;
        msgrecv.msg_iovlen = 2;
        iovrecv[0].iov_base = &recvhdr;
        iovrecv[0].iov_len = sizeof(struct hdr);
        iovrecv[1].iov_base = inbuff;
        iovrecv[1].iov_len = inbytes;
	
	do
        {
                n = Recvmsg(fd, &msgrecv, 0);

        }while (n < sizeof(struct hdr));

	return (n-sizeof(struct hdr));	
}


ssize_t udp_send(struct connection* conn, int msgType, char* buf)
{
	ssize_t n;
	struct iovec iovsend[2], iovrecv[1];
	struct hdr recvhdr;

	bzero(&msgsend, sizeof(msgsend));
	bzero(&msgrecv, sizeof(msgrecv));

	if (rttinit == 0)
        {
                rtt_init(&rttinfo);
                rttinit = 1;   
                rtt_d_flag = 1;
        }

	conn->header.seq++;
	conn->header.msgType = msgType;

	msgsend.msg_iov = iovsend;
        msgsend.msg_iovlen = 2;	
	iovsend[0].iov_base = &conn->header;
	iovsend[0].iov_len = sizeof(struct hdr);
	iovsend[1].iov_base = buf;
        iovsend[1].iov_len = sizeof(buf);
	
	iovrecv[0].iov_base = &recvhdr;
        iovrecv[0].iov_len = sizeof(struct hdr);

	Signal(SIGALRM, sig_alrm);
        rtt_newpack(&rttinfo);
        
sendagain:
	conn->header.ts = rtt_ts(&rttinfo); 
        if(sendmsg(conn->sockfd, &msgsend, 0) == -1)
		printf("error udp_send() errno:%d\n", errno);
 
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
                printf("Retransmitting. rtt_rto:%d\n", rttinfo.rtt_rto);
                goto sendagain;
        }
        
        do
        {
                n = Recvmsg(conn->sockfd, &msgrecv, 0);
        }while (n < sizeof(struct hdr) || recvhdr.seq != conn->header.seq);
        
        alarm(0);
        rtt_stop(&rttinfo, rtt_ts(&rttinfo) - recvhdr.ts);
        return (n - sizeof(struct hdr));

}

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
			rttinit = 0;        /* reinit in case we're called again */
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
