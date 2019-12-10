/*
 * SHM_Buffer.h
 *
 *  Created on: Nov 4, 2011
 *      Author: pgm
 */

#ifndef SHM_BUFFER_H_
#define SHM_BUFFER_H_

class SHM_Buffer {
public:
	virtual ~SHM_Buffer();
	static SHM_Buffer& instance();

	bool getValue(int** value);
	bool getValue(long long** value);

	void print();
private:
	SHM_Buffer();

	char* fname;
	int fd;
	long long* data;
	long long* cursor;
	int* refcount;
	void clear();

};

#endif /* SHM_BUFFER_H_ */

