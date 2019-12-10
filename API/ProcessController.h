/*
 * ProcessController.h
 *
 *  Created on: Feb 12, 2011
 *      Author: pgm
 */

#ifndef PROCESSCONTROLLER_H_
#define PROCESSCONTROLLER_H_

#include <popt.h>		/** ugly */
#include <string>
#include <map>

using namespace std;

class AcqDataModel;
class ProcessController;

class ValidatorData {
	ProcessController& pc;
	unsigned pss;
	bool adjusted;
	unsigned msecs_at_esoff;	/* optional timestamp at event */
	unsigned msecs_at_tblock_start;
	unsigned tblockN;

	ValidatorData(ProcessController& _pc);

public:
	unsigned esoff;
	unsigned esoff2;

	int tblock;
	int blocks123[3];		/* before, during, after blocks */
	int ecount[3];
	int nblocks;

	virtual ~ValidatorData() {}

	unsigned getSampleOffset(unsigned offset_in_block);

	void adjust(int samples);

	static ValidatorData *create(ProcessController&, char *input_line);
	static int evnum;
};

class ProcessControllerCreator;

class ProcessController {
public:
	AcqDataModel& dataModel;

protected:
	const char* key;
	vector<ProcessController*> peers;
	bool use_fork;
	int nice2;
	int euid;
	const char* src_fmt;
	int show_exit_level;

	ValidatorData* validatorData;

	virtual void processAction(void *pdata, int len, int tblock);

	void processRaw(const char* rawfname, int tblock = 0);

	virtual void processRawFiles(const char* tbdef);
	virtual int processTblock(const char *tbdef);
	/**< returns # events, <0 :: error */


	virtual void parentDump() {}
	/**< executed on parent side of fork() after dump. */

public:
	virtual int dump(const char* id) {
		cerr << key << " dump(" << id <<")\n";
		return 0;
	}
	virtual int dump() = 0;
	/**< returns # events, <0 :: error */

	ProcessController(AcqDataModel& _dataModel);
	virtual ~ProcessController() {}

	virtual void run(poptContext& opt_context) = 0;

	static int flen(const char *fname);

	/** factory */
	static ProcessController* create(
			const string key, AcqDataModel& dataModel);


	void addCal(const char *rawfname);

	void addPeer(ProcessController* peer);
};

/** Why the crazy stuff below?.
 * Well, ProcessControllerRegistry and  ProcessControllerCreator
 * allow new ProcessControllers to be added by static data initialization.
 * in other words, a plug-in.
 * @@todo there's probably a simpler way, but this works.
 */

class ProcessControllerRegistry
/** singleton */
{
	ProcessControllerRegistry() {

	}
public:
	static ProcessControllerRegistry& instance();
	void registerController(const string key, ProcessControllerCreator* pcc);

	map<const string, ProcessControllerCreator*> creators;
};

class ProcessControllerCreator {
public:
	virtual ProcessController* create(AcqDataModel& _dataModel) const = 0;

	virtual ~ProcessControllerCreator() {}
};

template <class T> class ControllerFactory : public ProcessControllerCreator {
public:
		ProcessController* create(AcqDataModel& _dataModel) const {
			return new T( _dataModel);
		}
};

#endif /* PROCESSCONTROLLER_H_ */
