/* ------------------------------------------------------------------------- */
/* file AcqDataModel.cpp						     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2010 Peter Milne, D-TACQ Solutions Ltd
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

/** @file AcqDataModel.cpp implementation of Data Model 
 * Refs: 
*/


using namespace std;

#include "local.h"
#include <vector>
#include <string>

#include "AcqDataModel.h"
#include "AcqType.h"

#include "Acq132.h"


#include <time.h>
#include <unistd.h>

#include "WorkingDir.h"
bool AcqDataModel::ignore_es;
/** @todo: handle larger buffers if required */
#define MAXBUF	0x600000


bool timebase_on_disk;		/* hack way to get format */

template <class T>
class LinearDataModel :public AcqDataModel {
	vector<NewEventSignature*> eventSignatures;
protected:
	int *raw_lut;
	vector<vector<T>* > cooked;	/* index lchan order, starting 1 */
	virtual void dumpTimebase(DumpDef& dd);

private:	
	void build_raw_lut(void);
	void allocate_cooked(void);

	FILE* esfp;

public:
	LinearDataModel(const AcqType& _acq_type, 
			string _scanlist, string _channel_mask):
				AcqDataModel(_acq_type, _scanlist, _channel_mask)
	{
		build_raw_lut();
		allocate_cooked();
		if (getenv("ES_DEBUG")){
			esfp = fopen(getenv("ES_DEBUG"), "w");
		}
	}
	virtual ~LinearDataModel();

	virtual void print();
	virtual void processRaw(void *data, int ndata_bytes);
	virtual void processCooked(const void *cdata, int ch, int ndata_bytes);
	virtual void dump(DumpDef& dd);

	virtual void clear();

	virtual void addEventSignature(NewEventSignature* es) {
		eventSignatures.push_back(es);
	}
	/** @@todo major plumbing required! */
	virtual vector<short>& getChannelData(int ch);

	virtual vector<NewEventSignature*>& getEventSignatures() {
		return eventSignatures;
	}
	virtual int getActualSamples() {
		return cooked[1]->size();
	}

	virtual void visitChannels(ChannelVisitor& visitor, int stride) {
		const int maxchan = acq_type.nchan;
		for (int ch = 1; ch <=maxchan; ++ch){
			const vector<short>& v(getChannelData(ch));
			for (unsigned ii = 0; ii < v.size(); ii += stride){
				visitor.onSample(ch, v[ii]);
			}
		}
	}
};

template <class T>
class LinearDataModelNoEs : public LinearDataModel<T> {

public:
	LinearDataModelNoEs(const AcqType& _acq_type,
			string _scanlist, string _channel_mask):
				LinearDataModel<T>(_acq_type, _scanlist, _channel_mask)
	{
		info("<%s>", sizeof(T)==2? "short": "int32");
	}
	virtual ~LinearDataModelNoEs() {}

	virtual void processRaw(void *data, int ndata_bytes);

};



/* specialized template works for shorts. ints can come later */
template <>
vector<short>& LinearDataModel<short>::getChannelData(int ch) {
	return *cooked[ch];
}

template <class T>
vector<short>& LinearDataModel<T>::getChannelData(int ch) {
	return AcqDataModel::getChannelData(ch);
}

// @@todo: only default scanlist, channel_mask supported
class Acq132DataModelAdapter: public AcqDataModel {
	Acq132DataModel acq132;
public:
	Acq132DataModelAdapter(const AcqType& _acq_type, 
			    string _scanlist, string _channel_mask):
		AcqDataModel(_acq_type, _scanlist, _channel_mask),
// @@todo .. deliberately pick default scanlist, channel mask at this point
			acq132(_acq_type.model)
	{
	
	}	
	virtual void print() {
		acq132.print();	
	}
	virtual void processRaw(void *data, int ndata_bytes)
	{
		/* sizeof(short) forces unsigned -
		 * remember ndata_bytes "cleverly" encodes NO ES
		 * on negative values
		 */
		acq132.process((short*)data, ndata_bytes/2);
	}
	virtual void dump(DumpDef& dd) {
		Channel::dump_mode = file_write_mode;
		dbg(1, "dump \"%s\" fullSet %d",
					dd.root.c_str(), dd.specifiesFullSet());
		if (dd.specifiesFullSet()){
			acq132.dump(dd.root, dd.event_sample_start);
			if (!dd.appending){
				dumpFormat(dd.root);
			}
		}else{
			acq132.dump(dd);
		}

		dbg(1, "99");
	}
	virtual void visitChannels(ChannelVisitor& visitor, int stride) {
		// @@todo hardcode alert

		dbg(1, "01: channels HARDCODE 32");

		for (int ichan = 1; ichan !=32; ++ichan){
			const vector<short>& v(acq132.getChannel(ichan));
			for (unsigned ii = 0; ii < v.size(); ii += stride){
				visitor.onSample(ichan, v[ii]);
			}
		}
	}
	virtual vector<int>& getEvents() {
		dbg(1, "impl size %d", acq132.getEvents().size());
		return acq132.getEvents();
	}
	virtual vector<short>& getChannelData(int ch) {
		return acq132.getChannel(ch);
	}
	virtual void clear() {
		acq132.clear();
	}
	virtual void setWallClockPolicy(unsigned msecs_start){
		AcqDataModel::setWallClockPolicy(msecs_start);
		acq132.setWallClockPolicy(msecs_start, true);
	}
	virtual void setMaxsamples(int _maxsamples) {
		acq132.setMaxsamples(_maxsamples);
	}
	virtual void addEventSignature(NewEventSignature* es) {
		acq132.pushEvent(es->getSampleCursor());
	}
};



void AcqDataModel::print(void)
{
	printf("AcqDataModel::print()\n");	
}

void AcqDataModel::clear(int expected_samples)
{
	printf("AcqDataModel::clear()\n");
}
void AcqDataModel::processRaw(void *data, int ndata_bytes)
{
	printf("AcqDataModel::processRaw(%p, %d)\n", data, ndata_bytes);
}

void AcqDataModel::processCooked(const void *cdata, int ch, int ndata_bytes)
{
	fprintf(stderr, "ERROR:AcqDataModel::processCooked(%p, %d, %d)\n",
		cdata, ch, ndata_bytes);
}

string AcqDataModel::pfx;

void AcqDataModel::dumpFormat(const string& dirFile, unsigned long start_sample)
{
	dbg(1, "dirFile:%s", dirFile.c_str());
	File format(dirFile, "format", "w");
	char fmt = acq_type.getWordSize() == 2? 's': 'S';
	double gain, offset;
	int ch;

	fprintf(format.getFp(), 
		"# format for model:%s nchan:%d word_size:%d\n", 
		acq_type.model.c_str(),
		acq_type.nchan,
		acq_type.word_size);

	;

	fprintf(format.getFp(), "# ");
	for (string::iterator it = ident.begin(); it != ident.end(); ++it){
		fprintf(format.getFp(), "%c", *it);
		if (*it == '\n'){
			fprintf(format.getFp(), "# ");	
		}
	}
	fprintf(format.getFp(), "\n");
	fprintf(format.getFp(), "# DATASOURCE\t%s\n", dataSourceName.c_str());
	{
		char tbuf[80];
		struct tm tm;

		gethostname(tbuf, 80);
		fprintf(format.getFp(), "# HOSTNAME\t%s\n", tbuf);

		time_t tt = time(0);

		fprintf(format.getFp(), "# CREATED\t%s\n",
				asctime_r(localtime_r(&tt, &tm), tbuf));


	}

	char pfx_path[128];
	if (strlen(pfx.c_str())){
		strcpy(pfx_path, pfx.c_str());
		//sprintf(pfx_path, "%s.", pfx.c_str());
	}else{
		pfx_path[0] = '\0';
	}

	unsigned start_sample_by_wallclock =
		(unsigned)(msecs_start * (1000000/Clock::sample_clock_ns));

	if (wallclock_policy != WCP_TIMED_AT_EVENT){
		start_sample_by_wallclock += start_sample;
	}
	fprintf(format.getFp(), "%sSTART_SAMPLE CONST UINT32 %lu\n",
			pfx_path, start_sample_by_wallclock);

	for (ch = 1; ch <= acq_type.getNumChannels(); ++ch){
// AI01	RAW	S	1
		fprintf(format.getFp(), "%s%s%02d\tRAW\t%c\t1\n",
				pfx_path, ch_name_core, ch, fmt);
	}
	for (ch = 1; has_timebase && ch <= acq_type.getNumChannels(); ++ch){
// AI01	RAW	S	1
		fprintf(format.getFp(), "%sTB%02d\tRAW\td\t1\n",
				pfx_path, ch, fmt);

		if (DumpDef::common_timebase){
			break;
		}
	}
	for (ch = 1; ch <= acq_type.getNumChannels(); ++ch){
// AI01	RAW	S	1
		acq_cal->getCal(ch, gain, offset);
		fprintf(format.getFp(), "%sV%02d\tLINCOM\t1\t%s%s%02d\t%g\t%g\n",
				pfx_path, ch,
				pfx_path, ch_name_core, ch, gain, offset);
	}
	if (timebase_on_disk){
		fprintf(format.getFp(), "/REFERENCE TB01\n");
	}
}

template <class T>
LinearDataModel<T>::~LinearDataModel()
{
	for (int ch = 1;  ch <= acq_type.getNumChannels(); ++ch){
		if (cooked[ch]){
			delete cooked[ch];
		}
	}

	delete [] raw_lut;
}

template <class T>
void LinearDataModel<T>::build_raw_lut(void)
{
	raw_lut = new int[acq_type.getNumChannels()];

	for (int ch = 1; ch <= acq_type.getNumChannels(); ++ch){
		raw_lut[acq_type.getChannelOffset(ch)] = ch;
	}
}
template <class T>
void LinearDataModel<T>::allocate_cooked(void)
{
	cooked.push_back(0);		/* index lchan order, starting 1 */	
	for (int ch = 1; ch <= acq_type.getNumChannels(); ++ch){
		cooked.push_back(new vector<T>);
	}
}


template <class T>
void LinearDataModel<T>::clear()
{
	for (int ch = 1; ch <= acq_type.getNumChannels(); ++ch){
		cooked[ch]->clear();
	}
}

template <class T>
void LinearDataModel<T>::print(void)
{
	printf("LinearDataModel::print()\n");	
}


template <class T>
void LinearDataModel<T>::processRaw(void *data, int ndata_bytes)
{
	dbg(2, "LinearDataModel::processRaw(%p, %d)\n", data, ndata_bytes);

	T* pt = (T*)data;
	int nt = ndata_bytes/sizeof(T);
	int nchan = acq_type.getNumChannels();
	int ch_samples = nt/nchan;
	unsigned long sample_cursor = 0;

	dbg(3, "%08x %08x %08x %08x", pt[0], pt[1], pt[2], pt[3]);

	int reserve_size = cooked[1]->size()+ch_samples;

	for (int ch = 1; ch < nchan; ++ch){
		cooked[ch]->reserve(reserve_size);
	}

	for (int sample = 0; sample < ch_samples; ++sample, pt += nchan){
		NewEventSignature *new_es;
		if ((new_es = acq_type.createES((short *)pt, sample_cursor)) != 0){
			if (esfp){
				fprintf(esfp, "%15ld,%15lu,%15f\n", new_es->getSampleCursor(),
							   new_es->getTimeStamp(), new_es->timeInSeconds());
			}
			addEventSignature(new_es);
		}else{
			for (int iraw = 0; iraw < nchan; ++iraw){
				cooked[raw_lut[iraw]]->push_back(pt[iraw]);
			}
			sample_cursor++;
		}
	}
}

template <class T>
void LinearDataModelNoEs<T>::processRaw(void *data, int ndata_bytes)
{
	dbg(1, "LinearDataModelNoEs::processRaw(%p, %d)\n", data, ndata_bytes);

	T* pt = (T*)data;
	int nt = ndata_bytes/sizeof(T);
	int nchan = AcqDataModel::acq_type.getNumChannels();
	int ch_samples = nt/nchan;
	unsigned long sample_cursor = 0;

	dbg(3, "%08x %08x %08x %08x", pt[0], pt[1], pt[2], pt[3]);

	int reserve_size = LinearDataModel<T>::cooked[1]->size()+ch_samples;

	for (int ch = 1; ch < nchan; ++ch){
		LinearDataModel<T>::cooked[ch]->reserve(reserve_size);
	}

	for (int sample = 0; sample < ch_samples; ++sample, pt += nchan){
		for (int iraw = 0; iraw < nchan; ++iraw){
			LinearDataModel<T>::cooked[LinearDataModel<T>::raw_lut[iraw]]->push_back(pt[iraw]);
		}
		sample_cursor++;
	}
}

template <class T>
void LinearDataModel<T>::processCooked(
		const void *cdata, int ch, int ndata_bytes)
{
	T* pt = (T*)cdata;
	int nt = ndata_bytes/sizeof(T);
	int ch_samples = nt;


	dbg(2, "%08x %08x %08x %08x", pt[0], pt[1], pt[2], pt[3]);

	cooked[ch]->reserve(cooked[ch]->size()+ch_samples);

	for (; nt > 0; nt--){
		cooked[ch]->push_back(*pt++);
	}
}

template <class T>
void LinearDataModel<T>::dumpTimebase(DumpDef& dd)
{
	dbg(2, "01");
	{
		vector<NewEventSignature*>::iterator it = eventSignatures.begin();
		for (int ix = 0; it != eventSignatures.end(); ++it, ++ix){
			dbg(3, "ES %5d cursor:%10d time %.6f\n",
					ix, (*it)->getSampleCursor(),
					(*it)->timeInSeconds());
		}
	}
	unsigned int sample_max = cooked[1]->size();
	const double es_isi = Clock::sample_clock_ns*1e-9;
	vector<NewEventSignature*>::iterator it = eventSignatures.begin();
	unsigned int sample_cursor = 0;

	if ((*it)->getSampleCursor() != 0){
		err("unable to create timebase unless es at sample 0");
		return;
	}

	double t1 = (*it)->timeInSeconds();
	dbg(2, "timebase start %e len %d", t1, sample_max);

	char tb_name[256];
	sprintf(tb_name, "%sTB01", AcqDataModel::pfx.c_str());

	File f(dd.root, tb_name, file_write_mode);

	for (; it != eventSignatures.end(); ++it){
		t1 = (*it)->timeInSeconds();

		while (sample_cursor++ < (*it)->getSampleCursor()){
			fwrite(&t1, sizeof(double), 1, f.getFp());
			t1 += es_isi;
		}
	}

	while (sample_cursor++ < sample_max){
		fwrite(&t1, sizeof(double), 1, f.getFp());
		t1 += es_isi;
	}

	timebase_on_disk = true;
	dbg(2, "99");
}



template <class T>
void LinearDataModel<T>::dump(DumpDef& dd)
{
	/** @@todo - handle pre/post. */
	dbg(2, "LinearDataModel::dump(%s)\n", dd.root.c_str());

	for (int ch = 1; ch <= acq_type.getNumChannels(); ++ch){
		char buf[16];
		sprintf(buf, "%sCH%02d", AcqDataModel::pfx.c_str(), ch);
		File f(dd.root, buf, file_write_mode);


		vector<T> &data = *cooked[ch];

		T* pd = &data[0];

		if (ch == 1) dbg(1, "file: %s mode:%s sizeof(T):%d samples:%d\n",
				f.getPath().c_str(), file_write_mode, sizeof(T), data.size());

		dbg(2+(ch!=1), "ch:%02d size:%d %08x %08x %08x %08x",
		    ch, data.size(), pd[0], pd[1], pd[2], pd[3]);

		fwrite(&data[0], sizeof(T), data.size(), f.getFp());
		if (WorkingDir::FRIGGIT){
			T gash = 0;
			fwrite(&gash, sizeof(T), 1, f.getFp());			// force that kst update by fair means or foul !
		}
	}

	if (eventSignatures.size() > 0){
		dumpTimebase(dd);
	}
}

AcqDataModel* AcqDataModel::create(
		const AcqType& acq_type,
		string _scanlist, string _channel_mask)
{
	dbg(2, "model :%s", acq_type.model.c_str());

	if (acq_type.model.find("acq132") != string::npos){
		return new Acq132DataModelAdapter(
			acq_type, _scanlist, _channel_mask);
	}else if (ignore_es){
		switch(acq_type.getWordSize()){
		case 2:
			return new LinearDataModelNoEs<short>(
				acq_type, _scanlist, _channel_mask);
		case 4:
			return new LinearDataModelNoEs<int>(
				acq_type, _scanlist, _channel_mask);
		default:
			err("TODO: non-integer data size");
			return 0;
		}
	}else{
		switch(acq_type.getWordSize()){
		case 2:
			return new LinearDataModel<short>(
				acq_type, _scanlist, _channel_mask);
		case 4:
			return new LinearDataModel<int>(
				acq_type, _scanlist, _channel_mask);
		default:
			err("TODO: non-integer data size");
			return 0;
		}
	}
}

AcqDataModel::AcqDataModel(
		const AcqType& _acq_type, 
		string _scanlist, string _channelMask) :
		acq_type(_acq_type),
		scanlist(_scanlist),
		channelMask(_channelMask),
		has_timebase(true),
		ch_name_core("CH")
{
	acq_cal = AcqCal::create(acq_type);
}

void AcqDataModel::setAcqCal(AcqCal* _acq_cal){
	if (acq_cal){
		AcqCal::destroy(acq_cal);
	}
	acq_cal = _acq_cal;
}

void AcqDataModel::dump(string root){
	DumpDef dd(root);
	dump(dd);
}

void AcqDataModel::setFileWritePolicy(FILE_WRITE_POLICY policy)
{
	switch(policy){
	case APPEND:
		file_write_mode = "a"; break;
	case OVERWRITE:
		file_write_mode = "w"; break;
	}
}
AcqDataModel::FILE_WRITE_POLICY AcqDataModel::getFileWritePolicy()
{

	if (strcmp(file_write_mode, "a") == 0){
		return APPEND;
	}else if (strcmp(file_write_mode, "w") == 0){
		return OVERWRITE;
	}else{
		assert(0);
		return APPEND;	/* doesn't happen */
	}
}

const char* AcqDataModel::file_write_mode = "a";

enum WCP AcqDataModel::wallclock_policy;
