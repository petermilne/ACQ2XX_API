/* ------------------------------------------------------------------------- */
/* file ContinuousProcessController.cpp                                                                 */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2011 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>
 *  Created on: Nov 21, 2011
 *      Author: pgm

    http://www.d-tacq.com

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

/** @file ContinuousProcessController.cpp simple controller for
 * continuous data input
@verbatim
./wait-data | acq_demux_continuous -a acq164.def -t 0 -O COOKED -d 1 -i -

[dt100@rhum4 ACQ164_STREAM_DIRFILE]$ cat wait-data
#!/bin/bash

DATA=~dt100/DATA/acq164_012

inotifywait --format %w%f -e close_write -m $DATA

COOKED is a dirfile, plot with kst.

@endverbatim
 *
 */
#include "local.h"

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
#include <sys/wait.h>


#include "popt.h"
#include <string.h>
#include <vector>

#include "AcqType.h"
#include "AcqDataModel.h"
#include "ProcessController.h"
#include "acq_demux.h"

#include "SHM_Buffer.h"
#include "EnvFile.h"
#include "Timer.h"
#include "WorkingDir.h"

class ContinuousProcessController : public ProcessController {
	virtual void processAction(void *pdata, int len, int tblock);

	DumpDef dd;
public:
	ContinuousProcessController(AcqDataModel& _dataModel):
		ProcessController(_dataModel),
		dd(WorkingDir::outbase, 0, 0)
	{
		if (WorkingDir::cleanup){
			char *cmd = new char[128];
			sprintf(cmd, "rtm -f %s/format %s/CH??",
					WorkingDir::outbase, WorkingDir::outbase);
		}
	}
	virtual ~ContinuousProcessController()
	{}

	virtual int dump();
	virtual void run(poptContext& opt_context);
};

int ContinuousProcessController::dump()
{
	printf("dump\n");
	return 0;
}

void ContinuousProcessController::processAction(void *pdata, int len, int tblock)
{
	ProcessController::processAction(pdata, len, tblock);
	dataModel.dump(dd);
	if (dd.event_sample_start == 0){
		dataModel.dumpFormat(dd.root, 0);
	}
	dd.event_sample_start += dataModel.getActualSamples();
	dataModel.clear();
}

void ContinuousProcessController::run(poptContext& opt_context)
{
	char latest_tblock[80] = {};
	DumpDef dd("COOKED", 0, 0);
	dbg(1, "01");

	if (!Args::config_fp){
		const char* arg;
		while((arg = poptGetArg(opt_context)) != 0){
			dbg(2, "process:%s", arg);
			processRawFiles(arg);

		}
	}else{
		while(fgets(latest_tblock, sizeof(latest_tblock), Args::config_fp)){
			chomp(latest_tblock);
			dbg(2, "latest:%s", latest_tblock);
			processRawFiles(latest_tblock);
		}
	}
}


class ContinuousInitializesDefaults {
public:
	ContinuousInitializesDefaults() {
		ProcessControllerRegistry::instance().registerController(
				"acq_demux_continuous",
				new ControllerFactory<ContinuousProcessController>());
	}
};


static ContinuousInitializesDefaults ID;
