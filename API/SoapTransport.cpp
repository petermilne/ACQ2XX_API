/* ------------------------------------------------------------------------- */
/* file SoapTransport.cpp			                             */
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

/** @file SoapTransport.cpp defines a Transport for SOAP service. STUB
 * ACQ2xx features a SOAP webservice that allows network control.
 * We already have an API "SOAPI" to use SOAP directly, this transport is
 * an adapter to match SOAPI to the generic API.
 */
#include "local.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "acq_transport.h"
#include "SoapTransport.h"



class SoapTransport: public Transport {
/** Transport implementation connects to locate linux Kernel Device Driver. */
public:
	SoapTransport(const char* id):	Transport(id)
	{
		fprintf(stderr, "SoapTransport WORKTODO");	
	}

	virtual STATUS acqcmd(
		const char* command, char *response, int maxresponse)
	/**< send an "acqcmd" (acquisition command or query) to the card.
	 * @param command - the command (or query) to send
	 * @param response - user buffer to collect response.
	 * @param maxresponse - maximum response size required.
	 */
	{
		return STATUS_WORKTODO;
	}

	virtual STATUS acq2sh(
		const char* command, char *response, int maxresponse)
	/**< run a remote shell command or query. 
	 * @param command - the command (or query) to send
	 * @param response - user buffer to collect response.
	 * @param maxresponse - maximum response size required.
	 */
	{
		return STATUS_WORKTODO;
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
	 */
	{
		return STATUS_WORKTODO;
	}
};



Transport* SoapTransportFactory::createTransport(const char* id)
/**< SoapTransport factory method - create if id begins http:// . */
{
	if (strstr(id, "http://") != 0){
		return new SoapTransport(id);
	}else{
		return 0;
	}
}
