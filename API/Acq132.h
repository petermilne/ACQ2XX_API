/* ------------------------------------------------------------------------- */
/* Acq132.h : interface for ACQ132 BURST MODE decoder                        */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2009 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>
 *                      http://www.d-tacq.com/

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

#ifndef __ACQ132_H__
#define __ACQ132_H__

#include <map>
#include <vector>
#include <string>

#include "AcqType.h"

#define CH_QUAD	4

#define LEFT	'L'
#define RIGHT   'R'

#define SCAN_A	'A'
#define SCAN_B  'B'
#define SCAN_C  'C'
#define SCAN_D  'D'

#define FIFOSAM	128

#define CHANROW	8

using namespace std;

#define MAXDECIM	16
#define PRESCALE	4

#define TBLOCK_ELEMS (0x600000/32/sizeof(short))

#define NSEC_PER_MSEC	1000000

class Acq132DataModel;

class Channel {
	string dump_path;
	string name;
	string fileName;

	string& makeFileName();
public:
	static const char* dump_mode;

	const int ch;
	vector<short> data;

	Channel(int _ch);
	void print();
	void dump(const string& root);
	void dump(const string& root, unsigned s1, unsigned s2);
	const string* getDumpPath() const { return &dump_path; }

	void clear();
};


class EventSignature {

protected:
	short raw[CHANROW];
	const unsigned long sample_cursor;
	int has_timestamp;

	EventSignature(short _raw[], unsigned long _sample_cursor);
public:
	virtual ~EventSignature() {}

	virtual unsigned getSampleCursor() { return sample_cursor; }
	unsigned getRawSampleCursor() { return sample_cursor; }
	virtual unsigned getCount() const = 0;
	virtual unsigned getSamplesInPulse(EventSignature *es1);

	static EventSignature*
		create(short _raw[], unsigned long _sample_cursor);
		/** Factory - returns 0 if not an ES */

	const short* getRaw(void) const {
		return raw;
	}	

	bool hasTimeStamp(void) const {
		return has_timestamp;
	}

	virtual void print(void) { }
};

class Quad {
	Channel* channels[CH_QUAD];
	Channel* unique_channels[CH_QUAD];
	int max_ch;
	int oversample;
	const char scan;
	const char lr;
	int speed;

	void setChannels(string mask);
public:
	Quad(char _scan, char _lr, string mask);

	char getScan() const { return scan; }	

	void print();

	void consume(int ic, short raw);

	void dump(const string& root);
	void dump(const string& root, unsigned s1, unsigned s2);

	void clear();
	friend class DQuad;
};


static string stringFromChar(const char c)
{
	static char s[2];
	s[0] = c;
	s[1] = '\0';
	return *(new string(s));
}

class DQuad {
	Acq132DataModel& parent;
	const string id;
	const string mask;
	Quad left;
	Quad right;
	unsigned long sample_cursor;

	const double clock_interval;
	const long _tsample;

	unsigned first_s1;

	DQuad(Acq132DataModel& _parent, char _scan, string _mask)
		: parent(_parent), id(stringFromChar(_scan)), mask(_mask),
		left(_scan, LEFT, mask),
		right(_scan, RIGHT, mask),
		sample_cursor(0),
		clock_interval(PRESCALE * Clock::sample_clock_ns),
/* @@todo .. valid multirate .. but more commonly sample_clock is OUTPUT CLOCK
		_tsample((long)(Clock::sample_clock_ns * MAXDECIM/left.speed))
*/
		_tsample((long)Clock::sample_clock_ns),
		first_s1(0),
		eventSignatures(),
		eventOffsets()
	{
	}			

	static vector<DQuad*> instances;

	int dumpES(const string& root);
	void dumpTimebasePulse(File& ft, double ns, int samples_in_pulse,
		unsigned tsample);
	double dumpTimebaseUntil(
			File& ft, double t1, 
			unsigned& begin, unsigned end, int div);
	void dumpTimebase(const string& root, unsigned ess = 0);
	void dumpTimebaseDR(const string& root);
	void dumpTimebase(DumpDef& dd, EventSignature& es, unsigned s1, unsigned s2);
	void linkTimebase(const string& root, Quad& quad);
	vector<EventSignature *> eventSignatures;
	vector<int> eventOffsets;

public:
	static DQuad* instance(Acq132DataModel& parent, char scan, string mask);


	char getScan() const { return left.getScan(); }

	void consume(short raw_row[]);

	void print();
	void dump(const string& root, unsigned ess = 0);
	void dump(DumpDef& dd);

	void addES(EventSignature* es, int event_offset);

	unsigned long tsample() {
		return _tsample;
	}
	double startNS(EventSignature* es) const {
		return clock_interval * es->getCount();
	}

	unsigned long getCursor() const {
		return sample_cursor;
	}

	int scanES(const string& root);
	/**< comb ES list, looking for and removing duplicates */

	vector<int>& getEvents() {
		return eventOffsets;
	}

	void clear();
};

#define MASK32 \
  "11111111111111111111111111111111"
//"12345678901234567890123456789012"

#define ACQ132_DEFSCAN "DCBA"

class Acq132DataModel {
	map<int, Channel*> the_channels;
	vector<DQuad*> scanlist;
	vector<int> eventOffsets;			/* event offsets */
	int maxsamples;
	int actual_samples;

	void trimSamples();

	void processSubBlock(short *data, int ndata);
	void processAll(short *data, int ndata);

public:
	bool timed_at_event;
	unsigned msecs_start;
	bool pushEvent_called;

	Acq132DataModel(
		string _model_def, 
		string _scanlist = ACQ132_DEFSCAN, 
		string _channelMask = MASK32);
	virtual ~Acq132DataModel() {}

	void print();
	void process(short *data, int ndata);
	void dump(const string& root, unsigned ess = 0);
	void dump(DumpDef& dd);
	virtual void setMaxsamples(int _maxsamples) {
		maxsamples = _maxsamples;
	}
	vector<short>& getChannel(int ichan);
	virtual vector<int>& getEvents();

	void pushEvent(int offset) {
		pushEvent_called = true;
		eventOffsets.push_back(offset);
	}
	void clear();

	void setWallClockPolicy(unsigned _msecs_start, bool timed_at_event);
};


#endif
