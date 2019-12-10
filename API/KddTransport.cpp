/* ------------------------------------------------------------------------- */
/* file KddTransport.cpp			                             */
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

/** @file KddTransport.cpp defines the Kernel Device Driver Transport.
 * D-TACQ offers 2 types of Linux Kernel Device Driver, this Transport is
 * compatible with both of them:
 * - PCI backplane driver, for shared memory space systems.
 * - dt100-hub Network driver, emulates the PCI driver but with no shared
 *   memory, no plugin cards.
 *
 * D-TACQ does NOT offer a Windows(TM) kernel device driver (WDM) at this
 * time; to control ACQ2xx from Windows use the SOAP or Dt100Transports.
 */
#include "local.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "acq_transport.h"
#include "KddTransport.h"



#define MFMT "/dev/acq32/acq32.%d.m%d"
#define SHELL_FMT "/dev/acq200/acq200.%d.rsh"

#define ERROR_STRING    "ACQ32:ERROR"
#define ERROR_LEN       strlen(ERROR_STRING)



class KddTransport: public Transport {
/** Transport implementation connects to locate linux Kernel Device Driver. */	
	FILE* fd_cmd;
	FILE* fd_shell;

	STATUS processCommand(
		FILE* fd,
		const char* command, char *response, int maxresponse)	
	{

		dbg(1, "->%s", command);

		fputs(command, fd);
		if (command[strlen(command)-1] != '\n'){
			fputs("\n", fd);
		}

	        char reply[80];		
	        int nout;
		int err = STATUS_OK;
		char* pr = response;
		int complete = 0;

		*pr = '\0';

	        for (nout = 0; fgets(reply, sizeof(reply), fd) != NULL; ++nout){

			dbg(1, "<-%s", reply);

	                if (strncmp(reply, ERROR_STRING, ERROR_LEN) == 0){
		                err = -1;
			}
	                if (STREQN(reply, "EOF", 3)){
				complete = 1;
		                break;
			}

			int headroom = maxresponse - (pr - response);
			strncpy(pr, reply, headroom);
			pr += strlen(pr);
		}

		dbg(2, "nout %d len %d err %d", nout, pr-response, err);

		if (!complete){
			err("incomplete response %s", response);
			if (nout == 0){
				perror( "zero chars");
			}
	                return -1;
		}else if (STATUS_ERR(err)){
			return err;
		}else{
			if (*(pr-1) == '\n'){
				*--pr = '\0';
			}
			return pr - response;
	        }
        }

	char channel_dev[80];
	FILE *fp_data;

	const int slot;
	char cmd_dev[80];
	char shell_dev[80];

private:
	void closeSession(FILE* &fd){
		if (fd){
			fclose(fd);
			fd = 0;
		}
	}
	void openSession(FILE* &fd, char* dev){
		closeSession(fd);
		fd = fopen(dev, "r+");
		if (fd == 0){
			die(errno, dev);
		}
	}
protected:
	void openShellSession() {
		openSession(fd_shell, shell_dev);
	}
	void closeShellSession() {
		closeSession(fd_shell);
	}
	void openCmdSession() {
		openSession(fd_cmd, cmd_dev);
	}
	void closeCmdSession() {
		closeSession(fd_cmd);
	}

public:
	KddTransport(const char* id):	
	Transport(id),  fd_cmd(0), fd_shell(0), fp_data(0), slot(atoi(id))
	{
		memset(channel_dev, 0, sizeof(channel_dev));
		
		sprintf(cmd_dev, MFMT, slot, slot);
		sprintf(shell_dev, SHELL_FMT, slot);

		openShellSession();
		openCmdSession();
	}
	~KddTransport() {
		closeShellSession();
		closeCmdSession();
	}

	virtual STATUS acqcmd(
		const char* command, char *response, int maxresponse)
	/**< send an "acqcmd" (acquisition command or query) to the card.
	 * @param command - the command (or query) to send
	 * @param response - user buffer to collect response.
	 * @param maxresponse - maximum response size required.
	 */
	{
		return processCommand(fd_cmd, command, response, maxresponse);
	}

	virtual STATUS acq2sh(
		const char* command, char *response, int maxresponse)
	/**< run a remote shell command or query. 
	 * @param command - the command (or query) to send
	 * @param response - user buffer to collect response.
	 * @param maxresponse - maximum response size required.
	 */
	{
		return processCommand(fd_shell, command, response, maxresponse);
	}

	virtual STATUS waitStateChange(
		int timeout, char* response, int maxresponse)
	/**< block until remote state changes or timeout. */
	{
		return STATUS_WORKTODO;
	}

	virtual STATUS readChannel(
		int channel, short* data,
		int nsamples, int start = 0, int stride = 1)
	/**< read and output raw data for channel
         * @param channel - channel number 1..N
	 * @param data - caller's buffer
         * @param nsamples - max samples to read
         * @param start - start sample in data set
	 * @param stride - stride [subsample] value
	 * @returns actual samples returned or STATUS_ERR
	 * @todo - start, stride dont work at this time
	 */
	{
		char _channel_dev[80];
		sprintf(_channel_dev, "/dev/acq32/acq32.%d.%02d", 
			slot, channel);

		if (fp_data != 0){
			if (strcmp(_channel_dev, channel_dev) == 0){
				;
			}else{
				fclose(fp_data);
				fp_data = 0;
			}
		}
		if (fp_data == 0){
			strcpy(channel_dev, _channel_dev);
			fp_data = fopen(channel_dev, "r");
			if (fp_data == 0){
				perror(channel_dev);
				return errno;
			}
		}
		return fread(data, sizeof(short), nsamples, fp_data);
	}
};


class HostdrvTransport: public KddTransport {
/** HOSTDRV doesn't handle multiple writes on one shell session, so we
 *  decorate KddTransport, to start a new session each time.
 *  This is a HOSTDRV defect, but sessions are cheap on HOSTDRV..
 *  @todo BEWARE: state polling can be too fast 
 */
public:
	HostdrvTransport(const char* id):
		KddTransport(id)
	{}

	virtual STATUS acq2sh(
		const char* command, char *response, int maxresponse)
	/**< run a remote shell command or query. 
	 * @param command - the command (or query) to send
	 * @param response - user buffer to collect response.
	 * @param maxresponse - maximum response size required.
	 */
	{
		openShellSession();
		int rc = KddTransport::acq2sh(command, response, maxresponse);
		closeShellSession();
		return rc;
	}
	
};

Transport* KddTransportFactory::createTransport(const char* id)
/**< KddTransport Factory - create if id is a simple integer slot#. */
{
	int slot = atoi(id);
	char slot_id[10];
	sprintf(slot_id, "%d", slot);

	if (strcmp(id, slot_id) == 0){
		if (slot < 10){
			return new HostdrvTransport(id);
		}else{ 
			return new KddTransport(id);
		}
	}else{
		return 0;
	}
}
