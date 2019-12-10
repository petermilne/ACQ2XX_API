/* ------------------------------------------------------------------------- */
/* file streamer.cpp							     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2009 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of Version 2 of the GNU General Public License
 as published by the Free Software Foundation;

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		     */
/* ------------------------------------------------------------------------- */

/** @file streamer.cpp example application streams data from dt100d port.
 - program auto-detects card type:
 - nchannels 32/64/96
 - word_size 2/3/4
 - lock-in or normal
 - IF lock-in : reference selection

 - default output option prints mean (64) value of each channel in columns.
 - use cut to select channels of interest.
 - it's trivial to collect an bunch of data to plot in a math tool like gnu-octave.
 - for large volume data collection, use "-o dirfile" option.
  - DirFile is a simple binary file format 
  - essentially a directory with a binary file per channel.
  - Format:http://getdata.sourceforge.net/dirfile.html 
  - Plot:http://kst.kde.org/handbook/index.html

<pre>
Usage: streamer [OPTION...]
  -f, --device=STRING
  -b, --board=STRING
  -v, --verbose=INT
  --version
  -T, --showtags
  -x, --showhex
  -n, --nomean
  -m, --mean
  -o dirfile : output in DirFile format (use kst to view)
  --time-frames

Help options:
  -?, --help              Show this help message
  --usage                 Display brief usage message


Example usage:
[peter@rhum API]$  ./x86/bin/streamer -T -f 192.168.1.216 | cut -f 1-8 | head -n 5
         0	           0	-17067	-18570	     0	     0	-17009	-18589
         1	           0	-22293	-24254	     0	    -1	-22217	-24279
         2	           0	-22290	-24256	     0	    -1	-22216	-24282
         3	           0	-22290	-24257	     0	    -1	-22216	-24282
         4	           0	-22291	-24255	     0	    -1	-22217	-24281

</pre>
 */

#include <popt.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>

#include "local.h"
#include "acq2xx_api.h"
#include "acq_transport.h"
#include "AcqType.h"
#include "Frame.h"

#include "DataStreamer.h"
#include "DirfileFrameHandler.h"

int acq200_debug;
int timeout;
char* device;
int do_shell;

#define RETERR(cmd) if (STATUS_ERR(rc = cmd)) {		\
	err("failed: \"%s\" code:%d", #cmd, rc);	\
	return rc;	\
}



static int show_mean = 1;
static int format_hex;
static int show_tags;
static char* outbase;

static int time_frames;

#define US	1000000

class FrameTimer : public FrameHandler {
	struct timeval old_time;
	unsigned old_frame;
	unsigned long long old_sample;


	double tv_difftime(struct timeval& old_time, struct timeval& new_time)
	{
		double dt = (new_time.tv_sec - old_time.tv_sec) * US;

		if (old_time.tv_usec > new_time.tv_usec){
			dt -= US;
			dt += (US + new_time.tv_usec) - old_time.tv_usec;
		}else{
			dt += new_time.tv_usec - old_time.tv_usec;
		}
		return dt/US;
	}

public:
	FrameTimer() {
		old_time.tv_sec = old_time.tv_usec = 0;
	        old_frame = 0;
		old_sample = 0;
		fprintf(stderr, "FrameTimer: ID\tSAMPLE\tDT\tFPS\n");
	}
	virtual void onFrame(
			Acq2xx& _card, const AcqType& _acqType,
			const Frame* frame)
	{
		struct timeval new_time;
		gettimeofday(&new_time, 0);
		unsigned long long new_sample = frame->getStartSampleNumber();
		double dt, fps;
		unsigned new_frame = frame->getID();

		if (old_time.tv_sec == 0){
			dt = fps = 0;
		}else{
			dt = tv_difftime(old_time, new_time);
			fps = (new_frame - old_frame)/dt;
		}
		printf("%5d\t%10lld\t%6.4f\t%6.2f\t", 
		       new_frame, new_sample, dt, fps);
		old_time = new_time;
		old_sample = new_sample;
		old_frame = new_frame;
	}
};

int main(int argc, const char** argv)
{
	struct poptOption opt_table[] = {
		{ "device",     'f', POPT_ARG_STRING, &device,		'f' },
		{ "board",      'b', POPT_ARG_STRING, &device,		'b' },
		{ "verbose",    'v', POPT_ARG_INT,    &acq200_debug,	0   },
		{ "version",    0,   POPT_ARG_NONE,   0,		'V' },
		{ "showtags",   'T', POPT_ARG_INT,   &show_tags,	'T'  },
		{ "showhex",	'x', POPT_ARG_NONE,   0,                'x' },
		{ "nomean",     'n', POPT_ARG_NONE,   0,		'n' },
		{ "mean",       'm', POPT_ARG_INT,   &show_mean,        'm' },
		{ "outbase",    'o', POPT_ARG_STRING, &outbase,         0 },
		{ "time-frames", 0,  POPT_ARG_INT,    &time_frames,     0 },
		POPT_AUTOHELP
		POPT_TABLEEND		
	};
	int rc;
	poptContext opt_context = 
		poptGetContext(argv[0], argc, argv, opt_table, 0);

	while ( (rc = poptGetNextOpt(opt_context)) > 0 ){
		switch(rc){
		case 'x':
			format_hex = 1;
			break;

		default:
			break;
		}
	}	

	if (device == 0){
		err("streamer [opts]");
		return 1;
	}

	Transport *t = Transport::getTransport(device);	
	Acq2xx card(t);

	DataStreamer* dataStreamer = DataStreamer::create(
				card, AcqType::getAcqType(card));


	if (show_tags){
		/* must be first to secure LH columns of report .. */
		dataStreamer->addFrameHandler(
			DataStreamer::createTagHandler(show_tags));
	}

	if (time_frames){
		dataStreamer->addFrameHandler(new FrameTimer());
	}
	if (show_mean){
		dataStreamer->addFrameHandler(
			DataStreamer::createMeanHandler(
				AcqType::getAcqType(card), format_hex));
	}
	if (show_mean || show_tags||time_frames){
		dataStreamer->addFrameHandler(
			DataStreamer::createNewlineHandler());	       
	}
	if (outbase){
		if (mkdir(outbase, 0777) == 0 || errno == EEXIST){
			;
		}else{
			err("failed to mkdir(\"%s\")", outbase);
			exit(-errno);
		}
		dataStreamer->addFrameHandler(
			DirfileFrameHandler::create(
				AcqType::getAcqType(card), outbase));
	}
	return dataStreamer->streamData();

	/* t, dataStreamer deleted on exit() */
}
