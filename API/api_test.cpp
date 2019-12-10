/* ------------------------------------------------------------------------- */
/* file api_test.cpp							     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2008 Peter Milne, D-TACQ Solutions Ltd
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

/** @file api_test.cpp implementation of api_test using API.
 * exercise the methods of Acq2xx, first by setting properties,
 * then run a capture.
 * Then you can use acqread to look at the data
 *
<pre>
  opts:
  -T : use hard trigger 
  --trigger-type lemo-master : set.route in lemo out fpga pxi : chassis master
  --trigger-type pxi-slave   : set.route in pxi out fpga      : chassis slave
			       default: front panel lemo-standalone
</pre>
 */

#include <popt.h>

#include "local.h"
#include "acq2xx_api.h"
#include "acq_transport.h"

int acq200_debug;
int timeout;
char* device;
int do_shell;

#define RETERR(cmd) if (STATUS_ERR(rc = cmd)) {		\
	err("failed: \"%s\" code:%d", #cmd, rc);	\
	return rc;	\
}


#define POST 1024000


const char *trigger_type = "lemo-standalone";

int main(int argc, const char** argv)
{
	struct poptOption opt_table[] = {
		{ "device",     'f', POPT_ARG_STRING, &device,		'f' },
		{ "board",      'b', POPT_ARG_STRING, &device,		'b' },
		{ "shell",      's', POPT_ARG_STRING, &device,		's' },
		{ "verbose",    'v', POPT_ARG_INT,    &acq200_debug,	0   },
		{ "version",    0,   POPT_ARG_NONE,   0,		'V' },
		{ "trigger",    'T', POPT_ARG_NONE,   0,		'T' },
		{ "trigger-type", 0, POPT_ARG_STRING, &trigger_type,	0 },
		{ "timeout",    't', POPT_ARG_INT,    &timeout,		't',
		  "timeout in seconds"},
		POPT_AUTOHELP
		POPT_TABLEEND		
	};
	int rc;
	poptContext opt_context = 
		poptGetContext(argv[0], argc, argv, opt_table, 0);
	int soft_trigger = 1;

	while ( (rc = poptGetNextOpt(opt_context)) > 0 ){
		switch(rc){
		case 's':
			do_shell = 1;
			break;	
		case 'T':
			soft_trigger = 0;
			break;
		}
	}	

	if (device == 0){
		err("usage api_test -f DEVICE command ...");
		return -1;
	}
	Transport *t = Transport::getTransport(device);	
	Acq2xx card(t);
	int channels;

	printf("Test starts here:");

	printf("getAvailableChannels\n");
	RETERR(card.getAvailableChannels(channels));
	printf("getAvailableChannels: %d\n", channels);

	int iclock = 250000;
	printf("setInternalClock(%d)\n", iclock);
	RETERR(card.setInternalClock(iclock));	
	iclock = -1234;
	RETERR(card.getInternalClock(iclock));
	printf("getInternalClock value %d\n", iclock);

	printf("setChannelMask\n");
	//                   12345678901234567890123456789012
	char* channelMask = new char[96];
	strcpy(channelMask, "11111111111111111111111111111111");

	RETERR(card.selectChannels(channelMask));


	RETERR(card.getSelectedChannels(channelMask, &channels));
	printf("getChannelMask %s number of channels %d\n", 
			channelMask, channels);


	if (soft_trigger){
		RETERR(card.setSoftTriggeredMode(POST));
	}else{
		if (strcmp(trigger_type, "lemo-master") == 0){
			// lemo master
			RETERR(card.setRoute(D3, R_LEMO, R_FPGA, R_PXI));
		}else if (strcmp(trigger_type, "pxi-slave") == 0){
			// slave card in same chassis
			RETERR(card.setRoute(D3, R_PXI, R_FPGA));
		}else{
			// standalone lemo trg
			RETERR(card.setRoute(D3, R_LEMO, R_FPGA));
		}
		dbg(1, "now set the mode");
		RETERR(card.setTriggeredPostMode(POST));
	}

	RETERR(card.setArm());
	
	enum STATE s;
	int post;
	do {
		sleep(1);
		RETERR(card.getState(s));
		RETERR(card.getNumSamples(&post));
		printf("getState:%s total:%d\n", toString(s), post);
	} while(s != ST_STOP);

	
	delete [] channelMask;
	return 0;
}
