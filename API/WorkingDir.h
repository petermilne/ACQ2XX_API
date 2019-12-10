/*
 * WorkingDir.h
 *
 *  Created on: Feb 5, 2011
 *      Author: pgm
 */

#ifndef WORKINGDIR_H_
#define WORKINGDIR_H_

#define ABSDIR	true

class WorkingDir {
	char* wd;
public:
	WorkingDir(const char *arg1);
	WorkingDir(const char* arg1, bool absolute);

	~WorkingDir();
	const char* name() const {
		return wd;
	}
	static const char* outbase;
	static bool cleanup;
	static bool use_cooking;
	static bool FRIGGIT;
};
#endif /* WORKINGDIR_H_ */
