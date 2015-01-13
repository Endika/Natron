//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */


#ifndef PROJECTSERIALIZATION_H
#define PROJECTSERIALIZATION_H

#include "Global/Macros.h"
#ifndef Q_MOC_RUN
CLANG_DIAG_OFF(unused-parameter)
// /opt/local/include/boost/serialization/smart_cast.hpp:254:25: warning: unused parameter 'u' [-Wunused-parameter]
#include <boost/archive/xml_iarchive.hpp>
CLANG_DIAG_ON(unused-parameter)
#include <boost/archive/xml_oarchive.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/scoped_ptr.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/version.hpp>
#endif

#include "Global/GitVersion.h"
#include "Global/GlobalDefines.h"
#include "Global/MemoryInfo.h"
#include "Engine/ProjectPrivate.h"
#include "Engine/Project.h"
#include "Engine/TimeLine.h"
#include "Engine/Node.h"
#include "Engine/NodeSerialization.h"
#include "Engine/KnobSerialization.h"

#define PROJECT_SERIALIZATION_INTRODUCES_NATRON_VERSION 2
#define PROJECT_SERIALIZATION_REMOVES_NODE_COUNTERS 3
#define PROJECT_SERIALIZATION_REMOVES_TIMELINE_BOUNDS 4
#define PROJECT_SERIALIZATION_VERSION PROJECT_SERIALIZATION_REMOVES_TIMELINE_BOUNDS

class AppInstance;
class ProjectSerialization
{
    std::list< NodeSerialization > _serializedNodes;
    std::list<Format> _additionalFormats;
    std::list< boost::shared_ptr<KnobSerialization> > _projectKnobs;
    SequenceTime _timelineCurrent;
    qint64 _creationDate;
    AppInstance* _app;

public:

    ProjectSerialization(AppInstance* app)
        : _timelineCurrent(0)
        , _creationDate(0)
        , _app(app)
    {
    }

    ~ProjectSerialization()
    {
        _serializedNodes.clear();
    }

    void initialize(const Natron::Project* project);

    SequenceTime getCurrentTime() const
    {
        return _timelineCurrent;
    }

    const std::list< boost::shared_ptr<KnobSerialization>  > & getProjectKnobsValues() const
    {
        return _projectKnobs;
    }

    const std::list<Format> & getAdditionalFormats() const
    {
        return _additionalFormats;
    }

    const std::list< NodeSerialization > & getNodesSerialization() const
    {
        return _serializedNodes;
    }

    qint64 getCreationDate() const
    {
        return _creationDate;
    }


    friend class boost::serialization::access;
    template<class Archive>
    void save(Archive & ar,
              const unsigned int /*version*/) const
    {
        
        std::string natronVersion(NATRON_APPLICATION_NAME);
        natronVersion.append(" v" NATRON_VERSION_STRING);
        natronVersion.append(" from git branch " GIT_BRANCH);
        natronVersion.append(" commit " GIT_COMMIT);
        natronVersion.append("  for ");
#ifdef __NATRON_WIN32__
        natronVersion.append("  Windows ");
#elif defined(__NATRON_OSX__)
        natronVersion.append("  MacOSX ");
#elif defined(__NATRON_LINUX__)
        natronVersion.append("  Linux ");
#endif
        natronVersion.append(isApplication32Bits() ? "32bit" : "64bit");
        ar & boost::serialization::make_nvp("NatronVersion",natronVersion);
        
        int nodesCount = (int)_serializedNodes.size();
        ar & boost::serialization::make_nvp("NodesCount",nodesCount);

        for (std::list< NodeSerialization >::const_iterator it = _serializedNodes.begin();
             it != _serializedNodes.end();
             ++it) {
            ar & boost::serialization::make_nvp("item",*it);
        }
        int knobsCount = _projectKnobs.size();
        ar & boost::serialization::make_nvp("ProjectKnobsCount",knobsCount);
        for (std::list< boost::shared_ptr<KnobSerialization> >::const_iterator it = _projectKnobs.begin();
             it != _projectKnobs.end();
             ++it) {
            ar & boost::serialization::make_nvp( "item",*(*it) );
        }
        ar & boost::serialization::make_nvp("AdditionalFormats", _additionalFormats);
        ar & boost::serialization::make_nvp("Timeline_current_time", _timelineCurrent);
        ar & boost::serialization::make_nvp("CreationDate", _creationDate);
    }

    template<class Archive>
    void load(Archive & ar,
              const unsigned int version)
    {
        if (version >= PROJECT_SERIALIZATION_INTRODUCES_NATRON_VERSION) {
            std::string natronVersion;
            ar & boost::serialization::make_nvp("NatronVersion",natronVersion);
            
        } 
        assert(_app);
        int nodesCount;
        ar & boost::serialization::make_nvp("NodesCount",nodesCount);
        for (int i = 0; i < nodesCount; ++i) {
            NodeSerialization ns(_app);
            ar & boost::serialization::make_nvp("item",ns);
            _serializedNodes.push_back(ns);
        }

        int knobsCount;
        ar & boost::serialization::make_nvp("ProjectKnobsCount",knobsCount);
        
        for (int i = 0; i < knobsCount; ++i) {
            boost::shared_ptr<KnobSerialization> ks(new KnobSerialization);
            ar & boost::serialization::make_nvp("item",*ks);
            _projectKnobs.push_back(ks);
        }

        ar & boost::serialization::make_nvp("AdditionalFormats", _additionalFormats);
        ar & boost::serialization::make_nvp("Timeline_current_time", _timelineCurrent);
        if (version < PROJECT_SERIALIZATION_REMOVES_TIMELINE_BOUNDS) {
            SequenceTime left,right;
            ar & boost::serialization::make_nvp("Timeline_left_bound", left);
            ar & boost::serialization::make_nvp("Timeline_right_bound", right);
        }
        if (version < PROJECT_SERIALIZATION_REMOVES_NODE_COUNTERS) {
            std::map<std::string,int> _nodeCounters;
            ar & boost::serialization::make_nvp("NodeCounters", _nodeCounters);
        }
        ar & boost::serialization::make_nvp("CreationDate", _creationDate);
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()
};

BOOST_CLASS_VERSION(ProjectSerialization,PROJECT_SERIALIZATION_VERSION)

#endif // PROJECTSERIALIZATION_H
