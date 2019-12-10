#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inotifytools/inotifytools.h>
#include <inotifytools/inotify.h>

/*
 * libinotifytools example program.
 * Compile with gcc -linotifytools example.c
 */

#define DIR_WATCH_FMT	"DIR_WATCH_FMT"
#define DIR_WATCH_EVENT "DIR_WATCH_EVENT"

int main(int argc, char* argv[]) {
	const char* wd = argc > 1? argv[1]: ".";
	char fmt[80];
	unsigned long trig = IN_CLOSE_WRITE;

	if (getenv(DIR_WATCH_FMT)){
		strcpy(fmt, getenv(DIR_WATCH_FMT));
		strcat(fmt, "\n");
	}else{
		strcpy(fmt, "%T %w%f %e\n"); 
	}
	if (getenv(DIR_WATCH_EVENT)){
		trig = strtoul(getenv(DIR_WATCH_EVENT), 0, 0);
	}

        // initialize and watch the entire directory tree from the current working
        // directory downwards for all events
        if ( !inotifytools_initialize()
          || !inotifytools_watch_recursively( wd, trig) ) {
                fprintf(stderr, "%s\n", strerror( inotifytools_error() ) );
                return -1;
        }

        // set time format to 24 hour time, HH:MM:SS
        inotifytools_set_printf_timefmt( "%T" );

        // Output all events as "<timestamp> <path> <events>"
        struct inotify_event * event = inotifytools_next_event( -1 );
        while ( event ) {
                inotifytools_printf( event, fmt );
		fflush(stdout);
                event = inotifytools_next_event( -1 );
        }
	return 0;
}
