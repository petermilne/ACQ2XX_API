/*
 * ProcessController.cpp
 *
 *  Created on: Feb 12, 2011
 *      Author: pgm
 */


#include <assert.h>
#include <iostream>
#include <map>
#include <vector>
#include <set>


#include <stdlib.h>
#include <libgen.h>	/* dirname() */
#include <unistd.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>


#include "popt.h"

#include "ProcessController.h"
#include "acq_demux.h"


#define LM_FEED 	"/dev/acq200/data.dma/tbstat2"
#define LM_LINE_SZ	32

#define TBLOCK_LEN	0x600000	/* @@todo read the knob */
int MAXQ = 16;


#include "local.h"

#include "AcqType.h"
#include "AcqDataModel.h"

#include "CommandBuffer.h"
#include "Timer.h"
#include "WorkingDir.h"

extern "C" {
	extern int acq200_debug;
};



#define _ACQ_DEMUX_TBLOCK_ROOT	"/dev/acq200/tblocks/%s"
#define ACQ_DEMUX_TBLOCK_ROOT \
	(getenv("ACQ_DEMUX_TBLOCK_ROOT")? \
	 getenv("ACQ_DEMUX_TBLOCK_ROOT"): _ACQ_DEMUX_TBLOCK_ROOT)

ProcessController::ProcessController(AcqDataModel& _dataModel) :
	dataModel(_dataModel),
	use_fork(true), nice2(0), euid(0),
	src_fmt(ACQ_DEMUX_TBLOCK_ROOT),
	show_exit_level(1)
{
	int n2;
	if ((n2 = getenvInt("ACQ_DEMUX_NICE2", 0)) != 0){
		if (n2 < 0){
			use_fork = false;
		}else{
			use_fork = true;
			nice2 = n2;
		}
	}
	euid = getenvInt("ACQ_DEMUX_EUID", euid);
}

int ProcessController::flen(const char *fname) {
	struct stat statbuf;
	int rc = stat(fname, &statbuf);
	if (rc == 0){
		return statbuf.st_size;
	}else{
		cerr << "stat() failed on file " << fname << endl;
		perror("");
		exit(-1);
		return -1;
	}
}



void addLiveCal(AcqDataModel& dataModel)
{
#ifdef __arm
	static bool acq_cal_has_been_set;

	if (!acq_cal_has_been_set){
		system("get.vin >/tmp/local.vin");
		dataModel.setAcqCal(
				AcqCal::create(dataModel.getAcqType(), "/tmp/local.vin"));
		acq_cal_has_been_set = true;
	}
#else
#warning "feature not available in x86 (it could be..)"
#endif
}

void ProcessController::addCal(const char *rawfname)
{
	char cal_file[128];
	struct stat stat_buf;
	sprintf(cal_file, "%s.vin", rawfname);

	if (stat(cal_file, &stat_buf) == 0){
		dataModel.setAcqCal(
			AcqCal::create(dataModel.getAcqType(), cal_file));
	}
}

unsigned ValidatorData::getSampleOffset(unsigned offset_in_block)
{
	if (msecs_at_esoff == 0){
		return pss + offset_in_block;
	}else{
		pc.dataModel.setWallClockPolicy(
			AcqDataModel::wallclock_policy == WCP_TIMED_AT_EVENT?
					msecs_at_esoff: msecs_at_tblock_start);
		//@@todo .. NB DR is an issue
		return offset_in_block;
	}
}

void ValidatorData::adjust(int samples){
	if (!adjusted){
		dbg(1, "pss:%d adj:%d new:%d msecs_at_esoff:%d",
				pss, samples, pss+samples,
				msecs_at_esoff);

		if (msecs_at_esoff){
			if (AcqDataModel::wallclock_policy ==
							WCP_TIMED_AT_EVENT){
				return;
			}
			int dms = (int)(samples*Clock::sample_clock_ns/1000000);
			msecs_at_esoff += dms;
			msecs_at_tblock_start += dms;
			pc.dataModel.setWallClockPolicy(msecs_at_tblock_start);
		}else{
			pss += samples;
		}
		adjusted = true;
	}
}
ValidatorData::ValidatorData(ProcessController& _pc) :
		pc(_pc),
		pss(0),  adjusted(0),
		msecs_at_esoff(0), msecs_at_tblock_start(0),
		tblockN(0), esoff(0), esoff2(0),
		tblock(0), nblocks(0) {
	/* Java is so much better .. */
	blocks123[0] = blocks123[1] = blocks123[2] = 0;
}

ValidatorData* ValidatorData::create(ProcessController& _pc, char* input_line)
/** @factory */
{
/*
 * tblock=030,031,999 pss=17399808 esoff=0x0006c000,0x00372000 ecount=2,2,0 msec=17308 tblockN=177
 */

	ValidatorData tv(_pc);
	const int NCONV = 11;
	int nconv;

	if ((nconv = sscanf(input_line,
	"tblock=%d,%d,%d pss=%u esoff=%x,%x ecount=%d,%d,%d msec=%u tblockN=%u",
		tv.blocks123+0, tv.blocks123+1, tv.blocks123+2,
		&tv.pss, &tv.esoff, &tv.esoff2,
		tv.ecount+0, tv.ecount+1, tv.ecount+2,
		&tv.msecs_at_esoff,
		&tv.tblockN)) == NCONV){

		AcqDataModel::wallclock_policy = WCP_TIMED_AT_EVENT;

		unsigned offsam =
			tv.esoff/tv.pc.dataModel.getAcqType().getSampleSize();
		unsigned delta_ms = (unsigned)
				(offsam * Clock::sample_clock_ns /1000000);

		if (tv.msecs_at_esoff >= delta_ms){
			tv.msecs_at_tblock_start =
					tv.msecs_at_esoff -	delta_ms;
		}else{
			err("esoff*clock greater than msecs");
		}
		tv.tblock = tv.blocks123[1];
/*
		for (int ib = 0; ib <= 2; ++ib){
			tv.nblocks += tv.blocks123[ib] != 999;
		}
*/
		tv.nblocks = 3;
		if (tv.esoff2 == 0){
			tv.esoff2 = tv.esoff;
		}
		ValidatorData* vdata = new ValidatorData(_pc);
		memcpy(vdata, &tv, sizeof(tv));

		return vdata;
	}
	err("validation failed %d conversions out of %d", nconv, NCONV);
	return 0;
}



class InputValidator {
public:
	virtual bool isValidInput(char* input_line) {
		return false;
	}
	virtual ~InputValidator() {}

	static InputValidator instance;
};

InputValidator InputValidator::instance;

class LiveFeedProcessController : public ProcessController {

protected:
	int line_sz;
	const char* feed;
	int maxq;

	InputValidator *inputValidator;
public:
	LiveFeedProcessController(
			AcqDataModel& _dataModel,
			const char* _feed,
			int _line_sz = LM_LINE_SZ) :
		ProcessController(_dataModel),
		line_sz(_line_sz),
		feed(_feed)
	{
		inputValidator = &InputValidator::instance;
		maxq = MAXQ;
	}
	virtual void run(poptContext& opt_context);
};

void ProcessController::processAction(void *pdata, int len, int tblock)
{
	dbg(2, "pdata %p len %d", pdata, len);

	dataModel.processRaw(pdata, len);
	dbg(2, "99");
}



void ProcessController::processRaw(const char *rawfname, int tblock) {
	dbg(1, "rawfname:\"%s\"", rawfname);
	int fd = open(rawfname, O_RDONLY);
	int len = flen(rawfname);
	void *pdata;

	if (fd <= 0) {
		cerr << "Failed to open file:" << rawfname << endl;
		perror("");
		exit(-1);
	}

	pdata = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);

	dbg(2, "fname %s len %d pdata %p", rawfname, len, pdata);

	if (pdata == MAP_FAILED) {
		cerr << "Failed to map file:" << rawfname << " len:" << len
				<< endl;
		perror("");
		exit(-1);
	}

#ifdef __arm
	addCal(dataModel, rawfname);
#endif
	dataModel.setDataSourceName(rawfname);

	processAction(static_cast<char*>(pdata)+Args::startoff, len, tblock);

	munmap(pdata, len);
	close(fd);
}

void ProcessController::processRawFiles(const char* tbdef)
{
	if (strstr(tbdef, ",") == 0){
		processRaw(tbdef);
	}else{
		char buf[128];
		char *str;
		char *tp;
		char *tbid;


		dbg(1, "files:\"%s\"", tbdef);

		strncpy(buf, tbdef, sizeof(buf)-1);

		for(str = buf; (tbid = strtok_r(str, ",", &tp)); str = NULL){
			char tbname[80];
			sprintf(tbname, src_fmt, tbid);
			dbg(2, "tbid:\"%s\" tbname:\"%s\"", tbid, tbname);
			processRaw(tbname, strtol(tbid, 0, 10));
		}
	}
}


int ProcessController::processTblock(const char* tbid){
	dbg(1, "01");

	if (use_fork){
		/* we fork() to do the process - now we don't need to worry about
		 * any cleanup/memory leak in dataModel!
		 */
		int pid;

		if ((pid = fork()) == 0){
			if (nice2){
				nice(nice2);
			}
			if (euid){
				setegid((gid_t)(euid+1));/* D-TACQ convention */
				seteuid((uid_t)(euid));
			}

			processRawFiles(tbid);

			int rc = dump();
			dbg(1, "exit(%d)", rc);
			exit(rc);
			return rc;		/* doesn't happen */
		}else{
			dbg(1, "40 pid %d", pid);
			int status;
			Timer timer;
			wait(&status);

			if (WIFEXITED(status)){
				parentDump();
				dbg(show_exit_level,
				    "90: child status %d ret %d time:%.2f s",
				    status, WEXITSTATUS(status),
				    timer.timeFromStart());
				dbg(1, "\n\n");
				return WEXITSTATUS(status);
			}else{
				err("CHILD PROCESS FAILED %d", status);
				return 0;
			}
		}
	}else{
		processRawFiles(tbid);

		int rc = dump();
		parentDump();
		dataModel.clear();
		return rc;
	}
	dbg(1, "99");
}

static bool isComment(char *myline)
{
	return myline[0] == '\n' || myline[0] == '#';
}

bool acq_state_idle(void)
{
	FILE* fp = fopen("/dev/dtacq/state", "r");
	if (fp == 0) return false;			/* on HOST? */

	char state[16];

	if (fgets(state, 16, fp)){
		return atoi(state) == 0;
	}

	return false;
}

void LiveFeedProcessController::run(poptContext& opt_context)
{
	dbg(1, "01");

	addLiveCal(dataModel);
	CommandBuffer& cb = *CommandBuffer::create(feed, line_sz, maxq);
	char* lm_line = new char[line_sz+1];

	int tblock_count = 0;
	int max_tblocks = 0;	/* no limit */
	bool quit_on_stop = false;

	max_tblocks = getenvInt("MAX_TBLOCKS", max_tblocks);
	quit_on_stop = getenvInt("QUIT_ON_STOP", quit_on_stop);

	while(max_tblocks == 0 || tblock_count < max_tblocks){
		cb.getNext(lm_line, line_sz);

		if (isComment(lm_line)){
			continue;
		}else if (inputValidator->isValidInput(lm_line)){
			processTblock(lm_line);
			cb.writeBack(lm_line);
		}else{
			err("bad line: \"%s\"", lm_line);
		}
		cb.dump(lm_line);

		if (quit_on_stop && !cb.hasDataAvailable() && acq_state_idle()){
			printf("COMPLETE");
			break;
		}
	}

	delete [] lm_line;
}



class TblockLiveFeedInputValidator : public InputValidator {
public:
	virtual bool isValidInput(char* input_line){
		if (strchr("0123456789", input_line[0]) &&
		    strchr("0123456789", input_line[1]) &&
		    strchr("0123456789", input_line[2])){
			input_line[3] = '\0';
			return true;
		}else{
			return false;
		}
	}
};


class SubsetProcessController : public LiveFeedProcessController {

protected:
	int sample_size;

	virtual void processAction(void *pdata, int len, int tblock);
public:
	SubsetProcessController(AcqDataModel& _dataModel, const char *_feed) :
		LiveFeedProcessController(_dataModel, _feed)
	{
		sample_size = dataModel.getAcqType().getWordSize()*
				dataModel.getAcqType().getNumChannels();
	}
	virtual ~SubsetProcessController() {}
};

void SubsetProcessController::processAction(void *pdata, int len, int tblock)
/* tackle 128 sample bites of the data to the limit of maxlen */
{
	int maxlen_sam = Args::maxlen;

	int nbites = maxlen_sam>128? (maxlen_sam / 128): 1;
	int stride = len/nbites;
	int runlen = MIN(maxlen_sam, 128) * sample_size;
	int start = Args::startoff;

	dbg(1, "stride %d runlen %d, len %d", stride, runlen, len);

	for ( ; start + runlen < len; start += stride){
		dbg(1, "45: call ProcessController::processAction %d", start);
		ProcessController::processAction((char*)pdata+start, runlen, tblock);
		dbg(1, "50: done start=%d", start);
	}

	dbg(1, "99 start %d runlen %d len %d",
			start, runlen, len);
}

class LiveLogProcessController : public SubsetProcessController {
	unsigned ess;
	int samples_per_tblock;
	bool appending;
protected:
	virtual int processTblock(const char* tbid){
		dbg(1,"01: call ProcessController::processTblock then set appending");

		int rc = ProcessController::processTblock(tbid);
		appending = true;
		return rc;
	}
	virtual int dump() {

		dbg(1, "ess %u samples_per_tblock %d sample_size %d app %d",
					ess, samples_per_tblock, sample_size,
					appending);

		dbg(1, "%p 001 appending %d", this, appending);
		DumpDef dd(WorkingDir::outbase ==0? "/tmp/live_log": WorkingDir::outbase, ess);

		dd.setAppending(appending);

		const char* acq_demux_id = getenv("ACQ_DEMUX_ID");
		if (acq_demux_id){
			string pfx(acq_demux_id);
			pfx.append(".");

			dataModel.setPrefix(pfx);
		}
		dataModel.dump(dd);
		return 0;
	}

	virtual void parentDump() {
		ess += samples_per_tblock;
		dbg(1, "ess %d", ess);
	}
public:
	LiveLogProcessController(AcqDataModel& _dataModel):
		SubsetProcessController(_dataModel, LM_FEED),
		ess(0),
		appending(false)
	{
		assert(sample_size != 0);
		samples_per_tblock = TBLOCK_LEN/sample_size;
		inputValidator = new TblockLiveFeedInputValidator();
		maxq = 1;	/* best latency */
	}
};


#define MEANFILE "/dev/shm/live_means"

class MeanVisitor: public ChannelVisitor {
	AcqDataModel& dataModel;
	int fd;
	int *pmeans;
	int *nmeans;
	int len;

	void clear() {
		memset(pmeans, 0, len*sizeof(int));
		memset(nmeans, 0, len*sizeof(int));
	}
public:
	MeanVisitor(AcqDataModel& _dataModel, const char* meanfile = MEANFILE) :
		dataModel(_dataModel)
	{

		fd = open(meanfile, O_WRONLY|O_CREAT, 0777);
		if (fd <= 0){
			cerr << "Failed to open file:" << meanfile << endl;
			perror("");
			exit(-1);
		}
		len = 32+1;		// @@todo also, index from 1?
		pmeans = new int[len];
		nmeans = new int[len];
		clear();

		dbg(1, "01 file:%s fd:%d", meanfile, fd);
	}
	virtual ~MeanVisitor() {
		close(fd);
		delete [] pmeans;
		delete [] nmeans;
	}

	void dump() {
		dbg(1, "01");

		for (int ii = 0; ii < len; ++ii){
			int nm = nmeans[ii];
			if (nm){
				pmeans[ii] /= nm;
			}
		}
		write(fd, pmeans, len*sizeof(int));
		clear();
		dbg(1, "99");
	}
	virtual void onSample(int ichan, int sample)
	{
		dbg(2, "chan:%2d sample:%u", ichan, sample);
		pmeans[ichan] += sample;
		nmeans[ichan]++;
	}
};
class LiveMeanProcessController : public SubsetProcessController {

	static char* makeFname(const char* id, char* buf, int maxbuf) {
		snprintf(buf, maxbuf, "/dev/shm/%s", id);
		return buf;
	}

protected:
	virtual int dump() {
		MeanVisitor channelVisitor(dataModel);
		dataModel.visitChannels(channelVisitor);
		channelVisitor.dump();
		return 0;
	}
	virtual int dump(const char* id){
		char fname[128];
		MeanVisitor channelVisitor(dataModel, makeFname(id, fname, sizeof(fname)));
		dataModel.visitChannels(channelVisitor, 100);
		channelVisitor.dump();
		return 0;
	}
public:
	LiveMeanProcessController(AcqDataModel& _dataModel);
};

LiveMeanProcessController::LiveMeanProcessController(AcqDataModel& _dataModel) :
	SubsetProcessController(_dataModel, LM_FEED)
/* map shared memory, set up semaphore ? */
{
	dbg(1, "01");
}

#define _EV_FEED	"/dev/acq200/data.dma/tbstat_ev"

#define EV_FEED (getenv("EV_FEED")? getenv("EV_FEED"): _EV_FEED)


class LivePrePostProcessController;

class EvLiveFeedInputValidator : public InputValidator {
	LivePrePostProcessController& parent;

public:
	virtual bool isValidInput(char* input_line);
	EvLiveFeedInputValidator(LivePrePostProcessController& _parent) :
		parent(_parent)
	{}
};

/* @@todo hack!! */
#define SAMPLE_SIZE	(32 * sizeof(short))
#define TBLOCK_LEN	0x600000
#define TBLOCK_HALF	0x300000
#define TBLOCK_GT	0x400000
#define TBLOCK_LT	0x200000
#define TBLOCK_TRIM	0x100000

class LivePrePostProcessController : public LiveFeedProcessController {
	void preprocessCallout();
	void postprocessCallout();
protected:
	virtual int dump();

	File event_log;

	int previous;
	bool rc_previous;
	int adjacent_search_length;
public:
	bool tblockAlreadyKnown(int tblock){
		if (tblock == 999){
			return true;
		}else if (tblock == previous){
			return !rc_previous;
		}else{
			previous = tblock;
			return false;
		}
	}
	virtual void processAction(void *pdata, int len, int tblock);

	LivePrePostProcessController(AcqDataModel& _dataModel) :
		LiveFeedProcessController(_dataModel, EV_FEED, 256),
		event_log(WorkingDir::outbase, "tbstat_ev"),
		rc_previous(getenvInt("ACQ_DEMUX_PROCESS_PREVIOUS", true))
	{
		MAXQ = 128;
		previous = 999;
		memset(&validatorData, 0, sizeof(validatorData));
		inputValidator = new EvLiveFeedInputValidator(*this);
		show_exit_level = 0;

		if (getenvInt("ACQ_DEMUX_ADJACENT_SEARCH_HALF", 0) != 0){
			adjacent_search_length = TBLOCK_HALF;
		}else{
			adjacent_search_length = TBLOCK_LEN;
		}
	}

	virtual int processTblock(const char *tbdef);
	friend class EvLiveFeedInputValidator;

	static inline int B2S(int bytes){
		int samples;

		samples = bytes/(int)SAMPLE_SIZE;/* (signed) essential!!?? */
		dbg(1, "bytes:%d samples:%d", bytes, samples);
		return samples;
	}
};



#define ACQ132_BLOCKLEN	(32 * sizeof(short) * 256)

unsigned round_up(unsigned offset)
{
	if ((offset & (ACQ132_BLOCKLEN-1)) == 0){
		return offset;
	}else{
		offset &= ~(ACQ132_BLOCKLEN-1);
		offset += ACQ132_BLOCKLEN;
		return offset;
	}
}
unsigned round_down(unsigned offset){
	return offset &= ~(ACQ132_BLOCKLEN-1);
}

unsigned round_up_samples(int samples) {
	return round_up(samples * 32 * sizeof(short));
}


void
LivePrePostProcessController::processAction(void *pdata, int len, int tblock) {
	const char *optimise = "none";

	dbg(1, "01: tblock %03d [%03d,%03d,%03d] pdata:%p rawlen 0x%06x optimised:%s",
		tblock,
		validatorData->blocks123[0], validatorData->blocks123[1],
		validatorData->blocks123[2], pdata, len, optimise);

	if (validatorData->tblock == tblock) {
		unsigned low = round_up_samples(Args::pre) + ACQ132_BLOCKLEN;
		unsigned high =
			len - (round_up_samples(Args::post) + ACQ132_BLOCKLEN);

		if (round_down(validatorData->esoff) > low){
			unsigned del = round_down(validatorData->esoff) - low;
			pdata = (char*)pdata + del;
			validatorData->adjust(B2S(del));
			len -= del;
			optimise = "single block > pre";
			dbg(1, "45: tblock %03d pdata:%p rawlen 0x%06x optimised:%s",
						tblock, pdata, len, optimise);
		}
		if (round_up(validatorData->esoff2) < high){
			unsigned del = high - round_up(validatorData->esoff2);
			len -= del;
			optimise = "single block < post";
			dbg(1, "45: tblock %03d pdata:%p rawlen 0x%06x optimised:%s",
							tblock, pdata, len, optimise);
		}
	} else if (tblock == validatorData->blocks123[0]) {
		int pre_bytes = round_up_samples(Args::pre);
		/* optimise previous - maybe start halfway */
		pdata = (char*) pdata + (TBLOCK_LEN - pre_bytes);
		validatorData->adjust(B2S(-pre_bytes));
		len = pre_bytes;
		AcqDataModel::setProcessNoStashES(len);
		optimise = "left block";
	} else if (tblock == validatorData->blocks123[2]) {
		int post_bytes = round_up_samples(Args::post);
		/* optimise next - finish halfway */
		len = post_bytes;
		AcqDataModel::setProcessNoStashES(len);
		optimise = "right block";
	}

	dbg(0, "99: tblock %03d pdata:%p rawlen 0x%06x optimised:%s",
							tblock, pdata, len, optimise);
	ProcessController::processAction(pdata, len, tblock);
}




/* get the triplet [singlet]
 * set parent.validatorData.tblock as the middle [first] number
 * eliminate tblocks that have been already processed, pass the rest on
 */
bool EvLiveFeedInputValidator::isValidInput(char* input_line){
	parent.event_log.writeln(input_line);

	dbg(1, "input_line:\"%s\"", input_line);

	if (parent.validatorData != 0){
		delete parent.validatorData;
	}

	ValidatorData* vdata;
	if ((vdata = ValidatorData::create(parent, input_line)) != 0){
		char tblocks_to_process[80];
		tblocks_to_process[0] = '\0';
		int nblocks = 0;

		for (int iblock = 0; iblock < vdata->nblocks; ++iblock){
			if (!parent.tblockAlreadyKnown(
					vdata->blocks123[iblock])){
				char block_id[5];
				snprintf(block_id, 4, "%03d",
					vdata->blocks123[iblock]);
				if (strlen(tblocks_to_process)){
					strcat(tblocks_to_process, ",");
				}
				strcat(tblocks_to_process, block_id);
				++nblocks;
			}
		}

		if (nblocks){
			dbg(1, "99 tblocks_to_process \"%s\"", tblocks_to_process);
			strcpy(input_line, tblocks_to_process);
			parent.validatorData = vdata;
			return true;
		}
	}
	return false;
}


void spawn_task(const char *cbuf){
	int rc = system(cbuf);

	if (rc == -1){
		err("system() failed");
		exit(-1);
	}
	if (WEXITSTATUS(rc) != 0){
		err("call to %s failed, drop out", cbuf);
		exit(WEXITSTATUS(rc));
	}
}
void LivePrePostProcessController::preprocessCallout()
{
	if (char *cmd = getenv("PP_PREP_CALLOUT")){
		char cbuf[128];
		snprintf(cbuf, 128, "%s %d", cmd, validatorData->evnum);

		spawn_task(cbuf);
	}
}

void LivePrePostProcessController::postprocessCallout()
{
	if (char *cmd = getenv("PP_POST_CALLOUT")){
		char cbuf[128];
		snprintf(cbuf, 128, "%s %d", cmd, validatorData->evnum);

		spawn_task(cbuf);
	}
}

/*
 * making a group format:
ls -1 * | grep COOKED | sed -e 's!^!/INCLUDE !' -e 's/:$//'
/INCLUDE 00_0124870400.COOKED
/INCLUDE 01_0126870015.COOKED
/INCLUDE 02_0127003648.COOKED
 *
 * >format
 ls -1 * | grep COOKED | sed -e 's!^!INCLUDE !' -e 's!:$!/format!' >format
 */

int LivePrePostProcessController::processTblock(const char *tbdef)
{
	preprocessCallout();

	int rc = LiveFeedProcessController::processTblock(tbdef);

	if (rc < 0){
		err("LiveFeedProcessController::processTblock returned %d", rc);
	}else if (rc == 0){
		err("LiveFeedProcessController::processTblock NO EVENTS");
	}else{
		validatorData->evnum += rc;
		if (rc > 1){
			dbg(1, "evnum:%d multiple events in Tblock: %d",
					validatorData->evnum, rc);
		}else{
			dbg(1, "single event processed");
		}

		postprocessCallout();
	}
	return rc;
}

static void updateEvnIndicator(int evnum)
{
	char evn_buf[80];
	sprintf(evn_buf, "%d\n", evnum);
	File evn("/tmp", "acq_demux_evn", "w");
	evn.writeln(evn_buf);
}

int LivePrePostProcessController::dump() {
	int num_events = 0;
	int evnum = validatorData->evnum;

	for (vector<int>::iterator event_it = dataModel.getEvents().begin();
			event_it != dataModel.getEvents().end();
			++event_it, ++num_events){

		int event_offset = *event_it;

		++evnum;

		dbg(1, "evnum:%d dump event at %d", evnum, event_offset);

		char ev_id[80];
		const char* acq_demux_id = getenv("ACQ_DEMUX_ID");
		if (acq_demux_id){
			strncpy(ev_id, acq_demux_id, 80);
		}

		sprintf(ev_id, "%s%sEV%02d%s",
				acq_demux_id? acq_demux_id: "",
				acq_demux_id? ".": "", evnum, ".");
		string pfx(ev_id);
		dataModel.setPrefix(pfx);

		char _current_root[128];

		sprintf(_current_root, "%03d", evnum);


		WorkingDir mydir(_current_root);
		DumpDef dd(mydir.name(),
				validatorData->getSampleOffset(event_offset),
				event_offset, Args::pre, Args::post);

		dbg(1, "dirFile %s pss %lu", dd.root.c_str(), dd.event_sample_start);
		dataModel.dump(dd);
		dataModel.dumpFormat(dd.root, event_offset +
				validatorData->getSampleOffset(event_offset));
		dbg(1, "finished with event %d", evnum);

		updateEvnIndicator(evnum);
	}

	const char *pj = getenv("ACQ_DEMUX_POSTEVENT_JOB");
	if (pj){
		system(pj);
	}

	dbg(1, "return num_events %d", num_events);
	return num_events;
}


class OfflineProcessController: public ProcessController {
	const char* arg1;
public:
	OfflineProcessController(AcqDataModel& _dataModel):
		ProcessController(_dataModel),
		arg1(0)
	{}

	virtual int dump();
	virtual void run(poptContext& opt_context);
};

class DecimateProcessController: public ProcessController {
	const int decimate;
public:
	DecimateProcessController(AcqDataModel& _dataModel):
		ProcessController(_dataModel),
		decimate(10)
	{
		dbg(1, "setFileWritePolicy OVERWRITE");
		dataModel.setFileWritePolicy(AcqDataModel::OVERWRITE);
	}

	virtual int dump() { return -1; }
	virtual void run(poptContext& opt_context) {}

	virtual int dump(const char* id);
};
int OfflineProcessController::dump()
{
	printf("OfflineProcessController::dump() %s\n", arg1);
	WorkingDir mydir(arg1);
	string dirFile(mydir.name());
	if (Args::pre == 0 && Args::post == 0){
		dataModel.dump(dirFile);
	}else{
		DumpDef dd(dirFile, 0, 0, Args::pre, Args::post);
		dataModel.dump(dd);
	}
	dataModel.dumpFormat(dirFile);
	return 0;
}
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static bool readyForUpdate(const char* root)
{
	struct stat statb;

	if (stat(root, &statb) == 0){
		time_t now;
		time(&now);
		if (statb.st_mtime == now){
			return false;			/* rate limit 1Hz */
		}
	}else{
		if (mkdir(root, 0777) != 0){
			perror(root);
			return false;
		}
	}

	return true;
}
int DecimateProcessController::dump(const char* id)
{
	char root[128];
	sprintf(root, "/dev/shm/%s_wf", id);

	if (readyForUpdate(root)){
		string dirFile(root);

/*
		string newDirFile(dirFile+".new");
		mkdir(newDirFile.c_str(), 0777);
		dbg(1, "01, writing dirfile: %s", root);
		dataModel.dump(newDirFile);
		dataModel.dumpFormat(newDirFile);
		rename(newDirFile.c_str(), dirFile.c_str());
*/
		dataModel.dump(dirFile);
		dataModel.dumpFormat(dirFile);
		string marker_command("date +%y%m%d%H%M%S.%N >" + dirFile + ".fin");
		system(marker_command.c_str());
		return 1;
	}else{
		return 0;
	}
}
void OfflineProcessController::run(poptContext& opt_context)
{
	const char* arg;
	int nargs = 0;

	while((arg = poptGetArg(opt_context)) != 0){
		processRaw(arg);
		if (nargs == 0){
			if (arg1 == 0){
				arg1 = arg;
			}
		}
		++nargs;
	}
	dump();
}

class IncomingProcessController: public ProcessController {

	const char* tb_name;
	int pulse;
	int pps;		/* pulse per second */
	int second;
	int minute;
	char *current_root;

	void copyLatest(char *complete_second);

	void runPulse(char time[], char file[], char event[]);

protected:
	virtual void processRawFiles(const char* tbdef) {
		processRaw(tbdef);
	}
public:
	IncomingProcessController(AcqDataModel& _dataModel):
		ProcessController(_dataModel),
		pulse(0)
	{
		pps = getenvInt("PULSE_PER_SECOND", 5);
		src_fmt = "";
		current_root = new char[256];

		char current[256];
		sprintf(current, "%s/current", WorkingDir::outbase);
		mkdir(current, 0777);
	}
	virtual ~IncomingProcessController() {
		delete [] current_root;
	}

	virtual int dump();
	virtual void run(poptContext& opt_context);
};



void IncomingProcessController::copyLatest(char *complete_second)
{
	char cmd[256];
#if 1
	sprintf(cmd,
	"mkdir %s/current.new;cp -r %s/* %s/current.new;make.format %s/current.new",
		WorkingDir::outbase, complete_second, WorkingDir::outbase, WorkingDir::outbase);
	system(cmd);
	sprintf(cmd, "mv -T %s/current.new %s/current", WorkingDir::outbase, WorkingDir::outbase);
	system(cmd);
#endif
#if 0
	"rm -Rf %s/current/*;cp -r %s/* %s/current;make.format %s/current",
			WorkingDir::outbase, complete_second, WorkingDir::outbase, WorkingDir::outbase);
	system(cmd);
#endif
	/* this really doesn't work well. Re-write using getdata library? */
}

void IncomingProcessController::runPulse(char time[], char file[], char event[])
{
	if (pulse%pps == 0){
		copyLatest(current_root);
		++second;

		if ((pulse/pps)%60 == 0){
			++minute;
			second = 0;
			sprintf(current_root, "%s/%03d",
					WorkingDir::outbase, minute);
			if (mkdir(current_root, 0777)){
				err("failed to create \"%s\"", current_root);
				exit(errno);
			}
		}

		sprintf(current_root, "%s/%03d/%02d",
				WorkingDir::outbase, minute, second);
		if (mkdir(current_root, 0777)){
			err("failed to create \"%s\"", current_root);
			exit(errno);
		}
	}
	char _pulse_pfx[80];
	sprintf(_pulse_pfx, "%02d_", (pulse%pps)+1);
	string pulse_pfx(_pulse_pfx);
	dataModel.setPrefix(pulse_pfx);

	++pulse;

	dbg(1, "file:\"%s\"", file);
	processTblock(tb_name = file);
	dataModel.clear();
}
void IncomingProcessController::run(poptContext& opt_context)
{
	const char* arg = poptGetArg(opt_context);
	char waits_tblocks[80];
	char latest_tblock[80] = {};
	char time[80];
	char file[80];
	char event[80];

	if (arg == 0){
		arg = ".";
	}

	sprintf(waits_tblocks, "dir-watch %s", arg);

	FILE *fstat = popen(waits_tblocks, "r");

	if (fstat == 0){
		err("failed to spawn \"%s\"", waits_tblocks);
		exit(errno);
	}
	while(fgets(latest_tblock, 80, fstat)){
		dbg(2, "tblock: \"%s\"", latest_tblock);
		if (sscanf(latest_tblock, "%s %s %s", time, file, event) == 3){
			runPulse(time, file, event);
		}
	}
	pclose(fstat);
}

int IncomingProcessController::dump()
{
	dbg(1, "");
	char wd[128];

	sprintf(wd, "%s/%03d", current_root, pulse);


	WorkingDir mydir(wd, ABSDIR);
	DumpDef dd(mydir.name(), 0, 0, 0, 0);
/*
		   event_offset + validatorData.pss,
		   event_offset, pre, post);
*/
	dbg(1, "dirFile %s pss %lu", dd.root.c_str(), dd.event_sample_start);
	dataModel.dump(dd);
	dataModel.dumpFormat(dd.root, 0);
	return 0;
}


ProcessControllerRegistry& ProcessControllerRegistry::instance()
{
	static ProcessControllerRegistry pcr;

	return pcr;
}
void ProcessControllerRegistry::registerController(const string key, ProcessControllerCreator* pcc)
{
	creators[key] = pcc;
}

ProcessController* ProcessController::create(
		const string key,  AcqDataModel& dataModel)
{
	ProcessControllerCreator* pcc = ProcessControllerRegistry::instance().creators[key];

	if (pcc){
		return pcc->create(dataModel);
	}

	map<const string, ProcessControllerCreator*>::const_iterator it;
	for (it = ProcessControllerRegistry::instance().creators.begin();
	     it != ProcessControllerRegistry::instance().creators.end(); ++it){
		cerr << it->first << endl;
		if (it->first == key){
			pcc = it->second;
			return pcc->create(dataModel);
		}
	}
	exit(1);
#if 0
	if (pcc){
		pcc->key = key;
		return pcc;
	}

	}else if (key == "make.acq200.format"){
		/*
		addLiveCal(dataModel);
		dataModel.has_timebase = false;
		dataModel.ch_name_core = "";
		dataModel.dumpFormat(WorkingDir::outbase?
				WorkingDir::outbase: "/dev/acq200/data");
		*/
		return 0;
	}else if (key == "acq_demux-trial"){
		/*
		dataModel.print();
		*/
		return 0;
	}
#endif
}

void ProcessController::addPeer(ProcessController* peer)
{
	peers.push_back(peer);
}

class InitializesDefaults {

public:
	InitializesDefaults() {
		ProcessControllerRegistry::instance().registerController(
				"acq_demux",
				new ControllerFactory<OfflineProcessController>());
		ProcessControllerRegistry::instance().registerController(
				"acq_demux-lm",
				new ControllerFactory<LiveMeanProcessController>());
		ProcessControllerRegistry::instance().registerController(
				"acq_demux-ll",
				new ControllerFactory<LiveLogProcessController>());
		ProcessControllerRegistry::instance().registerController(
				"acq_demux-lpp",
				new ControllerFactory<LivePrePostProcessController>());
		ProcessControllerRegistry::instance().registerController(
				"acq_demux_incoming",
				new ControllerFactory<IncomingProcessController>());
		ProcessControllerRegistry::instance().registerController(
						"acq_demux_decim",
						new ControllerFactory<DecimateProcessController>());
	}
};

int ValidatorData::evnum;

static InitializesDefaults ID;


