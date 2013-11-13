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

#include <stdlib.h>
#include <iostream>

#include "faust/OSCControler.h"
#include "faust/osc/FaustFactory.h"
#include "faust/OSCIO.h"

#include "OSCSetup.h"
#include "OSCFError.h"
#include "RootNode.h"

using namespace std;

namespace oscfaust
{

#define kVersion	 0.93f
#define kVersionStr	"0.93"

static const char* kUDPPortOpt	= "-port";
static const char* kUDPOutOpt	= "-outport";
static const char* kUDPErrOpt	= "-errport";
static const char* kUDPDestOpt	= "-desthost";
static const char* kXmitOpt		= "-xmit";

bool OSCControler::gXmit = false;		// a static variable to control the transmission of values
										// i.e. the use of the interface as a controler

//--------------------------------------------------------------------------
// utilities for command line arguments 
//--------------------------------------------------------------------------
static int getPortOption (int argc, char *argv[], const std::string& option, int defaultValue)
{
	for (int i=0; i < argc-1; i++) {
		if (option == argv[i]) {
			int val = strtol( argv[i+1], 0, 10);
			if (val) return val;
		}
	}
	return defaultValue;
}

static const char* getDestOption (int argc, char *argv[], const std::string& option, const char* defaultValue)
{
	for (int i=0; i < argc-1; i++) {
		if (option == argv[i])
			return argv[i+1];
	}
	return defaultValue;
}

static bool getXmitOption (int argc, char *argv[], const std::string& option, bool defaultValue)
{
	for (int i=0; i < argc-1; i++) {
		if (option == argv[i]) {
			int val = strtol( argv[i+1], 0, 10);
			return val ? true : false;
		}
	}
	return defaultValue;
}


//--------------------------------------------------------------------------
OSCControler::OSCControler (int argc, char *argv[], GUI* ui, OSCIO* io)
	: fUDPPort(kUDPBasePort), fUDPOut(kUDPBasePort+1), fUPDErr(kUDPBasePort+2), fIO(io)
{
	fUDPPort = getPortOption (argc, argv, kUDPPortOpt, fUDPPort);
	fUDPOut  = getPortOption (argc, argv, kUDPOutOpt, fUDPOut);
	fUPDErr  = getPortOption (argc, argv, kUDPErrOpt, fUPDErr);
	fDestAddress = getDestOption (argc, argv, kUDPDestOpt, "localhost");
	gXmit = getXmitOption (argc, argv, kXmitOpt, false);

	fFactory = new FaustFactory(ui, io);
	fOsc	= new OSCSetup();
}

OSCControler::~OSCControler ()
{ 
	quit(); 
	delete fFactory;
	delete fOsc;
}

//--------------------------------------------------------------------------
float OSCControler::version()				{ return kVersion; }
const char* OSCControler::versionstr()	{ return kVersionStr; }

//--------------------------------------------------------------------------
static std::string quote (const char* str)	{ 
	std::string outstr ( str );
	outstr.insert (0, 1, '\'');
	outstr += '\''; 
	return outstr;
}

//--------------------------------------------------------------------------
// start the network services
void OSCControler::run ()
{
	SRootNode rootnode = fFactory->root();		// first get the root node
	if (rootnode) {
		// informs the root node of the udp ports numbers (required to handle the 'hello' message
		rootnode->setPorts (&fUDPPort, &fUDPOut, &fUPDErr);
		// starts the network services
		fOsc->start (rootnode, fUDPPort, fUDPOut, fUPDErr, getDesAddress());

		// and outputs a message on the osc output port
		oscout << OSCStart("Faust OSC version") << versionstr() << "-"
				<< quote(rootnode->getName()).c_str() << "is running on UDP ports "
				<<  fUDPPort << fUDPOut << fUPDErr;
        
        // and also on the standard output 
        cout << "Faust OSC version " << versionstr() << " application "
             << quote(rootnode->getName()).c_str() << " is running on UDP ports "
             <<  fUDPPort << ", " << fUDPOut << ", " << fUPDErr << endl;

		if (fIO) oscout << " using OSC IO - in chans: " << fIO->numInputs() << " out chans: " << fIO->numOutputs();
		oscout << OSCEnd();
	}
}

//--------------------------------------------------------------------------
const char*	OSCControler::getRootName()		{ return fFactory->root()->getName(); }
void		OSCControler::quit ()			{ fOsc->stop(); }

}
