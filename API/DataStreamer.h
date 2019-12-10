/* ------------------------------------------------------------------------- */
/* file DataStreamer.h							     */
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

/** @file DataStreamer.h continuous data streaming handler.
 *
<pre>
  opts:
</pre>
 */

#ifndef __DATA_STREAMER_H__
#define __DATA_STREAMER_H__

class FrameHandler {
public:
	virtual ~FrameHandler() {}

	virtual void onFrame(
			Acq2xx& _card, const AcqType& _acqType,
			const Frame* frame) = 0;
};

class DataStreamer {

protected:
	Acq2xx& card;
	const AcqType& acqType;

	DataStreamer(
		Acq2xx& _card, 
		const AcqType& _acqType) :
		card(_card), acqType(_acqType)
	{}
public:
	virtual ~DataStreamer() {}

	virtual int streamData() = 0;

	static DataStreamer* create(Acq2xx& card, const AcqType& acqType);
	static FrameHandler* createMeanHandler(
		const AcqType& acqType, bool format_hex = 0);
	static FrameHandler* createTagHandler(int detail);
	static FrameHandler* createNewlineHandler();

	virtual int addFrameHandler(FrameHandler *handler) = 0;
	virtual int delFrameHandler(FrameHandler *handler) = 0;
};

#endif // __DATA_STREAMER_H__
