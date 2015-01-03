//  Natron
//
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */
#include "KnobFactory.h"

#include "Global/GlobalDefines.h"

#include "Engine/Knob.h"
#include "Engine/KnobFile.h"
#include "Engine/KnobTypes.h"

#include "Engine/AppManager.h"
#include "Engine/LibraryBinary.h"

using namespace Natron;
using std::make_pair;
using std::pair;


/*Class inheriting Knob and KnobGui, must have a function named BuildKnob and BuildKnobGui with the following signature.
   This function should in turn call a specific class-based static function with the appropriate param.*/
typedef KnobHelper* (*KnobBuilder)(KnobHolder*  holder, const std::string &description, int dimension,bool declaredByPlugin);

/***********************************FACTORY******************************************/
KnobFactory::KnobFactory()
{
    loadBultinKnobs();
}

KnobFactory::~KnobFactory()
{
    for (std::map<std::string, LibraryBinary *>::iterator it = _loadedKnobs.begin(); it != _loadedKnobs.end(); ++it) {
        delete it->second;
    }
    _loadedKnobs.clear();
}



template<typename K>
static std::pair<std::string,LibraryBinary *>
knobFactoryEntry()
{
    std::string stub;
    //boost::shared_ptr<KnobHelper> knob( K::BuildKnob(NULL, stub, 1) );
    std::map<std::string, void *> functions;

    functions.insert( make_pair("BuildKnob", (void *)&K::BuildKnob) );
    LibraryBinary *knobPlugin = new LibraryBinary(functions);

    return make_pair(K::typeNameStatic(), knobPlugin);
}

void
KnobFactory::loadBultinKnobs()
{
    _loadedKnobs.insert( knobFactoryEntry<File_Knob>() );
    _loadedKnobs.insert( knobFactoryEntry<Int_Knob>() );
    _loadedKnobs.insert( knobFactoryEntry<Double_Knob>() );
    _loadedKnobs.insert( knobFactoryEntry<Bool_Knob>() );
    _loadedKnobs.insert( knobFactoryEntry<Button_Knob>() );
    _loadedKnobs.insert( knobFactoryEntry<OutputFile_Knob>() );
    _loadedKnobs.insert( knobFactoryEntry<Choice_Knob>() );
    _loadedKnobs.insert( knobFactoryEntry<Separator_Knob>() );
    _loadedKnobs.insert( knobFactoryEntry<Group_Knob>() );
    _loadedKnobs.insert( knobFactoryEntry<Color_Knob>() );
    _loadedKnobs.insert( knobFactoryEntry<String_Knob>() );
    _loadedKnobs.insert( knobFactoryEntry<Parametric_Knob>() );
    _loadedKnobs.insert( knobFactoryEntry<Path_Knob>() );
    _loadedKnobs.insert( knobFactoryEntry<Page_Knob>() );
}

boost::shared_ptr<KnobHelper> KnobFactory::createKnob(const std::string &id,
                                                      KnobHolder*  holder,
                                                      const std::string &description,
                                                      int dimension,
                                                      bool declaredByPlugin) const
{
    std::map<std::string, LibraryBinary *>::const_iterator it = _loadedKnobs.find(id);

    if ( it == _loadedKnobs.end() ) {
        return boost::shared_ptr<KnobHelper>();
    } else {
        std::pair<bool, KnobBuilder> builderFunc = it->second->findFunction<KnobBuilder>("BuildKnob");
        if (!builderFunc.first) {
            return boost::shared_ptr<KnobHelper>();
        }
        KnobBuilder builder = (KnobBuilder)(builderFunc.second);
        boost::shared_ptr<KnobHelper> knob( builder(holder, description, dimension,declaredByPlugin) );
        if (!knob) {
            boost::shared_ptr<KnobHelper>();
        }
        boost::shared_ptr<KnobSignalSlotHandler> handler( new KnobSignalSlotHandler(knob) );
        knob->setSignalSlotHandler(handler);
        knob->populate();
        if (holder) {
            holder->addKnob(knob);
        }

        return knob;
    }
}

