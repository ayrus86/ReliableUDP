#include    "unprtt.h"

int     rtt_d_flag = 0;         /* debug flag; can be set by caller */

/*
 * Calculate the RTO value based on current estimators:
 *      smoothed RTT plus four times the deviation
 */

//#define RTT_RTOCALC(ptr) ((ptr)->rtt_srtt + (4.0 * (ptr)->rtt_rttvar))

static int rtt_minmax(int rto)
{
	if (rto < RTT_RXTMIN)
		rto = RTT_RXTMIN;
	else if (rto > RTT_RXTMAX)
		rto = RTT_RXTMAX;
	return (rto);
}

void rtt_init(struct rtt_info *ptr)
{
	struct timeval tv;
	Gettimeofday(&tv, NULL);
	ptr->rtt_base = tv.tv_sec;   /* # sec since 1/1/1970 at start */
	ptr->rtt_rtt = 0;
	ptr->rtt_srtt = 0;
	ptr->rtt_rttvar = 0;
	ptr->rtt_rto = 0;
	ptr->rtt_rto = rtt_minmax(ptr->rtt_rto); /* first RTO at (srtt + (4 * rttvar)) = 3 seconds */
	printf("in rtt_init(). rtt_rto:%d\n", ptr->rtt_rto);
}

uint32_t rtt_ts(struct rtt_info *ptr)
{
	uint32_t ts;
	struct timeval tv;
	Gettimeofday(&tv, NULL);
	ts = ((tv.tv_sec - ptr->rtt_base) * 1000000) + (tv.tv_usec);
	return (ts);
}

void rtt_newpack(struct rtt_info *ptr)
{
	ptr->rtt_nrexmt = 0;
}

int rtt_start(struct rtt_info *ptr)
{
	return  ptr->rtt_rto;
	//((int) (ptr->rtt_rto + 0.5));    /* round float to int */
}


void rtt_stop(struct rtt_info *ptr, uint32_t ms)
{
	ptr->rtt_rtt = ms; /* measured RTT in microseconds */
	ptr->rtt_rtt -= (ptr->rtt_srtt>>3);
	ptr->rtt_srtt += ptr->rtt_rtt;
	if(ptr->rtt_rtt < 0)
		ptr->rtt_rtt = -ptr->rtt_rtt;
	ptr->rtt_rtt -= (ptr->rtt_rttvar>>2);
	ptr->rtt_rttvar += ptr->rtt_rtt;
	ptr->rtt_rto = (ptr->rtt_srtt>>3) + ptr->rtt_rttvar;
}

int rtt_timeout(struct rtt_info *ptr)
{
	ptr->rtt_rto *= 2;          /* next RTO */
	ptr->rtt_rto = rtt_minmax(ptr->rtt_rto);
	if (++ptr->rtt_nrexmt > RTT_MAXNREXMT)
		return (-1);            /* time to give up for this packet */
	return (0);
}
