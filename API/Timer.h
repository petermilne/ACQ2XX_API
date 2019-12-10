/* ------------------------------------------------------------------------- */
/* file Timer.h								     */
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

/** @file Timer.h Timer class .
*/

#ifndef TIMER_H_
#define TIMER_H_
#include <sys/time.h>

class Timer {
	struct timeval tv1;
public:
	Timer() {
		gettimeofday(&tv1, NULL);
	}
	double timeFromStart() {
		struct timeval tv2;

		gettimeofday(&tv2, NULL);
		if (tv2.tv_sec > tv1.tv_sec && tv2.tv_usec < tv1.tv_usec){
			tv2.tv_usec += 1000000;
			tv2.tv_sec -= 1;
		}
		return tv2.tv_sec - tv1.tv_sec +
				1e-6 * (tv2.tv_usec - tv1.tv_usec);
	}
};


#endif /* TIMER_H_ */
