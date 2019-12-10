/* ------------------------------------------------------------------------- */
/* file acq2xx_api.h				                             */
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

/** @file acq2xx_api.cpp ACQ2xx C++ API Implementation. */

#include "local.h"
#include "acq_transport.h"
#include "acq2xx_api.h"

#define RETERR(rc) if (STATUS_ERR(rc)) return rc

#define ERR_SCAN1	-101
#define ERR_SCAN2	-102
#define ERR_SCAN3	-103
#define ERR_SCAN4	-104

#define CMD	128
#define REPLY	128

#define ECASE(ev) case ev: return #ev;

const char* toString(enum STATE s)
{
	switch(s){
		ECASE(ST_STOP); 
		ECASE(ST_ARM);
		ECASE(ST_RUN);
		ECASE(ST_TRIGGER);
		ECASE(ST_POSTPROCESS);
		ECASE(ST_CAPDONE);
	default:
		return "unknown";
	}
}
const char* toString(enum acq2xx__DOx dox)
{
	switch(dox){

	case DO_NONE:	
		return "DO_NONE";
	case DO0:
		return "DO0";
	case DO1: 
		return "DO1";
	case DO2: 
		return "DO2";
	case DO3: 
		return "DO3";
	case DO4:
		return "DO4";
	case DO5:
		return "DO5";
	default:
		return "";
	}
}

const char* toString(enum acq2xx__EDGE edge)
{
	return edge==EDGE_FALLING? "falling": "rising";
}

const char* toString(enum acq2xx__DIx dix)
{
	switch(dix) {
	case DI_NONE:  
		return "DI_NONE";
	case DI0:
		return "DI0";
	case DI1:
		return "DI1";
	case DI2:
		return "DI2";
	case DI3:	
		return "DI3";
	case DI4:
		return "DI4";
	case DI5:
		return "DI5";
	default:
		return "";
	}
}

const char *toString(enum acq2xx_Dx dx)
{
	switch(dx){
	case D0: return "d0";
	case D1: return "d1";
	case D2: return "d2";
	case D3: return "d3";
	case D4: return "d4";
	case D5: return "d5";
	default:
		return "none";
	}
}

const char* toString(
	enum acq2xx_RoutePort port
	)
{
	switch(port){
	case R_LEMO: return "lemo ";
	case R_FPGA: return "fpga ";
	case R_PXI:  return "pxi ";
	case R_RIO:  return "rio ";
	default:
		return "";
	}
}



const char* toString(
	enum acq2xx_RoutePort port,
	enum acq2xx_RoutePort port2,
	enum acq2xx_RoutePort port3)
{	
	static char str[80];

	strcpy(str, toString(port));
	strcat(str, toString(port2));
	strcat(str, toString(port3));
	return str;
}


Acq2xx::Acq2xx(Transport* _transport):
	transport(_transport), 
		ranges(0),
		ai(-1), ao(0)
{

}
STATUS Acq2xx::setRoute(
	enum acq2xx_Dx dx,
	enum acq2xx_RoutePort in,
	enum acq2xx_RoutePort out,
	enum acq2xx_RoutePort out2,
	enum acq2xx_RoutePort out3)
/**< make a signal routing (connection). 
 * "signal" is a digital control signal
 * @param dx - the signal
 * @param in - source: input port (one only)
 * @param out - sink: output port(s) 
 */
{
	char reply[REPLY];
	char cmd[CMD];

	sprintf(cmd, "set.route %s in %s out %s\n",
		toString(dx), toString(in), toString(out, out2, out3));
	
	int rc = transport->acq2sh(cmd, reply, REPLY);
	RETERR(rc);

	return STATUS_OK;
}


STATUS Acq2xx::setExternalClock(
	enum acq2xx__DIx dix, int div, enum acq2xx__DOx dox)
/**< set External Clock definition.
 * @param dix - the signal
 * @param div - integer divide of dix
 * @param dox - optional output for divided signal
 */ 
{
	return STATUS_WORKTODO;
}

STATUS Acq2xx::setInternalClock(
	int hz,	enum acq2xx__DOx dox)
/**< set Internal Clock defiition.
 * @param hz - frequency in Hz
 * @param dox - output the clock on this line
 */
{
	char reply[REPLY];
	char cmd[CMD];
	
	if (dox != DO_NONE){
		sprintf(cmd, "setInternalClock %d %s\n", hz, toString(dox));
	}else{
		sprintf(cmd, "setInternalClock %d\n", hz);
	}
	int rc = transport->acqcmd(cmd, reply, REPLY);
	RETERR(rc);

	return STATUS_OK;
}

STATUS Acq2xx::getInternalClock(int& hz)
/**< get the actual Internal Clock frequency.
 * @param hz - output value
 */
{
	char reply[REPLY];
	int rc = transport->acqcmd("getInternalClock", reply, REPLY);
	RETERR(rc);
	int clock;
	if (sscanf(reply, "ACQ32:getInternalClock=%d", &clock) != 1){
		return ERR_SCAN1;
	}
	hz = clock;
	return STATUS_OK;
}

STATUS Acq2xx::getAvailableChannels(int& maxChannels)
/**< get the number of channels on the card.
 * @param maxChannels - output available channels.
 */
{
	if (ai == -1){
		char reply[REPLY];
	
		int rc = transport->acqcmd("getAvailableChannels", 
							reply, REPLY);
		RETERR(rc);
		if (sscanf(reply, "ACQ32:getAvailableChannels AI=%d AO=%d",
			   &ai, &ao) != 2){

			return ERR_SCAN2;
		}
	}

	maxChannels = ai;
	return STATUS_OK;
}

STATUS Acq2xx::selectChannels(const char* channelMask)
/**< set the active channel mask.
 *   NB not all masks are valid, hardware will select nearest mask
 *   that includes all channels.
 * @param channelMask - mask selects active channels 1=> enabled
 */
{
	char reply[REPLY];
	char cmd[CMD];
	
	sprintf(cmd, "setChannelMask %s\n", channelMask);
	int rc = transport->acqcmd(cmd, reply, REPLY);
	RETERR(rc);

	return STATUS_OK;
}

STATUS Acq2xx::getSelectedChannels(char* channelMask, int *count)
/**< get the actual effective channel mask.
 * @param channelMask - outputs the actual mask
 * @param count - outputs selected channel count
 */
{
	char reply[REPLY];
	int rc = transport->acqcmd("getChannelMask", reply, REPLY);
	RETERR(rc);

	if (sscanf(reply, "ACQ32:getChannelMask=%s", channelMask) != 1){
		return ERR_SCAN1;
	}
	if (count != 0){
		int n = 0;
		for (int ic = 0; channelMask[ic]; ++ic){
			if (channelMask[ic] == '1'){
				++n;
			}
		}
		*count = n;
	}
	return STATUS_OK;
}

STATUS Acq2xx::getChannelRanges(acq2xx_VRange * ranges, int maxRanges)
/**< get a list of calibrated range values for each channel.
 * @param ranges - user allocated buffer to hold values.
 * @param maxRanges - length of user buffer
 */
{
	char reply[4096];
	int rc = transport->acq2sh("get.vin\n", reply, 4096);
	RETERR(rc);
	
	char* cursor = reply;
	
	for(int ch = 1; ch <= maxRanges; ++ch){
		int endc;

		dbg(1, "scanning from [%d] len %d", 
				cursor-reply, strlen(cursor));

		if (sscanf(cursor, "%f,%f,%n", 
			   &ranges[ch].vmin, &ranges[ch].vmax, &endc) >= 2){

		}else{
			err("failed at channel %d: \"%s\"", ch, cursor);
			return ERR_SCAN2;
		}
		cursor += endc;
	}	

	return STATUS_OK;
}

STATUS Acq2xx::setPrePostMode(
	int prelen, int postlen, 
	enum acq2xx__DIx dix,
	enum acq2xx__EDGE edge)
/**< configure the capture Mode.
 * @param prelen - number of samples before trigger
 * @param postlen - number of samples after trigger
 * @param dix - signal line for trigger
 * @param edge - sense of the signal
 */
{
	char reply[REPLY];
	char cmd[CMD];
	const char* s_dix = dix==DI_NONE? "none": toString(dix);
	const char* s_edge = dix==DI_NONE? "": toString(edge);

	sprintf(cmd, "set.pre_post_mode %d %d %s %s\n",
		prelen, postlen, s_dix, s_edge);
	int rc = transport->acq2sh(cmd, reply, REPLY);
	RETERR(rc);
	return STATUS_OK;
}


STATUS Acq2xx::setArm()
/**< arm the card to start the capture. */
{
	char reply[REPLY];

	int rc = transport->acqcmd("setArm\n", reply, REPLY);
	RETERR(rc);
	return STATUS_OK;	
}

STATUS Acq2xx::setAbort()
/**< abort a capture */
{
	char reply[REPLY];

	int rc = transport->acqcmd("setAbort\n", reply, REPLY);
	RETERR(rc);
	return STATUS_OK;
}

STATUS Acq2xx::getState(enum STATE& state)
/**< output card state.
 * @param state - output state value.	
 */
{
	char reply[REPLY];
	int _state;

	int rc = transport->acqcmd("getState\n", reply, REPLY);
	RETERR(rc);
	
	if (sscanf(reply, "ACQ32:%d ST_", &_state) != 1){
		return ERR_SCAN1;
	}
	state = (enum STATE)_state;
	return STATUS_OK;

}
	
STATUS Acq2xx::waitState(enum STATE state, int timeout)
/**< wait for selected State to occur, or timeout
 * @param state - state to wait for
 * @param timeout - timeout in msec
 */
{
	return STATUS_WORKTODO;
}

STATUS Acq2xx::getNumSamples(int* total, int* pre, int* post, int *elapsed)
/**< query current capture state.
 * @param total - output total samples
 * @param pre - output pre trigger samples
 * @param post - output post- trigger samples
 * @param elapsed - output samples since arm
 * NB: any param can be null, and is then ignored
 */
{
	char response[128];
	int rc = transport->acqcmd("getNumSamples", response, sizeof(response));
	RETERR(rc);
	
	int _total, _pre, _post, _elapsed;
	
	if (sscanf(response, "ACQ32:getNumSamples=%d pre=%d post=%d elapsed=%d",
		   &_total, &_pre, &_post, &_elapsed) != 4){
		err("failed to scan 4 values \"%s\"", response);
		return -1;
	}

	if (total)	*total	= _total;
	if (pre)	*pre	= _pre;
	if (post)	*post	= _post;
	if (elapsed)	*elapsed= _elapsed;

	dbg(1, "SUCCESS %d %d %d %d", _total, _pre, _post, _elapsed);

	return STATUS_OK;
}
		
STATUS Acq2xx::readChannel(int channel, short* data,
		   int nsamples, int start, int stride)
/**< read and output raw data for channel
 * @param channel - channel number 1..N
 * @param data - caller's buffer
 * @param nsamples - max samples to read
 * @param start - start sample in data set
 * @param stride - stride [subsample] value
 * @returns actual samples returned or STATUS_ERR
 */
{
	return transport->readChannel(channel, data, nsamples, start, stride);
}

STATUS Acq2xx::readChannelVolts(int channel, float* data_volts,
			int nsamples, int start, int stride)
/**< read and output calibrated data for channel
 * @param channel - channel number 1..N
 * @param data_volts - caller's buffer
 * @param nsamples - max samples to read
 * @param start - start sample in data set
 * @param stride - stride [subsample] value
 * @returns actual samples returned or STATUS_ERR
 */
{

	int rc;

	if (ranges == 0){
		int maxChannels;
		rc = getAvailableChannels(maxChannels);

		if (STATUS_ERR(rc)){
			return rc;
		}
	       
		ranges = new acq2xx_VRange[maxChannels+1]; /* index from 1 */

		rc = getChannelRanges(ranges, maxChannels);		
		if (STATUS_ERR(rc)){
			return rc;
		}
	}
	short* data = new short[nsamples];
	rc = readChannel(channel, data, nsamples, start, stride);

/*
 * (v - v1)/(v2 - v1) = (r - r1)/(r2 - r1)
 * v = v1 + (r - r1)*(v2 - v1)/(r2 - r1)
 *
 * where r1 = -32768, r2 = 32767
 */
#define R1 -32768
#define R2 32767
#define RR (R2 - R1)

	if (STATUS_IS_OK(rc)){
		float vmin = ranges[channel].vmin;
		float vmax = ranges[channel].vmax;
		float vpeak = vmax - vmin;
	
		for (int isam = 0; isam != rc; ++isam){
			data_volts[isam] = vmin + (data[isam] - R1)*vpeak/RR;
		}
	}
		
	delete [] data;	
	return rc;
}


STATUS Acq2xx::readStreamingFrame(Frame* frame, unsigned id)
/**< For streaming data, read the frame
 * @param frame - caller buffer to fill with data
 * @param id - previous frame # - id=0 means "start streaming"
 * @returns STATUS_OK or STATUS_ERR
 */
{
	return transport->readStreamingFrame(frame, id);
}

STATUS Acq2xx::stopStreaming(void)
/**< stops and clears up a previous streaming connection
 * @returns STATUS_OK or STATUS_ERR
 */
{
	return transport->stopStreaming();
}
