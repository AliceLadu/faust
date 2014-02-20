/*

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


#ifndef __FaustNode__
#define __FaustNode__

#include <string>
#include <vector>

#include "faust/osc/MessageDriven.h"
#include "faust/osc/Message.h"

#include "faust/gui/GUI.h"
class GUI;

namespace oscfaust
{

/**
 * map (rescale) input values to output values
 */
template <typename C> struct mapping
{
//	const C fMinIn;	
//	const C fMaxIn;
	const C fMinOut;
	const C fMaxOut;
//	const C fScale;

//	mapping(C imin, C imax, C omin, C omax) : fMinIn(imin), fMaxIn(imax), 
//											fMinOut(omin), fMaxOut(omax), 
//											fScale( (fMaxOut-fMinOut)/(fMaxIn-fMinIn) ) {}
	mapping(C omin, C omax) : fMinOut(omin), fMaxOut(omax) {}
//	C scale (C x) { C z = (x < fMinIn) ? fMinIn : (x > fMaxIn) ? fMaxIn : x; return fMinOut + (z - fMinIn) * fScale; }
	C clip (C x) { return (x < fMinOut) ? fMinOut : (x > fMaxOut) ? fMaxOut : x; }
};

//--------------------------------------------------------------------------
/*!
	\brief a faust node is a terminal node and represents a faust parameter controler
*/
template <typename C> class FaustNode : public MessageDriven, public uiItem
{
	mapping<C>	fMapping;
	
	bool	store (C val)			{ *fZone = fMapping.clip(val); return true; }
	void	sendOSC () const;


	protected:
		FaustNode(const char *name, C* zone, C init, C min, C max, const char* prefix, GUI* ui) 
			: MessageDriven (name, prefix), uiItem (ui, zone), fMapping(min, max)
			{ *zone = init; }
			
		virtual ~FaustNode() {}

	public:
		typedef SMARTP<FaustNode<C> > SFaustNode;
		static SFaustNode create (const char* name, C* zone, C init, C min, C max, const char* prefix, GUI* ui)	
        { 
            SFaustNode node = new FaustNode(name, zone, init, min, max, prefix, ui); 
            /*
                Since FaustNode is a subclass of uiItem, the pointer will also be kept in the GUI class, and it's desallocation will be done there.
                So we don't want to have smartpointer logic desallocate it and we increment the refcount.
            */
            node->addReference(); 
            return node; 
        }

		virtual bool	accept( const Message* msg )			///< handler for the 'accept' message
		{
			if (msg->size() == 1) {			// checks for the message parameters count
											// messages with a param count other than 1 are rejected
				int ival; float fval;
				if (msg->param(0, fval)) return store (C(fval));				// accepts float values
				else if (msg->param(0, ival)) return store (C(ival));	// but accepts also int values
			}
			return MessageDriven::accept(msg);
		}

		virtual void	get (unsigned long ipdest) const;		///< handler for the 'get' message
		virtual void 	reflectZone()			{ sendOSC (); fCache = *fZone;}
};



} // end namespoace

#endif
