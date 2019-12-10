/*
 * CommandBuffer.cpp
 *
 *  Created on: Jul 27, 2010
 *      Author: pgm
 */


#include <assert.h>
#include <iostream>
#include <map>
#include <vector>
#include <list>

#include "local.h"


#include <stdlib.h>
#include <libgen.h>	/* dirname() */
#include <unistd.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "CommandBuffer.h"
#include "iclient3.h"
#include <sys/select.h>

#include "AcqType.h"		// File
using namespace std;

class CommandBufferImpl : public CommandBuffer {
	list<char *> pool;
	list<char *> queue;
	int maxlen;

	unsigned discards;
	unsigned processed;
	unsigned no_data_waiting;
	unsigned blocked_on_read;

	unsigned hitide;

	int fd;
	fd_set inset;
	struct timeval timeout;
	enum POLICY { POL_LOWLAT, POL_NODROP } policy;

	char *getBuffer()
	/**< return from pool, else steal one from queue. */
	{
		char *rc;

		if (!pool.empty()){
			rc = pool.front();
			pool.pop_front();
		}else{
			assert(!queue.empty());
			++discards;
			rc = queue.front();
			queue.pop_front();
		}
		return rc;
	}
	bool inputDataAvailable() {
		struct timeval timeout2 = timeout;
		FD_SET(fd, &inset);
		int rc = select(fd+1, &inset, 0, 0, &timeout2);

		if (rc < 0){
			perror("select failed");
			_exit(errno);
		}
		return rc == 1 && FD_ISSET(fd, &inset);
	}

	void readLinePushQueue() {
		char *lm_line = getBuffer();
		int rc;

		memset(lm_line, 0, maxlen);

		if ((rc = read(fd, lm_line, maxlen-1)) > 0){
			chomp(lm_line);
			dbg(1, "\"%s\"", lm_line);
			queue.push_back(lm_line);
		}else if (rc < 0){
			perror("command read error");
			_exit(errno);
		}else{
			err("read returned 0");
		}
	}
public:
	CommandBufferImpl(const char* fname, int _maxlen = 80, int maxq=4) :
		pool(), queue(), maxlen(_maxlen),
		discards(0), processed(0),
		no_data_waiting(0), blocked_on_read(0), hitide(0)
	{
		memset(&timeout, 0, sizeof(timeout));
		for (int ibuf = 0; ibuf < maxq; ++ibuf){
			pool.push_back(new char [maxlen]);
		}
		if (maxq > 2){
			policy = POL_NODROP;
		}else{
			policy = POL_LOWLAT;
		}

		fd = open(fname, O_RDWR);
		if (fd < 0){
			err("failed to open \"%s\"", fname);
			perror("");
			_exit(errno);
		}
		FD_ZERO(&inset);

		dump(FN);
	}
	~CommandBufferImpl() {
		while(!queue.empty()){
			delete [] queue.front();
			queue.pop_front();
		}
		while (!pool.empty()){
			delete [] pool.front();
			pool.pop_front();
		}
		close(fd);
	}

	void getNext(char* ubuf, int _maxlen) {
		bool nodata = true;

		/* first, clear any waiting commands */

		while(inputDataAvailable()){
			nodata = false;

			if (policy == POL_NODROP && pool.empty()){
				break;
			}else{
				readLinePushQueue();
			}
		}
		/* if we don't have a command, block until one shows */
		if (queue.empty()){
			readLinePushQueue();
			blocked_on_read++;
		}

		if (queue.size() > hitide){
			hitide = queue.size();
		}

		/* pop the next command in queue and output it */
		char *cmd = queue.front();
		queue.pop_front();
		strncpy(ubuf, cmd, _maxlen);

		dbg(1, "p:%d q:%d \"%s\"", pool.size(), queue.size(), cmd);

		pool.push_back(cmd);
		++processed;


		if (nodata){
			++no_data_waiting;
		}
	}
	void dump(const char *user) {
		char buf[80];
		char *cp;
		const char *root = "";

		if ((cp = getenv("ACQ_DEMUX_COMMAND_LOG")) == 0){
			sprintf(buf, "acq_demux.%d.log", getpid());
			cp = buf;
			root = "/dev/shm";
		}

		File log(root, cp, "w");
		fprintf(log.getFp(),
			"q:%d hitide:%d max:%d proc:%u/%u disc:%u "
			"no_data_waiting:%u blocked_on_read %u: %s\n",
			queue.size(), hitide, maxlen,
			processed, discards+processed,
			discards,
			no_data_waiting, blocked_on_read, user);
	}
	virtual int writeBack(char* ubuf){
		dbg(1, "\"%s\"", ubuf);
		return write(fd, ubuf, strlen(ubuf));
	}
	virtual bool hasDataAvailable(void) {
		return !queue.empty() || inputDataAvailable();
	}
};

class CommandBufferDbg: public CommandBuffer {
	FILE *fp;
public:
	CommandBufferDbg(const char *fname){
		fp = fopen(fname, "r");
		assert(fp != 0);
	}
	virtual ~CommandBufferDbg() {
		fclose(fp);
	}
	void getNext(char* ubuf, int _maxlen) {
		if (fgets(ubuf, _maxlen, fp) == 0){
			info("quitting time!");
			_exit(0);
		}
	}

	virtual bool hasDataAvailable(void) {
		return true;
	}
};
CommandBuffer* CommandBuffer::create(
			const char* fname, int _maxlen, int maxq)
{
	if (strncmp(fname, "/dev/", 5)){
		return new CommandBufferDbg(fname);
	}else{
		return new CommandBufferImpl(fname, _maxlen, maxq);
	}
}

