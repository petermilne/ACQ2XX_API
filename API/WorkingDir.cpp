/*
 * WorkingDir.cpp
 *
 *  Created on: Feb 5, 2011
 *      Author: pgm
 */

#include "local.h"

#include <string>

#include <stdlib.h>
#include <libgen.h>	/* dirname() */
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "WorkingDir.h"

using namespace std;

static char *__mymkdir(const char *arg1, const char* suffix)
{
	char* dir = new char[256];
	char tmp[256];

	dir[0] = '\0';


	if (WorkingDir::outbase){
		strcpy(dir, WorkingDir::outbase);
	}
	if (arg1 != 0){
		if (WorkingDir::outbase){
			strcpy(tmp, arg1);
			strcat(dir, basename(tmp));
		}else{
			strcpy(dir, arg1);
		}
	}
	strcat(dir, suffix);
	if (mkdir(dir, 0777) == 0){
		return dir;
	}else if (errno == EEXIST){
		if (WorkingDir::cleanup){
			string s = "rm -Rf ";
			s += dir;
			s += "/*";
			system(s.c_str());
		}
		return dir;
	}else{
		sprintf(tmp, "mkdir %s failed", dir);
		exit(-errno);
	}
}

static void rmrf(const char* path)
/* BEWARE: this could easily trash your system */
{
	const char* FMT = "rm -Rf %s";
	char *cmd = new char[strlen(path)+strlen(FMT)];

	sprintf(cmd, FMT, path);
	system(cmd);
}

#define COOKING	".COOKING"
#define COOKED  ".COOKED"


WorkingDir::WorkingDir(const char *arg1)
{
	wd = __mymkdir(arg1, use_cooking? COOKING: COOKED);
	printf("WorkingDir:%s\n", wd);
}
WorkingDir::WorkingDir(const char* arg1, bool absolute){
	if (use_cooking){
		printf( "ERROR: use_cooking set, reset it\n");
		use_cooking = 0;
	}
	if (use_cooking){
		wd = new char [strlen(arg1)+strlen(COOKING)+1];
		sprintf(wd, "%s%s", arg1, COOKING);
	}else{
		wd = new char [strlen(arg1)+strlen(COOKED)+1];
		sprintf(wd, "%s%s", arg1, COOKED);
	}
	printf("WorkingDir call mkdir(%s)", wd);
	if (mkdir(wd, 0777)){
		err("failed to create wd \"%s\"", wd);
		exit(errno);
	}
}

WorkingDir::~WorkingDir() {
	char cname[128];

	if (use_cooking){
		char *idx = strstr(wd, COOKING);
		if (!idx){
			err("FAILED to find %s suffix", COOKING);
		}else{
			int rootlen = idx - wd;
			strncpy(cname, wd, rootlen);
			cname[rootlen] = '\0';
			strcat(cname, COOKED);
			rmrf(cname);
			rename(wd, cname);
		}
	}
	delete [] wd;
}

const char* WorkingDir::outbase = 0;
bool WorkingDir::cleanup = 0;
bool WorkingDir::use_cooking = true;
bool WorkingDir::FRIGGIT = false;
