/*
 * MdsProcessController.cpp
 *
 *  Created on: Feb 12, 2011
 *      Author: pgm
 */

#define TIMEBASE_DEBUG

#include "local.h"

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
#include <string.h>
#include <vector>

#include "AcqType.h"
#include "AcqDataModel.h"
#include "ProcessController.h"
#include "acq_demux.h"

#include "MdsProcessController.h"
#include "SHM_Buffer.h"
#include "EnvFile.h"
#include "Timer.h"
#include <mdsobjects.h>
using namespace MDSplus;


#define REUSE_TREE	1

bool no_store = false;

char *fgets_t(char *s, int size, FILE* stream, int timeout);

#define TBCHAN	0		/* "CH00" is holds timebase */

#define PS_HEADER 0xfeedc0de
#define PS_FOOTER 0xdeadc0de

struct ProcessState {
	unsigned ps_header;
	unsigned long long timestamp;
	int nfiles;
	int nsegs;
	char last_file[128];
	unsigned ps_footer;

	ProcessState(){
		memset(this, 0, sizeof(ProcessState));
		ps_header = PS_HEADER;
		ps_footer = PS_FOOTER;
	}
};

typedef vector<int> ActiveChannels;
typedef vector<int>::iterator ActiveChannelsIt;

class MdsWrapper {

protected:
	ActiveChannels active_channels;
	AcqDataModel& dataModel;



	void storeChannels(Tree *tree, const char* field_fmt,
			Data* start, Data* end, Data* dimension,
			unsigned istart, unsigned len);
	void storeTimebase(Tree *tree, const char* field_fmt,
				Data* start, Data* end, Data* dimension,
				unsigned istart, unsigned len,
				double t1, double isi);


public:
	virtual void mdsPutSegments(Tree *tree, const char *field_fmt) = 0;
	void mdsPutCal(Tree *tree, const char *field_fmt);
	void mdsPut(const char* tree, const char* field_fmt);

	MdsWrapper(AcqDataModel& _dataModel);
	virtual ~MdsWrapper() {}

	virtual void setN1(long long t1) {
		err("setN1 not supported by this wrapper");
		return;
	}
	virtual void incrementN1(int incr) {
		err("incrementN1 not supported by this wrapper");
		return;
	}
	virtual long long getN1() {
		err("getN1() not supported by this wrapper");
		return 0;
	}
	void setActiveChannel(int channel, bool on);
	void setActiveChannelRange(int c1, int c2, bool on);
	void clearActiveChannelRange(void){
		active_channels.clear();
	}

	static Tree *createTree(const char* _tree){
		try {
			// Tree signature should be const char*
			char *tree_tmp = new char[strlen(_tree)+1];
			strcpy(tree_tmp, _tree);
			char edit[32]; strcpy(edit, "EDIT");

			Tree *tree = new Tree(tree_tmp, 0, edit);
			return tree;
		} catch(MdsException *exc) {
			cerr << "ERROR createTree: mds \"" << _tree << "\": " << *exc << " ";
			perror("errno set? :");
			exit(1);
		}
	}
};

MdsWrapper::MdsWrapper(AcqDataModel& _dataModel) :
	dataModel(_dataModel)
{
	setActiveChannelRange(1, dataModel.getAcqType().getNumChannels(), true);
}


void MdsWrapper::setActiveChannel(int channel, bool on)
{
	for (ActiveChannelsIt it = active_channels.begin();
								it < active_channels.end(); it++){
		if (*it == channel){
			if (on){
				return;
			}else{
				active_channels.erase(it);
				return;
			}
		}
	}
	if (on){
		active_channels.push_back(channel);
	}
}

/* this isn't the most efficient implementation, but it's only an init thing */
void MdsWrapper::setActiveChannelRange(int c1, int c2, bool on)
{
	for (int cx = c1; cx <= c2; ++cx){
		setActiveChannel(cx, on);
	}
}


class ContinuousMdsWrapper : public MdsWrapper {
	/** continuous capture, no timestamp **/
protected:
	long long *n1;		/* _may_ be used by subclass */


public:
	ContinuousMdsWrapper(AcqDataModel& _dataModel) :
		MdsWrapper(_dataModel)
	{
		SHM_Buffer::instance().getValue(&n1);
	}
	virtual ~ContinuousMdsWrapper() {}
	virtual void setN1(long long _n1) {
		*n1 = _n1;
	}
	virtual void incrementN1(int incr) {
		*n1 += incr;
	}
	virtual long long getN1() {
		return *n1;
	}
	virtual void mdsPutSegments(Tree *tree, const char *field_fmt);
};


class SegmentMdsWrapper : public MdsWrapper {
public:
	SegmentMdsWrapper(AcqDataModel& _dataModel) :
		MdsWrapper(_dataModel)
	{}
	virtual void mdsPutSegments(Tree *tree, const char *field_fmt);
};

class TimestampedSegmentMdsWrapper : public MdsWrapper {
	void storeChannels(Tree *tree, const char *field_fmt,
			unsigned long long ts, unsigned istart, unsigned len);
	virtual void mdsPutSegments(Tree *tree, const char *field_fmt);
public:
	TimestampedSegmentMdsWrapper(AcqDataModel& _dataModel) :
		MdsWrapper(_dataModel)
	{}
};

class MdsProcessController : public ProcessController {
	void initEnv();

protected:
	virtual int dump() {

#ifdef REUSE_TREE
		errno = 0;
		try {
			mdsWrapper->mdsPutSegments(mds_tree, field);
		} catch(MdsException *ex){
			cerr << "ERROR put segments: " << *ex << " ";
			perror("errno?");
			exit(2);
		}
		errno = 0;
		try {
			mds_tree->write();
		} catch(MdsException *ex){
			cerr << "ERROR write: " << *ex << " ";
			perror("errno?");
			exit(3);
		}
#else
		mdsWrapper->mdsPut(tree, field);
#endif
		return 0;
	}

	MdsWrapper* mdsWrapper;
	Tree* mds_tree;
	const char *field;
	char *eventName;
	double eventMin;

	Timer *shot_time;
	double last_time;
	ProcessState* state;
	int fd_state;
	bool first_time;
	bool remove_when_done;

	int consecutive_child_errors;
	const int CHILD_ERROR_MAX;

	void mdsPut(const char* tree, const char* field_fmt);

	void onSegmentComplete(int segment);

	void createState(const char* tree, const char *field);

	virtual bool getDef(char *fname, int maxdef);

	void createTree(const char* tree_name, const char* _field){
		field = _field;
#ifdef REUSE_TREE
		try {
			mds_tree = mdsWrapper->createTree(tree_name);
			mdsWrapper->mdsPutCal(mds_tree, field);
		} catch(MdsException *ex){
			cerr << "ERROR creating tree:\"" << tree_name << "\" :" << *ex << " ";
			perror("errno?:");
			exit(1);
		}
#endif
	}
	void runArgsChild(const char* tree, char *fname);
	void runArgs(const char* tree, const char* field, char *fname);
public:
	MdsProcessController(AcqDataModel& _dataModel, MdsWrapper* _mdsWrapper);
	MdsProcessController(AcqDataModel& _dataModel);
	virtual ~MdsProcessController();

	virtual void processRawFiles(const char* tbdef);
	virtual void run(poptContext& opt_context);

	static void set_alarm(bool enable);
	static int rtrim;		/* trim samples from end of pulse */

	static bool isReadableFile(const char* path);
};

int MdsProcessController::rtrim;

bool MdsProcessController::isReadableFile(const char* path)
{
	struct stat sb;

	if (stat(path, &sb) == 0){
		return sb.st_size > 0;
	}
	return false;
}
bool MdsProcessController::getDef(char *fname, int maxdef) {
	if (!first_time){
		set_alarm(1);
	}
	first_time = false;
	while (true){
		if (fgets(fname, maxdef, Args::config_fp) == 0){
			return false;
		}

		set_alarm(0);
		if (fname[0] == '#'){
			continue;
		}else{
			chomp(fname);
			if (!isReadableFile(fname)){
				err("input not appropriate file \"%s\"", fname);
				exit(1);
			}
			dbg(1, "%s", fname);
			return true;
		}
	}
}
void MdsProcessController::processRawFiles(const char* tbdef)
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

void MdsProcessController::onSegmentComplete(int segment)
{
	if (eventName && shot_time->timeFromStart() - last_time > eventMin){
		char *key = getenv("CPP_EVENTS_GOOD");
		if (key != 0 && atoi(key)){
			Data* data = new Int32(segment);
			dbg(1, "call setEvent \"%s\"", eventName);
			Event::setEvent(eventName, data);
			dbg(2, "done setEvent \"%s\"", eventName);
			deleteData(data);
			dbg(2, "deleteData done");
		}else{
			FILE *pp = popen("/usr/local/mdsplus/bin/tdic", "w");
			fprintf(pp, "setevent(\"%s\");\nexit\n", eventName);
			pclose(pp);
		}
		last_time = shot_time->timeFromStart();
	}
}

void MdsProcessController::initEnv()
{
	char *key;

	if ((key = getenv("MDS_EVENT")) != 0){
		eventName = new char[128];
		int nc = sscanf(key, "%s %f", eventName, &eventMin);
		if (nc == 1){
			eventMin = 1;
		}
	}else{
		eventName = 0;
	}

	if ((key = getenv("MDS_ACTIVE_CHANNELS")) != 0){
		int c1, c2;
		if (sscanf(key, "%d,%d", &c1, &c2) == 2){
			info("setting active channel range %d,%d", c1, c2);
			mdsWrapper->clearActiveChannelRange();
			mdsWrapper->setActiveChannelRange(c1, c2, true);
		}
	}
	if ((key = getenv("MDS_RTRIM")) != 0){
		rtrim = atoi(getenv("MDS_RTRIM"));
	}
}
MdsProcessController::MdsProcessController(
		AcqDataModel& _dataModel, MdsWrapper* _mdsWrapper) :
	ProcessController(_dataModel),
	mdsWrapper(_mdsWrapper),
	first_time(true),
	remove_when_done(true),
	consecutive_child_errors(0),
	CHILD_ERROR_MAX(10)
{
	initEnv();
}

MdsProcessController::MdsProcessController(AcqDataModel& _dataModel) :
	ProcessController(_dataModel),
	first_time(true),
	remove_when_done(true),
	consecutive_child_errors(0),
	CHILD_ERROR_MAX(10)
{
	// TODO Auto-generated constructor stub
	const char* key = getenv("MDS_USE_TIMESTAMP_SEGS");
	if (key && atoi(key) == 1){
		mdsWrapper = new TimestampedSegmentMdsWrapper(dataModel);
	}else{
		mdsWrapper = new SegmentMdsWrapper(dataModel);
	}

	initEnv();
}

MdsProcessController::~MdsProcessController() {
	// TODO Auto-generated destructor stub
}


void MdsWrapper::storeChannels(Tree *tree, const char* field_fmt,
			Data* start, Data* end, Data* dimension,
			unsigned istart, unsigned len)
{
	int nchan = dataModel.getAcqType().getNumChannels();

	if (getenv("ACQ_DEMUX_CHANNEL_ONE_ONLY")){
		nchan = 1;				/* reduced data for testing */
	}

	for (ActiveChannelsIt it = active_channels.begin();
							it < active_channels.end(); it++){
		int ch = *it;
		char node_name[128];
		snprintf(node_name, 128, field_fmt, ch);
		Array *data = new Int16Array(
				&dataModel.getChannelData(ch)[istart], len);
		TreeNode *node = tree->getNode(node_name);
		node->beginSegment(start, end, dimension, data);
		deleteData(node);
		deleteData(data);
	}
}

void MdsWrapper::storeTimebase(Tree *tree, const char* field_fmt,
		Data* start, Data* end, Data* dimension,
		unsigned istart, unsigned len,
		double t1, double isi)
{
	double* tb_seg = new double[len];
	double tx = t1;
	char node_name[128];
	snprintf(node_name, 128, field_fmt, TBCHAN);

	for (unsigned ii = 0; ii != len; ++ii){
		tb_seg[ii] = tx;
		tx += isi;
	}
	Array *data = new Float64Array(tb_seg, len);
	TreeNode *node = tree->getNode(node_name);
	node->beginSegment(start, end, dimension, data);
	deleteData(node);
	deleteData(data);
	delete [] tb_seg;
}

void ContinuousMdsWrapper::mdsPutSegments(Tree *tree, const char *field_fmt)
{
	double isi_sec = Clock::sample_clock_ns/1e9;
	double t1 = (double)getN1() * isi_sec;

	unsigned int len = dataModel.getChannelData(1).size();
	double len_sec = (double)len * isi_sec;

	Data* start = new Float64(t1);
	Data* end   = new Float64(t1 + len_sec);
	Data* dimension = new Range(start, end, new Float64(isi_sec));

	dbg(1, "t1:%.3f len:%d", t1, len);

	if (!no_store){
		storeChannels(tree, field_fmt, start, end, dimension, 0, len);
		storeTimebase(tree, field_fmt, start, end, dimension, 0, len,
				t1, isi_sec);
	}else{
		info("storeSeg STUBBED");
	}

	incrementN1(len);		/* handles case where there are no id files. */
	deleteData(dimension);
}

void SegmentMdsWrapper::mdsPutSegments(Tree *tree, const char *field_fmt)
{
	vector<NewEventSignature*> eventSignatures =
			dataModel.getEventSignatures();
	vector<NewEventSignature*>::iterator it = eventSignatures.begin();
	unsigned int sample_cursor = 0;

	if (it == eventSignatures.end()){
		err("no event signatures");
		exit(1);
	}
	if ((*it)->getSampleCursor() != 0){
		err("unable to create timebase unless es at sample 0");
		exit(1);
	}
	double t1 = (*it)->timeInSeconds();
	double isi_sec = Clock::sample_clock_ns/1e9;
	unsigned int sample_max = dataModel.getChannelData(1).size();

#ifdef TIMEBASE_DEBUG
	File tb("/tmp", field_fmt);
#endif
	for (++it; it != eventSignatures.end(); ++it){
		double t2 = (*it)->timeInSeconds();
		unsigned c2 = (*it)->getSampleCursor();
		int len = c2 - sample_cursor - MdsProcessController::rtrim;
		double lensec = ((double)(len))*isi_sec;

		Data* start = new Float64(t1);
		Data* end   = new Float64(t1 + lensec);
		Data* dimension = new Range(start, end, new Float64(isi_sec));
#ifdef TIMEBASE_DEBUG
		fprintf(tb.getFp(), "%f,%f,%f,%d\n",
				t1, t1+lensec, isi_sec, len);
#endif
		dbg(1, "call storeSeg t1:%f t2:%f isi:%f", t1, t1+lensec, isi_sec);

		if (!no_store){
			storeChannels(tree, field_fmt, start, end, dimension,
					sample_cursor, len);
			storeTimebase(tree, field_fmt, start, end, dimension,
					sample_cursor, len,
					t1, isi_sec);
		}else{
			info("storeSeg STUBBED");
		}

		deleteData(dimension);
		t1 = t2;
		sample_cursor = c2;
	}

	if (sample_cursor < sample_max){
		int len = sample_max - sample_cursor - MdsProcessController::rtrim;
		double lensec = ((double)(len))*isi_sec;

		Data* start = new Float64(t1);
		Data* end   = new Float64(t1 + lensec);
		Data* dimension = new Range(start, end, new Float64(isi_sec));

		storeChannels(tree, field_fmt, start, end, dimension,
			sample_cursor, len);
		storeTimebase(tree, field_fmt, start, end, dimension,
			sample_cursor, len,
			t1, isi_sec);
	}
}


void TimestampedSegmentMdsWrapper::storeChannels(
	Tree *tree, const char *field_fmt, unsigned long long ts, unsigned istart, unsigned len)
{
	int nchan = dataModel.getAcqType().getNumChannels();

	if (getenv("ACQ_DEMUX_CHANNEL_ONE_ONLY")){
		nchan = 1;				/* reduced data for testing */
	}

	_int64 ts64 = ts;

	/* @@todo .. big assumption that ch starts at 1 and ends at N */
	for (int ch = 1; ch <= nchan; ++ch){
		char node_name[128];
		snprintf(node_name, 128, field_fmt, ch);
		Array *data = new Int16Array(
				&dataModel.getChannelData(ch)[istart], len);
		TreeNode *node = tree->getNode(node_name);
		node->putRow(data, &ts64);
		deleteData(data);
	}
}


void TimestampedSegmentMdsWrapper::mdsPutSegments(Tree *tree, const char *field_fmt)
{
	vector<NewEventSignature*> eventSignatures =
			dataModel.getEventSignatures();
	vector<NewEventSignature*>::iterator it = eventSignatures.begin();
	unsigned int sample_cursor = 0;

	if (it == eventSignatures.end()){
		err("no event signatures");
		exit(1);
	}
	if ((*it)->getSampleCursor() != 0){
		err("unable to create timebase unless es at sample 0");
		exit(1);
	}
	unsigned long long ts1 = (*it)->getTimeStamp();
	unsigned int sample_max = dataModel.getChannelData(1).size();

#ifdef TIMEBASE_DEBUG
	File tb("/tmp", field_fmt);
#endif
	for (++it; it != eventSignatures.end(); ++it){
		unsigned long long ts2 = (*it)->getTimeStamp();
		unsigned c2 = (*it)->getSampleCursor();

#ifdef TIMEBASE_DEBUG
		fprintf(tb.getFp(), "%ull %u\n", ts1, sample_cursor);
#endif
		if (!no_store){
			storeChannels(tree, field_fmt, ts1,
					sample_cursor, c2-sample_cursor);
		}else{
			info("storeSeg STUBBED");
		}

		ts1 = ts2;
		sample_cursor = c2;
	}

	if (sample_cursor < sample_max && !no_store){
		storeChannels(tree, field_fmt, ts1,
					sample_cursor, sample_max-sample_cursor);
	}
}

void MdsWrapper::mdsPutCal(Tree *tree, const char* field_fmt)
{
	int nchan = dataModel.getAcqType().getNumChannels();

	for (int ch = 1; ch <= nchan; ++ch){
		char node_name[128];
		char gain_name[128];
		char offset_name[128];

		snprintf(node_name, 128, field_fmt, ch);
		snprintf(gain_name, 128, "%s:GAIN_V", node_name);
		snprintf(offset_name, 128, "%s:OFFSET_V", node_name);

		double gain_v, offset_v;

		dataModel.getAcqCal()->getCal(ch, gain_v, offset_v);
		Float64* gv = new Float64(gain_v);
		Float64* ov = new Float64(offset_v);

		tree->getNode(gain_name)->putData(gv);
		tree->getNode(offset_name)->putData(ov);

		deleteData(gv);
		deleteData(ov);
	}
}
void MdsWrapper::mdsPut(const char* _tree, const char* field_fmt)
{
	Tree *tree = createTree(_tree);

	try {
		mdsPutCal(tree, field_fmt);
		mdsPutSegments(tree, field_fmt);
		tree->write();
		delete tree;
	} catch(MdsException *exc) {
		cerr << "ERROR mds \"" << _tree << "\": " << *exc << " ";
		perror("errno set? :");
		exit(1);
	}

}




void MdsProcessController::set_alarm(bool enable)
/* some lib code has nobbled normal alarm handling. So we have to get creative*/
{
	static FILE *wdt;

	if (wdt == 0){
		if (getenv("MDS_TIMEOUT")){
			int msecs = atoi(getenv("MDS_TIMEOUT"));

			char cmd[80];
			sprintf(cmd, "wdt %d %d", msecs, getpid());
			wdt = popen(cmd, "w");

			if (wdt == 0){
				perror("FAILED to spawn wdt");
				exit(errno);
			}
		}else{
			return;
		}

	}

	fprintf(wdt, "%d\n", enable);
}

void MdsProcessController::createState(const char* tree, const char *field)
{
	char shm_name[256];
	sprintf(shm_name, "/dev/shm/demux_mds-%s.%s", tree, field);

	fd_state = open(shm_name, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
	if (fd_state == -1){
		err("failed to create shm \"%s\"", shm_name);
		state = new ProcessState;
	}else{
		ProcessState p;
		write(fd_state, &p, sizeof(p));

		state = (ProcessState*)mmap(0, sizeof(p), PROT_READ|PROT_WRITE, MAP_SHARED,
				fd_state, 0);

		if (state == MAP_FAILED){
			perror("mmap failed");
			exit(errno);
		}
	}
}

void MdsProcessController::runArgsChild(const char* tree, char *fname)
{
	if (Args::log_fp != 0){
		dup2(fileno(Args::log_fp), 1);
		dup2(fileno(Args::log_fp), 2);
	}
	processRawFiles(fname);

	for (vector<ProcessController*>::iterator it = peers.begin();
			it < peers.end(); ++it){
		(*it)->dump(tree);
	}
	dump();

	if (remove_when_done) unlink(fname);
	// mds_tree destructor NOT called : good, parent will re-use.
	exit(0);
}
void MdsProcessController::runArgs(
		const char* tree, const char* field, char *fname)
{
	int rawblock = 0;

	for ( ; getDef(fname, 4096); ){
		pid_t worker;
		if ((worker = fork()) == 0){
			runArgsChild(tree, fname);
		}else{
			bool child_exit_error = true;
			int status;
			Timer timer;

			pid_t result = waitpid(worker, &status, 0);

			assert(result == worker);

			if (WIFEXITED(status)){
				char *last_file = rindex(fname, ',');
				if (last_file){
					last_file += 1;
				}else{
					last_file = fname;
				}
				int child_exit_code = WEXITSTATUS(status);
				printf("child pid:%d exit %d time: %.2f s file %s\n",
						worker, child_exit_code,
						timer.timeFromStart(), last_file);
				onSegmentComplete(rawblock++);
				if (child_exit_code == 0){
					child_exit_error = false;
				}
			}else{
				err("CHILD PROCESS FAILED %d", status);
			}

			if (child_exit_error){
				if (++consecutive_child_errors > CHILD_ERROR_MAX){
					err("too many child errors, dropping out");
					exit(1);
				}else{
					err("child exit error %d", consecutive_child_errors);
				}
			}else{
				consecutive_child_errors = 0;
			}
		}
	}
}
void MdsProcessController::run(poptContext& opt_context)
{
	const char* tree = poptGetArg(opt_context);
	const char* field = poptGetArg(opt_context);

	if (tree == 0 || field == 0){
		cerr << "ERROR: usage acq_demux-mds [opts] TREE FIELD [files]" <<endl;
		exit(1);
	}

	if (acq200_debug){
		cout << "acq_demux-mds TREE:" << tree << " FIELD:" << field << endl;
	}

	addCal(tree);
	shot_time = new Timer;
	last_time = shot_time->timeFromStart();

	//createState(tree, field);
	createTree(tree, field);

	if (Args::config_fp != 0){
		runArgs(tree, field, new char[4096]);
	}else{
		const char* fname;

		while((fname = poptGetArg(opt_context)) != 0){
			processRaw(fname);
		}
		mdsWrapper->mdsPut(tree, field);
	}

	dbg(1, "99");
}


class MdsContinuousProcessController : public MdsProcessController {

protected:
	const int files_per_segment;
	const int burn_after_reading;
	const int min_samples_segment;
	int ibuf;
	virtual bool getDef(char *fname, int maxdef);

public:
	MdsContinuousProcessController(AcqDataModel& _dataModel);
	virtual ~MdsContinuousProcessController();

	virtual void run(poptContext& opt_context);

	virtual void processRawFiles(const char* tbdef);
};


MdsContinuousProcessController::MdsContinuousProcessController(AcqDataModel& _dataModel) :
		MdsProcessController(_dataModel, new ContinuousMdsWrapper(_dataModel)),
		files_per_segment(getenvInt("MDS_FILES_PER_SEGMENT", 6)),
		burn_after_reading(getenvInt("MDS_BURN_AFTER_READING", 0)),
		min_samples_segment(getenvInt("MDS_MIN_SAMPLES_SEGMENT", -1)),
		ibuf(0)
{
	remove_when_done = false;
	src_fmt = "%s";
}

MdsContinuousProcessController::~MdsContinuousProcessController()
{

}

#define FLEN			0x100000	/* files (membufs) are this size */
#define PAGE_SIZE       4096

void MdsContinuousProcessController::processRawFiles(const char* tbdef)
{
	if (strstr(tbdef, ",") == 0){
		processRaw(tbdef);
	}else{
		void *pbuf;
		char* pdata;

		int rc;
		int totlen = files_per_segment*FLEN;

		rc = posix_memalign(&pbuf, PAGE_SIZE, totlen);
		if (rc == 0){
			pdata = (char*)pbuf;
			dbg(2, "buf:%p", pdata);
		}else{
			perror("posix_memalign failed");
			exit(rc);
		}

		dbg(1, "files:\"%s\"", tbdef);

		char* nbuf = new char[strlen(tbdef)];
		char *str;
		char *tp;
		char *tbid;
		int ii = 0;
		int* fd = new int[files_per_segment];
		char** map = new char*[files_per_segment];
		char** names = new char*[files_per_segment];

		strcpy(nbuf, tbdef);

		for(str = nbuf; (tbid = strtok_r(str, ",", &tp)); str = NULL, ++ii){
			assert(ii < files_per_segment);

			names[ii] = tbid;
			dbg(2, "tbid:\"%s\"", tbid);

			fd[ii] = open(names[ii], O_RDONLY);
			if (fd[ii] < 0){
				perror(names[ii]);
				exit(errno);
			}
			map[ii] = (char*)mmap(pdata+ii*FLEN, FLEN,
					PROT_READ, MAP_PRIVATE|MAP_FIXED,
					fd[ii], 0);
		}

		dataModel.setDataSourceName(names[0]);
		processAction(pdata, totlen, 0);

		for ( ; --ii >= 0; ){
			munmap(map[ii], FLEN);
			close(fd[ii]);
			if (burn_after_reading){
				dbg(2, "unlink(%s)", names[ii]);
				unlink(names[ii]);
				char id[256];
				sprintf(id, "%s.id", names[ii]);
				unlink(id);
			}
		}

		delete [] names;
		delete [] fd;
		delete [] map;
		delete [] nbuf;
	}
}
bool MdsContinuousProcessController::getDef(char *fname, int maxdef) {
	/* collect "files_per_segment" names before returning
	 * no validation is attempted ...
	 */
	if (!first_time){
		set_alarm(1);
	}
	first_time = false;
	fname[0] = '\0';
	int acc_samples = 0;
	while (true){
		int cursor = strlen(fname);
		if (fgets(fname+cursor, maxdef-cursor, Args::config_fp) == 0){
			return false;
		}

		set_alarm(0);
		if (fname[cursor] == '#'){
			fname[cursor] = '\0';
			continue;
		}else{
			chomp(fname);

			if (!isReadableFile(fname+cursor)){
				err("input is not a file \"%s\"", fname+cursor);
				exit(1);
			}
			/** id file - optional source of parametric data. */
			char* id = strstr(fname, ".id");
			if (id != 0){
				EnvFile envFile(fname+cursor);
				if (ibuf == 0){
					long long nsamples1;
					if (envFile.getValue("NSAMPLES", nsamples1)){
						dbg(1, "nsamples:%lld", nsamples1);
						mdsWrapper->setN1(nsamples1);
					}else{
						err("\"%s\" failed to get nsamples", envFile.getFname());
					}
				}
				int nsamples_file;
				if (envFile.getValue("NSAMPLES_FILE", nsamples_file)){
					acc_samples += nsamples_file;
				}

				*id = '\0';
			}
			if (++ibuf == files_per_segment ||
			   (min_samples_segment >= 0 && acc_samples >= min_samples_segment)){
				ibuf = 0;
				dbg(2, "%s", fname);
				return true;
			}else{
				strcat(fname, ",");
			}
		}
	}
}
void MdsContinuousProcessController::run(poptContext& opt_context)
{
	MdsProcessController::run(opt_context);
}

class MdsInitializesDefaults {
public:
	MdsInitializesDefaults() {
		info("B1000 build date:%s", __DATE__);
		ProcessControllerRegistry::instance().registerController(
				"acq_demux-mds",
				new ControllerFactory<MdsProcessController>());
		ProcessControllerRegistry::instance().registerController(
				"acq_demux-mds-continuous",
				new ControllerFactory<MdsContinuousProcessController>());
	}
};




MdsInitializesDefaults ID1;
