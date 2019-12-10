/*
 * SHM_Buffer.cpp
 *
 *  Created on: Nov 4, 2011
 *      Author: pgm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <assert.h>
#include <unistd.h>
#include "SHM_Buffer.h"

#define SHM_SIZE	4096U

#define SHM_MAX_ELEMS	(SHM_SIZE/sizeof(long long))


void SHM_Buffer::clear()
{
	char *buf = new char[SHM_SIZE];
	memset(buf, 0, SHM_SIZE);
	write(fd, buf, SHM_SIZE);
	delete [] buf;
}

SHM_Buffer::SHM_Buffer() {
	const char* temp = "/dev/shm/acq_demux-mds-XXXXXX";
	if (getenv("ACQ_DEMUX_SHM")){
		temp = getenv("ACQ_DEMUX_SHM");
	}
	fname = new char[strlen(temp)+1];
	strcpy(fname, temp);
	fd = mkstemp(fname);
	if (fd == -1){
		perror(temp);
		exit(errno);
	}

	clear();
	// mmap it.
	data = (long long*)mmap(0, SHM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	if (data == MAP_FAILED){
		perror("mmap failed");
		exit(errno);
	}
	cursor = data;

	assert(getValue(&refcount));

	*refcount = 1;
}

SHM_Buffer::~SHM_Buffer() {
	bool close_down = (*refcount -= 1) <= 0;

	munmap(data, SHM_SIZE);
	close(fd);

	/* refcount doesn't work ... do not use
	if (close_down){
		unlink(fname);
	}
	*/
	delete [] fname;
}
bool SHM_Buffer::getValue(int** value) {
	if (cursor-data > SHM_MAX_ELEMS){
		return false;
	}else{
		*value = (int*)cursor;
		++cursor;
		return true;
	}
}
bool SHM_Buffer::getValue(long long** value){
	if (cursor-data > SHM_MAX_ELEMS){
		return false;
	}else{
		*value = (long long*)cursor;
		++cursor;
		return true;
	}
}
SHM_Buffer& SHM_Buffer::instance() {
	static SHM_Buffer* _instance;

	if (!_instance){
		_instance = new SHM_Buffer();
	}else{
		*_instance->refcount += 1;
	}
	return *_instance;
}

void SHM_Buffer::print() {
	printf("pid:%5d \"%s\" refcount:%d nelems:%d\n",
			getpid(), fname, refcount, cursor-data-1);
}
