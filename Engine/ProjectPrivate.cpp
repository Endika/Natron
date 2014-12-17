//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

#include "ProjectPrivate.h"

#include <QDebug>
#include <QTimer>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include "Engine/AppManager.h"
#include "Engine/AppInstance.h"
#include "Engine/NodeSerialization.h"
#include "Engine/TimeLine.h"
#include "Engine/EffectInstance.h"
#include "Engine/Project.h"
#include "Engine/Node.h"
#include "Engine/ProjectSerialization.h"
#include "Engine/OfxEffectInstance.h"
#include "Engine/AppManager.h"
#include "Engine/ViewerInstance.h"
#include "Engine/Settings.h"
namespace Natron {
ProjectPrivate::ProjectPrivate(Natron::Project* project)
    : _publicInterface(project)
      , projectLock()
      , projectName("Untitled." NATRON_PROJECT_FILE_EXT)
      , hasProjectBeenSavedByUser(false)
      , ageSinceLastSave( QDateTime::currentDateTime() )
      , lastAutoSave()
      , projectCreationTime(ageSinceLastSave)
      , builtinFormats()
      , additionalFormats()
      , formatMutex()
      , envVars()
      , formatKnob()
      , addFormatKnob()
      , viewsCount()
      , previewMode()
      , colorSpace8bits()
      , colorSpace16bits()
      , colorSpace32bits()
      , natronVersion()
      , originalAuthorName()
      , lastAuthorName()
      , projectCreationDate()
      , saveDate()
      , timeline( new TimeLine(project) )
      , autoSetProjectFormat(appPTR->getCurrentSettings()->isAutoProjectFormatEnabled())
      , currentNodes()
      , project(project)
      , lastTimelineSeekCaller()
      , isLoadingProjectMutex()
      , isLoadingProject(false)
      , isLoadingProjectInternal(false)
      , isSavingProjectMutex()
      , isSavingProject(false)
      , autoSaveTimer( new QTimer() )

{
    autoSaveTimer->setSingleShot(true);
}

bool
ProjectPrivate::restoreFromSerialization(const ProjectSerialization & obj,
                                         const QString& name,
                                         const QString& path,
                                         bool isAutoSave,
                                         const QString& realFilePath)
{
    
    bool mustShowErrorsLog = false;
    
    /*1st OFF RESTORE THE PROJECT KNOBS*/

    projectCreationTime = QDateTime::fromMSecsSinceEpoch( obj.getCreationDate() );


    /*we must restore the entries in the combobox before restoring the value*/
    std::vector<std::string> entries;

    for (std::list<Format>::const_iterator it = builtinFormats.begin(); it != builtinFormats.end(); ++it) {
        QString formatStr = Natron::generateStringFromFormat(*it);
        entries.push_back( formatStr.toStdString() );
    }

    const std::list<Format> & objAdditionalFormats = obj.getAdditionalFormats();
    for (std::list<Format>::const_iterator it = objAdditionalFormats.begin(); it != objAdditionalFormats.end(); ++it) {
        QString formatStr = Natron::generateStringFromFormat(*it);
        entries.push_back( formatStr.toStdString() );
    }
    additionalFormats = objAdditionalFormats;

    formatKnob->populateChoices(entries);
    autoSetProjectFormat = false;

    const std::list< boost::shared_ptr<KnobSerialization> > & projectSerializedValues = obj.getProjectKnobsValues();
    const std::vector< boost::shared_ptr<KnobI> > & projectKnobs = project->getKnobs();


    /// 1) restore project's knobs.
    for (U32 i = 0; i < projectKnobs.size(); ++i) {
        ///try to find a serialized value for this knob
        for (std::list< boost::shared_ptr<KnobSerialization> >::const_iterator it = projectSerializedValues.begin(); it != projectSerializedValues.end(); ++it) {
            
            if ( (*it)->getName() == projectKnobs[i]->getName() ) {
                
                ///EDIT: Allow non persistent params to be loaded if we found a valid serialization for them
                //if ( projectKnobs[i]->getIsPersistant() ) {
                
                Choice_Knob* isChoice = dynamic_cast<Choice_Knob*>(projectKnobs[i].get());
                if (isChoice) {
                    const TypeExtraData* extraData = (*it)->getExtraData();
                    const ChoiceExtraData* choiceData = dynamic_cast<const ChoiceExtraData*>(extraData);
                    assert(choiceData);
                    
                    Choice_Knob* serializedKnob = dynamic_cast<Choice_Knob*>((*it)->getKnob().get());
                    assert(serializedKnob);
                    isChoice->choiceRestoration(serializedKnob, choiceData);
                } else {
                    projectKnobs[i]->clone( (*it)->getKnob() );
                }
                //}
                break;
            }
        }
        if (projectKnobs[i] == envVars) {
            
            ///For eAppTypeBackgroundAutoRunLaunchedFromGui don't change the project path since it is controlled
            ///by the main GUI process
            if (appPTR->getAppType() != AppManager::eAppTypeBackgroundAutoRunLaunchedFromGui) {
                autoSetProjectDirectory(isAutoSave ? realFilePath : path);
            }
            _publicInterface->onOCIOConfigPathChanged(appPTR->getOCIOConfigPath(),false);
        }

    }

    /// 2) restore the timeline
    timeline->setBoundaries( obj.getLeftBoundTime(), obj.getRightBoundTime() );
    timeline->seekFrame(obj.getCurrentTime(),NULL,Natron::eTimelineChangeReasonPlaybackSeek);

    ///On our tests restoring nodes + connections takes approximatively 20% of loading time of a project, hence we update progress
    ///for each node of 0.2 / nbNodes
    
    /// 3) Restore the nodes
    const std::list< NodeSerialization > & serializedNodes = obj.getNodesSerialization();
    bool hasProjectAWriter = false;

    ///If a parent of a multi-instance node doesn't exist anymore but the children do, we must recreate the parent.
    ///Problem: we have lost the nodes connections. To do so we restore them using the serialization of a child.
    ///This map contains all the parents that must be reconnected and an iterator to the child serialization
    std::map<boost::shared_ptr<Natron::Node>, std::list<NodeSerialization>::const_iterator > parentsToReconnect;

    /*first create all nodes*/
    int nodesRestored = 0;
    for (std::list< NodeSerialization >::const_iterator it = serializedNodes.begin(); it != serializedNodes.end(); ++it) {
        ++nodesRestored;
        
        std::string pluginID = it->getPluginID();
        
        if ( appPTR->isBackground() && (pluginID == NATRON_VIEWER_ID || pluginID == "Viewer") ) {
            //if the node is a viewer, don't try to load it in background mode
            continue;
        }

        ///If the node is a multiinstance child find in all the serialized nodes if the parent exists.
        ///If not, create it

        if ( !it->getMultiInstanceParentName().empty() ) {
            bool foundParent = false;
            for (std::list< NodeSerialization >::const_iterator it2 = serializedNodes.begin(); it2 != serializedNodes.end(); ++it2) {
                if ( it2->getPluginLabel() == it->getMultiInstanceParentName() ) {
                    foundParent = true;
                    break;
                }
            }
            if (!foundParent) {
                ///Maybe it was created so far by another child who created it so look into the nodes
                for (std::vector<boost::shared_ptr<Natron::Node> >::iterator it2 = currentNodes.begin(); it2 != currentNodes.end(); ++it2) {
                    if ( (*it2)->getName() == it->getMultiInstanceParentName() ) {
                        foundParent = true;
                        break;
                    }
                }
                ///Create the parent
                if (!foundParent) {
                    boost::shared_ptr<Natron::Node> parent = project->getApp()->createNode( CreateNodeArgs( pluginID.c_str(),
                                                                                                            "",
                                                                                                            it->getPluginMajorVersion(),
                                                                                                            it->getPluginMinorVersion(),
                                                                                                           -1,
                                                                                                           true,
                                                                                                           INT_MIN,
                                                                                                           INT_MIN,
                                                                                                           true,
                                                                                                           true,
                                                                                                           QString(),
                                                                                                           CreateNodeArgs::DefaultValuesList()) );
                    parent->setName( it->getMultiInstanceParentName().c_str() );
                    parentsToReconnect.insert( std::make_pair(parent, it) );
                }
            }
        }

        boost::shared_ptr<Natron::Node> n = project->getApp()->loadNode( LoadNodeArgs(pluginID.c_str()
                                                                                      ,it->getMultiInstanceParentName()
                                                                                      ,it->getPluginMajorVersion()
                                                                                      ,it->getPluginMinorVersion(),&(*it),false) );
        if (!n) {
            QString text( QObject::tr("The node ") );
            text.append( pluginID.c_str() );
            text.append( QObject::tr(" was found in the script but doesn't seem \n"
                                     "to exist in the currently loaded plug-ins.") );
            appPTR->writeToOfxLog_mt_safe(text);
            mustShowErrorsLog = true;
            continue;
        }
        if ( n->isOutputNode() ) {
            hasProjectAWriter = true;
        }
        if (serializedNodes.size() > 0) {
            _publicInterface->getApp()->progressUpdate(_publicInterface, (0.2 * nodesRestored) / serializedNodes.size());
        }
    }


    if ( !hasProjectAWriter && appPTR->isBackground() ) {
        project->clearNodes();
        throw std::invalid_argument("Project file is missing a writer node. This project cannot render anything.");
    }


    /// 4) connect the nodes together, and restore the slave/master links for all knobs.
    for (std::list< NodeSerialization >::const_iterator it = serializedNodes.begin(); it != serializedNodes.end(); ++it) {
        if ( appPTR->isBackground() && (it->getPluginID() == NATRON_VIEWER_ID) ) {
            //ignore viewers on background mode
            continue;
        }


        boost::shared_ptr<Natron::Node> thisNode;
        for (U32 j = 0; j < currentNodes.size(); ++j) {
            if ( currentNodes[j]->getName() == it->getPluginLabel() ) {
                thisNode = currentNodes[j];
                break;
            }
        }
        if (!thisNode) {
            continue;
        }

        ///for all nodes that are part of a multi-instance, fetch the main instance node pointer
        const std::string & parentName = it->getMultiInstanceParentName();
        if ( !parentName.empty() ) {
            thisNode->fetchParentMultiInstancePointer();
        }

        ///restore slave/master link if any
        const std::string & masterNodeName = it->getMasterNodeName();
        if ( !masterNodeName.empty() ) {
            ///find such a node
            boost::shared_ptr<Natron::Node> masterNode;
            for (U32 j = 0; j < currentNodes.size(); ++j) {
                if (currentNodes[j]->getName() == masterNodeName) {
                    masterNode = currentNodes[j];
                    break;
                }
            }
            if (!masterNode) {
                appPTR->writeToOfxLog_mt_safe(QString("Cannot restore the link between " + QString(it->getPluginLabel().c_str()) + " and " + masterNodeName.c_str()));
                mustShowErrorsLog = true;
            }
            thisNode->getLiveInstance()->slaveAllKnobs( masterNode->getLiveInstance() );
        } else {
            thisNode->restoreKnobsLinks(*it,currentNodes);
        }

        const std::vector<std::string> & inputs = it->getInputs();
        for (U32 j = 0; j < inputs.size(); ++j) {
            if ( !inputs[j].empty() && !project->getApp()->getProject()->connectNodes(j, inputs[j],thisNode.get()) ) {
                std::string message = std::string("Failed to connect node ") + it->getPluginLabel() + " to " + inputs[j];
                appPTR->writeToOfxLog_mt_safe(message.c_str());
                mustShowErrorsLog =true;
            }
        }
    }

    ///Also reconnect parents of multiinstance nodes that were created on the fly
    for (std::map<boost::shared_ptr<Natron::Node>, std::list<NodeSerialization>::const_iterator >::const_iterator
         it = parentsToReconnect.begin(); it != parentsToReconnect.end(); ++it) {
        const std::vector<std::string> & inputs = it->second->getInputs();
        for (U32 j = 0; j < inputs.size(); ++j) {
            if ( !inputs[j].empty() && !project->getApp()->getProject()->connectNodes(j, inputs[j],it->first.get()) ) {
                std::string message = std::string("Failed to connect node ") + it->first->getPluginLabel() + " to " + inputs[j];
                appPTR->writeToOfxLog_mt_safe(message.c_str());
                mustShowErrorsLog =true;

            }
        }
    }
    
    _publicInterface->getApp()->progressUpdate(_publicInterface, 0.25);
    
    ///The next for loop is about 50% of loading time of a project
    
    ///Now that everything is connected, check clip preferences on all OpenFX effects
    std::list<Natron::Node*> markedNodes;
    std::list<Natron::Node*> nodesToRestorePreferences;
    for (U32 i = 0; i < currentNodes.size(); ++i) {
        if (currentNodes[i]->isOutputNode()) {
            nodesToRestorePreferences.push_back(currentNodes[i].get());
        }
    }
    
    int count = 0;
    for (std::list<Natron::Node*>::iterator it = nodesToRestorePreferences.begin(); it!=nodesToRestorePreferences.end(); ++it,++count) {
        (*it)->restoreClipPreferencesRecursive(markedNodes);
        _publicInterface->getApp()->progressUpdate(_publicInterface,
                                                   ((double)(count+1) / (double)nodesToRestorePreferences.size()) * 0.5 + 0.25);
    }
    
    ///We should be now at 75% progress...

    nodeCounters = obj.getNodeCounters();
    
    QDateTime time = QDateTime::currentDateTime();
    autoSetProjectFormat = false;
    hasProjectBeenSavedByUser = true;
    projectName = name;
    projectPath = isAutoSave ? realFilePath : path;
    ageSinceLastSave = time;
    lastAutoSave = time;
    
    return !mustShowErrorsLog;
} // restoreFromSerialization

bool
ProjectPrivate::findFormat(int index,
                           Format* format) const
{
    if ( index >= (int)( builtinFormats.size() + additionalFormats.size() ) ) {
        return false;
    }

    int i = 0;
    if ( index >= (int)builtinFormats.size() ) {
        ///search in the additional formats
        index -= builtinFormats.size();

        for (std::list<Format>::const_iterator it = additionalFormats.begin(); it != additionalFormats.end(); ++it) {
            if (i == index) {
                assert( !it->isNull() );
                *format = *it;

                return true;
            }
            ++i;
        }
    } else {
        ///search in the builtins formats
        for (std::list<Format>::const_iterator it = builtinFormats.begin(); it != builtinFormats.end(); ++it) {
            if (i == index) {
                assert( !it->isNull() );
                *format = *it;

                return true;
            }
            ++i;
        }
    }

    return false;
}
    
void
ProjectPrivate::autoSetProjectDirectory(const QString& path)
{
    std::string pathCpy = path.toStdString();
    if (!pathCpy.empty() && pathCpy[pathCpy.size() -1] == '/') {
        pathCpy.erase(pathCpy.size() - 1, 1);
    }
    std::string env = envVars->getValue();
    std::map<std::string, std::string> envMap;
    Project::makeEnvMap(env, envMap);
    
    ///If there was already a OCIO variable, update it, otherwise create it
    
    std::map<std::string, std::string>::iterator foundPROJECT = envMap.find(NATRON_PROJECT_ENV_VAR_NAME);
    if (foundPROJECT != envMap.end()) {
        foundPROJECT->second = pathCpy;
    } else {
        envMap.insert(std::make_pair(NATRON_PROJECT_ENV_VAR_NAME, pathCpy));
    }
    
    std::string newEnv;
    for (std::map<std::string, std::string>::iterator it = envMap.begin(); it!=envMap.end();++it) {
        newEnv += NATRON_ENV_VAR_NAME_START_TAG;
        // In order to use XML tags, the text inside the tags has to be escaped.
        newEnv += Project::escapeXML(it->first);
        newEnv += NATRON_ENV_VAR_NAME_END_TAG;
        newEnv += NATRON_ENV_VAR_VALUE_START_TAG;
        newEnv += Project::escapeXML(it->second);
        newEnv += NATRON_ENV_VAR_VALUE_END_TAG;
    }
    if (env != newEnv) {
        if (appPTR->getCurrentSettings()->isAutoFixRelativeFilePathEnabled()) {
            _publicInterface->fixRelativeFilePaths(NATRON_PROJECT_ENV_VAR_NAME, pathCpy,false);
        }
        envVars->setValue(newEnv, 0);
    }
}
    

    
} // namespace Natron
