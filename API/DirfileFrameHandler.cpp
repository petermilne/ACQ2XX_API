/* ------------------------------------------------------------------------- */
/* file DirfileFrameHandler.cpp						     */
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

/** @file DirfileFrameHandler.cpp - adapter connects stream data to Dirfile output.
 *
<pre>
  opts:
</pre>
 */
#include "local.h"
#include "acq2xx_api.h"
#include "acq_transport.h"
#include "AcqType.h"
#include "Frame.h"
#include "AcqDataModel.h"

#include "DataStreamer.h"
#include "DirfileFrameHandler.h"

template <class T>
class ConcreteDirfileFrameHandler: public DirfileFrameHandler {
	AcqDataModel * model;
	const string root;

public:
	ConcreteDirfileFrameHandler(AcqDataModel* _model, string _root):
		model(_model), root(_root)
	{
		model->dumpFormat(root);
		Clock::sample_clock_ns = 100000;
	}
	virtual void onFrame(
			Acq2xx& _card, const AcqType& _acqType,
			const Frame* frame)
	{
		const ConcreteFrame<T> *cf = 
			dynamic_cast<const ConcreteFrame<T> *>(frame);

		model->clear();

		for (int ch = 1; ch <= _acqType.getNumChannels(); ++ch){
			model->processCooked(
				cf->getChannel(ch), ch, FRAME_SAMPLES*sizeof(T));
		}
		model->addEventSignature(new UserEventSignature(cf->getStartSampleNumber()));
		model->dump(root);
	}
};

FrameHandler* DirfileFrameHandler::create(
	const AcqType& acqType, string outbase)
{
	AcqDataModel *model = AcqDataModel::create(acqType);

	if (acqType.getWordSize() == sizeof(short)){
		return new ConcreteDirfileFrameHandler<short>(model, outbase);
	}else{
		return new ConcreteDirfileFrameHandler<int>(model, outbase);
	}	
}
