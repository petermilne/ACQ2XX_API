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

/** @file Frame.cpp implements ACQ streaming frame. 
 * Refs: see ICD for frame definition
*/

#include "local.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Frame.h"

#include "AcqType.h"

#include "acq32busprot.h"

Frame::Frame(const AcqType& _acqType) :
	acqType(_acqType),
	nchan(acqType.getNumChannels()), 
	word_size(acqType.getWordSize()) 
{
}

Frame::~Frame()
{
}
static int nx_bit(unsigned short tag)
{
	return (tag&0x8000) != 0;
}

void Frame::computeStartSampleNumber(void)
{
	int ibit;

	startSampleNumber = 0;

	for (ibit = 0; ibit < 48; ++ibit){
		if (nx_bit(tags[ibit])){
			startSampleNumber |= (1 << ibit);
		}
	}
	dbg(2, "startSampleNumber %lld", startSampleNumber);
}

unsigned short Frame::getExtra(void) const
{
	unsigned short extra = 0;
	int ibit;

	for (ibit = 0; ibit < 16; ++ibit){
		if (nx_bit(tags[48+ibit])){
			extra |= (1 <<ibit);
		}
	}
	return extra;	
}

unsigned char Frame::getDIO(void) const
{
	return tags[MFX_DIO] & 0x00ff;
}

int Frame::appendTag(int isam, unsigned short tag)
{
	unsigned short id = tag & 0x00ff;
	int isam_tag = (tag >> 8) & 0x3f;

	switch(isam){
	case 0:
		if (id == 0xfe){
			break;
		}else{
			err("TAG is NOT TAG1 0x%04x", tag);
			goto error;
		}
	case 1:
		if (id == 0xed){
			break;
		}else{
			err("TAG is NOT TAG2 0x%04x", tag);
			goto error;
		}
	}

	if (isam_tag != isam){
		err("ISAM_TAG != isam");
		goto error;
	}
	
	dbg(3, "STORING tag %02d %04x %d", isam, tag, nx_bit(tag));

	tags[isam] = tag;

	switch(isam){
	case FRAME_SAMPLES-1:
		state = FULL;
		computeStartSampleNumber();		
	}

	return 0;

error:
	state = INVALID;
	return -1;

}

int Frame::buildFrame(unsigned id, Frame *frame, void *raw, int nraw) {
	frame->id = id;
	frame->state = EMPTY;
	return frame->fillData(raw, nraw);
}
