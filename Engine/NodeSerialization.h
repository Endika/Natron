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

#ifndef NODESERIALIZATION_H
#define NODESERIALIZATION_H

#include <string>

#include "Global/Macros.h"
#ifndef Q_MOC_RUN
CLANG_DIAG_OFF(unused-parameter)
// /opt/local/include/boost/serialization/smart_cast.hpp:254:25: warning: unused parameter 'u' [-Wunused-parameter]
#include <boost/archive/xml_iarchive.hpp>
CLANG_DIAG_ON(unused-parameter)
#include <boost/archive/xml_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/version.hpp>
#endif
#include "Engine/KnobSerialization.h"
#include "Engine/RotoSerialization.h"


#define NODE_SERIALIZATION_V_INTRODUCES_ROTO 2
#define NODE_SERIALIZATION_INTRODUCES_MULTI_INSTANCE 3
#define NODE_SERIALIZATION_CURRENT_VERSION NODE_SERIALIZATION_INTRODUCES_MULTI_INSTANCE

namespace Natron {
class Node;
}
class AppInstance;

class NodeSerialization
{
public:

    typedef std::list< boost::shared_ptr<KnobSerialization> > KnobValues;

    ///Used to serialize
    NodeSerialization(const boost::shared_ptr<Natron::Node> & n,bool serializeInputs = true,bool copyKnobs = false);

    ////Used to deserialize
    NodeSerialization(AppInstance* app)
        : _isNull(true)
        , _nbKnobs(0)
        , _knobsValues()
        , _knobsAge(0)
        , _pluginLabel()
        , _pluginID()
        , _pluginMajorVersion(-1)
        , _pluginMinorVersion(-1)
        , _hasRotoContext(false)
        , _node()
        , _app(app)
    {
    }

    ~NodeSerialization()
    {
        _knobsValues.clear(); _inputs.clear();
    }

    const KnobValues & getKnobsValues() const
    {
        return _knobsValues;
    }

    const std::string & getPluginLabel() const
    {
        return _pluginLabel;
    }

    const std::string & getPluginID() const
    {
        return _pluginID;
    }

    const std::vector<std::string> & getInputs() const
    {
        return _inputs;
    }
    
    void setInputs(const std::vector<std::string>& inputs) {
        _inputs = inputs;
    }

    int getPluginMajorVersion() const
    {
        return _pluginMajorVersion;
    }

    int getPluginMinorVersion() const
    {
        return _pluginMinorVersion;
    }

    bool isNull() const
    {
        return _isNull;
    }

    U64 getKnobsAge() const
    {
        return _knobsAge;
    }

    const std::string & getMasterNodeName() const
    {
        return _masterNodeName;
    }

    boost::shared_ptr<Natron::Node> getNode() const
    {
        return _node;
    }

    bool hasRotoContext() const
    {
        return _hasRotoContext;
    }

    const RotoContextSerialization & getRotoContext() const
    {
        return _rotoContext;
    }

    const std::string & getMultiInstanceParentName() const
    {
        return _multiInstanceParentName;
    }

private:

    bool _isNull;
    int _nbKnobs;
    KnobValues _knobsValues;
    U64 _knobsAge;
    std::string _pluginLabel;
    std::string _pluginID;
    int _pluginMajorVersion;
    int _pluginMinorVersion;
    std::string _masterNodeName;
    std::vector<std::string> _inputs;
    bool _hasRotoContext;
    RotoContextSerialization _rotoContext;
    boost::shared_ptr<Natron::Node> _node;
    AppInstance* _app;
    std::string _multiInstanceParentName;

    friend class boost::serialization::access;
    template<class Archive>
    void save(Archive & ar,
              const unsigned int /*version*/) const
    {
        ar & boost::serialization::make_nvp("Plugin_label",_pluginLabel);
        ar & boost::serialization::make_nvp("Plugin_id",_pluginID);
        ar & boost::serialization::make_nvp("Plugin_major_version",_pluginMajorVersion);
        ar & boost::serialization::make_nvp("Plugin_minor_version",_pluginMinorVersion);
        ar & boost::serialization::make_nvp("KnobsCount", _nbKnobs);

        for (KnobValues::const_iterator it = _knobsValues.begin(); it != _knobsValues.end(); ++it) {
            ar & boost::serialization::make_nvp( "item",*(*it) );
        }
        ar & boost::serialization::make_nvp("Inputs_map",_inputs);
        ar & boost::serialization::make_nvp("KnobsAge",_knobsAge);
        ar & boost::serialization::make_nvp("MasterNode",_masterNodeName);
        ar & boost::serialization::make_nvp("HasRotoContext",_hasRotoContext);
        if (_hasRotoContext) {
            ar & boost::serialization::make_nvp("RotoContext",_rotoContext);
        }
        ar & boost::serialization::make_nvp("MultiInstanceParent",_multiInstanceParentName);
    }

    template<class Archive>
    void load(Archive & ar,
              const unsigned int version)
    {
        if (version > NODE_SERIALIZATION_CURRENT_VERSION) {
            throw std::invalid_argument("The project you're trying to load contains data produced by a more recent "
                                        "version of Natron, which makes it unreadable");
        }

        assert(_app);
        ar & boost::serialization::make_nvp("Plugin_label",_pluginLabel);
        ar & boost::serialization::make_nvp("Plugin_id",_pluginID);
        ar & boost::serialization::make_nvp("Plugin_major_version",_pluginMajorVersion);
        ar & boost::serialization::make_nvp("Plugin_minor_version",_pluginMinorVersion);
        ar & boost::serialization::make_nvp("KnobsCount", _nbKnobs);
        for (int i = 0; i < _nbKnobs; ++i) {
            boost::shared_ptr<KnobSerialization> ks(new KnobSerialization);
            ar & boost::serialization::make_nvp("item",*ks);
            _knobsValues.push_back(ks);
        }
        ar & boost::serialization::make_nvp("Inputs_map",_inputs);
        ar & boost::serialization::make_nvp("KnobsAge",_knobsAge);
        ar & boost::serialization::make_nvp("MasterNode",_masterNodeName);
        _isNull = false;

        if (version >= NODE_SERIALIZATION_V_INTRODUCES_ROTO) {
            ar & boost::serialization::make_nvp("HasRotoContext",_hasRotoContext);
            if (_hasRotoContext) {
                ar & boost::serialization::make_nvp("RotoContext",_rotoContext);
            }
        }
        if (version >= NODE_SERIALIZATION_INTRODUCES_MULTI_INSTANCE) {
            ar & boost::serialization::make_nvp("MultiInstanceParent",_multiInstanceParentName);
        }
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()
};


BOOST_CLASS_VERSION(NodeSerialization, NODE_SERIALIZATION_CURRENT_VERSION)


#endif // NODESERIALIZATION_H
