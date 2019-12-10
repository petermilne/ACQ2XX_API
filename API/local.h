#define FN __PRETTY_FUNCTION__
#include "../include/local.h"
#include <unistd.h>

	
static inline void die(int errcode, const char *s)
{
        perror(s);
        _exit(errcode);
}

static inline long getenvInt(const char* key, long default_value)
{
	const char* pv = getenv(key);

	if (pv){
		return strtol(pv, 0, 0);
	}else{
		return default_value;
	}
}
#define STATUS_WORKTODO -42
