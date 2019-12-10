/* ------------------------------------------------------------------------- */
/* file AcqType.cpp							     */
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

/** @file AcqType.cpp Encapsulates ACQ2xx product design details .
	 like channel mapping, number of channels, word size.

	 * pulls details from card using get.personality
	
 * Refs: see ICD for frame definition
*/
#include "local.h"

#include <iostream>
#include <string>
#include <map>
#include <stdlib.h>
#include "acq2xx_api.h"
#include "acq_transport.h"
#include "AcqType.h"

#include "string.h"

#define CMD     128
#define REPLY   512


using namespace std;

#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define TS_SHM	"/dev/shm/acq_timestamp"

class FakeTimestamp {
	int fd;
	void *mapping;

	bool make_mapping(void){
		mapping = mmap(0, sizeof(unsigned long), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
		return mapping != MAP_FAILED;
	}
	void init_value(void){
		unsigned long ts = 0;
		write(fd, &ts, sizeof(ts));
	}
	FakeTimestamp() {
		if ((fd = open(TS_SHM, O_RDWR)) >= 0){
			if (!make_mapping()){
				init_value();
				if (!make_mapping()){
					err("make_mapping failed");
					exit(errno);
				}
			}
		}else{
			fd = open(TS_SHM, O_RDWR|O_CREAT, 0666);
			if (fd < 0){
				perror(TS_SHM);
				exit(errno);
			}else{
				init_value();
				if (!make_mapping()){
					err("make_mapping failed");
					exit(errno);
				}
			}
		}
	}
	~FakeTimestamp() {
		munmap(mapping, sizeof(unsigned long));
		close(fd);
	}
public:
	static FakeTimestamp& instance() {
		static FakeTimestamp _instance;
		return _instance;
	}

	unsigned long getTimeStamp(void) {
		return *(unsigned long*)mapping;
	}
	unsigned long updateTimeStamp(long delta) {
		return *(unsigned long*)mapping = getTimeStamp() + delta;
	}
};

double NewEventSignature::last_time = -1.0;
static long fake_it_usecs = -1;

double NewEventSignature::timeInSeconds(void) const {
		double time_now = timestamp * Clock::es_clock_ns/1e9;
		if (time_now == last_time){
			err("time_now == last_time %f", time_now);
		}
		last_time = time_now;
		return time_now;
}

class Acq196EventSignature: public NewEventSignature {
	short* raw;
	int nchan;

	Acq196EventSignature(
			short _raw[], unsigned long _sample_cursor,	int _nchan) :
				NewEventSignature(_sample_cursor),
				raw(_raw), nchan(_nchan)
	{
		if (fake_it_usecs > 0){
			timestamp =
				FakeTimestamp::instance().updateTimeStamp(fake_it_usecs);
		}else{
			const u16 lsw = _raw[16];
			const u16 msw = _raw[17];

			u32 ts = (msw <<16) | lsw;
			timestamp = ts;
			dbg(1, "01: timestamp: %04x|%04x %08llx",
					msw<<16, lsw, timestamp);
		}

	}
public:

	static int is_signature(short *data, int nchan);
	static NewEventSignature* create(
			short _raw[], unsigned long _sample_cursor, int _nchan);
};


class Acq196AcqType : public AcqType {

public:
	Acq196AcqType(int _nchannels, int _word_size = 2) :
		AcqType("ACQ196", _nchannels, _word_size)		
	{
		info("model:%s nchannels:%d word_size:%d\n",
		     model.c_str(), nchan, word_size);

		DumpDef::common_timebase = true;
		Clock::es_clock_ns = Clock::sample_clock_ns;
	}
	Acq196AcqType() :
		AcqType("ACQ196", 96, sizeof(short))		
	{
	}
	virtual int getChannelOffset(int physchan) const {
		if (physchan >= 1 && physchan <= 96){
			return LUT[physchan];
		}else{
			return -1;
		}
	};

	static const int LUT[96+1];	/* index from 1 */

	virtual NewEventSignature* createES(
		short _raw[], unsigned long _sample_cursor) const
	{
		return Acq196EventSignature::create(_raw, _sample_cursor, nchan);
	}
};

const int Acq196AcqType::LUT[96+1] = {
	/* [ 0] */ -1,
	/* [ 1] */  0,
	/* [ 2] */ 16,
	/* [ 3] */  1,
	/* [ 4] */ 17,
	/* [ 5] */  8,
	/* [ 6] */ 24,
	/* [ 7] */  9,
	/* [ 8] */ 25,
	/* [ 9] */  2,
	/* [10] */ 18,
	/* [11] */  3,
	/* [12] */ 19,
	/* [13] */ 10,
	/* [14] */ 26,
	/* [15] */ 11,
	/* [16] */ 27,
	/* [17] */  4,
	/* [18] */ 20,
	/* [19] */  5,
	/* [20] */ 21,
	/* [21] */ 12,
	/* [22] */ 28,
	/* [23] */ 13,
	/* [24] */ 29,
	/* [25] */  6,
	/* [26] */ 22,
	/* [27] */  7,
	/* [28] */ 23,
	/* [29] */ 14,
	/* [30] */ 30,
	/* [31] */ 15,
	/* [32] */ 31,
	/* [33] */ 32,
	/* [34] */ 48,
	/* [35] */ 33,
	/* [36] */ 49,
	/* [37] */ 40,
	/* [38] */ 56,
	/* [39] */ 41,
	/* [40] */ 57,
	/* [41] */ 34,
	/* [42] */ 50,
	/* [43] */ 35,
	/* [44] */ 51,
	/* [45] */ 42,
	/* [46] */ 58,
	/* [47] */ 43,
	/* [48] */ 59,
	/* [49] */ 36,
	/* [50] */ 52,
	/* [51] */ 37,
	/* [52] */ 53,
	/* [53] */ 44,
	/* [54] */ 60,
	/* [55] */ 45,
	/* [56] */ 61,
	/* [57] */ 38,
	/* [58] */ 54,
	/* [59] */ 39,
	/* [60] */ 55,
	/* [61] */ 46,
	/* [62] */ 62,
	/* [63] */ 47,
	/* [64] */ 63,
	/* [65] */ 64,
	/* [66] */ 80,
	/* [67] */ 65,
	/* [68] */ 81,
	/* [69] */ 72,
	/* [70] */ 88,
	/* [71] */ 73,
	/* [72] */ 89,
	/* [73] */ 66,
	/* [74] */ 82,
	/* [75] */ 67,
	/* [76] */ 83,
	/* [77] */ 74,
	/* [78] */ 90,
	/* [79] */ 75,
	/* [80] */ 91,
	/* [81] */ 68,
	/* [82] */ 84,
	/* [83] */ 69,
	/* [84] */ 85,
	/* [85] */ 76,
	/* [86] */ 92,
	/* [87] */ 77,
	/* [88] */ 93,
	/* [89] */ 70,
	/* [90] */ 86,
	/* [91] */ 71,
	/* [92] */ 87,
	/* [93] */ 78,
	/* [94] */ 94,
	/* [95] */ 79,
	/* [96] */ 95
};


class NullAcqType : public AcqType {
public:
	NullAcqType() :
	AcqType("null", 96, sizeof(short))
	{}

	virtual int getChannelOffset(int physchan) const {
		return physchan;
	}
};


class Acq132AcqType : public AcqType {
	
public:
	Acq132AcqType(const string _model, int _nchannels, int _word_size) :
		AcqType(_model, _nchannels, _word_size)
	{
		dbg(1, "model:%s nchannels:%d word_size:%d",
		     model.c_str(), nchan, word_size);
	}
	virtual int getChannelOffset(int physchan) const {
		// @@TODO ACQ132 is different
		return physchan;
	}	
};

/*
 * acq164
[000000] [000001] [000002] [000003] [000004] [000005] [000006] [000007]
aa55f160 aa55f160 aa55f160 aa55f160 aa55f160 aa55f160 aa55f160 aa55f160 [0]
aa55f160 aa55f160 aa55f160 aa55f160 aa55f160 aa55f160 aa55f160 000347d7 [1]
000347d7 000347d7 000347d7 000347d7 000347d7 000347d7 000347d7 000347d7 [2]
000347d7 000347d7 000347d7 000347d7 000347d7 000347d7 000347d7 aa55f160 [3]
aa55f260 aa55f260 aa55f260 aa55f260 aa55f260 aa55f260 aa55f260 aa55f260 [4]
aa55f260 aa55f260 aa55f260 aa55f260 aa55f260 aa55f260 aa55f260 000346d7 [5]
000346d7 000346d7 000346d7 000346d7 000346d7 000346d7 000346d7 000346d7 [6]
000346d7 000346d7 000346d7 000346d7 000346d7 000346d7 000346d7 aa55f260 [7]
 */

class Acq164EventSignature: public NewEventSignature {
	unsigned *raw;
	int nchan;

	Acq164EventSignature(
			unsigned _raw[], unsigned long _sample_cursor, int _nchan) :
				NewEventSignature(_sample_cursor),
				raw(_raw), nchan(_nchan)
	{
		if (fake_it_usecs > 0){
			timestamp = FakeTimestamp::instance().updateTimeStamp(fake_it_usecs);
		}else{
			timestamp = _raw[16];
			has_timestamp = true;
			dbg(3, "01: timestamp: %08llx", timestamp);
		}

	}
public:
	static int is_signature(unsigned *data, int nchan);
	static NewEventSignature* create(
			unsigned _raw[], unsigned long _sample_cursor, int _nchan);
};

class Acq164AcqType : public AcqType {
public:
	Acq164AcqType(int _nchannels, int _word_size = 4) :
		AcqType("ACQ164", _nchannels, _word_size)		
	{
		info("model:%s nchannels:%d word_size:%d\n",
		     model.c_str(), nchan, word_size);
	}
	Acq164AcqType() :
		AcqType("ACQ164", 64, 4)		
	{
	}
	virtual int getChannelOffset(int physchan) const {
		if (physchan >= 1 && physchan <= 64){
			return LUT[physchan];
		}else{
			return -1;
		}
	};

	static const int LUT[64+1];	/* index from 1 */


	virtual NewEventSignature* createES(
		short _raw[], unsigned long _sample_cursor) const
	{
		unsigned* uraw = reinterpret_cast<unsigned *>(_raw);
		return Acq164EventSignature::create(uraw, _sample_cursor, nchan);
	}
};

#define ACQ164_MAGIC 		0xaa55f000
#define ACQ164_MAGIC_MASK 	0xfffff000
int Acq164EventSignature::is_signature(unsigned *data, int nchan){
	for (int ii = 1; ii <= 7; ii++){
		if ((data[ii]&ACQ164_MAGIC_MASK) != ACQ164_MAGIC){
			if (ii > 5){
				dbg(1, "No magic at %d", ii);
				FILE* pp = popen("/usr/bin/hexdump -ve '96/2 \"%04x \" \"n\"'", "w");
				fwrite(data, sizeof(unsigned), nchan, pp);
				pclose(pp);
			}
			return 0;
		}
	}

	return 1;
}

NewEventSignature* Acq164EventSignature::create(
			unsigned _raw[],
			unsigned long _sample_cursor,
			int _nchan)
{
	if (is_signature(_raw, _nchan)){
		if (fake_it_usecs == -1){
			if (getenv("ACQ_DEMUX_ES_FAKE_IT")){
				fake_it_usecs = atoi(getenv("ACQ_DEMUX_ES_FAKE_IT"));
			}
		}
		return new Acq164EventSignature(_raw, _sample_cursor, _nchan);
	}else{
		return 0;
	}
}
const int Acq164AcqType::LUT[64+1] = {
	/* 0 */ -1,
	/* 1 */ 0,
	/* 2 */ 4,
	/* 3 */ 8,
	/* 4 */ 12,
	/* 5 */ 1,
	/* 6 */ 5,
	/* 7 */ 9,
	/* 8 */ 13,
	/* 9 */ 2,
	/*10 */ 6,
	/*11 */ 10,
	/*12 */ 14,
	/*13 */ 3,
	/*14 */ 7,
	/*15 */ 11,
	/*16 */ 15,
	/*17 */ 19,
	/*18 */ 23,
	/*19 */ 27,
	/*20 */ 31,
	/*21 */ 18,
	/*22 */ 22,
	/*23 */ 26,
	/*24 */ 30,
	/*25 */ 17,
	/*26 */ 21,
	/*27 */ 25,
	/*28 */ 29,
	/*29 */ 16,
	/*30 */ 20,
	/*31 */ 24,
	/*32 */ 28,
	/*33 */ 32,
	/*34 */ 36,
	/*35 */ 40,
	/*36 */ 44,
	/*37 */ 33,
	/*38 */ 37,
	/*39 */ 41,
	/*40 */ 45,
	/*41 */ 34,
	/*42 */ 38,
	/*43 */ 42,
	/*44 */ 46,
	/*45 */ 35,
	/*46 */ 39,
	/*47 */ 43,
	/*48 */ 47,
	/*49 */ 51,
	/*50 */ 55,
	/*51 */ 59,
	/*52 */ 63,
	/*53 */ 50,
	/*54 */ 54,
	/*55 */ 58,
	/*56 */ 62,
	/*57 */ 49,
	/*58 */ 53,
	/*59 */ 57,
	/*60 */ 61,
	/*61 */ 48,
	/*62 */ 52,
	/*63 */ 56,
	/*64 */ 60,
};

class Acq196MacAcqType : public AcqType {
/* original single reference MAC */
	static int numchan(const string& refmask);

public:
	Acq196MacAcqType(int _nchan, int _word_size):
		AcqType("ACQ196", _nchan, _word_size)
	{
		info("model:%s nchannels:%d word_size:%d\n",
		     model.c_str(), nchan, word_size);
	}

	Acq196MacAcqType(const string& refmask, int _word_size) :
		AcqType("ACQ196", numchan(refmask), _word_size)		
	{
		info("model:%s nchannels:%d word_size:%d\n",
		     model.c_str(), nchan, word_size);
	}
	virtual int getChannelOffset(int physchan) const {
		if (physchan >= 1 && physchan <= 96){
			return Acq196AcqType::LUT[physchan];
		}else{
			return -1;
		}
	};

};


int  Acq196MacAcqType::numchan(const string& refmask)
{
	/* parse string of form N,N,N,N  : N={1..7} bit mask */
	int numblocks = 0;
	const char* pr = refmask.c_str();

	while(*pr){
		switch(*pr){
		case '7':
			numblocks += 3; break;
		case '1':
		case '2':
		case '4':
			numblocks += 1; break;
		case '3':
		case '5':
		case '6':
			numblocks += 2; break;
		}
		pr++;
	}
	return numblocks * 32;
}


class PentaLockinAcqType : public AcqType {

	static int getEffectiveNchan(Acq2xx &card);
public:
	PentaLockinAcqType(Acq2xx& card, int _nchan, int _word_size):
		AcqType("ACQ196", getEffectiveNchan(card), _word_size)
	{
		info("model:%s nchannels:%d word_size:%d\n",
		     model.c_str(), nchan, word_size);
	}
	virtual int getChannelOffset(int physchan) const {
		unsigned block = physchan/32;
		unsigned offset = physchan%32;

		return 32*block + Acq196AcqType::LUT[offset];
	};
};


int PentaLockinAcqType::getEffectiveNchan(Acq2xx &card)
{
	char reply[REPLY];

	dbg(1, "Get Type From Card\n");
	int rc = card.getTransport()->acq2sh(
		"get.sys /dev/LOCKIN/knobs/enable", reply, REPLY);

	if (STATUS_ERR(rc)){
		err("ERROR: failed to get type from card\n");
		return -1;
	}

	char hexstring[REPLY];
	if (strncmp(reply, "0x", 2) == 0){
		strcpy(hexstring, reply);
	}else{
		strcpy(hexstring, "0x");
		strcat(hexstring, reply);
	}
	unsigned enable_mask = strtoul(hexstring, 0, 0);
	unsigned cursor = 0x8000;
	int nblocks = 0;
	
	for (; cursor; cursor >>= 1){
		if (cursor&enable_mask){
			++nblocks;
		}
	}

	dbg(1, "enable:%04x blocks:%d\n", enable_mask, nblocks);

	return nblocks*32;
}

class Acq400AcqType : public AcqType {

public:
	Acq400AcqType(const string _model, int _nchannels, int _word_size = 2) :
		AcqType(_model, _nchannels, _word_size)
	{
		info("model:%s nchannels:%d word_size:%d\n",
		     model.c_str(), nchan, word_size);

		DumpDef::common_timebase = true;
	}
	Acq400AcqType() :
		AcqType("ACQ400", 4, sizeof(short))
	{
	}
	virtual int getChannelOffset(int physchan) const {
		if (physchan >= 1 && physchan <= nchan){
			return physchan -1;
		}else{
			return -1;
		}
	};

};


NullAcqType nullAcqTypeInstance;



typedef map<string, string> KeyValueMap;

KeyValueMap acqPrams;

static string model;

#include <strings.h>

static int parse(const char personality[])
{
	const char* l0;
	const char* l1;
	char aline[80];
	char* split;
	int nprams = 0;

	for (l0 = personality; (l1 = index(l0, '\n')) != 0; l0 = l1+1){
		strncpy(aline, l0, l1-l0);
		aline[l1-l0] = '\0';
		dbg(3, "line:\"%s\"", aline);
		if (strlen(aline) && (split = index(aline, '='))){
			*split++ = '\0';
			acqPrams[aline] = split;	
		}
	}

	map<string,string>::iterator iter;
	for (iter = acqPrams.begin(); iter != acqPrams.end(); ++iter){
		dbg(2, "key:\"%s\" value:\"%s\"\n",
			iter->first.c_str(), iter->second.c_str());
		++nprams;
	}

	dbg(2, "parse done: %d elements\n", acqPrams.size());

	return nprams;
}

const AcqType& AcqType::_getAcqType(const char personality[], Acq2xx* card)
{
	dbg(2, "here with personality \"%s\"", personality);

	if (parse(personality) < 0){
		return nullAcqTypeInstance;	
	}
	
	map<string,string>::iterator iter;
	for( iter = acqPrams.begin(); iter != acqPrams.end(); ++iter ) {
		dbg(3, "key:\"%s\" = value:\"%s\"",
		    iter->first.c_str(), iter->second.c_str());
	}

	int word_size = atoi(acqPrams["WORD_SIZE"].c_str());
	dbg(2, "word_size:%d", word_size);
	if (word_size == 0){
		word_size = sizeof(short);
	}
	int nchan = atoi(acqPrams["AICHAN"].c_str());
	dbg(2, "nchan:%d", nchan);
	if (nchan == 0){
		nchan = 96;
	}

	if (acqPrams["ACQ"] == "acq196" ){
		return *new Acq196AcqType(nchan, word_size);
	}else if (acqPrams["ACQ"] == "acq196m"){
		if (acqPrams["HAS_MAC"] == "1"){
			if (acqPrams["REF_MASK"] == ""){
				return *new Acq196MacAcqType(nchan, word_size);
			}else{
				return *new Acq196MacAcqType(
					acqPrams["REF_MASK"], word_size);
			}
		}else if (acqPrams["HAS_MAC"] == "2"){
			if (!card){
				fprintf(stderr, "needs a card\n");
				return nullAcqTypeInstance;
			}else{
				return *new PentaLockinAcqType(
						*card, nchan, word_size);
			}
		}else{
			return *new Acq196AcqType(nchan, word_size);
		}
	}else if (acqPrams["ACQ"] == "acq164"){
		return *new Acq164AcqType(nchan, word_size);
	}else if (acqPrams["ACQ"].find("acq132") != string::npos){
		return *new Acq132AcqType(acqPrams["ACQ"], nchan, word_size);
	}else if (acqPrams["ACQ"].find("acq400") != string::npos){
		return *new Acq400AcqType(acqPrams["ACQ"], nchan, word_size);
	}else if (acqPrams["ACQ"].find("acq420") != string::npos){
		return *new Acq400AcqType(acqPrams["ACQ"], nchan, word_size);
	}else{
		return nullAcqTypeInstance;
	}
}

const AcqType& AcqType::getAcqType(Acq2xx& card)
{
	char reply[REPLY];

	dbg(1, "Get Type From Card\n");
	int rc = card.getTransport()->acq2sh("get.personality", reply, REPLY);

	if (STATUS_ERR(rc)){
		err("ERROR: failed to get type from card\n");
		return nullAcqTypeInstance;
	}else{
		return _getAcqType(reply, &card);
	}
}


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>


const AcqType& AcqType::getAcqType(const char* fname)
{
	char errbuf[80];
	struct stat sb;
	int fd = open(fname, O_RDONLY);
	if (fd < 0){
		snprintf(errbuf, sizeof(errbuf),
				"%s: failed to open config file \"%s\"",
				FN, fname);
		perror(errbuf);
		return nullAcqTypeInstance;
	}
	int rc = fstat(fd, &sb);
	if (rc != 0){
		snprintf(errbuf, sizeof(errbuf),
				"%s: failed to fstat() file \"%s\"",
				FN, fname);
		perror(errbuf);
		return nullAcqTypeInstance;
	}

	char *env = (char *)calloc(sb.st_size+1, 1);
	int nread = read(fd, env, sb.st_size);
	if (nread > 0){
		env[nread] = '\0';
		const AcqType& acqType = _getAcqType(env);
		free(env);
		return acqType;
	}else{
		perror("Failed to read env");
		free(env);
		return nullAcqTypeInstance;
	}
}

File::File(string root, string ext, const char* mode) 
	: path(root + string("/") + ext)
{
	fp = fopen(path.c_str(), mode);
	if (fp == 0){
		cerr << "ERROR: failed to open file:" << path << endl;
		exit(-1);
	}
}

File::~File()
{
	fclose(fp);
}

int File::write(double* aDouble)
{
#ifdef __arm
#warning ARM MIXED-ENDIAN DOUBLE HACK
	union {
		double dd;
		unsigned us[2];
	} mix;
	unsigned tmp;
	mix.dd = *aDouble;
	SWAP(mix.us[0], mix.us[1], tmp);
	return fwrite(&mix, sizeof(mix), 1, fp);
#else
	return fwrite(aDouble, sizeof(double), 1, fp);
#endif
}

int File::write(short* aShort)
{
	return fwrite(aShort, sizeof(short), 1, fp);
}

int File::writeln(const char* str){
	int rc = fputs(str, fp);
	if (str[strlen(str)-1] != '\n'){
		rc += fputs("\n", fp);
	}
	fflush(fp);
	return rc;
}

#define ACQ196_MAGIC (short)0xaa55
int Acq196EventSignature::is_signature(short *data, int nchan){
	for (int ii = 1; ii <= 7; ii += 2){
		if (data[ii] != ACQ196_MAGIC){
			if (ii > 5){
				dbg(1, "No magic at %d", ii);
				FILE* pp = popen("/usr/bin/hexdump -ve '96/2 \"%04x \" \"n\"'", "w");
				fwrite(data, sizeof(short), nchan, pp);
				pclose(pp);
			}
			return 0;
		}
	}

	return 1;
}

NewEventSignature* Acq196EventSignature::create(
			short _raw[],
			unsigned long _sample_cursor,
			int _nchan)
{
	if (is_signature(_raw, _nchan)){
		if (fake_it_usecs == -1){
			if (getenv("ACQ_DEMUX_ES_FAKE_IT")){
				fake_it_usecs = atoi(getenv("ACQ_DEMUX_ES_FAKE_IT"));
			}
		}
		return new Acq196EventSignature(_raw, _sample_cursor, _nchan);
	}else{
		return 0;
	}
}

double Clock::sample_clock_ns;	/** < 0 :: NO TIMEBASE. */
double Clock::es_clock_ns = 1000;	/** ES clocked at 1MHz default */

bool DumpDef::common_timebase;
