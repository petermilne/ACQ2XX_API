/* ------------------------------------------------------------------------- */
/* file acq_api_common.h				                     */
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

#ifndef __ACQ_API_COMMON_H__
#define __ACQ_API_COMMON_H__

#define STATUS int
#define STATUS_OK 0
#define STATUS_WORKTODO -42
#define STATUS_FEATURE_NOT_IMPLEMENTED	-43
#define STATUS_ERR(status)	((status) < 0)
#define STATUS_IS_OK(status)	((status) >= 0)


#endif /* __ACQ_API_COMMON_H__ */
