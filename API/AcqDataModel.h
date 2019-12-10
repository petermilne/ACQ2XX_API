/* ------------------------------------------------------------------------- */
/* file AcqDataModel.h							     */
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

/** @file AcqDataModel.h generic Data Model for all ACQxxx raw data
 * Refs: 
*/

#ifndef __ACQDATAMODEL_H__
#define __ACQDATAMODEL_H__

using namespace std;
#include <string>
#include <vector>

#define DEFSCAN	"DEFSCAN"	/* operate in normal sequence */
#define DEFMASK "DEFMASK"	/* operate in normal sequence */


class AcqType;
class AcqCal;
class DumpDef;
class NewEventSignature;

class ChannelVisitor {
public:
	virtual ~ChannelVisitor() {}
	virtual void onSample(int ichan, int sample) = 0;
};

enum WCP {
	WCP_COUNT_FROM_TBLOCK_START,
	WCP_TIMED_AT_EVENT
};

class AcqDataModel {
	int maxsamples;

protected:
	static bool ignore_es;
	const AcqType& acq_type;
	string scanlist;
	string channelMask;
	AcqCal* acq_cal;
	string ident;
	string dataSourceName;
	static const char* file_write_mode;

	unsigned msecs_start;

	AcqDataModel(
		const AcqType& _acq_type, 
		string _scanlist, string _channelMask);

public:
	static bool processNoStashES(int& len) {
		if (len < 0){
			len = -len;
			return true;
		}else{
			return false;
		}
	}
	static void setProcessNoStashES(int &len){
		len = -len;
	}
	bool has_timebase;
	const char* ch_name_core;

	static string pfx;

	virtual ~AcqDataModel() {}

	void setAcqCal(AcqCal* _acq_cal);
	void setIdent(string& _ident) {
		ident = _ident;
	}
	void setDataSourceName(string _dataSourceName){
		dataSourceName = _dataSourceName;
	}
	void setPrefix(string& _pfx){
		pfx = _pfx;
	}
	virtual void addEventSignature(NewEventSignature* _es) = 0;

	const AcqType& getAcqType() { return acq_type; }
	const AcqCal* getAcqCal() { return acq_cal; }

	virtual void dumpFormat(const string& dirFile, unsigned long start_sample = 0);
	virtual void print();
	virtual void processRaw(void *data, int ndata_bytes);
	virtual void processCooked(const void *cdata, int ch, int ndata_bytes);

	void dump(string root);
	virtual void dump(DumpDef& dd) = 0;
	virtual void clear(int expected_samples);

	virtual void setMaxsamples(int _maxsamples) {
		maxsamples = _maxsamples;
	}

	virtual void visitChannels(ChannelVisitor& visitor, int stride = 1) = 0;

	virtual vector<int>& getEvents() {
		static vector<int> empty;
		return empty;
	}

	virtual void clear() {};

	static AcqDataModel* create(
		const AcqType& acq_type,
		string _scanlist  = DEFSCAN, 
		string _channel_mask = DEFMASK);

	virtual vector<short>& getChannelData(int ch) {
		static vector<short> empty;
		return empty;
	}
	virtual vector<NewEventSignature*>& getEventSignatures() {
		static vector<NewEventSignature*> empty;
		return empty;
	}



	static enum WCP wallclock_policy;
	static void setIgnoreEs(bool ignore) {
		info("setting %s", ignore? "true": "false");
		ignore_es = ignore;
	}

	virtual void setWallClockPolicy(
			unsigned _msecs_start){
		msecs_start = _msecs_start;
	}

	virtual int getActualSamples() {
		return -1;
	}
	enum FILE_WRITE_POLICY { APPEND, OVERWRITE };
	static void setFileWritePolicy(FILE_WRITE_POLICY policy);
	static enum FILE_WRITE_POLICY getFileWritePolicy();
};



#endif	// __ACQDATAMODEL_H__
