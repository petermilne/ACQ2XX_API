#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include "SHM_Buffer.h"


int main(int argc, char* argv[])
{
	long long* count;
	
	assert(SHM_Buffer::instance().getValue(&count));
	
	bool child = fork() == 0;
		
	SHM_Buffer::instance().print();
	sleep(1);
	for (int ii = 0; ii < 100; ++ii){
		if (child){
			*count += 1;
		}
		printf("%d %lld\n", getpid(), *count);
		usleep(100000);
	}
}