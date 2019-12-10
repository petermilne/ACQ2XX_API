/* ------------------------------------------------------------------------- */
/* Acq132.cpp: implementation of ACQ132 BURST MODE decoder                   */
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

#include <assert.h>
#include <iostream>
#include <map>
#include <math.h>
#include <string.h>

#include "local.h"
#include "Acq132.h"
#include "AcqDataModel.h"

// @@todo dynamic NUMCHANNELS?
#define NUMCHAN	32


/* Event data fields from rev 307+ */
#define ACQ132_ES_DEC_CNT_MASK 0xff00
#define ACQ132_ES_LAT_MASK	0x00fc

#define ACQ132_ES_DEC_CNT(es)	(((es)&ACQ132_ES_DEC_CNT_MASK)>>8)
#define ACQ132_ES_LAT_CNT(es)	(((es)&ACQ132_ES_LAT_MASK)>>2)

/* 2 sample pipeline, could be more: eg with FIR.
 * values found for decimating boxcar found empirically */
/* NACC [1..16] */
static const int es_cold_offset_samples[129] = { /* [1..128] */
/*      1,2,3,4,5,6,7,8,9,a,b,c,d,e,f,0 */
	0,
	-5,-1,
              0,2,1,1,2,2,2,2,2,2,2,2,2,2,	/* 0 */
        2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,	/* 1 */
        2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,	/* 2 */
        2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,	/* 3 */
        2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,	/* 4 */
        2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,	/* 5 */
        2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,	/* 6 */
        2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,	/* 7 */
};


Channel::Channel(int _ch)
	: dump_path(""), ch(_ch)
{
	char buf[16];
	sprintf(buf, "CH%02d", ch);

	name = buf;
	data.reserve(TBLOCK_ELEMS);
}

string& Channel::makeFileName() {
	return fileName = AcqDataModel::pfx + name;
}
void Channel::print() { 
	cerr << "Channel:" << ch << " len:" << data.size() << endl;
}

void Channel::dump(const string& root)
{
	File f(root, makeFileName(), Channel::dump_mode);

	fwrite(&data[0], sizeof(short), data.size(), f.getFp());
	dump_path = f.getPath();
}

void Channel::dump(const string& root, unsigned s1, unsigned s2)
{
	File f(root, makeFileName(), Channel::dump_mode);

	int len = (s2 <= data.size()? s2: data.size()) - s1;

	fwrite(&data[s1], sizeof(short), len, f.getFp());
	dump_path = f.getPath();
}

void Channel::clear()
{
	data.clear();
}

class RGM_EventSignature : public EventSignature {

	static bool isES(const short _raw[]);

	RGM_EventSignature(short _raw[], unsigned long _sample_cursor):
		EventSignature(_raw, _sample_cursor)
		{
			has_timestamp = 1;
			dbg(1, "%d", _sample_cursor);
		}
public:
	static EventSignature* create(
		short _raw[], unsigned long _sample_cursor)

	{
		if (isES(_raw)){
			return new RGM_EventSignature(_raw, _sample_cursor);
		}else{
			return 0;
		}
	}
	virtual unsigned getCount() const {
		union {
			unsigned short s[2];
			unsigned u;
		} tmp;
		
		tmp.s[1] = raw[CHANROW/2];
		tmp.s[0] = raw[CHANROW/2+2];
		return tmp.u;	
	}
};

class DR_EventSignature : public EventSignature {

	static bool isES(const short _raw[]);

	DR_EventSignature(short _raw[], unsigned long _sample_cursor) :
		EventSignature(_raw, _sample_cursor)
		{
			dbg(3, "%d", _sample_cursor);
		}
public:

	static EventSignature* create(
		short _raw[], unsigned long _sample_cursor)

	{
		if (isES(_raw)){
			return new DR_EventSignature(_raw, _sample_cursor);
		}else{
			return 0;
		}
	}

	virtual unsigned getCount() const {
		return 0;
	}

	bool isTransitionToDiv1(void) const {
		return (raw[3] & 0x1) != 0;
	}

	bool isTransitionToDiv0(void) const {
		return (raw[3] & 0x2) != 0;
	}

	bool isBogusTransition(void) const {
		bool A = isTransitionToDiv1();
		bool B = isTransitionToDiv0();
		return !(A ^ B);
	}

	virtual void print(void) {
		printf("DR %u %04x %04x %04x %04x\n",
		       getSampleCursor(),
		       raw[0], raw[1], raw[2], raw[3]);
	}

	int getOffset(int div){
		int offset = ACQ132_ES_LAT_CNT(raw[2]);
		offset += es_cold_offset_samples[div];
		return offset;		
	}
	int getDecimationPhase(void) {
		return ACQ132_ES_DEC_CNT(raw[2]);
	}
	virtual unsigned long getSampleCursor(int div) {

		return sample_cursor - getOffset(div);
	}
	virtual unsigned getSampleCursor() {
		return getSampleCursor(0);
	}

};

EventSignature::EventSignature(short _raw[], unsigned long _sample_cursor) 
:	sample_cursor(_sample_cursor), has_timestamp(0)
{
	memcpy(raw, _raw, sizeof(raw));
}

bool DR_EventSignature::isES(const short _raw[]){
	const unsigned short* rr = (const unsigned short *)_raw;

	int rc = (rr[0] == 0xaa55) && (rr[1] == 0xaa55) &&
		 (rr[4] == 0xaa55) && (rr[5] == 0xaa55);

	
	if (rc){
		dbg(2, "raw: %04x %04x %04x %04x %04x %04x %04x %04x rc:%d",
		    rr[0],	rr[1], rr[2],	rr[3],
		    rr[4],	rr[5], rr[6],	rr[7], rc);
	}else if (rr[0] == 0xaa55){
		dbg(3, 
		"raw part: %04x %04x %04x %04x %04x %04x %04x %04x rc:%d",
		    rr[0],	rr[1], rr[2],	rr[3],
		    rr[4],	rr[5], rr[6],	rr[7], rc);
	}
	return rc;
}

bool RGM_EventSignature::isES(const short _raw[]){
	const unsigned short* rr = (const unsigned short *)_raw;

	int rc = (rr[0] == 0xaa55) && (rr[1] == 0xaa55) &&
		 (rr[2] == 0xaa55) && (rr[3] == 0xaa55);

	
	if (rc){
		dbg(2, "raw: %04x %04x %04x %04x %04x %04x %04x %04x rc:%d",
		    rr[0],	rr[1], rr[2],	rr[3],
		    rr[4],	rr[5], rr[6],	rr[7], rc);
	}else if (rr[0] == 0xaa55){
		dbg(3, 
		"raw part: %04x %04x %04x %04x %04x %04x %04x %04x rc:%d",
		    rr[0],	rr[1], rr[2],	rr[3],
		    rr[4],	rr[5], rr[6],	rr[7], rc);
	}
	return rc;
}

EventSignature* EventSignature::create(
	short _raw[], unsigned long _sample_cursor)
{
	EventSignature *es = RGM_EventSignature::create(_raw, _sample_cursor);

	if (es){
		return es;
	}else{
		return es = DR_EventSignature::create(_raw, _sample_cursor);
	}
}


Quad::Quad(char _scan, char _lr, string mask) 
	: max_ch(0), scan(_scan),  lr(_lr)
{
	if (acq200_debug > 1){
		cerr << "Quad() " << scan << lr << endl;
	}
	setChannels(mask);
	string::const_iterator cii;

	for(cii = mask.begin(); cii != mask.end(); cii++){
		char cc = *cii;
		if (cc != '0'){
			if (cc >= '1' && cc <= '9'){
				speed = cc - '0';
			}else if (cc >= 'a' && cc <= 'z'){
				speed = cc - 'a' + 10;
			}else if (cc >= 'A' && cc <= 'Z' ){
				speed = cc - 'A' + 10 + 26;
			}else{
				speed = 1;
				cerr << "Bad speed code: " << cc << endl;
			}
		}
	}
};

void Quad::print() {
	cerr << "Quad: scan:" << scan << 
		" lr:" << lr << "speed:" << speed <<endl;

	for (int ic = 0; ic < CH_QUAD; ++ic){
		if (channels[ic] != 0){
			channels[ic]->print();
		}
	}
}

void Quad::consume(int ic, short raw){
	if (channels[ic] != 0){
		channels[ic]->data.push_back(raw);
	}
}




void DQuad::consume(short raw_row[]) {
	unsigned short *rr = (unsigned short *)raw_row;
	dbg(4, "raw: %04x %04x %04x %04x %04x %04x %04x %04x",
	    rr[0],	rr[1], rr[2],	rr[3],
	    rr[4],	rr[5], rr[6],	rr[7]);
	
	for (int ic = 0; ic < 2*CH_QUAD; ){
		left.consume(ic/2,  rr[ic]); ic++;
		right.consume(ic/2, rr[ic]); ic++;
	}
	++sample_cursor;
}

void DQuad::print() {
	cerr << "DQuad: " << endl;
	left.print();
	right.print();
	cerr << "ES len: " << eventSignatures.size() << endl;
}


void DQuad::addES(EventSignature* _es, int eventOffset)
{
	dbg(3, "01 offset %d", eventOffset);

	if (eventOffsets.size() == 0 || eventOffset != eventOffsets.back()){
		eventSignatures.push_back(_es);
		eventOffsets.push_back(eventOffset);
	}
}

vector<DQuad*> DQuad::instances;

#define NQUAD	4

struct ChannelMap {
	char scan;
	char lr;
	Channel channels[NQUAD];
};

struct ChannelMap default_channelMaps [] = {
{ SCAN_A, LEFT,  { Channel(13), Channel(14), Channel(15), Channel(16) } },
{ SCAN_A, RIGHT, { Channel(29), Channel(30), Channel(31), Channel(32) } },
{ SCAN_B, LEFT,  { Channel( 9), Channel(10), Channel(11), Channel(12) } },
{ SCAN_B, RIGHT, { Channel(25), Channel(26), Channel(27), Channel(28) } },
{ SCAN_C, LEFT,  { Channel( 5), Channel( 6), Channel( 7), Channel( 8) } },
{ SCAN_C, RIGHT, { Channel(21), Channel(22), Channel(23), Channel(24) } },
{ SCAN_D, LEFT,  { Channel( 1), Channel( 2), Channel( 3), Channel( 4) } },
{ SCAN_D, RIGHT, { Channel(17), Channel(18), Channel(19), Channel(20) } },	
};

struct ChannelMap lfp_channelMaps [] = {
{ SCAN_A, LEFT,  { Channel( 3), Channel( 4), Channel( 1), Channel( 2) } },
{ SCAN_A, RIGHT, { Channel(19), Channel(20), Channel(17), Channel(18) } },
{ SCAN_B, LEFT,  { Channel( 7), Channel( 8), Channel( 5), Channel( 6) } },
{ SCAN_B, RIGHT, { Channel(23), Channel(24), Channel(21), Channel(22) } },
{ SCAN_C, LEFT,  { Channel(11), Channel(12), Channel( 9), Channel(10) } },
{ SCAN_C, RIGHT, { Channel(27), Channel(28), Channel(25), Channel(26) } },
{ SCAN_D, LEFT,  { Channel(15), Channel(16), Channel(13), Channel(14) } },
{ SCAN_D, RIGHT, { Channel(31), Channel(32), Channel(29), Channel(30) } },
};

struct ChannelMap *channelMaps = default_channelMaps;

#define NCHANNELMAPS 8

class Cmask : public string {

public:
	Cmask(string a_string) 
	: string(a_string)
	{
		string::const_iterator cii;
		int ii = 0;

		for(cii = begin(); cii != end(); cii++, ++ii){
			if (*cii != '0'){
				replace(ii, 1, "1");
			}
		}		
	}
};


void Quad::setChannels(string cs_mask)
{
	int ix = (scan-SCAN_A)*2 + (lr==RIGHT);
	Cmask c_mask(cs_mask);

	assert(ix >= 0 && ix <= NCHANNELMAPS);

	ChannelMap *map = &channelMaps[ix];

	if (c_mask == "1111"){
		channels[0] = &map->channels[0];
		channels[1] = &map->channels[1];
		channels[2] = &map->channels[2];
		channels[3] = &map->channels[3];
	}else if (c_mask == "1010"){
		channels[0] = &map->channels[2];	/* 3 */
		channels[1] = &map->channels[2];	/* 3 */
		channels[2] = &map->channels[0];	/* 1 */
		channels[3] = &map->channels[0];	/* 1 */
	}else if (c_mask == "1000"){
		channels[0] = 
		channels[1] = 
		channels[2] = 
		channels[3] = &map->channels[0];	/* 1 */
	}else{
		cerr << "Unknown mask:" << c_mask << endl;
	}

	for (int ch = 0; ch < CH_QUAD; ++ch){
		for (int ch0 = 0; ch0 < max_ch; ++ch0){
			if (channels[ch] == unique_channels[ch0]){
				goto next_channel;
			}
		}
		unique_channels[max_ch++] = channels[ch];

	next_channel:
		continue;
	}


}

void Quad::dump(const string& root)
{
	dbg(2, "root %s", root.c_str());
	for (int ch = 0; ch < max_ch; ++ch){
		unique_channels[ch]->dump(root);
	}
}


void Quad::dump(const string& root, unsigned s1, unsigned s2)
{
	dbg(2, "root %s", root.c_str());
	for (int ch = 0; ch < max_ch; ++ch){
		unique_channels[ch]->dump(root, s1, s2);
	}
}

void Quad::clear()
{
	for (int ch = 0; ch < max_ch; ++ch){
		unique_channels[ch]->clear();
	}
}

DQuad* DQuad::instance(Acq132DataModel& _parent, char scan, string mask)
{
	vector<DQuad*>::iterator iter;
	DQuad *dq;

	for (iter = instances.begin(); iter != instances.end(); ++iter){
		if ((*iter)->getScan() == scan){
			cerr << "DQuad factory re-use:" << scan << endl;
			return dq = *iter;
		}
	}
	instances.push_back(dq = new DQuad(_parent, scan, mask));
	return dq;
}


unsigned EventSignature::getSamplesInPulse(EventSignature *es1)
{
	return es1->sample_cursor - sample_cursor;
}

void DQuad::dumpTimebasePulse(
	File& ft, double ns, int samples_in_pulse,
	unsigned tsample)
{
	double sec = ns/1e9;
	double sample_sec = ((double)tsample) / 1e9;

	for (int isam = 0; isam < samples_in_pulse; ++isam){
		ft.write(&sec);
		sec += sample_sec;
	}
}

void DQuad::dumpTimebase(const string& root, unsigned ess)
{
	char tb_name[80];

	sprintf(tb_name, "%sTB%s", AcqDataModel::pfx.c_str(), id.c_str());
	File ft(root, tb_name);

	if (eventSignatures.size() == 0){
		double ns = ess * Clock::sample_clock_ns;
		dumpTimebasePulse(
			ft, ns, left.channels[0]->data.size(), _tsample);
		return;
	}

	vector<EventSignature*>::iterator iter = eventSignatures.begin();
	EventSignature* es0 = *iter;
	for (++iter; iter != eventSignatures.end(); ++iter){
		EventSignature* es1 = *iter;

		dumpTimebasePulse(ft, startNS(es0),
			es0->getSamplesInPulse(es1), _tsample);
		es0 = es1;
	}
	/* pick up last pulse */
	
	dumpTimebasePulse(ft, startNS(es0),
		left.channels[0]->data.size() - es0->getSampleCursor(), 
		_tsample);
}

double DQuad::dumpTimebaseUntil(
	File& ft, double t1, unsigned& begin, unsigned end, int div)
{
       	unsigned uu;
	double dt = Clock::sample_clock_ns * 1e-9 * div;

	dbg(1, "%g %u %d dt %g", t1, end, div, dt);

	for (uu = begin; uu < end; ++uu){
		ft.write(&t1);
		t1 += dt;
	}
	begin = end;
	return t1;
}


void DQuad::dumpTimebase(DumpDef& dd, EventSignature& es, unsigned s1, unsigned s2)
{
	char tb_name[80];
	sprintf(tb_name, "%sTB%s", AcqDataModel::pfx.c_str(), id.c_str());
	File ft(dd.root, tb_name, Channel::dump_mode);
	unsigned delta_sam = dd.event_sample_start - dd.event_offset + s1;
	double t1_nsec;

	if (parent.timed_at_event){
		if (es.getSampleCursor() ==
				(*eventSignatures.begin())->getSampleCursor()){
			t1_nsec = (double)parent.msecs_start*NSEC_PER_MSEC -
							dd.pre * _tsample;
			first_s1 = s1;
		}else{
			t1_nsec = (double)parent.msecs_start*NSEC_PER_MSEC +
				(s1 - first_s1 - dd.pre) * _tsample;
		}
	}else{
		t1_nsec = (double)(delta_sam) *_tsample +
			  parent.msecs_start * NSEC_PER_MSEC;
	}

	dbg(1, "%s t1 %f _tsample %lu",
		AcqDataModel::pfx.c_str(), t1_nsec/1.0e9, _tsample);

	dumpTimebasePulse(ft, t1_nsec, s2 - s1, _tsample);
}

static void dumpES_num(
	File& f, int offset, int decim_phase, int es_num, int len)
{
	int *buf = new int[len];
	
	buf[0] = offset;
	buf[1] = offset;
	buf[2] = decim_phase;
	buf[3] = decim_phase;
	buf[4] = 128;
	buf[5] = 0;
	for (int ii = 6; ii < len; ++ii){
		buf[ii] = es_num;
	}
	fwrite(buf, sizeof(int), len, f.getFp());
	delete [] buf;
}

static void dumpTimestamp(File& f, int sample, double t1, DR_EventSignature* dr)
{
	int div = dr->isTransitionToDiv0()? DualRate::div1: DualRate::div0;

	fprintf(f.getFp(), "%10d %10.6f %3d %3d %d\n",
		sample, t1, 
		dr->getDecimationPhase(), dr->getOffset(div),
		div);
}


void DQuad::dumpTimebaseDR(const string& root)
/* first, decide if start in HI or LO
 * then, accumulate time at the appropriate rate
 */
{
	// **todo ... use pfx
	File ft(root, "TB"+id);
	File tbix(root, "TBX"+id);
	File es_table(root, "ESvT"+id);

	vector<EventSignature*>::iterator iter;
	int es_count = 0;
	double ttotal = 0;
	unsigned sample = 0;
	DR_EventSignature* dr = 0;

	for (iter = eventSignatures.begin(); iter != eventSignatures.end(); ++iter){
		EventSignature* es = *iter;
		if (es->hasTimeStamp()){
			dbg(1, "%s:skip bogus RGM event at %d", 
			    id.c_str(), es_count);
			es->print();
			continue;
		}else{
			int div;

			dr = dynamic_cast<DR_EventSignature*>(es);

			if (!dr){
				err("DOWNCAST to DR_EventSignature failed??");
				continue;
			}else if (dr->isBogusTransition()){
				dbg(1, "skip bogus DR transition");
				continue;		
			}else{
				div = dr->isTransitionToDiv0() ?
					DualRate::div1: DualRate::div0;

				dumpTimestamp(es_table, sample, ttotal, dr);

				dumpES_num(tbix, 
					   dr->getOffset(div),
					   dr->getDecimationPhase(),
					   es_count, 
					   dr->getSampleCursor(div) - sample);

				ttotal = dumpTimebaseUntil(ft, ttotal, sample,
					   dr->getSampleCursor(div), div);

			}
		}
		++es_count;
	}

	if (dr){	
		dumpTimebaseUntil(ft, ttotal, sample, getCursor(),	
			  dr->isTransitionToDiv0()? 
				DualRate::div0: DualRate::div1);
	}
		
}
int DQuad::dumpES(const string& root)
{
	File f(root, "ES"+id);
	vector<EventSignature*>::iterator iter;
	int es_count = 0;

	for (iter = eventSignatures.begin(); iter != eventSignatures.end(); ++iter){
		unsigned count = (*iter)->getCount();
		fwrite(&count, sizeof(unsigned), 1, f.getFp());
		++es_count;
	}
	return es_count;
}


int DQuad::scanES(const string& root)
{
	File f(root, "ES-dump"+id);
	vector<EventSignature*>::iterator iter;
	vector<EventSignature*>::iterator behind;
	int es_count = 0;

	for (iter = eventSignatures.begin(); iter != eventSignatures.end(); ++iter, ++es_count){
		unsigned long sc = (*iter)->getSampleCursor();
		fwrite(&sc, sizeof(unsigned long), 1, f.getFp());
		fwrite((*iter)->getRaw(), sizeof(short)*CHANROW, 1, f.getFp());
	}
	return es_count;
}
void DQuad::linkTimebase(const string& root, Quad& quad)
{
	string tb = AcqDataModel::pfx + "TB"+id;
	int maxlen = root.length()+ AcqDataModel::pfx.length() + 20;
	char *ch_path = new char[maxlen];

	for (int ic = 0; ic < quad.max_ch; ++ic){
		snprintf(ch_path, maxlen, "%s/%sTB%02d", root.c_str(),
				AcqDataModel::pfx.c_str(),
				quad.unique_channels[ic]->ch);
		dbg(2, "calling symlink %s %s", tb.c_str(), ch_path);
		symlink(tb.c_str(), ch_path);

		if (DumpDef::common_timebase){
			break;
		}
	}
	delete [] ch_path;
}
void DQuad::dump(const string& root, unsigned ess)
{
	dumpES(root);

	if (Clock::sample_clock_ns > 0){
		if (DualRate::isDualRate()){
			dumpTimebaseDR(root);
		}else{
			dumpTimebase(root, ess);
		}
		if (ess == 0){
			linkTimebase(root, left);
			linkTimebase(root, right);
		}
	}
	left.dump(root);
	right.dump(root);
}

void DQuad::clear()
{
	left.clear();
	right.clear();
	vector<EventSignature *>::iterator esit = eventSignatures.begin();
	for (; esit != eventSignatures.end(); ++esit){
		delete (*esit);
	}
	eventSignatures.clear();
	eventOffsets.clear();
	sample_cursor = 0;
}
void DQuad::dump(DumpDef& dd)
{
	vector<EventSignature *>::iterator esit = eventSignatures.begin();
	for (; esit != eventSignatures.end(); ++esit){
		EventSignature *esp = *esit;


		dbg(2, "sample_cursor %d with %d %s",
				esp->getRawSampleCursor(), dd.event_offset,
				esp->getRawSampleCursor() == dd.event_offset?
						"MATCH": "no match");

		if (esp->getRawSampleCursor() == dd.event_offset){
			unsigned sc = esp->getSampleCursor();
			unsigned s1 = sc > dd.pre? sc - dd.pre: 0;
			unsigned s2 = sc + dd.post; //** @@todo overrun caught later?

			dbg(2, "sample Cursor %ld s1 %d s2 %d\n", sc, s1, s2);

			left.dump(dd.root, s1, s2);
			right.dump(dd.root, s1, s2);

			if (dd.dump_timebase){
				if (Clock::sample_clock_ns > 0){
					dumpTimebase(dd, *esp, s1, s2);
					linkTimebase(dd.root, left);
					linkTimebase(dd.root, right);
				}
				if (DumpDef::common_timebase){
					dd.dump_timebase = false;
				}
			}

			return;
		}
	}
	err("FAILED to match event_offset");
}


Acq132DataModel::Acq132DataModel(
	string model_def,
	string _scanlist, string _channelMask) 
	: the_channels(),  maxsamples(0x7fffffff), actual_samples(0),
	  timed_at_event(false), pushEvent_called(false)
{
#ifdef IKNOWWHATSWRONG
	string cm1 = "x" + _channelMask;
#else
	char abuf[128];
	sprintf(abuf, "x%s", _channelMask.c_str());
	string cm1 = abuf;
#endif

	dbg(1, "model_def %s scanlist %s channelMask %s",
		model_def.c_str(), _scanlist.c_str(), _channelMask.c_str());
	dbg(2, "cm1 len %d \"%s\"", cm1.length(), cm1.c_str());

	if (model_def.find("lfp") != string::npos){
		dbg(1, "setting lfp channel map");
		channelMaps = lfp_channelMaps;
	}

	for (int ii = 0; ii != NCHANNELMAPS; ++ii){
		for (int jj = 0; jj != NQUAD; ++jj){
			Channel *channel = &channelMaps[ii].channels[jj];
			the_channels[channel->ch] = channel;
		}
	}
#if 0
	for (int ii = 1; ii != 32; ++ii){
		dbg(1, "ii %d ch %d", ii, the_channels[ii]->ch);
	}
#endif

	string cmD = cm1.substr( 1, 4); string cmD2 = cm1.substr(17, 4);
	string cmC = cm1.substr( 5, 4); string cmC2 = cm1.substr(21, 4);
	string cmB = cm1.substr( 9, 4); string cmB2 = cm1.substr(25, 4);
	string cmA = cm1.substr(13, 4); string cmA2 = cm1.substr(29, 4);

	assert(cmD == cmD2);
	assert(cmC == cmC2);
	assert(cmB == cmB2);
	assert(cmA == cmA2);

	map<char, string> masks;
	masks[SCAN_A] = cmA;
	masks[SCAN_B] = cmB;
	masks[SCAN_C] = cmC;
	masks[SCAN_D] = cmD;

	for (unsigned iscan = 0; iscan < _scanlist.length(); ++iscan){
		char scan = _scanlist[iscan];
		scanlist.push_back(DQuad::instance(*this, scan, masks[scan]));
	}
}
vector<short>& Acq132DataModel::getChannel(int ichan)
{
	return the_channels[ichan]->data;
}

void Acq132DataModel::print()
{
	vector<DQuad*>::iterator iter;

	cerr << "Acq132Data:" << endl;
	for (iter = scanlist.begin(); iter != scanlist.end(); ++iter){
		(*iter)->print();
	}	
}

static int SCAN_LIST3_HACK = getenv("SCAN_LIST3_HACK") != 0;

void Acq132DataModel::processAll(short *data, int ndata)
{
	vector<DQuad*>::iterator iter;

	dbg(2, "01 %p %d", data, ndata);

	int _actual_samples = 0;
	int hack_alert_count = 0;
	int first_time = 1;
	int isam;
	bool stash_es = true;

	if (AcqDataModel::processNoStashES(ndata)){
		stash_es = false;
	}

	while (ndata > 0 && actual_samples < maxsamples){

		for (iter = scanlist.begin(); iter != scanlist.end(); ++iter){

			DQuad *dquad = *iter;
			_actual_samples = actual_samples;

			if (first_time+acq200_debug > 2){
				dquad->print();
			}

			for (isam = 0; ndata > 0 && isam < FIFOSAM; ++isam){
				int is_data;

				EventSignature *es = 
					EventSignature::create(
						data, dquad->getCursor());
				if (es){
					if (stash_es){
						dquad->addES(es, dquad->getCursor());
					}
					is_data = 0;
				}else{
					dquad->consume(data);
					is_data = 1;
				}
				data += CHANROW;
				ndata -= CHANROW;

				if (is_data && ++_actual_samples >= maxsamples){
					break;
				}

			}
		}

		first_time = 0;

		if (SCAN_LIST3_HACK){
/** this is a HACK to test a theory about 1111111111110000 case*/
			int len = 0;
			for (iter = scanlist.begin(); 
				iter != scanlist.end(); ++iter){
				++len;
			}
			if (len == 3){
				if (hack_alert_count++ == 0){
					info("HACK ALERT scanlist 3, skip one");
				}
				data += CHANROW * FIFOSAM;
				ndata -= CHANROW * FIFOSAM;
				
			}
			actual_samples = _actual_samples;
		}
	}


	dbg(2, "99");
}

void Acq132DataModel::processSubBlock(short *data, int ndata)
{
	vector<DQuad*>::iterator iter;

	dbg(1, "01 %p %d", data, ndata);

	int nsam = ndata / NUMCHAN;
	int first_time = 1;

	while (ndata > 0 && actual_samples < maxsamples){

		for (iter = scanlist.begin(); iter != scanlist.end(); ++iter){
			DQuad *dquad = *iter;

			if (first_time+acq200_debug > 2){
				dquad->print();
			}

			for (int isam = 0; ndata > 0 && isam < nsam; ++isam){
				int is_data;

				EventSignature *es =
					EventSignature::create(
						data, dquad->getCursor());
				if (es){
					dquad->addES(es, dquad->getCursor());
					is_data = 0;
				}else{
					dquad->consume(data);
					is_data = 1;
				}
				data += CHANROW;
				ndata -= CHANROW;
			}
			data += CHANROW * (FIFOSAM - nsam);
		}
		first_time = 0;
	}

	dbg(1, "99");
}
void Acq132DataModel::process(short *data, int ndata)
{
	dbg(2, "%p %d abs %d", data, ndata, abs(ndata));

	if (abs(ndata) < FIFOSAM*NUMCHAN){
		processSubBlock(data, ndata);
	}else{
		processAll(data, ndata);
	}
}

void Acq132DataModel::trimSamples(void)
{
	map<int, Channel*>::iterator it;
	unsigned shortest = 0;
	bool shortest_set = false;

	if (maxsamples){
		shortest = maxsamples;
		shortest_set = true;

	}
	for (it = the_channels.begin(); it != the_channels.end(); ++it){
		if (!shortest_set || it->second->data.size() < shortest){
			shortest = it->second->data.size();
			shortest_set = true;
		}
	}

	if (!shortest_set){
		return;
	}
	for (it = the_channels.begin(); it != the_channels.end(); ++it){
		for (unsigned ndel = it->second->data.size()-shortest; ndel; --ndel){
			it->second->data.pop_back();
		}
	}
}

void Acq132DataModel::dump(const string& root, unsigned ess)
{
	dbg(2, "root %s", root.c_str());

	trimSamples();

	for (vector<DQuad*>::iterator iter = scanlist.begin();
			iter != scanlist.end(); ++iter){
		(*iter)->dump(root, ess);
	}

	for (vector<DQuad*>::iterator iter = scanlist.begin();
			iter != scanlist.end(); ++iter){
		(*iter)->scanES(root);
	}
}

void Acq132DataModel::setWallClockPolicy(
		unsigned _msecs_start, bool _timed_at_event)
{
	msecs_start = _msecs_start;
	timed_at_event = _timed_at_event;
}
void Acq132DataModel::clear()
{
	for (vector<DQuad*>::iterator iter = scanlist.begin();
			iter != scanlist.end(); ++iter){
		(*iter)->clear();
	}
}
void Acq132DataModel::dump(DumpDef&dd)
{
	// @@todo ... DO SOMETHING with dd. ie index to offset and don't repeat yourself ..
	vector<DQuad*>::iterator iter;

	dbg(1, "root %s", dd.root.c_str());

	for (iter = scanlist.begin(); iter != scanlist.end(); ++iter){
		(*iter)->dump(dd);
	}
}

vector<int>& Acq132DataModel::getEvents()
{
	DQuad* dquad = scanlist[0];
	vector<int>& dqEvents(dquad->getEvents());

	if (pushEvent_called){
		dbg(2, "inserting dquad %c", dquad->getScan());
		eventOffsets.insert(eventOffsets.end(),
				dqEvents.begin(), dqEvents.end());
		pushEvent_called = false;
	}
	if (eventOffsets.empty()){
		dbg(2, "using dquad %c", dquad->getScan());
		return dqEvents;
	}else{
		return eventOffsets;
	}
}

/* pick some defaults:
 * default ts_clock = 1MHz
 * default sample clock is 32MHz, prescale 4
 */

#ifndef API
double Clock::sample_clock_ns = 1000.0/32;
#endif

int DualRate::div0;
int DualRate::div1;
const char* Channel::dump_mode = getenv("CH_MODE")? getenv("CH_MODE"):"a";
