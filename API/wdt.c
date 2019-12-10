/*
 * wdt.c : watchdog timer process
 * wdt time-msec parent [parent]
 * monitors stdin
 * '1' => enable
 * '0' => disable
 * If enabled and timeout => kill parents
 */

#include "local.h"
#include <sys/types.h>
#include <signal.h>

extern int fgets_t(char *s, int size, FILE* stream, int timeout);

int wdt(int timeout_ms)
/* returns != 0 if wdt happened */
{
	int enabled = 0;
	char buf[80];

	while(1){
		int rc = fgets_t(buf, 80, stdin, timeout_ms);

		if (rc > 0){
			switch(buf[0]){
			case '1':
				enabled = 1; break;
			case '0':
				enabled = 0; break;
			}
		}else if (rc == 0){
			if (enabled){
				return 1;
			}
		}else{
			return rc;
		}
	}
}


int main(int argc, char* argv[]){
	int timeout;
	char** pids;
	int npids;
	int rc;
	pid_t signal = getenv("WDT_SIGNAL")? atoi(getenv("WDT_SIGNAL")): SIGKILL;

	if (argc < 3){
		err("USAGE: wdt timeout pid [pid]");
		return 1;
	}

	timeout = atoi(argv[1]);
	pids = &argv[2];
	npids = argc-2;

	if ((rc = wdt(timeout)) > 0){
		int ii = 0;
		for (ii = 0; ii != npids; ++ii){
			kill(atoi(pids[ii]), signal);
		}
	}
	return rc;
}
