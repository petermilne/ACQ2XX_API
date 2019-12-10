/* ------------------------------------------------------------------------- */
/* file DataStreamer.cpp						     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2009 Peter Milne, D-TACQ Solutions Ltd
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

/** @file DataStreamer.cpp : example APP, continuous data streaming handler.
 *
<pre>
  opts:
</pre>
 */

#include <popt.h>

#include <vector>

#include "local.h"
#include "acq2xx_api.h"
#include "acq_transport.h"
#include "AcqType.h"
#include "Frame.h"

#include "DataStreamer.h"

#define MAXCH 10
#define FRAME_LEN	64

class Fixup {
public:
	virtual ~Fixup() {}

	virtual int fixup(int raw) const { 
		return raw; 
	}
};


class FixupUnsignedShort: public Fixup {
public:
	virtual ~FixupUnsignedShort() {}

	virtual int fixup(int raw) const {
		return raw & 0x0000ffff;
	}
};


template <class T>
class FrameHandlerComputingStats : public FrameHandler {
	static const char* fmt_hex;
	static const char* fmt_dec;
	static FixupUnsignedShort fixupUnsignedShort;	
	static Fixup nullFixup;

	const char *fmt;
	const Fixup *fixup;

	void doStats(const AcqType& acqType, const ConcreteFrame<T>* frame)
		/**< output mean of each channel value over frame ..
		 **  ie LP filter the output.
		 **/
	{
		for (int ch = 1; ch <= acqType.getNumChannels(); ++ch){
			const T* ch_data = frame->getChannel(ch);
			int sum = 0;
			int ii;
			for (ii = 0; ii < FRAME_LEN; ++ii){
				sum += ch_data[ii];
			}
			printf(fmt, fixup->fixup(sum/FRAME_LEN));
		}
	}

public:
	void onFrame(
		Acq2xx& card, const AcqType& acqType, const Frame* frame)
	{
		doStats(acqType, dynamic_cast<const ConcreteFrame<T> *>(frame));
	}

	FrameHandlerComputingStats(bool format_hex){
		fmt = format_hex? fmt_hex: fmt_dec;
		if (sizeof(T) == sizeof(short) && format_hex){
			fixup = &fixupUnsignedShort;
		}else{
			fixup = &nullFixup;
		}
	}
};

template <> const char* FrameHandlerComputingStats<short>::fmt_hex = "%04x\t";
template <> const char* FrameHandlerComputingStats<short>::fmt_dec = "%6d\t";

template <> const char* FrameHandlerComputingStats<int>::fmt_hex = "%08x\t";
template <> const char* FrameHandlerComputingStats<int>::fmt_dec = "%11d\t";

template <class T>
Fixup FrameHandlerComputingStats<T>::nullFixup;
template <class T>
FixupUnsignedShort FrameHandlerComputingStats<T>::fixupUnsignedShort;


class FrameHandlerShowsTag : public FrameHandler {
	int detail;

	void showMetadata(const Frame& frame){
		char outline[80];
		int cursor = sprintf(outline, "%10d\t%12lld\t", 
			frame.getID(), frame.getStartSampleNumber());

		if (detail > 1){
			sprintf(outline+cursor, "0x%04x\t0x%04x\t",
				frame.getDIO(), frame.getExtra());	
		}

		printf(outline);
	}
public:
	FrameHandlerShowsTag(int _detail) : 
		detail(_detail) 
	{}

	void onFrame(
		Acq2xx& card, const AcqType& acqType, const Frame* frame)
	{
		showMetadata(*frame);		       
	}
};

class FrameHandlerPrintsNewline : public FrameHandler {
public:
	void onFrame(
		Acq2xx& card, const AcqType& acqType, const Frame* frame)
	{
		printf("\n");		       
	}	
};

template <class T> 
class ConcreteDataStreamer : public DataStreamer {
	vector<FrameHandler*> frameHandlers;

public:
	ConcreteDataStreamer<T> (Acq2xx& _card, const AcqType& _acqType) :
		DataStreamer(_card, _acqType)
	{

	}

	virtual int streamData()
	{
		ConcreteFrame<T> frame(acqType);
		Frame* pf = &frame;
		unsigned id = 0;

		while(1){
			int rc = card.readStreamingFrame(pf, id++);
			dbg(1, "GOT FRAME [%10d] %s sample %lld",
			    id , rc == 0? "OK": "ERR",
			    frame.getStartSampleNumber());

			vector<FrameHandler*>::iterator it;
			for (it = frameHandlers.begin(); 
				it < frameHandlers.end(); ++it){
				(*it)->onFrame(card, acqType, pf);
			}
		}	
		return 0;
	}

	int addFrameHandler(FrameHandler *handler) 
	{
		frameHandlers.push_back(handler);
		return 0;
	}
	int delFrameHandler(FrameHandler *handler)
	{
		vector<FrameHandler*>::iterator it;

		for (it = frameHandlers.begin(); 
			it < frameHandlers.end(); ++it){
			if (*it == handler){
				frameHandlers.erase(it);
				break;
			}
		}
		return 0;
	}
};





DataStreamer* DataStreamer::create(Acq2xx& card, const AcqType& acqType)
{
/** @@todo .. decide other sizes */
	if (acqType.getWordSize() == sizeof(short)){
		return new ConcreteDataStreamer<short>(card, acqType);
	}else{
		return new ConcreteDataStreamer<int>(card, acqType);
	}
}


FrameHandler* DataStreamer::createMeanHandler(
		const AcqType& acqType, bool format_hex)
{
	if (acqType.getWordSize() == sizeof(short)){
		return new FrameHandlerComputingStats<short>(format_hex);
	}else{
		return new FrameHandlerComputingStats<int>(format_hex);
	}	
}
FrameHandler* DataStreamer::createTagHandler(int detail)
{
	return new FrameHandlerShowsTag(detail);
}

FrameHandler* DataStreamer::createNewlineHandler() 
{
	return new FrameHandlerPrintsNewline();
}
