/* ------------------------------------------------------------------------- */
/* file Dt100Transport.cpp			                             */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2008 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of Version 2 of the GNU General Public License
    as published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/** @file Dt100Transport.cpp defines the dt100 Transport.
 *  ACQ2xx cards offer the "dt100 service" at port 0xd100
 *  This Transport controls the card using the dt100 service
 */
#include "local.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "acq_transport.h"
#include "Dt100Transport.h"
#include "Frame.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/uio.h>

#include "iclient3.h"

#define MASTER_TO 30
#define SLAVE_TO  10
#define PUT_TO	  10

int verbose = 0;

static int sample_size = sizeof(short);

class Dt100Transport: public Transport {
	struct State {
		SOCKET cmd_sock;
		SOCKET stat_sock;
		SOCKET stream_sock;

		const char* remotehost;
		const char* remoteport;
		char* my_id;
		char *stream_buf;
		int stream_buf_len;
	} s;

	void init(void) {
		memset(&s, 0, sizeof(s));
	}

/** Transport implementation connects to dt100d service on card. */
public:
	Dt100Transport(const char* id);
	virtual ~Dt100Transport();

	virtual STATUS acqcmd(
		const char* command, char *response, int maxresponse);
	/**< send an "acqcmd" (acquisition command or query) to the card.
	 * @param command - the command (or query) to send
	 * @param response - user buffer to collect response.
	 * @param maxresponse - maximum response size required.
	 */
	
	virtual STATUS acq2sh(
		const char* command, char *response, int maxresponse);
	/**< run a remote shell command or query. 
	 * @param command - the command (or query) to send
	 * @param response - user buffer to collect response.
	 * @param maxresponse - maximum response size required.
	 */
	
	virtual STATUS waitStateChange(
		int timeout, char* response, int maxresponse);
	/**< block until remote state changes or timeout. */

	virtual STATUS readChannel(
		int channel, short* data,
		int nsamples, int start = 0, int stride = 1);
	/**< read and output raw data for channel
         * @param channel - channel number 1..N
	 * @param data - caller's buffer
         * @param nsamples - max samples to read
         * @param start - start sample in data set
	 * @param stride - stride [subsample] value
	 * @returns actual samples returned or STATUS_ERR
	 */	

	/** streaming interface: not all transports can do this. */
	virtual STATUS startStreaming(void);
	virtual STATUS readStreamingFrame(Frame* frame, unsigned id);
	virtual STATUS stopStreaming(void);
};

#define VFPRINTF(fmt...) if (verbose) fprintf(stderr, ## fmt)
#define VFPRINTF2(fmt...) if (verbose > 1) fprintf(stderr, ## fmt)
static unsigned S_BUFLEN = (4096*16);
static char *S_buf[2];



static char signon_command[80];
static const char *prefix;
static struct timeval timeout;
struct timeval zero_time = {};


static void signon(
	int sock, const char *remotedev, const char* mode)
{
	char buf[80];
	int wait_prompt = 1;
#define SIGNON_FMT "dt100 open %s %s\n"	

	readline(sock, buf, sizeof(buf));
	dbg(2,"signon:%s\n", buf);

	if (strcmp(mode, "master") == 0){
		sprintf(signon_command, SIGNON_FMT, mode, remotedev);
		prefix = "acqcmd ";
		timeout.tv_sec = MASTER_TO;
	}else if (strncmp(mode, "data", 4) == 0){
		sprintf(signon_command, SIGNON_FMT, mode, remotedev);
		prefix = "dt100 ";
		timeout.tv_sec = SLAVE_TO;
	}else if (strcmp(mode, "stream") == 0){
		sprintf(signon_command, SIGNON_FMT, "data", remotedev);
		prefix = "dt100 ";
		timeout.tv_sec = SLAVE_TO;		
	}else if (strcmp(mode, "shell") == 0){
		sprintf(signon_command, SIGNON_FMT, "shell", remotedev);
		prefix = "";
		timeout.tv_sec = MASTER_TO;
	}else{ 
		/* mode: get, put */
		sprintf(signon_command, "%s %s\n", mode, remotedev);
		prefix = "";
		timeout.tv_sec = PUT_TO;
		wait_prompt = 0;
	}
	dbg(2, "signon:command:%s", signon_command);
	write(sock, signon_command, strlen(signon_command));
	if (wait_prompt){
		readline(sock, buf, sizeof(buf));
		dbg(2, "signon:response from %d:%s\n", sock, buf);	
	}
}

static SOCKET get_sock(void)
{
	SOCKET sock;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		die(errno, "socket");
	}

	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
		       (char*)&S_BUFLEN, sizeof(S_BUFLEN)) ){
		die(errno, "setsockopt() SO_RCVBUF failed\n");
	}
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF,
		       (char*)&S_BUFLEN, sizeof(S_BUFLEN)) ){
		die(errno, "setsockopt() SO_SNDBUF failed\n");
	}
	return sock;
}


static void connect_to(
	SOCKET sock, const char *remotehost, const char *remoteport)
{
	struct sockaddr_in peer;

 	set_address(remotehost, remoteport, &peer, "tcp" );

	if (connect(sock, (struct sockaddr *)&peer, sizeof(peer))){
		die(errno, "connect failed SOCK" );
	}
}



Dt100Transport::Dt100Transport(const char* id):	
	Transport(id)
{
	init();
	if (getenv("DT100_CONNECT_VERBOSE")){
		verbose = atoi(getenv("DT100_CONNECT_VERBOSE"));
		info("Dt100Transport host %s", id);
	}
	S_buf[0] = (char*)malloc(S_BUFLEN);
	S_buf[1] = (char*)malloc(S_BUFLEN);	

	s.my_id = new char[strlen(id)+1];
	strcpy(s.my_id, id);
	if (index(s.my_id, ':')){
		char *sep = index(s.my_id, ':');
		*sep++ = '\0';
		s.remotehost = s.my_id;
		s.remoteport = sep;
	}else{
		s.remotehost = s.my_id;
		s.remoteport = "0xd100";
	}
}

Dt100Transport::~Dt100Transport() 
{
	if (s.cmd_sock) close(s.cmd_sock);
	delete [] s.my_id;
}
#define ACQCMD "acqcmd "

STATUS Dt100Transport::acqcmd(
	const char* command, char *response, int maxresponse)
/**< send an "acqcmd" (acquisition command or query) to the card.
 * @param command - the command (or query) to send
 * @param response - user buffer to collect response.
 * @param maxresponse - maximum response size required.
 */
{
	char* my_command = new char[strlen(command) + strlen(ACQCMD) + 1];
	strcpy(my_command, ACQCMD);
	strcat(my_command, command);
	STATUS rc = acq2sh(my_command, response, maxresponse);
	delete [] my_command;
	return rc;
}


#define DUMMYCMD "hostname\n"

STATUS Dt100Transport::acq2sh(
	const char* command, char *response, int maxresponse)
/**< run a remote shell command or query. 
 * @param command - the command (or query) to send
 * @param response - user buffer to collect response.
 * @param maxresponse - maximum response size required.
 */
{
	if (!s.cmd_sock){
		s.cmd_sock = get_sock();
		connect_to(s.cmd_sock, s.remotehost, s.remoteport);
		signon(s.cmd_sock, "1", "shell");
	}

	int buflen = MAX(maxresponse+20, 1024);
	char* rxbuf = new char[buflen];
	char *pend;
	int ibuf = 0;
	int rc = -1;



	dbg(1, "->%s", command);
	rc = write(s.cmd_sock, command, strlen(command));
	if (command[strlen(command)-1] != '\n'){
		write(s.cmd_sock, "\n", 1);
	}

	if (rc <= 0){
		die(errno, "write() failed");
	}

	while(ibuf < buflen-1 && 
		(rc = read(s.cmd_sock, rxbuf+ibuf, buflen-ibuf)) > 0){
		ibuf += rc;
		rxbuf[ibuf] = '\0';

		dbg(1, "<-%s", rxbuf);

		if ((pend = strstr(rxbuf, "EOF")) != 0){
			rc = pend - rxbuf;

			int cnum, ecode = 0;
			if (sscanf(pend, "EOF %d %d", &cnum, &ecode) == 2){
				if (ecode > 0){
					rc = -ecode;
				}
			}else{
				err("non conformant EOF \"%s\"", pend);
			}
			*pend = '\0';
			strcpy(response, rxbuf);
			break;
		}
	}


	delete []rxbuf;

	return rc;
}

#define STATCMD "cat /dev/acq200/tblocks/acqstate\n"

STATUS Dt100Transport::waitStateChange(
	int timeout, char* response, int maxresponse)
/**< block until remote state changes or timeout. 
 *  @@todo timeout not implemented
 */
{
	if (!s.stat_sock){
		s.stat_sock = get_sock();
		connect_to(s.stat_sock, s.remotehost, s.remoteport);
		signon(s.stat_sock, "1", "shell");
		write(s.stat_sock, STATCMD, strlen(STATCMD));
	}
	int rc = readline(s.stat_sock, response, maxresponse);
	return rc;
}

#define MAXREAD	0x10000

int readb(int sock, char* buf, int nbytes)
{
	int total;
	int nread;

	for (total = 0; total < nbytes; total += nread){
		dbg(2, "read(%d)", nbytes);
		nread = read(sock, buf+total, nbytes-total);
		dbg(2, "read(%d) returned %d", nbytes-total, nread);
		if (nread < 0){
			return nread;
		} 
	}

	return total;
}
STATUS Dt100Transport::readChannel(
	int channel, short* data,
	int nsamples, int start, int stride)
/**< read and output raw data for channel
 * @param channel - channel number 1..N
 * @param data - caller's buffer
 * @param nsamples - max samples to read
 * @param start - start sample in data set
 * @param stride - stride [subsample] value
 * @returns actual samples returned or STATUS_ERR
 */
{
	char channel_dev[80];
	int remain;
	int nsam;
	int nbytes;
	int rc;

	sprintf(channel_dev, "/dev/acq32/acq32.1.%02d", channel);
	SOCKET dsock = get_sock();
	connect_to(dsock, s.remotehost, s.remoteport);
	signon(dsock, channel_dev, "data1");

	dbg(1, "channel: %d dev: %s signon OK", channel, channel_dev);

	for (remain = nsamples; remain > 0 ; remain -= nsam, start += nsam ){
		nsam = MIN(remain, MAXREAD);
		char command[80];
		char reply[80];

		sprintf(command, "dt100 read %d %d %d\n",
			start, start+nsam, stride);

		dbg(1, "asking:%s", command);

		write(dsock, command, strlen(command));
		rc = readline(dsock, reply, sizeof(reply));

		dbg(1, "dt100:\"%s\"", reply);
		if (rc < 0 ){
			return rc;
		}else if (sscanf(reply, "DT100:%d bytes", &nbytes) == 0){
			err("reply doesn't scan \"%s\"", reply);
			return -10;
		}else{
			dbg(1, "DT100 says read %d bytes", nbytes);
			if (nbytes == 0){
				break;
			}else{
				/* @todo - size hardcoded as 2 bytes - should be wordsize .. short* data? */
				int nb = readb(dsock, (char*)data+2*(nsamples-remain), nbytes);
				nsam = nb/sample_size;
			}		
		}
	}
	dbg(1, "return %d", nsamples-remain);

	return nsamples-remain;
}


/** streaming interface: not all transports can do this. */
STATUS Dt100Transport::startStreaming(void)
{
	return STATUS_WORKTODO;
}

STATUS Dt100Transport::readStreamingFrame(Frame* frame, unsigned id)
{
	if (!s.stream_sock){
		if (s.stream_buf_len < frame->frameSize()){
			if (s.stream_buf){
				delete [] s.stream_buf;
			}
			s.stream_buf = new char[frame->frameSize()];
			s.stream_buf_len = frame->frameSize();
		}

		s.stream_sock = get_sock();
		connect_to(s.stream_sock, s.remotehost, s.remoteport);
		signon(s.stream_sock, "/dev/acq32/acq32.1.01", "stream");

		char command[80];
		sprintf(command, "%s stream 1 0 %d\n", 
				prefix,
				frame->sampleSize()/sizeof(unsigned));
		write(s.stream_sock, command, strlen(command));
	}

/* it's possible that we are readin misaligned from frame start.
 * we can put in code to adjust, but first up, we just try read a whole
 * frame.
 */
	int read_count = 0;
	int nread = 0;
	char *bp = s.stream_buf;
	while (nread < frame->frameSize()){
		int rc = read(s.stream_sock, bp, 
				frame->frameSize() - nread);
		if (rc < 0){
			die(errno, "read() failed");
		}
		read_count++;
		nread += rc;
		bp += rc;
	}

	dbg(1, "nread:%2d read_count:%d\n", nread, read_count);

	return Frame::buildFrame(id, frame, s.stream_buf, nread);
}

STATUS Dt100Transport::stopStreaming(void)
{
	int rc = close(s.stream_sock);
	s.stream_sock = 0;
	
	return rc;
}


Transport* Dt100TransportFactory::createTransport(const char* id)
{
	if (getenv("DT100_SAMPLE_SIZE")){
		sample_size = atoi(getenv("DT100_SAMPLE_SIZE"));
	}
	return new Dt100Transport(id);
}
