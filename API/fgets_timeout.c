#include <stdio.h>
#include <poll.h>

#include <stdlib.h>

int fgets_t(char *s, int size, FILE* stream, int timeout)
/* returns >1 on data, 0 on timeout, <0 on error */
{
	struct pollfd pollfd;
	int rc;

	pollfd.fd = fileno(stream);
	pollfd.events = POLLIN;

	rc = poll(&pollfd, 1, timeout);
	if (rc > 0){
		return fgets(s, size, stream);
	}else{
		return rc;
	}
}

#ifdef TESTHARNESS
int main(int argc, char *argv[]){
	char buf[80];
	int timeout = argc>1? atoi(argv[1]): 0;

	while(fgets_t(buf, 80, stdin, timeout)){
		printf("input:\"%s\"\n", buf);
	}
	printf("done\n");
	return 0;
}
#endif

