/* udpsimpletimer.c - simple udp server which collects latency information.
 *
 * This is based on the simple udp server found in the 4.3BSD IPC tutorial.
 * This version receives datagrams from multiple senders, and compiles
 * some statistics about their latency. It assumes the datagrams have
 * timestamps at the beginning of the data portion. The timestamps
 * themselves are just struct timeval, in host byte order, as produced by
 * gettimeofday().
 *
 * Packets that contain these timestamps can be produced by sendip.
 * Sample call:
 * 	sendip -l 2000 -p ipv4 -is 10.1.2.0/24 -id 10.2.3.4 \
 * 		-p udp -us r2 -ud 5000 -d t72 10.2.3.4
 * This will produce 2000 udp packets, with random 10.1.2.0/24 source
 * addresses and random source ports, and send them to 10.2.3.4:5000.
 * The packets will have 72-byte payloads, with the timestamp at the
 * beginning of the payload. The size of the timestamp is system
 * dependent; on 64-bit Linux systems, it's 16 bytes (2 8-bit integers).
 *
 * This version doesn't separate by source address, but simply lumps
 * all the packets together.
 *
 * Usage: udpsimpletimer <port>
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory.h>
#include <math.h>

/* 8191 - 1  - I keep these allocations at 16K each, as I have some
 * vague ideas about making a faster allocator for them.
 */
#define TIMENTRYSIZE	8191
typedef struct _timentry {
	u_int16_t used;
	u_int16_t times[TIMENTRYSIZE];
} Timentry;

/* Makes for a total of a bit over 32 M entries */
#define TABLESIZE	4097
typedef struct _tstore {
	u_int16_t used;
	Timentry *t[TABLESIZE];
} Tstore;


Tstore tstore;


Timentry *newtimentry(void)
{
	Timentry *answer;

	answer = (Timentry *)malloc(sizeof(struct _timentry));
	answer->used = 0;
	return answer;
}

void
storetimentry(int delaytime)
{
	int i;
	Timentry *te;

	/* If we fall off the end, we just ignore the entry */
	if (tstore.used > TABLESIZE) return;

	if (!tstore.used || tstore.t[tstore.used-1]->used >=TIMENTRYSIZE) {
		tstore.t[tstore.used] = newtimentry();
		++tstore.used;

	} 
	te = tstore.t[tstore.used-1];
	te->times[te->used] = delaytime;
	++te->used;
}

int 
timestats(double *mu, double *sigma, double *rho)
{
	int i, j;
	int n=0;
	double sumsquare=0.0, sum=0.0, top=0.0;
	double sigma2=0.0;
	double prev;
	Timentry *te;

	for (i=0; i < tstore.used; ++i) {
		te = tstore.t[i];
		n += te->used;
		for (j=0; j < te->used; ++j) {
			sumsquare += (double)te->times[j]*te->times[j];
			sum += (double)te->times[j];
		}
	}
	*mu = sum/(double)n;
	*sigma = sqrt((sumsquare - (double)n*(*mu)*(*mu))/(double)(n-1));

	prev = *mu;
	for (i=0; i < tstore.used; ++i) {
		te = tstore.t[i];
		for (j=0; j < te->used; ++j) {
			top += ((double)te->times[j] - *mu)*
				(prev - *mu);
			sigma2 += (prev - *mu) * (prev - *mu);
			prev = (double)te->times[j];
		}
	}
	*rho = top/sigma2;
	return n;
}

int
printtime(int pnum)
{
	int n;
	double mu, sigma, rho;

	n = timestats(&mu, &sigma, &rho);
	printf("%d packets, %d entries: mu %6.4f sigma %6.4f rho %6.4f", pnum, n, mu, sigma, rho);

	printf("\n");
	fflush(stdout);
	return n;
}

main(int argc, char **argv)
{
	int sock, length;
	struct sockaddr_in name, from;
	struct timeval t, now, waittime;
	int cc, pnum, lastpnum;
	fd_set dset;
	uint16_t port;

	/* Create socket from which to read. */
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("opening datagram socket");
		exit(1);
	}
	/* Create name with wildcards. */
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = INADDR_ANY;
	if (argc > 1)
		port = atoi(argv[1]);
	else
		port = 5000;
	name.sin_port = htons(port);
	if (bind(sock, (struct sockaddr *)&name, sizeof(name))) {
		perror("binding datagram socket");
		exit(1);
	}
	length = sizeof(struct sockaddr_in);

	/* We sit in a loop, waiting for datagrams. Every ten seconds,
	 * we wake up and print out statistics.
	 */
	lastpnum=0;
	for (pnum=0; ;) {
		FD_ZERO(&dset);
		FD_SET(sock, &dset);
		waittime.tv_sec = 10;
		if (select(sock+1, &dset, 0, 0, &waittime) < 0) {
			perror("select");
			continue;
		}
		if (FD_ISSET(sock, &dset)) {
			/* Read from the socket. We expect a timestamp in the
			* initial data portion
			*/
			cc = recvfrom(sock, (void *)&t, sizeof(t), 0,
				(struct sockaddr *)&from, &length);
			if (cc <= 0) {
				perror("receiving datagram packet");
				break;
			}
			gettimeofday(&now, NULL);
			storetimentry(1000000*(now.tv_sec-t.tv_sec)+
					now.tv_usec-t.tv_usec);
			++pnum;
		} else {
			/* timeout; dump data if new */
			if (pnum != lastpnum)
				(void) printtime(pnum);
			lastpnum = pnum;
		}
	}
	close(sock);
}
