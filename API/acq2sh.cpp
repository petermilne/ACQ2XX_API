/* ------------------------------------------------------------------------- */
/* file acq2sh.cpp							     */
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

/** @file acq2sh.cpp implementation of acq2sh "remote shell" using API.
 * acq2sh can run any shell command on the target device
 * WARNING: output is limited to 4096 bytes, for longer outputs, consider
 * saving to a file, then transferring the file
 */

#include "local.h"
#include "acq2xx_api.h"
#include "acq_transport.h"

#include "popt.h"

int acq200_debug;
char *device;
poptContext opt_context;

#define MAXBUF 4096
char command[MAXBUF] = {};
char response[MAXBUF] = {};


int main(int argc, const char** argv)
{
	struct poptOption opt_table[] = {
		{ "url",        'u', POPT_ARG_STRING, &device,     'u' },
		{ "device",     'f', POPT_ARG_STRING, &device,     'f' },
		{ "board",      'b', POPT_ARG_STRING, &device,      'b' },
		{ "shell",      's', POPT_ARG_STRING, &device,      's' },
		{ "verbose",    'v', POPT_ARG_INT,    &acq200_debug,    0   },
		{ "version",    0,   POPT_ARG_NONE,   0,           'V' },
		POPT_AUTOHELP
		POPT_TABLEEND		
	};
	int rc;

	opt_context = poptGetContext( argv[0], argc, argv, opt_table, 0 );
	while ( (rc = poptGetNextOpt( opt_context )) > 0 ){
		switch( rc ){
			;
		}
	}

	if (device == 0){
		err("usage acq2sh -f DEVICE command ...");
		return -1;
	}
	Acq2xx card(Transport::getTransport(device));

	if (poptPeekArg(opt_context)){	
		for (int first = 1; poptPeekArg(opt_context); first = 0){
			if (!first){
				strcat(command, " ");
			}
			strcat(command, poptGetArg(opt_context));
		}

		int rc = card.getTransport()->acq2sh(command, response, MAXBUF);

		puts(response);
		if (STATUS_IS_OK(rc)){
			return 0;
		}else{
			exit(rc);
		}	
	}else{		
		for (int line = 0; fgets(command, MAXBUF, stdin); ++line){
			printf(command);
			if (command[0] == '#' || strlen(command) < 2){
				continue;
			}else{
				rc = card.getTransport()->acq2sh(
	       					command, response, MAXBUF);
				puts(response);
				if (rc < 0){
					return rc;
				}
			}
		}
		return 0;
	}
}
