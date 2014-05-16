/*

  Faust Project

  Copyright (C) 2011 Grame

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

  Grame Research Laboratory, 9 rue du Garet, 69001 Lyon - France
  research@grame.fr

*/

#include <iostream>

#include "faust/osc/Message.h"

#include "OSCListener.h"

#include "osc/OscReceivedElements.h"
#include "ip/IpEndpointName.h"

using namespace std;
using namespace osc;

namespace oscfaust
{

//--------------------------------------------------------------------------
OSCListener::OSCListener(MessageProcessor *mp, int port) 
		: fSocket(0), fMsgHandler(mp), 
		  fRunning(false), fPort(port) 
{
	fSocket = new UdpListeningReceiveSocket(IpEndpointName( IpEndpointName::ANY_ADDRESS, fPort ), this);
	fPort = 0;
}

OSCListener::~OSCListener()	{ stop(); delete fSocket; }

//--------------------------------------------------------------------------
void OSCListener::run()
{ 
	fRunning = true;
	while (fRunning) {
			if (fPort) {
				delete fSocket;
                fSocket = NULL;
            	fSocket = new UdpListeningReceiveSocket(IpEndpointName( IpEndpointName::ANY_ADDRESS, fPort ), this);
				fPort = 0;
			}
        
            if(fSocket != NULL)
                fSocket->Run();

	}
}

//--------------------------------------------------------------------------
void OSCListener::ProcessMessage( const osc::ReceivedMessage& m, const IpEndpointName& src )
{
	Message* msg = new Message(m.AddressPattern());
	msg->setSrcIP (src.address);
	ReceivedMessageArgumentIterator i = m.ArgumentsBegin();
	while (i != m.ArgumentsEnd()) {
		if (i->IsString()) {
			msg->add<string>(i->AsStringUnchecked());			
		}
		else if (i->IsInt32()) {
			msg->add<int>(i->AsInt32Unchecked());			
		}
		else if (i->IsFloat()) {
			msg->add<float>(i->AsFloatUnchecked());			
		}
		i++;
	}
	fMsgHandler->processMessage (msg);
	delete msg;
}

} // end namespoace
