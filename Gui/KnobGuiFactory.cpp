//  Natron
//
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 *Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 *contact: immarespond at gmail dot com
 *
 */
#include "KnobGuiFactory.h"

#include "Engine/AppManager.h"
#include "Engine/LibraryBinary.h"

#include "Engine/Knob.h"
#include "Engine/KnobTypes.h"
#include "Engine/KnobFile.h"

#include "Gui/KnobGui.h"
#include "Gui/KnobGuiFile.h"
#include "Gui/KnobGuiTypes.h"
#include "Gui/DockablePanel.h"

using namespace Natron;
using std::make_pair;
using std::pair;


/*Class inheriting KnobGui, must have a function named BuildKnobGui with the following signature.
 This function should in turn call a specific class-based static function with the appropriate param.*/
typedef KnobHelper *(*KnobBuilder)(KnobHolder  *holder, const std::string &description, int dimension);
typedef KnobGui *(*KnobGuiBuilder)(boost::shared_ptr<KnobI> knob, DockablePanel* panel);

/***********************************FACTORY******************************************/
KnobGuiFactory::KnobGuiFactory()
{
    loadKnobPlugins();
}

KnobGuiFactory::~KnobGuiFactory()
{
    for (std::map<std::string, LibraryBinary *>::iterator it = _loadedKnobs.begin(); it != _loadedKnobs.end() ; ++it) {
        delete it->second;
    }
    _loadedKnobs.clear();
}

void KnobGuiFactory::loadKnobPlugins()
{
    std::vector<LibraryBinary *> plugins = AppManager::loadPlugins(NATRON_KNOBS_PLUGINS_PATH);
    std::vector<std::string> functions;
    functions.push_back("BuildKnobGui");
    for (U32 i = 0; i < plugins.size(); ++i) {
        if (plugins[i]->loadFunctions(functions)) {
            std::pair<bool, KnobBuilder> builder = plugins[i]->findFunction<KnobBuilder>("BuildKnob");
            if (builder.first) {
                KnobHelper *knob = builder.second(NULL, "", 1);
                _loadedKnobs.insert(make_pair(knob->typeName(), plugins[i]));
                delete knob;
            }
        } else {
            delete plugins[i];
        }
    }
    loadBultinKnobs();
}

template<typename K, typename KG>
static std::pair<std::string,LibraryBinary *>
knobGuiFactoryEntry()
{
    std::string stub;
    boost::scoped_ptr<KnobHelper> knob(K::BuildKnob(NULL, stub, 1));

    std::map<std::string, void *> functions;
    functions.insert(make_pair("BuildKnobGui", (void *)&KG::BuildKnobGui));
    LibraryBinary *knobPlugin = new LibraryBinary(functions);
    return make_pair(knob->typeName(), knobPlugin);
}

void KnobGuiFactory::loadBultinKnobs()
{
    _loadedKnobs.insert(knobGuiFactoryEntry<File_Knob,File_KnobGui>());
    _loadedKnobs.insert(knobGuiFactoryEntry<Int_Knob,Int_KnobGui>());
    _loadedKnobs.insert(knobGuiFactoryEntry<Double_Knob,Double_KnobGui>());
    _loadedKnobs.insert(knobGuiFactoryEntry<Bool_Knob,Bool_KnobGui>());
    _loadedKnobs.insert(knobGuiFactoryEntry<Button_Knob,Button_KnobGui>());
    _loadedKnobs.insert(knobGuiFactoryEntry<OutputFile_Knob,OutputFile_KnobGui>());
    _loadedKnobs.insert(knobGuiFactoryEntry<Choice_Knob,Choice_KnobGui>());
    _loadedKnobs.insert(knobGuiFactoryEntry<Separator_Knob,Separator_KnobGui>());
    _loadedKnobs.insert(knobGuiFactoryEntry<Group_Knob,Group_KnobGui>());
    _loadedKnobs.insert(knobGuiFactoryEntry<Color_Knob,Color_KnobGui>());
    _loadedKnobs.insert(knobGuiFactoryEntry<String_Knob,String_KnobGui>());
    _loadedKnobs.insert(knobGuiFactoryEntry<Bool_Knob,Bool_KnobGui>());
    _loadedKnobs.insert(knobGuiFactoryEntry<Parametric_Knob, Parametric_KnobGui>());
    _loadedKnobs.insert(knobGuiFactoryEntry<Path_Knob, Path_KnobGui>());
    // _loadedKnobs.insert(knobGuiFactoryEntry<Table_Knob, Table_KnobGui>());
}


KnobGui *KnobGuiFactory::createGuiForKnob(boost::shared_ptr<KnobI> knob, DockablePanel *container) const
{
    assert(knob);
    std::map<std::string, LibraryBinary *>::const_iterator it = _loadedKnobs.find(knob->typeName());
    if (it == _loadedKnobs.end()) {
        return NULL;
    } else {
        std::pair<bool, KnobGuiBuilder> guiBuilderFunc = it->second->findFunction<KnobGuiBuilder>("BuildKnobGui");
        if (!guiBuilderFunc.first) {
            return NULL;
        }
        KnobGuiBuilder guiBuilder = (KnobGuiBuilder)(guiBuilderFunc.second);
        return guiBuilder(knob, container);
    }
}

