/* ------------------------------------------------------------------------- */
/* file Frame.h							     */
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

/** @file Frame.h Defines ACQ streaming frame. 
 * Refs: see ICD for frame definition
*/

#ifndef __FRAME_H__
#define __FRAME_H__

#include <assert.h>

#include "AcqType.h"
#define FRAME_SAMPLES 64

typedef unsigned short Tag;

class AcqType;

class Frame {
protected:
	const AcqType& acqType;

	enum STATE {
		EMPTY, FULL, INVALID
	} state;
	unsigned id;
 	
	Tag tags[FRAME_SAMPLES];
	int nchan;
	int word_size;

	virtual int fillData(void *raw, int nraw) = 0;

	int appendTag(int isam, unsigned short tag);
	long long startSampleNumber;

	void computeStartSampleNumber();
	void reset(void) {
		state = EMPTY;
	}
public:
	Frame(const AcqType& acqType);
	virtual ~Frame();

	const Tag getTag(int isam) const {
		return tags[0];
	}
	virtual unsigned getID(void) const
	/**< return ID (frame number). */
	{
		return id;
	}
	enum STATE getState(void) {
		return state;
	}

	int sampleSize(void) const {
		return word_size * nchan;
	}

	int lineSize(void) const {
		return sampleSize() + sizeof(Tag);
	}
	int frameSize(void) const {
		return FRAME_SAMPLES * lineSize();
	}
	
	long long getStartSampleNumber(void) const {
		if (state == FULL){
			return startSampleNumber;
		}else{
			return -1;
		}
	}
	unsigned char getDIO(void) const;
	unsigned short getExtra(void) const;

	static int buildFrame(unsigned id, Frame *frame, void *raw, int nraw);
};

template <class T> class ConcreteFrame : public Frame {

	T* data;

protected:
	virtual int fillData(void *raw, int nraw);
public:
	ConcreteFrame<T>(const AcqType& _acqType);

	virtual ~ConcreteFrame<T>() {
		delete [] data;
	}

	virtual const T* getChannel(int ch) const;
};

/* need to include implementation to get it to link.
 * Stroustrup says so.
 */
 
#include "Frame.c"

#endif // __FRAME_H__
