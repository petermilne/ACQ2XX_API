/* ------------------------------------------------------------------------- */
/* file acqread.cpp							     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2008 Peter Milne, D-TACQ Solutions Ltd
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

/** @file acqread.cpp channel upload example using API.
 *  usage: acqread [opts] ch1[..chN] [length] [start] [stride]
 *  
 *  - ch1..chN : channel selection eg 1..32
 *  - length   : #samples to upload [default: all]
 *  - start    : start sample [default: 0]
 *  - stride   : stride [interval, subsample] value [default:1]
 *  - opts:
 *   - --url URL : URL of device
 *   - --device d: device name
 *   - --board  b: board number b [integer dvice index]
 *   - --slot   s: slot number s [now same as board]
 *   - --output file : output to file, can be sprintf() format eg file.%02d
 *   - --verbose : set verbose [debug] level
 *   - --version : print version
 *
 * example:
 @verbatim

# localhost:10009 is an ssh port-forward tunnel to target:53504

# read raw data channels 1-8, 1000 samples

./x86/bin/acqread -o /tmp/data.%02d -f localhost:10009 1-8 1000
ls -l /tmp/data.0?
-rw-rw-r-- 1 pgm pgm 2000 2008-05-12 11:38 /tmp/data.01
..
-rw-rw-r-- 1 pgm pgm 2000 2008-05-12 11:38 /tmp/data.08

hexdump -e '4/2 "%04x " "\n"' </tmp/data.01 | head
ffd2 ffd3 ffd0 ffd0
ffd4 ffcd ffd0 ffd1
ffd4 ffd2 ffd7 ffd0

# read volts channels 1-8, 1000 samples

./x86/bin/acqread -o /tmp/volts.%02d -V -f localhost:10009 1-8 1000

ls -l /tmp/volts.0?
-rw-rw-r-- 1 pgm pgm 4000 2008-05-12 11:45 /tmp/volts.01
..
-rw-rw-r-- 1 pgm pgm 4000 2008-05-12 11:45 /tmp/volts.08

hexdump -e '4/4 "%f " "\n"' </tmp/volts.01 | head
0.149466 0.149778 0.148840 0.148840
0.150091 0.147902 0.148840 0.149153
0.150091 0.149466 0.151030 0.148840

@endverbatim
 */

#include "local.h"

#include <errno.h>

#include "acq2xx_api.h"
#include "acq_transport.h"

#include "popt.h"

int acq200_debug;
char *device;
poptContext opt_context;

#define MAXBUF 1024
char command[MAXBUF] = {};
char response[MAXBUF] = {};

#define DBUF 0x100000
short dbuf[DBUF];

int channel1 = 1;
int channel2 = 1;
int start = 0;
int length = 0;
int stride = 1;

enum IARG {
	I_CHANNEL,
	I_LENGTH,
	I_START,
	I_STRIDE
};

const char* outfile = "-";

static void channel_decode(const char* range)
{
	if (sscanf(range, "%d-%d", &channel1, &channel2) == 2 ||
	    sscanf(range, "%d..%d", &channel1, &channel2) == 2  ){
		return;
	}else if (sscanf(range, "%d", &channel1) == 1){
		channel2 = channel1;
		return;
	}else{
		err("failed to decode channel \"%s\"", range);
	}	    
}


class FetchesData {

public:
	const int dsize;

	FetchesData(int _dsize): dsize(_dsize) 
	{}
	virtual ~FetchesData()
	{}

	virtual int readChannel(
	        Acq2xx& card,
		int ch, void* dbuf, int nread, int start, int stride) = 0;
};

class FetchesRaw : public FetchesData {
public:
	FetchesRaw() : FetchesData(sizeof(short)) 
	{
	}
	virtual int readChannel(
	        Acq2xx& card,
		int ch, void* dbuf, int nread, int start, int stride) 
	{
		return card.readChannel(ch, (short*)dbuf, nread, start, stride);
	}
};

class FetchesVolts : public FetchesData {
public:
	FetchesVolts() : FetchesData(sizeof(float))
	{

	}
	virtual int readChannel(
	        Acq2xx& card,
		int ch, void* dbuf, int nread, int start, int stride) 
	{
		return card.readChannelVolts(
				ch, (float*)dbuf, nread, start, stride);
	}	
};
class ChannelReader {
protected:
	Acq2xx& card;
	FetchesData& fetcher;
	FILE *fpout;

	void getoutfile(int channel)
	{
		if (strcmp(outfile, "-") == 0){
			fpout = stdout;		       
		}else{
			char fname[128];
			sprintf(fname, outfile, channel);
			if (strcmp(fname, outfile) == 0 && fpout != 0){
				; return;	// use same file
			}

			if (fpout){
				fclose(fpout);
			}		

			fpout = fopen(fname, "w");
			if (fpout == 0){
				err("unable to open file \"%s\"", fname);
				exit(errno);
			}
		}
	}

public:
ChannelReader(Acq2xx& _card, FetchesData& _fetcher) :
	card(_card), fetcher(_fetcher), fpout(0)
	{
	}


	int read_channel(int ch) {
		getoutfile(ch);
	
		int nread = -1;

		for (int total = 0; total < length; total += nread){
			nread = MIN(DBUF, length-total);
			nread = fetcher.readChannel(
				card, ch, dbuf, nread, start+total, stride);

			if (nread > 0){
				fwrite(dbuf, fetcher.dsize, nread, fpout);
				continue;
			}else if (nread < 0){
				err("readChannel returns ERROR %d", nread);
				exit(errno);
			}else if (nread == 0){
				err("readChannel() reaturns 0 at %d", total);
				break;
			}
		}	

		return nread;
	}
};

int main(int argc, const char** argv)
{
	struct poptOption opt_table[] = {
		{ "url",        'u', POPT_ARG_STRING, &device,     'u' },
		{ "device",     'f', POPT_ARG_STRING, &device,     'f' },
		{ "board",      'b', POPT_ARG_STRING, &device,      'b' },
		{ "shell",      's', POPT_ARG_STRING, &device,      's' },
		{ "output",     'o', POPT_ARG_STRING, &outfile,    },
		{ "volts",      'V', POPT_ARG_NONE,   0, 'V' },
		{ "verbose",    'v', POPT_ARG_INT,    &acq200_debug, 0},
		{ "version",    0,   POPT_ARG_NONE,   0,           'V' },
		POPT_AUTOHELP
		POPT_TABLEEND		
	};
	int rc;
	FetchesData *fetcher = new FetchesRaw();

	opt_context = poptGetContext( argv[0], argc, argv, opt_table, 0 );
	while ( (rc = poptGetNextOpt( opt_context )) > 0 ){
		switch( rc ){
		case 'V':
			fetcher = new FetchesVolts();
			break;
		default:
			;
		}
	}


	for (int iarg = 0; poptPeekArg(opt_context); ++iarg){
		const char* arg = poptGetArg(opt_context);
		switch(iarg){
		case I_CHANNEL:
			channel_decode(arg);
			break;
		case I_LENGTH:
			length = strtoul(arg, 0, 0);
			break;
		case I_START:
			start = strtoul(arg, 0, 0);
			break;
		case I_STRIDE:
			stride = strtoul(arg, 0, 0);
			break;
		default:
			err("too many args");
			return -1;			
		}
	}

	if (device == 0){
		err("usage acqread "
		    "channel[..channel2] [length] [start] [stride] ");
		return -1;
	}

	Acq2xx card(Transport::getTransport(device));

	if (length == 0){
		if (card.getNumSamples(&length) != STATUS_OK){
			err("getNumSamples failed");
			exit(errno);
		}
	}

	ChannelReader reader(card, *fetcher);

	for (int ch = channel1; ch <= channel2; ++ch){
		reader.read_channel(ch);
	}
}
