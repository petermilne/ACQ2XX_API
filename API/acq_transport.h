/* ------------------------------------------------------------------------- */
/* file acq_transport.h				                             */
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

/** @file acq_transport.h Generic ACQ2xx Transport Definition. */

#ifndef __ACQ_TRANSPORT_H__
#define __ACQ_TRANSPORT_H__ "$Revision: 1.4 $"

#include "acq_api_common.h"

class Frame;

class Transport {
	const char* id;

protected:
	Transport(const char* _id)
		:
	id(_id)
	{}
	/**< protected Constructor - clients should not create directly. */


public:
	static Transport *getTransport(const char* id);
	/**< Factory to create Transports. 
	 * responsibility of client to delete when done.
	 */

	virtual ~Transport() {}


	virtual STATUS acqcmd(
		const char* command, char *response, int maxresponse) = 0;
	/**< send an "acqcmd" (acquisition command or query) to the card.
	 * @param command - the command (or query) to send
	 * @param response - user buffer to collect response.
	 * @param maxresponse - maximum response size required.
	 */
	STATUS acqcmd(const char* command) {
		return acqcmd(command, 0, 0);
	}
	/**< send an "acqcmd" (acquisition command) to the card */

	virtual STATUS acq2sh(
		const char* command, char *response, int maxresponse) = 0;
	/**< run a remote shell command or query. 
	 * @param command - the command (or query) to send
	 * @param response - user buffer to collect response.
	 * @param maxresponse - maximum response size required.
	 */
	STATUS acq2sh(const char* command) {
		return acqcmd(command, 0, 0);
	}
	/**< run a remote shell command */

	virtual STATUS waitStateChange(
		int timeout, char* response, int maxresponse) = 0;
	/**< block until remote state changes or timeout. */

	virtual STATUS readChannel(
		int channel, short* data,
		int nsamples, int start = 0, int stride = 1) = 0;
	/**< read and output raw data for channel
         * @param channel - channel number 1..N
	 * @param data - caller's buffer
         * @param nsamples - max samples to read
         * @param start - start sample in data set
	 * @param stride - stride [subsample] value
	 * @returns actual samples returned or STATUS_ERR
	 */

	/** streaming interface: not all transports can do this. */
	virtual STATUS readStreamingFrame(Frame* frame, unsigned id) {
		return STATUS_FEATURE_NOT_IMPLEMENTED;
	}
	virtual STATUS stopStreaming(void) {
		return STATUS_FEATURE_NOT_IMPLEMENTED;
	}
};


#endif // __ACQ_TRANSPORT_H__
