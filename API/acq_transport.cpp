/* ------------------------------------------------------------------------- */
/* file acq_transport.cpp			                             */
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

/** @file acq_transport.cpp instantiates Transport.
  Transport* Transport::getTransport(const char* id)
 - id=simple integer - create KddTransport to local (linux) device driver.
 - id begins http:// - create a SoapTransport
 - id is a DNS name or ip-address - create a Dt100Transport
 - id begins ssh: - create SshTransport
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "acq_transport.h"
#include "Dt100Transport.h"
#include "KddTransport.h"
#include "SoapTransport.h"


Transport* Transport::getTransport(const char* id)
/**< Transport Factory. create appropriate Transport based on id. ]
*/
{
	Transport* t;

	if ((t = KddTransportFactory::createTransport(id)) != 0 ){
		return t;
	}else if ((t = SoapTransportFactory::createTransport(id)) != 0){
		return t;
	}else{
		return t = Dt100TransportFactory::createTransport(id);
	}
}
