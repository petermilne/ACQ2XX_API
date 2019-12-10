/* ------------------------------------------------------------------------- */
/* acq200_root.h - get set acq200 device control                             */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2003 Peter Milne, D-TACQ Solutions Ltd
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

#ifndef __ACQ200_ROOT_H__
#define __ACQ200_ROOT_H__

#include <stdarg.h>

#define DEV_ROOT "/dev/dtacq"
#define DRV_ROOT "/dev/dtacq_drv"

int acq200_getRoot(
	char * ROOT, char *fname, int nval, char *fmt, ... );
int acq200_setRoot(char* ROOT, char *fname, char* fmt, ...);



#endif /* __ACQ200_ROOT_H__ */

