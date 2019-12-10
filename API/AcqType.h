/* ------------------------------------------------------------------------- */
/* file AcqType.h							     */
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

/** @file AcqType.h Encapsulates ACQ2xx product design details .
	. like channel mapping
 * Refs: see ICD for frame definition
*/


#ifndef __ACQTYPE_H__
#define __ACQTYPE_H__

using namespace std;
#include <string>

class Acq2xx;

class NewEventSignature {
	static double last_time;
protected:
	const unsigned long sample_cursor;
	bool has_timestamp;
	unsigned long long timestamp;

	NewEventSignature(unsigned long _sample_cursor):
		sample_cursor(_sample_cursor),
		has_timestamp(false)
	{

	}

	NewEventSignature(bool _has_timestamp):
		sample_cursor(0),
		has_timestamp(_has_timestamp)
	{

	}

public:
	virtual ~NewEventSignature() {}

	bool hasTimeStamp(void) const {
		return has_timestamp;
	}
	unsigned long long  getTimeStamp(void) const {
		return hasTimeStamp()? timestamp: 0;
	}
	virtual double timeInSeconds(void) const;
	unsigned long getSampleCursor() {
		return sample_cursor;
	}
};


class AcqType {

protected:
	AcqType(const string _model, int _nchan, int _word_size) :
		model(_model), nchan(_nchan), word_size(_word_size)
	{}

	AcqType(const char* _model, int _nchan, int _word_size) :
		model(_model),
			nchan(_nchan), word_size(_word_size)
	{}

		static const AcqType& _getAcqType(
			const char personality[], Acq2xx* card = 0);
	/**< Factory: builds AcqType instance from env array. */

public:
	virtual ~AcqType() {}
	const string model;
	const int nchan;
	const int word_size;

	int getNumChannels() const {
		return nchan;
	}
	int getWordSize() const {
		return word_size;
	}
	int getSampleSize() const {
		return getNumChannels() * getWordSize();
	}

	virtual int getChannelOffset(int physchan) const = 0;

	static const AcqType& getAcqType(Acq2xx& card);
	/**< Factory: build AcqType instance by querying card. */
	static const AcqType& getAcqType(const char* fname);
	/**< Factory: build AcqType instance from ini file. */

	virtual NewEventSignature* createES (
			short _raw[], unsigned long _sample_cursor) const
	{
				return 0;
	}
};

class Clock {
public:
	static double sample_clock_ns;
	static double es_clock_ns;
};


class UserEventSignature: public NewEventSignature {
	const double start_time;
public:
	UserEventSignature(double _start_time):
		NewEventSignature(true),
		start_time(_start_time)
	{

	}
	virtual unsigned long long getTimeStamp(void) const {
		return (unsigned long long)(Clock::sample_clock_ns * start_time);
	}
	virtual double timeInSeconds(void) const {
		return start_time;
	}
};

class DualRate {
public:
	static int div0;
	static int div1;

	static bool isDualRate(void) {
		return div0 != 0 && div1 !=0;
	}
};

class File 
{
	FILE *fp;
	string path;
public:
	File(string root, string ext, const char* mode = "a");
	virtual ~File();
	
	FILE *getFp() { return fp; }

	const string getPath() const { return path; }

	int write(double* aDouble);
	int write(short* aShort);
	int writeln(const char* aline);
};


struct Pair {
	float p1;
	float p2;
};

#define DEFAULT_CAL	""
class AcqCal {
protected:
	const AcqType& acq_type;

public:
	AcqCal(const AcqType& _acq_type, string _base_name);
	virtual ~AcqCal() {}
	
	virtual int getCal(int ch, double& gain_v, double& offset_v) const = 0;
	/** ch index from 1 */

	static AcqCal* create(
		const AcqType& _acq_type, 
		string _base_name = string(DEFAULT_CAL));

	static void destroy(AcqCal* acq_cal);
};

class DumpDef {
public:
	enum { PP_NONE, PP_ALL };

	string root;
	unsigned event_sample_start; /* start samples from beginning of shot */
	unsigned event_offset;	     /* offset samples from beginning this data */
	unsigned pre;
	unsigned post;
	bool dump_timebase;
	bool appending;

	DumpDef(string _root,
		unsigned _ess = 0,
		unsigned _esoff = 0,
		int _pre = PP_NONE,
		int _post = PP_ALL):
			root(_root), event_sample_start(_ess),
			event_offset(_esoff), pre(_pre), post(_post),
			dump_timebase(true),
			appending(false)
	{}
	void print();
	bool specifiesFullSet() const {
		return pre == PP_NONE && post == PP_ALL;
	}

	void setAppending(bool _appending){
		appending = _appending;
	}
	static bool common_timebase;
};
#endif /* __ACQTYPE_H__ */
