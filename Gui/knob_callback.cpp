//  Powiter
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
*Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012. 
*contact: immarespond at gmail dot com
*
*/

 

 



#include "Gui/knob_callback.h"
#include "Gui/knob.h"
#include "Gui/dockableSettings.h"
#include "Core/node.h"
Knob_Callback::Knob_Callback(SettingsPanel *panel, Node *node){
    this->panel=panel;
    this->node=node;

}

Knob_Callback::~Knob_Callback(){
    for (U32 i = 0 ; i< knobs.size(); i++) {
        delete knobs[i];
    }
    knobs.clear();
}
void Knob_Callback::initNodeKnobsVector(){
    for(U32 i=0;i<knobs.size();i++){
        Knob* pair=knobs[i];
        node->addToKnobVector(pair);
    }

}
void Knob_Callback::createKnobDynamically(){
	const std::vector<Knob*>& node_knobs=node->getKnobs();
	foreach(Knob* knob,knobs){
		bool already_exists=false;
		for(U32 i=0;i<node_knobs.size();i++){
			if(node_knobs[i]==knob){
				already_exists=true;
			}
		}
		if(!already_exists){
			node->addToKnobVector(knob);
			panel->addKnobDynamically(knob);
		}
	}
}

void Knob_Callback::removeAndDeleteKnob(Knob* knob){
    node->removeKnob(knob);
    for (U32 i = 0; i< knobs.size(); i++) {
        if (knobs[i] == knob) {
            knobs.erase(knobs.begin()+i);
            break;
        }
    }
    panel->removeAndDeleteKnob(knob);
}