/* ------------------------------------------------------------------------- */
/* file Frame.c							     */
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

/** @file Frame.c Implementation of Frame template functions
*/


template<class T>
ConcreteFrame<T>::ConcreteFrame(const AcqType& _acqType):
	Frame(_acqType)
{
	assert(sizeof(T) == acqType.getWordSize());
	data = new T[acqType.getNumChannels() * FRAME_SAMPLES];
}


template<class T>
const T* ConcreteFrame<T>::getChannel(int lchan) const
/** looks up pchan and returns data set. */
{
	return &data[acqType.getChannelOffset(lchan) * FRAME_SAMPLES];
}




template<class T>
int ConcreteFrame<T>::fillData(void *raw, int nraw)
/**< fillData() - fill frame with incoming data
 * @@TODO: rejects on invalid tag. Should really try to resync. */
{
	int isam = 0;
	unsigned short *praw = (unsigned short*)raw;

	for (isam = 0; isam < FRAME_SAMPLES; ++isam){
		int rc = appendTag(isam, *praw);
		if (rc != 0){
			return rc;
		}

		/* this is likely to be a misaligned 32 bit access 
		 * could be horribly inefficient - may need optimising */

		T* pdata = (T*)(praw+1);
		for (int ichan = 0; ichan < nchan; ++ichan){
			data[ichan*FRAME_SAMPLES+isam] = pdata[ichan];
		}
		praw += lineSize()/sizeof(unsigned short);
	}	

	
	// temp: let's check what's happening!
	//write(1, tags, sizeof(tags));
	return 0;
}
