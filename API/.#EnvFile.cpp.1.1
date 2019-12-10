/*
 * EnvFile.cpp
 *
 *  Created on: Nov 3, 2011
 *      Author: pgm
 */

#include "local.h"

#include "EnvFile.h"

EnvFile::EnvFile(const char* fname) {
	buf = new char[ENVFILE_MAX];
	FILE *fp = fopen(fname, "r");

	int nb = fread(buf, 1, ENVFILE_MAX, fp);
	if (nb <= 0){
		err("bad return from fread()");
	}
	buf[nb] = '\0';
	fclose(fp);
}

EnvFile::~EnvFile() {
	delete [] buf;
}

const char* findKey(const char* buf, const char* key)
{
	char key2[80];
	sprintf(key2, "%s=", key);
	const char* cursor = strstr(buf, key2);
	if (cursor != 0){
		return cursor+strlen(key2);
	}else{
		return 0;
	}
}

bool EnvFile::getValue(const char* key, int& value)
{
	const char *vp = findKey(buf, key);
	if (vp){
		int xx;
		if (sscanf(vp, "%d", &xx) == 1){
			value = xx;
			return true;
		}
	}

	return false;
}
bool EnvFile::getValue(const char* key, long long& value)
{
	const char *vp = findKey(buf, key);
	if (vp){
		long long xx;
		if (sscanf(vp, "%lld", &xx) == 1){
			value = xx;
			return true;
		}
	}

	return false;
}
