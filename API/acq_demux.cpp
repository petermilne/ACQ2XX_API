/* ------------------------------------------------------------------------- */
/* file acq_demux.cpp							     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2010 Peter Milne, D-TACQ Solutions Ltd
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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/** @file acq_demux.cpp demuxes (de-channelizes) acqXXX raw data.
 * 
<pre>
 Usage: acq_demux [options] RAWFILES
	Where RAWFILES are file names containing raw data.
	Typically tblock uploads 000, 001, 002 ...
       
	options:
	-d --debug level : set debugging/
	-a --acq_type def-file : define ACQ type using def-file
        -n --nsamples samples : preset number of samples in output
        -T --sample_clock_ns  : define inter sample interval in nano secs
        -C --ChanneSpeedMask  : multirate - ACQ132 only
	-o --WorkingDir::outbase          : save output in DirFile format RAWFILE.COOKED/
	--dual-rate div1,div2 
</pre>
* DirFile output is compatible with kst.
<pre>
Typical def file:
[pgm@hoy API]$ cat acq132.def 
ACQ=acq132
WORD_SIZE=2
AICHAN=32
</pre>
*/


#include <assert.h>
#include <iostream>
#include <map>
#include <vector>
#include <set>


#include <stdlib.h>
#include <libgen.h>	/* dirname() */
#include <unistd.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "popt.h"

const char *VERID = "acq_demux $Revision: 1.52 $ B1025";

char channel_mask[80];
char scan_list[16];


#include "local.h"

#include "acq_demux.h"
#include "AcqType.h"
#include "AcqDataModel.h"

#include "CommandBuffer.h"
#include "Timer.h"
#include "WorkingDir.h"

#include "ProcessController.h"

extern "C" {
int acq200_debug = 0;
};

int nsamples;
const char *csm;
const char *acq_def_file = "acq_type.def";

const char *dual_rate_def = 0;


NewEventSignature* userStartTime;


static poptContext processOpts(int argc, const char** argv)
{
	char *infile = 0;
	static double start_time;
	int start_time_set = false;
	char *logfile;
	int overwrite = 0;
	int cooking = 1;

	static struct poptOption opt_table[] = {
	{ "debug",		'd', POPT_ARG_INT, &acq200_debug,	0 },
	{ "ignore-es",    'I', POPT_ARG_NONE, 0, 'I' },
	{ "acq_type",           'a', POPT_ARG_STRING, &acq_def_file,    0 },
	{ "input",              'i', POPT_ARG_STRING, &infile,		'i' },
	{ "logfile",		'L', POPT_ARG_STRING, &logfile,		'L' },
	{ "nsamples",		'n', POPT_ARG_INT, &nsamples,		0 },
	{ "sample_clock_ns",	'T', POPT_ARG_DOUBLE, 
						  &Clock::sample_clock_ns, 'T'},
	{ "ChannelSpeedMask",   'C', POPT_ARG_STRING, &csm,		'C' },
	{ "outbase",'O', POPT_ARG_STRING, &WorkingDir::outbase, 0 },
	{ "dual-rate",           0,  POPT_ARG_STRING, &dual_rate_def,    'd' },
	{ "cleanup",		 0,  POPT_ARG_INT, &WorkingDir::cleanup, 0 },
	{ "maxlen",		'm', POPT_ARG_INT, &Args::maxlen, 0 },
	{ "pre",		0,  POPT_ARG_INT, &Args::pre, 0 },
	{ "post",		0, POPT_ARG_INT, &Args::post, 0 },
	{ "startoff",		0, POPT_ARG_INT, &Args::startoff, 0 },
	{ "start-time",		't', POPT_ARG_DOUBLE, &start_time, 't' },
	{ "cooking",     0, POPT_ARG_INT, &cooking, 'c',
	  "default:1 (swap cooked data on complete) 0: overwrite cooked data"},
	{ "overwrite",   0, POPT_ARG_INT, &overwrite, 'o', },
	{ "FRIGGIT",     0, POPT_ARG_INT, &WorkingDir::FRIGGIT, 0 },
	POPT_AUTOHELP
        POPT_TABLEEND
	};

	if (getenv("ACQ200_DEBUG")){
		acq200_debug = atoi(getenv("ACQ200_DEBUG"));
	}

	if (getenv("MULTIPLE_TIMEBASE")){
		DumpDef::common_timebase =
				atoi(getenv("MULTIPLE_TIMEBASE")) ==0;
	}
	if (getenv("WCP_TIMED_AT_EVENT")){
		AcqDataModel::wallclock_policy = WCP_TIMED_AT_EVENT;
	}
	poptContext opt_context =
                poptGetContext(argv[0], argc, argv, opt_table, 0);
	int rc;

	while((rc = poptGetNextOpt(opt_context)) > 0){
		switch(rc){
		case 'i':
			if (strcmp(infile, "-") == 0){
				Args::config_fp = stdin;
			}else{
				Args::config_fp = fopen(infile, "r");
				if (!Args::config_fp){
					perror(infile);
					exit(1);
				}
			}
			break;
		case 'L':
			if (strcmp(logfile, "-") == 0){
				Args::log_fp = stdout;
			}else{
				Args::log_fp = fopen(logfile, "w");
				if (!Args::log_fp){
					perror(logfile);
					exit(1);
				}else{
					info("logging to %s", logfile);
				}
			}
			break;
		case 'd':
			if (sscanf(dual_rate_def, "%d,%d", 
				   &DualRate::div0, &DualRate::div1) != 2){
				err("usage: dual-rate div0,div1");
				exit(-1);
			}
		case 'T':
//@@todo			Clock::sample_clock_ns *= MAXDECIM;
//			Clock::sample_clock_ns = 1000;	/** @@todo popt bug? */
			break;
		case 't':
			start_time_set = true;
			break;
		case 'I':
			AcqDataModel::setIgnoreEs(true);
			break;
		case 'c':
			WorkingDir::use_cooking = cooking != 0;
			printf("WorkingDir::use_cooking set:%d\n", WorkingDir::use_cooking);
			break;
		case 'o':
			AcqDataModel::setFileWritePolicy(overwrite?
					AcqDataModel::OVERWRITE: AcqDataModel::APPEND);
			printf("setFileWritePolicy:%d", overwrite);
			break;
		default:
			break;
		}
	}

	if (start_time_set){
		userStartTime = new UserEventSignature(start_time);
	}
	return opt_context;
}

static string &makeIdent(int argc, const char **argv)
{
	string *strp = new string();
	string& s = *strp;

	s += VERID;
	s += "\n";

	for (int ii = 0; ii < argc; ++ii){
		s += argv[ii];
		s += " ";
	}
	s += "\n";
	return *strp;
}

void addPeer(ProcessController* pc, AcqDataModel& dataModel, const char* key)
{
	dbg(1, "key:%s", key);
	ProcessController *peer = ProcessController::create(key, dataModel);
	if (peer){
		pc->addPeer(peer);
	}else{
		err("failed to add peer %s", key);
	}
}
void addPeers(ProcessController* pc, AcqDataModel& dataModel)
{
	char *p1 = getenv("ACQ_DEMUX_PEER1");
	if (p1){
		addPeer(pc, dataModel, p1);
	}
	char *p2 = getenv("ACQ_DEMUX_PEER2");
	if (p2){
		addPeer(pc, dataModel, p2);
	}
}

int main(int argc, const char **argv)
{
	poptContext opt_context = processOpts(argc, argv);
	char namebuf[128];

	printf("%s\n", VERID);
	strcpy(namebuf, argv[0]);
	string pname(basename(namebuf));
	
	

	AcqDataModel& dataModel = *AcqDataModel::create(
		AcqType::getAcqType(acq_def_file), scan_list, channel_mask);

	dataModel.setIdent(makeIdent(argc, argv));
	if (userStartTime){
		dataModel.addEventSignature(userStartTime);
	}

	if (nsamples){
		dataModel.setMaxsamples(nsamples);
	}

	ProcessController *pc = ProcessController::create(pname, dataModel);

	addPeers(pc, dataModel);

	dbg(1, "%s %s", pname.c_str(), VERID);

	if (pc){
		pc->run(opt_context);
	}
}

int Args::maxlen;
int Args::startoff;
int Args::pre;
int Args::post;
FILE *Args::config_fp;
FILE *Args::log_fp;
