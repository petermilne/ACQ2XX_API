#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
	char buf[80];
	int fd = open(argv[1], O_RDONLY);
	int rc;

	if (fd < 0){
		fprintf(stderr, "ERROR failed to open %s\n", argv[1]);
		return errno;
	}

	while((rc = read(fd, buf, 80)) >= 0){
		if (rc == 0){
			printf("return 0\n");
			sleep(1);
		}else{
			printf("%d\n", rc);
		}
	}

	printf("ERROR: rc %d errno %d\n", rc, errno);
	perror("xxx");
	return errno;
}
