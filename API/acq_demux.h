/*
 * acq_demux.h
 *
 *  Created on: Feb 12, 2011
 *      Author: pgm
 */

#ifndef ACQ_DEMUX_H_
#define ACQ_DEMUX_H_

struct Args {
	static int maxlen;
	static int startoff;
	static int pre;
	static int post;

	static FILE *config_fp;
	static FILE *log_fp;
};

#endif /* ACQ_DEMUX_H_ */
