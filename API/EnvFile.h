/*
 * EnvFile.h
 *
 *  Created on: Nov 3, 2011
 *      Author: pgm
 */

#ifndef ENVFILE_H_
#define ENVFILE_H_

#define ENVFILE_MAX	4096

class EnvFile {
	char *buf;
	const char* fname;
public:
	EnvFile(const char* _fname);
	virtual ~EnvFile();

	bool getValue(const char* key, int& value);
	bool getValue(const char* key, long long& value);

	const char* getFname(void) { return fname; }
};

#endif /* ENVFILE_H_ */
