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

/** @file acq2xx_api.h ACQ2xx C++ API Definition. */

/** @mainpage ACQ2XX C++ API Definition.
 *  @version $Revision: 1.12 $
 *  @author peter.milne@d-tacq.com

- Aim:
  - provide a C++ control interface to D-TACQ ACQ2xx (ACQ216, ACQ196,ACQ132) 
  - the control interface is transport independent.
  - Applications interact with ACQ2xx card via instance of Acq2xx
  - The Transport is selected when the Acq2xx instance is created, and 
     may be ignored after that.
- @see API Definition
 - Acq2xx  : ACQ2xx functional control class
 - Transport : Transport wrapper, hides connectivity details
- @see Applications:
 - api_test.cpp
 - acqcmd.cpp
 - acq2sh.cpp
 - acqread.cpp
 - acq196_api_test.cpp : starts a continuous capture and monitors state, #samples
 - streamer.cpp : handles continuous streaming data, especially lock-in data
 - acq_demux.cpp : HOST-side channel demultiplexer.
 - acq_demux_continuous ContinuousProcessController.cpp:
 	 HOST side demux continuous raw tblocks -> dirfile
 - acq_demux-mds MdsProcessController.cpp :
 	HOST side demux, stores to MDSplus
 - acq_demux-mds-continuous : HOST side demux continuous raw tblocks -> MDSplus
- @see Transport Implementations:
 - Dt100Transport.cpp
 - KddTransport.cpp
 - SoapTransport.cpp
 - SshTransport.cpp [feasible, but not implemented]
 */

#ifndef __ACQ2XX_API_H__ 
#define __ACQ2XX_API_H__ "$Revision: 1.12 $"


#include "acq_api_common.h"

class Frame;
class Transport;


/** process states. */
enum STATE { 
	ST_STOP, 
	ST_ARM, 
	ST_RUN, 
	ST_TRIGGER, 
	ST_POSTPROCESS, 
	ST_CAPDONE 
};
/** input signal edge control. */
enum acq2xx__EDGE { 
	EDGE_FALLING, EDGE_RISING 
};
/** input signal line select. */
enum acq2xx__DIx { 
	DI_NONE, DI0, DI1, DI2, DI3, DI4, DI5 
};
/** output signal line select. */
enum acq2xx__DOx { 
	DO_NONE, DO0, DO1, DO2, DO3, DO4, DO5 
};

enum acq2xx_Dx {
	D_NONE, D0, D1, D2, D3, D4, D5	
};


const char* toString(enum STATE s);

/** capture modes. */
enum acq2xx__MODE { 
	MODE_SOFT_TRANSIENT, 
	MODE_TRIGGERED_CONTINUOUS, 
	MODE_GATED_TRANSIENT,
	MODE_SOFT_CONTINUOUS
};


struct acq2xx_VRange {
	float vmin; float vmax;
};



struct acq2xx__Signal {
/** aggregate holds full signal definition. */
	enum acq2xx__DIx dix;
	enum acq2xx__EDGE edge;
};


enum acq2xx_RoutePort {
/** Route source or sink, sink may be OR of multiple values. */
	R_NONE,
	R_LEMO,
	R_FPGA,
	R_PXI,
	R_RIO
};


/** Acq2xx class wraps all ACQ2xx functionality.
 *  Acq2xx uses a Transport instance to take care of communications,
 *  so the same Acq2xx class works with any Transport.
 */
class Acq2xx {

protected:       	
	Transport *transport;
	acq2xx_VRange *ranges;
	int ai, ao;

public:
	Acq2xx(Transport* transport);
	/**< Create a transport independent ACQ protocol object. */
	virtual ~Acq2xx()
	{}


	Transport* getTransport(){
		return transport;
	}
	STATUS setRoute(
		enum acq2xx_Dx dx,
		enum acq2xx_RoutePort in,
		enum acq2xx_RoutePort out,
		enum acq2xx_RoutePort out2 = R_NONE,
		enum acq2xx_RoutePort out3 = R_NONE);
	/**< make a signal routing (connection). 
         * "signal" is a digital control signal
	 * @param dx - the signal
         * @param in - source: input port (one only)
         * @param out - sink: output port (up to 3).
	*/

	STATUS setExternalClock(
		enum acq2xx__DIx dix,
		int div = 1,
		enum acq2xx__DOx dox = DO_NONE);
	/**< set External Clock definition.
         * @param dix - the signal
         * @param div - integer divide of dix
         * @param dox - optional output for divided signal
         */ 
	STATUS setInternalClock(
		int hz,
		enum acq2xx__DOx dox = DO_NONE);
	/**< set Internal Clock defiition.
         * @param hz - frequency in Hz
         * @param dox - output the clock on this line
         */

	STATUS getInternalClock(int& hz);
	/**< get the actual Internal Clock frequency.
         * @param hz - output value
         */

	STATUS getAvailableChannels(int& maxChannels);
	/**< get the number of channels on the card.
         * @param maxChannels - output available channels.
	 */

	STATUS selectChannels(const char* channelMask);
	/**< set the active channel mask.
         *   NB not all masks are valid, hardware will select nearest mask
         *   that includes all channels.
         * @param channelMask - mask selects active channels 1=> enabled
	 */

	STATUS getSelectedChannels(char * channelMask, int *count);
	/**< get the actual effective channel mask.
	 * @param channelMask - outputs the actual mask
	 * @param count - outputs selected channel count
         */

	STATUS getChannelRanges(acq2xx_VRange * ranges, int maxRanges);	
	/**< get a list of calibrated range values for each channel.
         * @param ranges - user allocated buffer to hold values.
	 *	index from 1
         * @param maxRanges - length of user buffer
	 */

	STATUS setPrePostMode(
		int prelen, int postlen,
		enum acq2xx__DIx dix = DI3,
		enum acq2xx__EDGE edge = EDGE_FALLING);
	/**< configure the capture Mode.
	 * @param prelen - number of samples before trigger
	 * @param postlen - number of samples after trigger
         * @param dix - signal line for trigger
         * @param edge - sense of the signal
	 */

	STATUS setTriggeredPostMode(int postlen) {
		return setPrePostMode(0, postlen);
	}	
	/**< configure a capture with POST samples only and a hard trigger
         * @param postlen - number of samples after trigger
	 */

	STATUS setSoftTriggeredMode(int postlen) {
		return setPrePostMode(0, postlen, DI_NONE);
	}
	/**< configure a capture with SOFT TRIGGER.
	 * @param postlen - number of samples to capture
	 */

	STATUS setArm();
	/**< arm the card to start the capture. */
	STATUS setAbort();
	/**< abort a capture */
	STATUS getState(enum STATE& state);
	/**< output card state.
	 * @param state - output state value.	
	 */
	
	STATUS waitState(enum STATE state, int timeout = 10000);
	/**< wait for selected State to occur, or timeout
	 * @param state - state to wait for
	 * @param timeout - timeout in msec
	 */

	STATUS getNumSamples(int* total, 
		int* pre = 0, int* post = 0, int *elapsed = 0);
	/**< query current capture state.
	 * @param total - output total samples
	 * @param pre - output pre trigger samples
	 * @param post - output post- trigger samples
	 * @param elapsed - output samples since arm
	 * NB: any param can be null, and is then ignored
	 */
		
	STATUS readChannel(int channel, short* data,
			   int nsamples, int start = 0, int stride = 1);
	/**< read and output raw data for channel
         * @param channel - channel number 1..N
	 * @param data - caller's buffer
         * @param nsamples - max samples to read
         * @param start - start sample in data set
	 * @param stride - stride [subsample] value
	 * @returns actual samples returned or STATUS_ERR
	 */

	STATUS readChannelVolts(int channel, float* data_volts,
			   int nsamples, int start = 0, int stride = 1);
	/**< read and output calibrated data for channel
         * @param channel - channel number 1..N
	 * @param data_volts - caller's buffer
         * @param nsamples - max samples to read
         * @param start - start sample in data set
	 * @param stride - stride [subsample] value
	 * @returns actual samples returned or STATUS_ERR
	 */

	STATUS readStreamingFrame(Frame* frame, unsigned id);
	/**< For streaming data, read the frame
         * @param frame - caller buffer to fill with data
	 * @param id - previous frame # - id=0 means "start streaming"
	 * @returns STATUS_OK or STATUS_ERR
	 */
	STATUS stopStreaming(void);
	/**< stops and clears up a previous streaming connection
	 * @returns STATUS_OK or STATUS_ERR
	 */
};


#endif //__ACQ2XX_API_H__  
