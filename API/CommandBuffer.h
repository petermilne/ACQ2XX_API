/*
 * CommandBuffer.h
 *
 *  Created on: Jul 24, 2010
 *      Author: pgm
 */

#ifndef COMMANDBUFFER_H_
#define COMMANDBUFFER_H_

class CommandBuffer {

protected:
	CommandBuffer() {}
public:
	virtual ~CommandBuffer() {}

	virtual void getNext(char* ubuf, int _maxlen) = 0;
	virtual void dump(const char *user) {}

	virtual int writeBack(char *ubuf) {
		return -1;
	}

	static CommandBuffer* create(
			const char* fname, int _maxlen = 80, int maxq=4);

	virtual bool hasDataAvailable(void) = 0;
};
#endif /* COMMANDBUFFER_H_ */
