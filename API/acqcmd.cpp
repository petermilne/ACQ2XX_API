/* ------------------------------------------------------------------------- */
/* file acqcmd.cpp							     */
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

/** @file acqcmd.cpp implementation of acqcmd using API.
 * acqcmd is the interface to the low level command interpreter (see ICD).
 */

#include <popt.h>

#include "local.h"
#include "acq2xx_api.h"
#include "acq_transport.h"

int acq200_debug;
int timeout;
char* device;
int do_shell;

#define MAXBUF 1024
char command[MAXBUF] = {};
char response[MAXBUF] = {};
char *poll_while;
char *poll_until;

int main(int argc, const char** argv)
{
	struct poptOption opt_table[] = {
		{ "device",     'f', POPT_ARG_STRING, &device,     'f' },
		{ "board",      'b', POPT_ARG_STRING, &device,      'b' },
		{ "shell",      's', POPT_ARG_STRING, &device,      's' },
		{ "verbose",    'v', POPT_ARG_INT,    &acq200_debug, 0   },
		{ "version",    0,   POPT_ARG_NONE,   0,           'V' },
		{ "while",      'w', POPT_ARG_STRING, &poll_while, 'p' },
		{ "until",      'u', POPT_ARG_STRING, &poll_until, 'p' },
		{ "timeout",    't', POPT_ARG_INT,    &timeout,    't',
		  "timeout in seconds"},
		POPT_AUTOHELP
		POPT_TABLEEND		
	};
	int rc;
	poptContext opt_context = 
		poptGetContext(argv[0], argc, argv, opt_table, 0);

	while ( (rc = poptGetNextOpt(opt_context)) > 0 ){
		switch(rc){
		case 's':
			do_shell = 1;
			break;	
		}
	}	

	if (device == 0){
		err("usage acqcmd -f DEVICE command ...");
		return -1;
	}
	Transport *t = Transport::getTransport(device);	

	if (poll_while){
		rc = t->acqcmd("getState", response, MAXBUF);
		while (strstr(response, poll_while)){
			rc = t->waitStateChange(timeout, response, MAXBUF);
			if (rc < 0){
				exit(rc);
			}
		}	
	}else if (poll_until){
		rc = t->acqcmd("getState", response, MAXBUF);
		while (strstr(response, poll_until) == 0){
			rc = t->waitStateChange(timeout, response, MAXBUF);
			if (rc < 0){
				exit(rc);
			}
		}
	}else{

		if (poptPeekArg(opt_context)){	
			for (int first=1; poptPeekArg(opt_context); first=0){
				if (!first){
					strcat(command, " ");
				}
				strcat(command, poptGetArg(opt_context));
			}

			if (do_shell){
				rc = t->acq2sh(command, response, MAXBUF);
			}else{
				rc = t->acqcmd(command, response, MAXBUF);
			}

			puts(response);

			if (STATUS_IS_OK(rc)){
				return 0;
			}else{
				exit(rc);
			}
		}else{
			// no batch processing at this time.
			exit(0);
		}
	}
}
