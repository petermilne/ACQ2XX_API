/* ------------------------------------------------------------------------- */
/* file DirfileFrameHandler.h						     */
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

/** @file DirfileFrameHandler.h - adapter connects stream data to Dirfile output.
 *
<pre>
  opts:
</pre>
 */

#ifndef __DIRFILEFRAMEHANDLER_H__
#define __DIRFILEFRAMEHANDLER_H__

class DirfileFrameHandler: public FrameHandler {
	virtual void onFrame(
			Acq2xx& _card, const AcqType& _acqType,
			const Frame* frame) = 0;
public:
	virtual ~DirfileFrameHandler() {}
	static FrameHandler* create(const AcqType& _acqType, string outbase);
};


#endif // __DIRFILEFRAMEHANDLER_H__
