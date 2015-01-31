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

// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>

#include "OfxParamInstance.h"

#include <iostream>
#include <boost/scoped_array.hpp>
#include <boost/math/special_functions/fpclassify.hpp>

//ofx extension
#include <nuke/fnPublicOfxExtensions.h>
#include <ofxParametricParam.h>

#include <QDebug>


#include "Engine/AppManager.h"
#include "Global/GlobalDefines.h"
#include "Engine/Knob.h"
#include "Engine/KnobFactory.h"
#include "Engine/KnobFile.h"
#include "Engine/KnobTypes.h"
#include "Engine/OfxEffectInstance.h"
#include "Engine/OfxClipInstance.h"
#include "Engine/OfxImageEffectInstance.h"
#include "Engine/ViewerInstance.h"
#include "Engine/Curve.h"
#include "Engine/OfxOverlayInteract.h"
#include "Engine/Format.h"
#include "Engine/Project.h"
#include "Engine/AppInstance.h"

using namespace Natron;


static std::string
getParamLabel(OFX::Host::Param::Instance* param)
{
    std::string label = param->getProperties().getStringProperty(kOfxPropLabel);

    if ( label.empty() ) {
        label = param->getProperties().getStringProperty(kOfxPropShortLabel);
    }
    if ( label.empty() ) {
        label = param->getProperties().getStringProperty(kOfxPropLongLabel);
    }
    if ( label.empty() ) {
        label = param->getName();
    }

    return label;
}

///anonymous namespace to handle keyframes communication support for Ofx plugins
/// in a generalized manner
namespace OfxKeyFrame {
OfxStatus
getNumKeys(boost::shared_ptr<KnobI> knob,
           unsigned int &nKeys)
{
    int sum = 0;

    if (knob->canAnimate()) {
        for (int i = 0; i < knob->getDimension(); ++i) {
            boost::shared_ptr<Curve> curve = knob->getCurve(i);
            assert(curve);
            sum += curve->getKeyFramesCount();
        }
    }
    nKeys =  sum;

    return kOfxStatOK;
}

OfxStatus
getKeyTime(boost::shared_ptr<KnobI> knob,
           int nth,
           OfxTime & time)
{
    if (nth < 0) {
        return kOfxStatErrBadIndex;
    }
    int dimension = 0;
    int indexSoFar = 0;
    while ( dimension < knob->getDimension() ) {
        ++dimension;
        int curveKeyFramesCount = knob->getKeyFramesCount(dimension);
        if ( nth >= (int)(curveKeyFramesCount + indexSoFar) ) {
            indexSoFar += curveKeyFramesCount;
            continue;
        } else {
            boost::shared_ptr<Curve> curve = knob->getCurve(dimension);
            assert(curve);
            KeyFrameSet set = curve->getKeyFrames_mt_safe();
            KeyFrameSet::const_iterator it = set.begin();
            while ( it != set.end() ) {
                if (indexSoFar == nth) {
                    time = it->getTime();

                    return kOfxStatOK;
                }
                ++indexSoFar;
                ++it;
            }
        }
    }

    return kOfxStatErrBadIndex;
}

OfxStatus
getKeyIndex(boost::shared_ptr<KnobI> knob,
            OfxTime time,
            int direction,
            int & index)
{
    int c = 0;

    for (int i = 0; i < knob->getDimension(); ++i) {
        if (!knob->isAnimated(i)) {
            continue;
        }
        boost::shared_ptr<Curve> curve = knob->getCurve(i);
        assert(curve);
        KeyFrameSet set = curve->getKeyFrames_mt_safe();
        for (KeyFrameSet::const_iterator it = set.begin(); it != set.end(); ++it) {
            if (it->getTime() == time) {
                if (direction == 0) {
                    index = c;
                } else if (direction < 0) {
                    if ( it == set.begin() ) {
                        index = -1;
                    } else {
                        index = c - 1;
                    }
                } else {
                    KeyFrameSet::const_iterator next = it;
                    ++next;
                    if ( next != set.end() ) {
                        index = c + 1;
                    } else {
                        index = -1;
                    }
                }

                return kOfxStatOK;
            }
            ++c;
        }
    }

    return kOfxStatFailed;
}

OfxStatus
deleteKey(boost::shared_ptr<KnobI> knob,
          OfxTime time)
{
    for (int i = 0; i < knob->getDimension(); ++i) {
        knob->deleteValueAtTime(time, i);
    }

    return kOfxStatOK;
}

OfxStatus
deleteAllKeys(boost::shared_ptr<KnobI> knob)
{
    for (int i = 0; i < knob->getDimension(); ++i) {
        knob->removeAnimation(i);
    }

    return kOfxStatOK;
}

// copy one parameter to another, with a range (NULL means to copy all animation)
OfxStatus
copyFrom(const boost::shared_ptr<KnobI> & from,
         const boost::shared_ptr<KnobI> &to,
         OfxTime offset,
         const OfxRangeD* range)
{
    ///copy only if type is the same
    if ( from->typeName() == to->typeName() ) {
        to->clone(from,offset,range);
        to->blockEvaluation();
        int dims = to->getDimension();
        for (int i = 0; i < dims; ++i) {
            if (i == dims - 1) {
                to->unblockEvaluation();
            }
            to->evaluateValueChange(i, Natron::eValueChangedReasonPluginEdited, true);
        }
    }

    return kOfxStatOK;
}
}

////////////////////////// OfxPushButtonInstance /////////////////////////////////////////////////

OfxPushButtonInstance::OfxPushButtonInstance(OfxEffectInstance* node,
                                             OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::Param::PushbuttonInstance( descriptor, node->effectInstance() )
{
    _knob = Natron::createKnob<Button_Knob>( node, getParamLabel(this) );
    const std::string & iconFilePath = descriptor.getProperties().getStringProperty(kOfxPropIcon,1);
    _knob->setIconFilePath(iconFilePath);
}

// callback which should set enabled state as appropriate
void
OfxPushButtonInstance::setEnabled()
{
    _knob->setAllDimensionsEnabled( getEnabled() );
}

// callback which should set secret state as appropriate
void
OfxPushButtonInstance::setSecret()
{
    _knob->setSecret( getSecret() );
}

void
OfxPushButtonInstance::setEvaluateOnChange()
{
    _knob->setEvaluateOnChange( getEvaluateOnChange() );
}

boost::shared_ptr<KnobI> OfxPushButtonInstance::getKnob() const
{
    return _knob;
}

////////////////////////// OfxIntegerInstance /////////////////////////////////////////////////


OfxIntegerInstance::OfxIntegerInstance(OfxEffectInstance* node,
                                       OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::Param::IntegerInstance( descriptor, node->effectInstance() )
{
    const OFX::Host::Property::Set &properties = getProperties();

    _knob = Natron::createKnob<Int_Knob>( node, getParamLabel(this) );

    int min = properties.getIntProperty(kOfxParamPropMin);
    int max = properties.getIntProperty(kOfxParamPropMax);
    int def = properties.getIntProperty(kOfxParamPropDefault);
    int displayMin = properties.getIntProperty(kOfxParamPropDisplayMin);
    int displayMax = properties.getIntProperty(kOfxParamPropDisplayMax);
    _knob->setDisplayMinimum(displayMin);
    _knob->setDisplayMaximum(displayMax);

    _knob->setMinimum(min);
    _knob->setIncrement(1); // kOfxParamPropIncrement only exists for Double
    _knob->setMaximum(max);
    _knob->setDefaultValue(def,0);
    std::string dimensionName = properties.getStringProperty(kOfxParamPropDimensionLabel,0);
    _knob->setDimensionName(0, dimensionName);
}

OfxStatus
OfxIntegerInstance::get(int & v)
{
    v = _knob->getValue();

    return kOfxStatOK;
}

OfxStatus
OfxIntegerInstance::get(OfxTime time,
                        int & v)
{
    v = _knob->getValueAtTime(time);

    return kOfxStatOK;
}

OfxStatus
OfxIntegerInstance::set(int v)
{
    _knob->setValueFromPlugin(v,0);

    return kOfxStatOK;
}

OfxStatus
OfxIntegerInstance::set(OfxTime time,
                        int v)
{
    if (!getCanAnimate()) {
        qDebug() << "Attempting to call setValueAtTime on a parameter that does not have animation enabled";
        return set(v);
    }
    _knob->setValueAtTimeFromPlugin(time,v,0);

    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void
OfxIntegerInstance::setEnabled()
{
    _knob->setAllDimensionsEnabled( getEnabled() );
}

// callback which should set secret state as appropriate
void
OfxIntegerInstance::setSecret()
{
    _knob->setSecret( getSecret() );
}

void
OfxIntegerInstance::setEvaluateOnChange()
{
    _knob->setEvaluateOnChange( getEvaluateOnChange() );
}

boost::shared_ptr<KnobI> OfxIntegerInstance::getKnob() const
{
    return _knob;
}

OfxStatus
OfxIntegerInstance::getNumKeys(unsigned int &nKeys) const
{
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}

OfxStatus
OfxIntegerInstance::getKeyTime(int nth,
                               OfxTime & time) const
{
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}

OfxStatus
OfxIntegerInstance::getKeyIndex(OfxTime time,
                                int direction,
                                int & index) const
{
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}

OfxStatus
OfxIntegerInstance::deleteKey(OfxTime time)
{
    return OfxKeyFrame::deleteKey(_knob, time);
}

OfxStatus
OfxIntegerInstance::deleteAllKeys()
{
    return OfxKeyFrame::deleteAllKeys(_knob);
}

OfxStatus
OfxIntegerInstance::copyFrom(const OFX::Host::Param::Instance &instance,
                             OfxTime offset,
                             const OfxRangeD* range)
{
    const OfxParamToKnob & other = dynamic_cast<const OfxParamToKnob &>(instance);

    return OfxKeyFrame::copyFrom(other.getKnob(), getKnob(), offset, range);
}

void
OfxIntegerInstance::onKnobAnimationLevelChanged(int,int lvl)
{
    Natron::AnimationLevelEnum l = (Natron::AnimationLevelEnum)lvl;

    assert( l == Natron::eAnimationLevelNone || getCanAnimate() );
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::eAnimationLevelNone);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::eAnimationLevelInterpolatedValue);
}

void
OfxIntegerInstance::setDisplayRange()
{
    int displayMin = getProperties().getIntProperty(kOfxParamPropDisplayMin);
    int displayMax = getProperties().getIntProperty(kOfxParamPropDisplayMax);

    _knob->setDisplayMinimum(displayMin);
    _knob->setDisplayMaximum(displayMax);
}

void
OfxIntegerInstance::setRange()
{
    int mini = getProperties().getIntProperty(kOfxParamPropMin);
    int maxi = getProperties().getIntProperty(kOfxParamPropMax);
    
    _knob->setMinimum(mini);
    _knob->setMaximum(maxi);
}

////////////////////////// OfxDoubleInstance /////////////////////////////////////////////////


OfxDoubleInstance::OfxDoubleInstance(OfxEffectInstance* node,
                                     OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::Param::DoubleInstance( descriptor,node->effectInstance() )
      , _node(node)
{
    const OFX::Host::Property::Set &properties = getProperties();
    const std::string & coordSystem = properties.getStringProperty(kOfxParamPropDefaultCoordinateSystem);

    _knob = Natron::createKnob<Double_Knob>( node, getParamLabel(this) );

    const std::string & doubleType = properties.getStringProperty(kOfxParamPropDoubleType);
    if ( (doubleType == kOfxParamDoubleTypeNormalisedX) ||
         ( doubleType == kOfxParamDoubleTypeNormalisedXAbsolute) ) {
        _knob->setNormalizedState(0, Double_Knob::eNormalizedStateX);
    } else if ( (doubleType == kOfxParamDoubleTypeNormalisedY) ||
                ( doubleType == kOfxParamDoubleTypeNormalisedYAbsolute) ) {
        _knob->setNormalizedState(0, Double_Knob::eNormalizedStateY);
    }

    double min = properties.getDoubleProperty(kOfxParamPropMin);
    double max = properties.getDoubleProperty(kOfxParamPropMax);
    double incr = properties.getDoubleProperty(kOfxParamPropIncrement);
    double def = properties.getDoubleProperty(kOfxParamPropDefault);
    int decimals = properties.getIntProperty(kOfxParamPropDigits);


    _knob->setMinimum(min);
    _knob->setMaximum(max);
    setDisplayRange();
    if (incr > 0) {
        _knob->setIncrement(incr);
    }
    if (decimals > 0) {
        _knob->setDecimals(decimals);
    }

    if (coordSystem == kOfxParamCoordinatesNormalised) {
        // The defaults should be stored as is, not premultiplied by the project size.
        // The fact that the default value is normalized should be stored in Knob or Double_Knob.

        // see http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#kOfxParamPropDefaultCoordinateSystem
        // and http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#APIChanges_1_2_SpatialParameters
        _knob->setDefaultValuesNormalized(def);
    } else {
        _knob->setDefaultValue(def,0);
    }

    std::string dimensionName = properties.getStringProperty(kOfxParamPropDimensionLabel,0);
    _knob->setDimensionName(0, dimensionName);
}

OfxStatus
OfxDoubleInstance::get(double & v)
{
    v = _knob->getValue();

    return kOfxStatOK;
}

OfxStatus
OfxDoubleInstance::get(OfxTime time,
                       double & v)
{
    v = _knob->getValueAtTime(time);

    return kOfxStatOK;
}

OfxStatus
OfxDoubleInstance::set(double v)
{
    _knob->setValueFromPlugin(v,0);

    return kOfxStatOK;
}

OfxStatus
OfxDoubleInstance::set(OfxTime time,
                       double v)
{
    if (!getCanAnimate()) {
        qDebug() << "Attempting to call setValueAtTime on a parameter that does not have animation enabled";
        return set(v);
    }
    _knob->setValueAtTimeFromPlugin(time,v,0);

    return kOfxStatOK;
}

OfxStatus
OfxDoubleInstance::derive(OfxTime time,
                          double & v)
{
    v = _knob->getDerivativeAtTime(time);

    return kOfxStatOK;
}

OfxStatus
OfxDoubleInstance::integrate(OfxTime time1,
                             OfxTime time2,
                             double & v)
{
    v = _knob->getIntegrateFromTimeToTime(time1, time2);

    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void
OfxDoubleInstance::setEnabled()
{
    _knob->setAllDimensionsEnabled( getEnabled() );
}

// callback which should set secret state as appropriate
void
OfxDoubleInstance::setSecret()
{
    _knob->setSecret( getSecret() );
}

void
OfxDoubleInstance::setEvaluateOnChange()
{
    _knob->setEvaluateOnChange( getEvaluateOnChange() );
}

void
OfxDoubleInstance::setDisplayRange()
{
    double displayMin = getProperties().getDoubleProperty(kOfxParamPropDisplayMin);
    double displayMax = getProperties().getDoubleProperty(kOfxParamPropDisplayMax);

    _knob->setDisplayMinimum(displayMin);
    _knob->setDisplayMaximum(displayMax);
}

void
OfxDoubleInstance::setRange()
{
    double mini = getProperties().getDoubleProperty(kOfxParamPropMin);
    double maxi = getProperties().getDoubleProperty(kOfxParamPropMax);
    
    _knob->setMinimum(mini);
    _knob->setMaximum(maxi);
}

boost::shared_ptr<KnobI>
OfxDoubleInstance::getKnob() const
{
    return _knob;
}

bool
OfxDoubleInstance::isAnimated() const
{
    return _knob->isAnimated(0);
}

OfxStatus
OfxDoubleInstance::getNumKeys(unsigned int &nKeys) const
{
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}

OfxStatus
OfxDoubleInstance::getKeyTime(int nth,
                              OfxTime & time) const
{
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}

OfxStatus
OfxDoubleInstance::getKeyIndex(OfxTime time,
                               int direction,
                               int & index) const
{
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}

OfxStatus
OfxDoubleInstance::deleteKey(OfxTime time)
{
    return OfxKeyFrame::deleteKey(_knob, time);
}

OfxStatus
OfxDoubleInstance::deleteAllKeys()
{
    return OfxKeyFrame::deleteAllKeys(_knob);
}

OfxStatus
OfxDoubleInstance::copyFrom(const OFX::Host::Param::Instance &instance,
                            OfxTime offset,
                            const OfxRangeD* range)
{
    const OfxParamToKnob & other = dynamic_cast<const OfxParamToKnob &>(instance);

    return OfxKeyFrame::copyFrom(other.getKnob(), getKnob(), offset, range);
}

void
OfxDoubleInstance::onKnobAnimationLevelChanged(int,int lvl)
{
    Natron::AnimationLevelEnum l = (Natron::AnimationLevelEnum)lvl;

    assert( l == Natron::eAnimationLevelNone || getCanAnimate() );
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::eAnimationLevelNone);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::eAnimationLevelInterpolatedValue);
}

////////////////////////// OfxBooleanInstance /////////////////////////////////////////////////

OfxBooleanInstance::OfxBooleanInstance(OfxEffectInstance* node,
                                       OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::Param::BooleanInstance( descriptor,node->effectInstance() )
{
    const OFX::Host::Property::Set &properties = getProperties();

    _knob = Natron::createKnob<Bool_Knob>( node, getParamLabel(this) );
    int def = properties.getIntProperty(kOfxParamPropDefault);
    _knob->setDefaultValue( (bool)def,0 );
}

OfxStatus
OfxBooleanInstance::get(bool & b)
{
    b = _knob->getValue();

    return kOfxStatOK;
}

OfxStatus
OfxBooleanInstance::get(OfxTime time,
                        bool & b)
{
    assert( Bool_Knob::canAnimateStatic() );
    b = _knob->getValueAtTime(time);

    return kOfxStatOK;
}

OfxStatus
OfxBooleanInstance::set(bool b)
{
    _knob->setValueFromPlugin(b,0);

    return kOfxStatOK;
}

OfxStatus
OfxBooleanInstance::set(OfxTime time,
                        bool b)
{
    if (!getCanAnimate()) {
        qDebug() << "Attempting to call setValueAtTime on a parameter that does not have animation enabled";
        return set(b);
    }
    assert( Bool_Knob::canAnimateStatic() );
    _knob->setValueAtTimeFromPlugin(time, b, 0);

    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void
OfxBooleanInstance::setEnabled()
{
    _knob->setAllDimensionsEnabled( getEnabled() );
}

// callback which should set secret state as appropriate
void
OfxBooleanInstance::setSecret()
{
    _knob->setSecret( getSecret() );
}

void
OfxBooleanInstance::setEvaluateOnChange()
{
    _knob->setEvaluateOnChange( getEvaluateOnChange() );
}

boost::shared_ptr<KnobI> OfxBooleanInstance::getKnob() const
{
    return _knob;
}

OfxStatus
OfxBooleanInstance::getNumKeys(unsigned int &nKeys) const
{
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}

OfxStatus
OfxBooleanInstance::getKeyTime(int nth,
                               OfxTime & time) const
{
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}

OfxStatus
OfxBooleanInstance::getKeyIndex(OfxTime time,
                                int direction,
                                int & index) const
{
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}

OfxStatus
OfxBooleanInstance::deleteKey(OfxTime time)
{
    return OfxKeyFrame::deleteKey(_knob, time);
}

OfxStatus
OfxBooleanInstance::deleteAllKeys()
{
    return OfxKeyFrame::deleteAllKeys(_knob);
}

OfxStatus
OfxBooleanInstance::copyFrom(const OFX::Host::Param::Instance &instance,
                             OfxTime offset,
                             const OfxRangeD* range)
{
    const OfxParamToKnob & other = dynamic_cast<const OfxParamToKnob &>(instance);

    return OfxKeyFrame::copyFrom(other.getKnob(), getKnob(), offset, range);
}

void
OfxBooleanInstance::onKnobAnimationLevelChanged(int,int lvl)
{
    Natron::AnimationLevelEnum l = (Natron::AnimationLevelEnum)lvl;

    assert( l == Natron::eAnimationLevelNone || getCanAnimate() );
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::eAnimationLevelNone);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::eAnimationLevelInterpolatedValue);
}

////////////////////////// OfxChoiceInstance /////////////////////////////////////////////////

OfxChoiceInstance::OfxChoiceInstance(OfxEffectInstance* node,
                                     OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::Param::ChoiceInstance( descriptor,node->effectInstance() )
{
    const OFX::Host::Property::Set &properties = getProperties();


    _knob = Natron::createKnob<Choice_Knob>( node, getParamLabel(this) );

    setOption(0); // this actually sets all the options

    int def = properties.getIntProperty(kOfxParamPropDefault);
    _knob->setDefaultValue(def,0);
}

OfxStatus
OfxChoiceInstance::get(int & v)
{
    v = _knob->getValue();

    return kOfxStatOK;
}

OfxStatus
OfxChoiceInstance::get(OfxTime time,
                       int & v)
{
    assert( Choice_Knob::canAnimateStatic() );
    v = _knob->getValueAtTime(time);

    return kOfxStatOK;
}

OfxStatus
OfxChoiceInstance::set(int v)
{
    if ( (0 <= v) && ( v < (int)_entries.size() ) ) {
        _knob->setValueFromPlugin(v,0);

        return kOfxStatOK;
    } else {
        return kOfxStatErrBadIndex;
    }
}

OfxStatus
OfxChoiceInstance::set(OfxTime time,
                       int v)
{
    if (!getCanAnimate()) {
        qDebug() << "Attempting to call setValueAtTime on a parameter that does not have animation enabled";
        return set(v);
    }
    if ( (0 <= v) && ( v < (int)_entries.size() ) ) {
        _knob->setValueAtTimeFromPlugin(time, v, 0);

        return kOfxStatOK;
    } else {
        return kOfxStatErrBadIndex;
    }
}

// callback which should set enabled state as appropriate
void
OfxChoiceInstance::setEnabled()
{
    _knob->setAllDimensionsEnabled( getEnabled() );
}

// callback which should set secret state as appropriate
void
OfxChoiceInstance::setSecret()
{
    _knob->setSecret( getSecret() );
}

void
OfxChoiceInstance::setEvaluateOnChange()
{
    _knob->setEvaluateOnChange( getEvaluateOnChange() );
}

void
OfxChoiceInstance::setOption(int /*num*/)
{
    int dim = getProperties().getDimension(kOfxParamPropChoiceOption);

    _entries.clear();
    std::vector<std::string> helpStrings;
    bool hashelp = false;
    for (int i = 0; i < dim; ++i) {
        std::string str = getProperties().getStringProperty(kOfxParamPropChoiceOption,i);
        std::string help = getProperties().getStringProperty(kOfxParamPropChoiceLabelOption,i);
        if ( !help.empty() ) {
            hashelp = true;
        }
        _entries.push_back(str);
        helpStrings.push_back(help);
    }
    if (!hashelp) {
        helpStrings.clear();
    }
    _knob->populateChoices(_entries, helpStrings);
}

boost::shared_ptr<KnobI>
OfxChoiceInstance::getKnob() const
{
    return _knob;
}

OfxStatus
OfxChoiceInstance::getNumKeys(unsigned int &nKeys) const
{
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}

OfxStatus
OfxChoiceInstance::getKeyTime(int nth,
                              OfxTime & time) const
{
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}

OfxStatus
OfxChoiceInstance::getKeyIndex(OfxTime time,
                               int direction,
                               int & index) const
{
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}

OfxStatus
OfxChoiceInstance::deleteKey(OfxTime time)
{
    return OfxKeyFrame::deleteKey(_knob, time);
}

OfxStatus
OfxChoiceInstance::deleteAllKeys()
{
    return OfxKeyFrame::deleteAllKeys(_knob);
}

OfxStatus
OfxChoiceInstance::copyFrom(const OFX::Host::Param::Instance &instance,
                            OfxTime offset,
                            const OfxRangeD* range)
{
    const OfxParamToKnob & other = dynamic_cast<const OfxParamToKnob &>(instance);

    return OfxKeyFrame::copyFrom(other.getKnob(), getKnob(), offset, range);
}

void
OfxChoiceInstance::onKnobAnimationLevelChanged(int,int lvl)
{
    Natron::AnimationLevelEnum l = (Natron::AnimationLevelEnum)lvl;

    assert( l == Natron::eAnimationLevelNone || getCanAnimate() );
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::eAnimationLevelNone);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::eAnimationLevelInterpolatedValue);
}

////////////////////////// OfxRGBAInstance /////////////////////////////////////////////////

OfxRGBAInstance::OfxRGBAInstance(OfxEffectInstance* node,
                                 OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::Param::RGBAInstance( descriptor,node->effectInstance() )
{
    const OFX::Host::Property::Set &properties = getProperties();

    _knob = Natron::createKnob<Color_Knob>(node, getParamLabel(this),4);
    double defR = properties.getDoubleProperty(kOfxParamPropDefault,0);
    double defG = properties.getDoubleProperty(kOfxParamPropDefault,1);
    double defB = properties.getDoubleProperty(kOfxParamPropDefault,2);
    double defA = properties.getDoubleProperty(kOfxParamPropDefault,3);
    _knob->setDefaultValue(defR,0);
    _knob->setDefaultValue(defG,1);
    _knob->setDefaultValue(defB,2);
    _knob->setDefaultValue(defA,3);

    const int dims = 4;
    std::vector<double> minimum(dims);
    std::vector<double> maximum(dims);
    std::vector<double> displayMins(dims);
    std::vector<double> displayMaxs(dims);

    // kOfxParamPropIncrement and kOfxParamPropDigits only have one dimension,
    // @see Descriptor::addNumericParamProps() in ofxhParam.cpp
    // @see gDoubleParamProps in ofxsPropertyValidation.cpp
    for (int i = 0; i < dims; ++i) {
        minimum[i] = properties.getDoubleProperty(kOfxParamPropMin,i);
        displayMins[i] = properties.getDoubleProperty(kOfxParamPropDisplayMin,i);
        displayMaxs[i] = properties.getDoubleProperty(kOfxParamPropDisplayMax,i);
        maximum[i] = properties.getDoubleProperty(kOfxParamPropMax,i);
        std::string dimensionName = properties.getStringProperty(kOfxParamPropDimensionLabel,i);
        _knob->setDimensionName(i, dimensionName);
    }

    _knob->setMinimumsAndMaximums(minimum, maximum);
    _knob->setDisplayMinimumsAndMaximums(displayMins, displayMaxs);
}

OfxStatus
OfxRGBAInstance::get(double & r,
                     double & g,
                     double & b,
                     double & a)
{
    r = _knob->getValue(0);
    g = _knob->getValue(1);
    b = _knob->getValue(2);
    a = _knob->getValue(3);

    return kOfxStatOK;
}

OfxStatus
OfxRGBAInstance::get(OfxTime time,
                     double &r,
                     double & g,
                     double & b,
                     double & a)
{
    r = _knob->getValueAtTime(time,0);
    g = _knob->getValueAtTime(time,1);
    b = _knob->getValueAtTime(time,2);
    a = _knob->getValueAtTime(time,3);

    return kOfxStatOK;
}

OfxStatus
OfxRGBAInstance::set(double r,
                     double g,
                     double b,
                     double a)
{
    _knob->setValueFromPlugin(r,0);
    _knob->setValueFromPlugin(g,1);
    _knob->setValueFromPlugin(b,2);
    _knob->setValueFromPlugin(a,3);

    return kOfxStatOK;
}

OfxStatus
OfxRGBAInstance::set(OfxTime time,
                     double r,
                     double g,
                     double b,
                     double a)
{
    if (!getCanAnimate()) {
        qDebug() << "Attempting to call setValueAtTime on a parameter that does not have animation enabled";
        return set(r,g,b,a);
    }
    _knob->setValueAtTimeFromPlugin(time,r,0);
    _knob->setValueAtTimeFromPlugin(time,g,1);
    _knob->setValueAtTimeFromPlugin(time,b,2);
    _knob->setValueAtTimeFromPlugin(time,a,3);

    return kOfxStatOK;
}

OfxStatus
OfxRGBAInstance::derive(OfxTime time,
                        double &r,
                        double & g,
                        double & b,
                        double & a)
{
    r = _knob->getDerivativeAtTime(time,0);
    g = _knob->getDerivativeAtTime(time,1);
    b = _knob->getDerivativeAtTime(time,2);
    a = _knob->getDerivativeAtTime(time,3);

    return kOfxStatOK;
}

OfxStatus
OfxRGBAInstance::integrate(OfxTime time1,
                           OfxTime time2,
                           double &r,
                           double & g,
                           double & b,
                           double & a)
{
    r = _knob->getIntegrateFromTimeToTime(time1, time2, 0);
    g = _knob->getIntegrateFromTimeToTime(time1, time2, 1);
    b = _knob->getIntegrateFromTimeToTime(time1, time2, 2);
    a = _knob->getIntegrateFromTimeToTime(time1, time2, 3);

    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void
OfxRGBAInstance::setEnabled()
{
    _knob->setAllDimensionsEnabled( getEnabled() );
}

// callback which should set secret state as appropriate
void
OfxRGBAInstance::setSecret()
{
    _knob->setSecret( getSecret() );
}

void
OfxRGBAInstance::setEvaluateOnChange()
{
    _knob->setEvaluateOnChange( getEvaluateOnChange() );
}

boost::shared_ptr<KnobI>
OfxRGBAInstance::getKnob() const
{
    return _knob;
}

bool
OfxRGBAInstance::isAnimated(int dimension) const
{
    return _knob->isAnimated(dimension);
}

bool
OfxRGBAInstance::isAnimated() const
{
    return _knob->isAnimated(0) || _knob->isAnimated(1) || _knob->isAnimated(2) || _knob->isAnimated(3);
}

OfxStatus
OfxRGBAInstance::getNumKeys(unsigned int &nKeys) const
{
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}

OfxStatus
OfxRGBAInstance::getKeyTime(int nth,
                            OfxTime & time) const
{
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}

OfxStatus
OfxRGBAInstance::getKeyIndex(OfxTime time,
                             int direction,
                             int & index) const
{
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}

OfxStatus
OfxRGBAInstance::deleteKey(OfxTime time)
{
    return OfxKeyFrame::deleteKey(_knob, time);
}

OfxStatus
OfxRGBAInstance::deleteAllKeys()
{
    return OfxKeyFrame::deleteAllKeys(_knob);
}

OfxStatus
OfxRGBAInstance::copyFrom(const OFX::Host::Param::Instance &instance,
                          OfxTime offset,
                          const OfxRangeD* range)
{
    const OfxParamToKnob & other = dynamic_cast<const OfxParamToKnob &>(instance);

    return OfxKeyFrame::copyFrom(other.getKnob(), getKnob(), offset, range);
}

void
OfxRGBAInstance::onKnobAnimationLevelChanged(int,int lvl)
{
    Natron::AnimationLevelEnum l = (Natron::AnimationLevelEnum)lvl;

    assert( l == Natron::eAnimationLevelNone || getCanAnimate() );
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::eAnimationLevelNone);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::eAnimationLevelInterpolatedValue);
}

////////////////////////// OfxRGBInstance /////////////////////////////////////////////////

OfxRGBInstance::OfxRGBInstance(OfxEffectInstance* node,
                               OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::Param::RGBInstance( descriptor,node->effectInstance() )
{
    const OFX::Host::Property::Set &properties = getProperties();

    _knob = Natron::createKnob<Color_Knob>(node, getParamLabel(this),3);
    double defR = properties.getDoubleProperty(kOfxParamPropDefault,0);
    double defG = properties.getDoubleProperty(kOfxParamPropDefault,1);
    double defB = properties.getDoubleProperty(kOfxParamPropDefault,2);
    _knob->setDefaultValue(defR, 0);
    _knob->setDefaultValue(defG, 1);
    _knob->setDefaultValue(defB, 2);

    const int dims = 3;
    std::vector<double> minimum(dims);
    std::vector<double> maximum(dims);
    std::vector<double> displayMins(dims);
    std::vector<double> displayMaxs(dims);

    // kOfxParamPropIncrement and kOfxParamPropDigits only have one dimension,
    // @see Descriptor::addNumericParamProps() in ofxhParam.cpp
    // @see gDoubleParamProps in ofxsPropertyValidation.cpp
    for (int i = 0; i < dims; ++i) {
        minimum[i] = properties.getDoubleProperty(kOfxParamPropMin,i);
        displayMins[i] = properties.getDoubleProperty(kOfxParamPropDisplayMin,i);
        displayMaxs[i] = properties.getDoubleProperty(kOfxParamPropDisplayMax,i);
        maximum[i] = properties.getDoubleProperty(kOfxParamPropMax,i);
        std::string dimensionName = properties.getStringProperty(kOfxParamPropDimensionLabel,i);
        _knob->setDimensionName(i, dimensionName);
    }

    _knob->setMinimumsAndMaximums(minimum, maximum);
    _knob->setDisplayMinimumsAndMaximums(displayMins, displayMaxs);
}

OfxStatus
OfxRGBInstance::get(double & r,
                    double & g,
                    double & b)
{
    r = _knob->getValue(0);
    g = _knob->getValue(1);
    b = _knob->getValue(2);

    return kOfxStatOK;
}

OfxStatus
OfxRGBInstance::get(OfxTime time,
                    double & r,
                    double & g,
                    double & b)
{
    r = _knob->getValueAtTime(time,0);
    g = _knob->getValueAtTime(time,1);
    b = _knob->getValueAtTime(time,2);

    return kOfxStatOK;
}

OfxStatus
OfxRGBInstance::set(double r,
                    double g,
                    double b)
{
    _knob->blockEvaluation();
    _knob->setValueFromPlugin(r,0);
    _knob->setValueFromPlugin(g,1);
    _knob->unblockEvaluation();
    _knob->setValueFromPlugin(b,2);

    return kOfxStatOK;
}

OfxStatus
OfxRGBInstance::set(OfxTime time,
                    double r,
                    double g,
                    double b)
{
    if (!getCanAnimate()) {
        qDebug() << "Attempting to call setValueAtTime on a parameter that does not have animation enabled";
        return set(r,g,b);
    }
    _knob->blockEvaluation();
    _knob->setValueAtTimeFromPlugin(time,r,0);
    _knob->setValueAtTimeFromPlugin(time,g,1);
    _knob->unblockEvaluation();
    _knob->setValueAtTimeFromPlugin(time,b,2);

    return kOfxStatOK;
}

OfxStatus
OfxRGBInstance::derive(OfxTime time,
                       double & r,
                       double & g,
                       double & b)
{
    r = _knob->getDerivativeAtTime(time,0);
    g = _knob->getDerivativeAtTime(time,1);
    b = _knob->getDerivativeAtTime(time,2);

    return kOfxStatOK;
}

OfxStatus
OfxRGBInstance::integrate(OfxTime time1,
                          OfxTime time2,
                          double &r,
                          double & g,
                          double & b)
{
    r = _knob->getIntegrateFromTimeToTime(time1, time2, 0);
    g = _knob->getIntegrateFromTimeToTime(time1, time2, 1);
    b = _knob->getIntegrateFromTimeToTime(time1, time2, 2);

    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void
OfxRGBInstance::setEnabled()
{
    _knob->setAllDimensionsEnabled( getEnabled() );
}

// callback which should set secret state as appropriate
void
OfxRGBInstance::setSecret()
{
    _knob->setSecret( getSecret() );
}

void
OfxRGBInstance::setEvaluateOnChange()
{
    _knob->setEvaluateOnChange( getEvaluateOnChange() );
}

boost::shared_ptr<KnobI>
OfxRGBInstance::getKnob() const
{
    return _knob;
}

bool
OfxRGBInstance::isAnimated(int dimension) const
{
    return _knob->isAnimated(dimension);
}

bool
OfxRGBInstance::isAnimated() const
{
    return _knob->isAnimated(0) || _knob->isAnimated(1) || _knob->isAnimated(2);
}

OfxStatus
OfxRGBInstance::getNumKeys(unsigned int &nKeys) const
{
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}

OfxStatus
OfxRGBInstance::getKeyTime(int nth,
                           OfxTime & time) const
{
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}

OfxStatus
OfxRGBInstance::getKeyIndex(OfxTime time,
                            int direction,
                            int & index) const
{
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}

OfxStatus
OfxRGBInstance::deleteKey(OfxTime time)
{
    return OfxKeyFrame::deleteKey(_knob, time);
}

OfxStatus
OfxRGBInstance::deleteAllKeys()
{
    return OfxKeyFrame::deleteAllKeys(_knob);
}

OfxStatus
OfxRGBInstance::copyFrom(const OFX::Host::Param::Instance &instance,
                         OfxTime offset,
                         const OfxRangeD* range)
{
    const OfxParamToKnob & other = dynamic_cast<const OfxParamToKnob &>(instance);

    return OfxKeyFrame::copyFrom(other.getKnob(),getKnob(), offset, range);
}

void
OfxRGBInstance::onKnobAnimationLevelChanged(int,int lvl)
{
    Natron::AnimationLevelEnum l = (Natron::AnimationLevelEnum)lvl;

    assert( l == Natron::eAnimationLevelNone || getCanAnimate() );
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::eAnimationLevelNone);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::eAnimationLevelInterpolatedValue);
}

////////////////////////// OfxDouble2DInstance /////////////////////////////////////////////////

OfxDouble2DInstance::OfxDouble2DInstance(OfxEffectInstance* node,
                                         OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::Param::Double2DInstance( descriptor,node->effectInstance() )
      , _node(node)
{
    const OFX::Host::Property::Set &properties = getProperties();
    const std::string & coordSystem = properties.getStringProperty(kOfxParamPropDefaultCoordinateSystem);
    const int dims = 2;

    _knob = Natron::createKnob<Double_Knob>(node, getParamLabel(this),dims);

    const std::string & doubleType = properties.getStringProperty(kOfxParamPropDoubleType);
    if ( (doubleType == kOfxParamDoubleTypeNormalisedXY) ||
         ( doubleType == kOfxParamDoubleTypeNormalisedXYAbsolute) ) {
        _knob->setNormalizedState(0, Double_Knob::eNormalizedStateX);
        _knob->setNormalizedState(1, Double_Knob::eNormalizedStateY);
    }
    
    bool isSpatial = doubleType == kOfxParamDoubleTypeNormalisedXY ||
    doubleType == kOfxParamDoubleTypeNormalisedXYAbsolute ||
    doubleType == kOfxParamDoubleTypeXY ||
    doubleType == kOfxParamDoubleTypeXYAbsolute;

    _knob->setSpatial(isSpatial);
    
    std::vector<double> minimum(dims);
    std::vector<double> maximum(dims);
    std::vector<double> increment(dims);
    std::vector<double> displayMins(dims);
    std::vector<double> displayMaxs(dims);
    std::vector<int> decimals(dims);
    boost::scoped_array<double> def(new double[dims]);

    // kOfxParamPropIncrement and kOfxParamPropDigits only have one dimension,
    // @see Descriptor::addNumericParamProps() in ofxhParam.cpp
    // @see gDoubleParamProps in ofxsPropertyValidation.cpp
    double incr = properties.getDoubleProperty(kOfxParamPropIncrement);
    int dig = properties.getIntProperty(kOfxParamPropDigits);
    for (int i = 0; i < dims; ++i) {
        minimum[i] = properties.getDoubleProperty(kOfxParamPropMin,i);
        maximum[i] = properties.getDoubleProperty(kOfxParamPropMax,i);
        increment[i] = incr;
        decimals[i] = dig;
        def[i] = properties.getDoubleProperty(kOfxParamPropDefault,i);

        std::string dimensionName = properties.getStringProperty(kOfxParamPropDimensionLabel,i);
        _knob->setDimensionName(i, dimensionName);
        
    }
    _knob->setMinimumsAndMaximums(minimum, maximum);
    setDisplayRange();
    _knob->setIncrement(increment);
    _knob->setDecimals(decimals);

    if (coordSystem == kOfxParamCoordinatesNormalised) {
        _knob->setDefaultValuesNormalized( dims,def.get() );
    } else {
        _knob->setDefaultValue(def[0], 0);
        _knob->setDefaultValue(def[1], 1);
    }
}

OfxStatus
OfxDouble2DInstance::get(double & x1,
                         double & x2)
{
    x1 = _knob->getValue(0);
    x2 = _knob->getValue(1);

    return kOfxStatOK;
}

OfxStatus
OfxDouble2DInstance::get(OfxTime time,
                         double & x1,
                         double & x2)
{
    x1 = _knob->getValueAtTime(time,0);
    x2 = _knob->getValueAtTime(time,1);

    return kOfxStatOK;
}

OfxStatus
OfxDouble2DInstance::set(double x1,
                         double x2)
{
    bool doEditEnd = false;

    if ( _node->isDoingInteractAction() ) {
        ignore_result(_node->effectInstance()->editBegin( getName() ));
        doEditEnd = true;
    }
    _knob->blockEvaluation();
    _knob->setValueFromPlugin(x1,0);
    _knob->unblockEvaluation();
    _knob->setValueFromPlugin(x2,1);
    if (doEditEnd) {
        _node->effectInstance()->editEnd();
    }

    return kOfxStatOK;
}

OfxStatus
OfxDouble2DInstance::set(OfxTime time,
                         double x1,
                         double x2)
{
    if (!getCanAnimate()) {
        qDebug() << "Attempting to call setValueAtTime on a parameter that does not have animation enabled";
        return set(x1,x2);
    }
    bool doEditEnd = false;

    if ( _node->isDoingInteractAction() ) {
        ignore_result(_node->effectInstance()->editBegin( getName() ));
        doEditEnd = true;
    }
    _knob->blockEvaluation();
    _knob->setValueAtTimeFromPlugin(time,x1,0);
    _knob->unblockEvaluation();
    _knob->setValueAtTimeFromPlugin(time,x2,1);
    if (doEditEnd) {
        _node->effectInstance()->editEnd();
    }

    return kOfxStatOK;
}

OfxStatus
OfxDouble2DInstance::derive(OfxTime time,
                            double &x1,
                            double & x2)
{
    x1 = _knob->getDerivativeAtTime(time,0);
    x2 = _knob->getDerivativeAtTime(time,1);

    return kOfxStatOK;
}

OfxStatus
OfxDouble2DInstance::integrate(OfxTime time1,
                               OfxTime time2,
                               double &x1,
                               double & x2)
{
    x1 = _knob->getIntegrateFromTimeToTime(time1, time2, 0);
    x2 = _knob->getIntegrateFromTimeToTime(time1, time2, 1);

    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void
OfxDouble2DInstance::setEnabled()
{
    _knob->setAllDimensionsEnabled( getEnabled() );
}

// callback which should set secret state as appropriate
void
OfxDouble2DInstance::setSecret()
{
    _knob->setSecret( getSecret() );
}

void
OfxDouble2DInstance::setEvaluateOnChange()
{
    _knob->setEvaluateOnChange( getEvaluateOnChange() );
}

void
OfxDouble2DInstance::setDisplayRange()
{
    std::vector<double> displayMins(2);
    std::vector<double> displayMaxs(2);

    displayMins[0] = getProperties().getDoubleProperty(kOfxParamPropDisplayMin,0);
    displayMins[1] = getProperties().getDoubleProperty(kOfxParamPropDisplayMin,1);
    displayMaxs[0] = getProperties().getDoubleProperty(kOfxParamPropDisplayMax,0);
    displayMaxs[1] = getProperties().getDoubleProperty(kOfxParamPropDisplayMax,1);
    _knob->setDisplayMinimumsAndMaximums(displayMins, displayMaxs);
}

void
OfxDouble2DInstance::setRange()
{
    std::vector<double> displayMins(2);
    std::vector<double> displayMaxs(2);
    
    displayMins[0] = getProperties().getDoubleProperty(kOfxParamPropMin,0);
    displayMins[1] = getProperties().getDoubleProperty(kOfxParamPropMin,1);
    displayMaxs[0] = getProperties().getDoubleProperty(kOfxParamPropMax,0);
    displayMaxs[1] = getProperties().getDoubleProperty(kOfxParamPropMax,1);
    _knob->setMinimumsAndMaximums(displayMins, displayMaxs);

}


boost::shared_ptr<KnobI>
OfxDouble2DInstance::getKnob() const
{
    return _knob;
}

bool
OfxDouble2DInstance::isAnimated(int dimension) const
{
    return _knob->isAnimated(dimension);
}

bool
OfxDouble2DInstance::isAnimated() const
{
    return _knob->isAnimated(0) || _knob->isAnimated(1);
}

OfxStatus
OfxDouble2DInstance::getNumKeys(unsigned int &nKeys) const
{
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}

OfxStatus
OfxDouble2DInstance::getKeyTime(int nth,
                                OfxTime & time) const
{
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}

OfxStatus
OfxDouble2DInstance::getKeyIndex(OfxTime time,
                                 int direction,
                                 int & index) const
{
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}

OfxStatus
OfxDouble2DInstance::deleteKey(OfxTime time)
{
    return OfxKeyFrame::deleteKey(_knob, time);
}

OfxStatus
OfxDouble2DInstance::deleteAllKeys()
{
    return OfxKeyFrame::deleteAllKeys(_knob);
}

OfxStatus
OfxDouble2DInstance::copyFrom(const OFX::Host::Param::Instance &instance,
                              OfxTime offset,
                              const OfxRangeD* range)
{
    const OfxParamToKnob & other = dynamic_cast<const OfxParamToKnob &>(instance);

    return OfxKeyFrame::copyFrom(other.getKnob(), getKnob(), offset, range);
}

void
OfxDouble2DInstance::onKnobAnimationLevelChanged(int,int lvl)
{
    Natron::AnimationLevelEnum l = (Natron::AnimationLevelEnum)lvl;

    assert( l == Natron::eAnimationLevelNone || getCanAnimate() );
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::eAnimationLevelNone);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::eAnimationLevelInterpolatedValue);
}

////////////////////////// OfxInteger2DInstance /////////////////////////////////////////////////

OfxInteger2DInstance::OfxInteger2DInstance(OfxEffectInstance* node,
                                           OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::Param::Integer2DInstance( descriptor,node->effectInstance() )
      , _node(node)
{
    const int dims = 2;
    const OFX::Host::Property::Set &properties = getProperties();


    _knob = Natron::createKnob<Int_Knob>(node, getParamLabel(this), dims);

    std::vector<int> minimum(dims);
    std::vector<int> maximum(dims);
    std::vector<int> increment(dims);
    std::vector<int> displayMins(dims);
    std::vector<int> displayMaxs(dims);
    boost::scoped_array<int> def(new int[dims]);

    for (int i = 0; i < dims; ++i) {
        minimum[i] = properties.getIntProperty(kOfxParamPropMin,i);
        displayMins[i] = properties.getIntProperty(kOfxParamPropDisplayMin,i);
        displayMaxs[i] = properties.getIntProperty(kOfxParamPropDisplayMax,i);
        maximum[i] = properties.getIntProperty(kOfxParamPropMax,i);
        increment[i] = 1; // kOfxParamPropIncrement only exists for Double
        def[i] = properties.getIntProperty(kOfxParamPropDefault,i);
        std::string dimensionName = properties.getStringProperty(kOfxParamPropDimensionLabel,i);
        _knob->setDimensionName(i, dimensionName);
    }

    _knob->setMinimumsAndMaximums(minimum, maximum);
    _knob->setIncrement(increment);
    _knob->setDisplayMinimumsAndMaximums(displayMins, displayMaxs);
    _knob->setDefaultValue(def[0], 0);
    _knob->setDefaultValue(def[1], 1);
}

OfxStatus
OfxInteger2DInstance::get(int & x1,
                          int & x2)
{
    x1 = _knob->getValue(0);
    x2 = _knob->getValue(1);

    return kOfxStatOK;
}

OfxStatus
OfxInteger2DInstance::get(OfxTime time,
                          int & x1,
                          int & x2)
{
    x1 = _knob->getValueAtTime(time,0);
    x2 = _knob->getValueAtTime(time,1);

    return kOfxStatOK;
}

OfxStatus
OfxInteger2DInstance::set(int x1,
                          int x2)
{
    bool doEditEnd = false;

    if ( _node->isDoingInteractAction() ) {
        ignore_result(_node->effectInstance()->editBegin( getName() ));
        doEditEnd = true;
    }

    _knob->blockEvaluation();
    _knob->setValueFromPlugin(x1,0);
    _knob->unblockEvaluation();
    _knob->setValueFromPlugin(x2,1);

    if (doEditEnd) {
        ignore_result(_node->effectInstance()->editEnd());
    }

    return kOfxStatOK;
}

OfxStatus
OfxInteger2DInstance::set(OfxTime time,
                          int x1,
                          int x2)
{
    if (!getCanAnimate()) {
        qDebug() << "Attempting to call setValueAtTime on a parameter that does not have animation enabled";
        return set(x1,x2);
    }
    bool doEditEnd = false;

    if ( _node->isDoingInteractAction() ) {
        ignore_result(_node->effectInstance()->editBegin( getName() ));
        doEditEnd = true;
    }
    _knob->blockEvaluation();
    _knob->setValueAtTimeFromPlugin(time,x1,0);
    _knob->unblockEvaluation();
    _knob->setValueAtTimeFromPlugin(time,x2,1);

    if (doEditEnd) {
        ignore_result(_node->effectInstance()->editEnd());
    }

    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void
OfxInteger2DInstance::setEnabled()
{
    _knob->setAllDimensionsEnabled( getEnabled() );
}

// callback which should set secret state as appropriate
void
OfxInteger2DInstance::setSecret()
{
    _knob->setSecret( getSecret() );
}

void
OfxInteger2DInstance::setDisplayRange()
{
    std::vector<int> displayMins(2);
    std::vector<int> displayMaxs(2);
    
    displayMins[0] = getProperties().getIntProperty(kOfxParamPropDisplayMin,0);
    displayMins[1] = getProperties().getIntProperty(kOfxParamPropDisplayMin,1);
    displayMaxs[0] = getProperties().getIntProperty(kOfxParamPropDisplayMax,0);
    displayMaxs[1] = getProperties().getIntProperty(kOfxParamPropDisplayMax,1);
    _knob->setDisplayMinimumsAndMaximums(displayMins, displayMaxs);
    
}

void
OfxInteger2DInstance::setRange()
{
    std::vector<int> displayMins(2);
    std::vector<int> displayMaxs(2);
    
    displayMins[0] = getProperties().getIntProperty(kOfxParamPropMin,0);
    displayMins[1] = getProperties().getIntProperty(kOfxParamPropMin,1);
    displayMaxs[0] = getProperties().getIntProperty(kOfxParamPropMax,0);
    displayMaxs[1] = getProperties().getIntProperty(kOfxParamPropMax,1);
    _knob->setMinimumsAndMaximums(displayMins, displayMaxs);
    
}

void
OfxInteger2DInstance::setEvaluateOnChange()
{
    _knob->setEvaluateOnChange( getEvaluateOnChange() );
}

boost::shared_ptr<KnobI>
OfxInteger2DInstance::getKnob() const
{
    return _knob;
}

OfxStatus
OfxInteger2DInstance::getNumKeys(unsigned int &nKeys) const
{
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}

OfxStatus
OfxInteger2DInstance::getKeyTime(int nth,
                                 OfxTime & time) const
{
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}

OfxStatus
OfxInteger2DInstance::getKeyIndex(OfxTime time,
                                  int direction,
                                  int & index) const
{
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}

OfxStatus
OfxInteger2DInstance::deleteKey(OfxTime time)
{
    return OfxKeyFrame::deleteKey(_knob, time);
}

OfxStatus
OfxInteger2DInstance::deleteAllKeys()
{
    return OfxKeyFrame::deleteAllKeys(_knob);
}

OfxStatus
OfxInteger2DInstance::copyFrom(const OFX::Host::Param::Instance &instance,
                               OfxTime offset,
                               const OfxRangeD* range)
{
    const OfxParamToKnob & other = dynamic_cast<const OfxParamToKnob &>(instance);

    return OfxKeyFrame::copyFrom(other.getKnob(), getKnob(), offset, range);
}

void
OfxInteger2DInstance::onKnobAnimationLevelChanged(int,int lvl)
{
    Natron::AnimationLevelEnum l = (Natron::AnimationLevelEnum)lvl;

    assert( l == Natron::eAnimationLevelNone || getCanAnimate() );
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::eAnimationLevelNone);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::eAnimationLevelInterpolatedValue);
}

////////////////////////// OfxDouble3DInstance /////////////////////////////////////////////////

OfxDouble3DInstance::OfxDouble3DInstance(OfxEffectInstance* node,
                                         OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::Param::Double3DInstance( descriptor,node->effectInstance() )
      , _node(node)
{
    const int dims = 3;
    const OFX::Host::Property::Set &properties = getProperties();


    _knob = Natron::createKnob<Double_Knob>(node, getParamLabel(this),dims);

    std::vector<double> minimum(dims);
    std::vector<double> maximum(dims);
    std::vector<double> increment(dims);
    std::vector<double> displayMins(dims);
    std::vector<double> displayMaxs(dims);
    std::vector<int> decimals(dims);
    boost::scoped_array<double> def(new double[dims]);

    // kOfxParamPropIncrement and kOfxParamPropDigits only have one dimension,
    // @see Descriptor::addNumericParamProps() in ofxhParam.cpp
    // @see gDoubleParamProps in ofxsPropertyValidation.cpp
    double incr = properties.getDoubleProperty(kOfxParamPropIncrement);
    int dig = properties.getIntProperty(kOfxParamPropDigits);
    for (int i = 0; i < dims; ++i) {
        minimum[i] = properties.getDoubleProperty(kOfxParamPropMin,i);
        displayMins[i] = properties.getDoubleProperty(kOfxParamPropDisplayMin,i);
        displayMaxs[i] = properties.getDoubleProperty(kOfxParamPropDisplayMax,i);
        maximum[i] = properties.getDoubleProperty(kOfxParamPropMax,i);
        increment[i] = incr;
        decimals[i] = dig;
        def[i] = properties.getDoubleProperty(kOfxParamPropDefault,i);
        std::string dimensionName = properties.getStringProperty(kOfxParamPropDimensionLabel,i);
        _knob->setDimensionName(i, dimensionName);
    }

    _knob->setMinimumsAndMaximums(minimum, maximum);
    _knob->setIncrement(increment);
    _knob->setDisplayMinimumsAndMaximums(displayMins, displayMaxs);
    _knob->setDecimals(decimals);
    _knob->setDefaultValue(def[0],0);
    _knob->setDefaultValue(def[1],1);
    _knob->setDefaultValue(def[2],2);
}

OfxStatus
OfxDouble3DInstance::get(double & x1,
                         double & x2,
                         double & x3)
{
    x1 = _knob->getValue(0);
    x2 = _knob->getValue(1);
    x3 = _knob->getValue(2);

    return kOfxStatOK;
}

OfxStatus
OfxDouble3DInstance::get(OfxTime time,
                         double & x1,
                         double & x2,
                         double & x3)
{
    x1 = _knob->getValueAtTime(time,0);
    x2 = _knob->getValueAtTime(time,1);
    x3 = _knob->getValueAtTime(time,2);

    return kOfxStatOK;
}

OfxStatus
OfxDouble3DInstance::set(double x1,
                         double x2,
                         double x3)
{
    
    bool doEditEnd = false;

    if ( _node->isDoingInteractAction() ) {
        ignore_result(_node->effectInstance()->editBegin( getName() ));
        doEditEnd = true;
    }
    _knob->blockEvaluation();
    _knob->setValueFromPlugin(x1,0);
    _knob->setValueFromPlugin(x2,1);
    _knob->unblockEvaluation();
    _knob->setValueFromPlugin(x3,2);
    if (doEditEnd) {
        ignore_result(_node->effectInstance()->editEnd());
    }

    return kOfxStatOK;
}

OfxStatus
OfxDouble3DInstance::set(OfxTime time,
                         double x1,
                         double x2,
                         double x3)
{
    if (!getCanAnimate()) {
        qDebug() << "Attempting to call setValueAtTime on a parameter that does not have animation enabled";
        return set(x1,x2,x3);
    }
    bool doEditEnd = false;

    if ( _node->isDoingInteractAction() ) {
        ignore_result(_node->effectInstance()->editBegin( getName() ));
        doEditEnd = true;
    }
    _knob->blockEvaluation();
    _knob->setValueAtTimeFromPlugin(time,x1,0);
    _knob->setValueAtTimeFromPlugin(time,x2,1);
    _knob->unblockEvaluation();
    _knob->setValueAtTimeFromPlugin(time,x3,2);
    if (doEditEnd) {
        ignore_result(_node->effectInstance()->editEnd());
    }

    return kOfxStatOK;
}

OfxStatus
OfxDouble3DInstance::derive(OfxTime time,
                            double & x1,
                            double & x2,
                            double & x3)
{
    x1 = _knob->getDerivativeAtTime(time,0);
    x2 = _knob->getDerivativeAtTime(time,1);
    x3 = _knob->getDerivativeAtTime(time,2);

    return kOfxStatOK;
}

OfxStatus
OfxDouble3DInstance::integrate(OfxTime time1,
                               OfxTime time2,
                               double &x1,
                               double & x2,
                               double & x3)
{
    x1 = _knob->getIntegrateFromTimeToTime(time1, time2, 0);
    x2 = _knob->getIntegrateFromTimeToTime(time1, time2, 1);
    x3 = _knob->getIntegrateFromTimeToTime(time1, time2, 2);

    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void
OfxDouble3DInstance::setEnabled()
{
    _knob->setAllDimensionsEnabled( getEnabled() );
}

// callback which should set secret state as appropriate
void
OfxDouble3DInstance::setSecret()
{
    _knob->setSecret( getSecret() );
}

void
OfxDouble3DInstance::setDisplayRange()
{
    std::vector<double> displayMins(3);
    std::vector<double> displayMaxs(3);
    
    
    displayMins[0] = getProperties().getDoubleProperty(kOfxParamPropDisplayMin,0);
    displayMins[1] = getProperties().getDoubleProperty(kOfxParamPropDisplayMin,1);
    displayMins[2] = getProperties().getDoubleProperty(kOfxParamPropDisplayMin,2);
    displayMaxs[0] = getProperties().getDoubleProperty(kOfxParamPropDisplayMax,0);
    displayMaxs[1] = getProperties().getDoubleProperty(kOfxParamPropDisplayMax,1);
    displayMaxs[2] = getProperties().getDoubleProperty(kOfxParamPropDisplayMax,2);
    _knob->setDisplayMinimumsAndMaximums(displayMins, displayMaxs);
}

void
OfxDouble3DInstance::setRange()
{
    std::vector<double> displayMins(3);
    std::vector<double> displayMaxs(3);
    
    displayMins[0] = getProperties().getDoubleProperty(kOfxParamPropMin,0);
    displayMins[1] = getProperties().getDoubleProperty(kOfxParamPropMin,1);
    displayMins[2] = getProperties().getDoubleProperty(kOfxParamPropMin,2);
    displayMaxs[0] = getProperties().getDoubleProperty(kOfxParamPropMax,0);
    displayMaxs[1] = getProperties().getDoubleProperty(kOfxParamPropMax,1);
    displayMaxs[2] = getProperties().getDoubleProperty(kOfxParamPropMax,2);
    _knob->setMinimumsAndMaximums(displayMins, displayMaxs);
    
}

void
OfxDouble3DInstance::setEvaluateOnChange()
{
    _knob->setEvaluateOnChange( getEvaluateOnChange() );
}

boost::shared_ptr<KnobI>
OfxDouble3DInstance::getKnob() const
{
    return _knob;
}

bool
OfxDouble3DInstance::isAnimated(int dimension) const
{
    return _knob->isAnimated(dimension);
}

bool
OfxDouble3DInstance::isAnimated() const
{
    return _knob->isAnimated(0) || _knob->isAnimated(1) || _knob->isAnimated(2);
}

OfxStatus
OfxDouble3DInstance::getNumKeys(unsigned int &nKeys) const
{
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}

OfxStatus
OfxDouble3DInstance::getKeyTime(int nth,
                                OfxTime & time) const
{
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}

OfxStatus
OfxDouble3DInstance::getKeyIndex(OfxTime time,
                                 int direction,
                                 int & index) const
{
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}

OfxStatus
OfxDouble3DInstance::deleteKey(OfxTime time)
{
    return OfxKeyFrame::deleteKey(_knob, time);
}

OfxStatus
OfxDouble3DInstance::deleteAllKeys()
{
    return OfxKeyFrame::deleteAllKeys(_knob);
}

OfxStatus
OfxDouble3DInstance::copyFrom(const OFX::Host::Param::Instance &instance,
                              OfxTime offset,
                              const OfxRangeD* range)
{
    const OfxParamToKnob & other = dynamic_cast<const OfxParamToKnob &>(instance);

    return OfxKeyFrame::copyFrom(other.getKnob(), getKnob(), offset, range);
}

void
OfxDouble3DInstance::onKnobAnimationLevelChanged(int,int lvl)
{
    Natron::AnimationLevelEnum l = (Natron::AnimationLevelEnum)lvl;

    assert( l == Natron::eAnimationLevelNone || getCanAnimate() );
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::eAnimationLevelNone);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::eAnimationLevelInterpolatedValue);
}

////////////////////////// OfxInteger3DInstance /////////////////////////////////////////////////

OfxInteger3DInstance::OfxInteger3DInstance(OfxEffectInstance*node,
                                           OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::Param::Integer3DInstance( descriptor,node->effectInstance() )
      , _node(node)
{
    const int dims = 3;
    const OFX::Host::Property::Set &properties = getProperties();


    _knob = Natron::createKnob<Int_Knob>(node, getParamLabel(this), dims);

    std::vector<int> minimum(dims);
    std::vector<int> maximum(dims);
    std::vector<int> increment(dims);
    std::vector<int> displayMins(dims);
    std::vector<int> displayMaxs(dims);
    boost::scoped_array<int> def(new int[dims]);

    for (int i = 0; i < dims; ++i) {
        minimum[i] = properties.getIntProperty(kOfxParamPropMin,i);
        displayMins[i] = properties.getIntProperty(kOfxParamPropDisplayMin,i);
        displayMaxs[i] = properties.getIntProperty(kOfxParamPropDisplayMax,i);
        maximum[i] = properties.getIntProperty(kOfxParamPropMax,i);
        int incr = properties.getIntProperty(kOfxParamPropIncrement,i);
        increment[i] = incr != 0 ?  incr : 1;
        def[i] = properties.getIntProperty(kOfxParamPropDefault,i);

        std::string dimensionName = properties.getStringProperty(kOfxParamPropDimensionLabel,i);
        _knob->setDimensionName(i, dimensionName);
    }

    _knob->setMinimumsAndMaximums(minimum, maximum);
    _knob->setIncrement(increment);
    _knob->setDisplayMinimumsAndMaximums(displayMins, displayMaxs);
    _knob->setDefaultValue(def[0],0);
    _knob->setDefaultValue(def[1],1);
    _knob->setDefaultValue(def[2],2);
}

OfxStatus
OfxInteger3DInstance::get(int & x1,
                          int & x2,
                          int & x3)
{
    x1 = _knob->getValue(0);
    x2 = _knob->getValue(1);
    x3 = _knob->getValue(2);

    return kOfxStatOK;
}

OfxStatus
OfxInteger3DInstance::get(OfxTime time,
                          int & x1,
                          int & x2,
                          int & x3)
{
    x1 = _knob->getValueAtTime(time,0);
    x2 = _knob->getValueAtTime(time,1);
    x3 = _knob->getValueAtTime(time,2);

    return kOfxStatOK;
}

OfxStatus
OfxInteger3DInstance::set(int x1,
                          int x2,
                          int x3)
{
   
    bool doEditEnd = false;

    if ( _node->isDoingInteractAction() ) {
        ignore_result(_node->effectInstance()->editBegin( getName() ));
        doEditEnd = true;
    }
    _knob->blockEvaluation();
    _knob->setValueFromPlugin(x1,0);
    _knob->setValueFromPlugin(x2,1);
    _knob->unblockEvaluation();
    _knob->setValueFromPlugin(x3,2);
    if (doEditEnd) {
        ignore_result(_node->effectInstance()->editEnd());
    }

    return kOfxStatOK;
}

OfxStatus
OfxInteger3DInstance::set(OfxTime time,
                          int x1,
                          int x2,
                          int x3)
{
    if (!getCanAnimate()) {
        qDebug() << "Attempting to call setValueAtTime on a parameter that does not have animation enabled";
        return set(x1,x2,x3);
    }
    bool doEditEnd = false;

    if ( _node->isDoingInteractAction() ) {
        ignore_result(_node->effectInstance()->editBegin( getName() ));
        doEditEnd = true;
    }
    _knob->blockEvaluation();
    _knob->setValueAtTimeFromPlugin(time,x1,0);
    _knob->setValueAtTimeFromPlugin(time,x2,1);
    _knob->unblockEvaluation();
    _knob->setValueAtTimeFromPlugin(time,x3,2);
    if (doEditEnd) {
        ignore_result(_node->effectInstance()->editEnd());
    }

    return kOfxStatOK;
}

// callback which should set enabled state as appropriate
void
OfxInteger3DInstance::setEnabled()
{
    _knob->setAllDimensionsEnabled( getEnabled() );
}

// callback which should set secret state as appropriate
void
OfxInteger3DInstance::setSecret()
{
    _knob->setSecret( getSecret() );
}

void
OfxInteger3DInstance::setDisplayRange()
{
    std::vector<int> displayMins(3);
    std::vector<int> displayMaxs(3);
    
    displayMins[0] = getProperties().getIntProperty(kOfxParamPropDisplayMin,0);
    displayMins[1] = getProperties().getIntProperty(kOfxParamPropDisplayMin,1);
    displayMins[2] = getProperties().getIntProperty(kOfxParamPropDisplayMin,2);
    displayMaxs[0] = getProperties().getIntProperty(kOfxParamPropDisplayMax,0);
    displayMaxs[1] = getProperties().getIntProperty(kOfxParamPropDisplayMax,1);
    displayMaxs[2] = getProperties().getIntProperty(kOfxParamPropDisplayMax,2);
    _knob->setDisplayMinimumsAndMaximums(displayMins, displayMaxs);
    
}

void
OfxInteger3DInstance::setRange()
{
    std::vector<int> displayMins(3);
    std::vector<int> displayMaxs(3);
    
    displayMins[0] = getProperties().getIntProperty(kOfxParamPropMin,0);
    displayMins[1] = getProperties().getIntProperty(kOfxParamPropMin,1);
    displayMins[2] = getProperties().getIntProperty(kOfxParamPropMin,2);
    displayMaxs[0] = getProperties().getIntProperty(kOfxParamPropMax,0);
    displayMaxs[1] = getProperties().getIntProperty(kOfxParamPropMax,1);
    displayMaxs[2] = getProperties().getIntProperty(kOfxParamPropMax,2);
    _knob->setMinimumsAndMaximums(displayMins, displayMaxs);
    
}

void
OfxInteger3DInstance::setEvaluateOnChange()
{
    _knob->setEvaluateOnChange( getEvaluateOnChange() );
}

boost::shared_ptr<KnobI>
OfxInteger3DInstance::getKnob() const
{
    return _knob;
}

OfxStatus
OfxInteger3DInstance::getNumKeys(unsigned int &nKeys) const
{
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}

OfxStatus
OfxInteger3DInstance::getKeyTime(int nth,
                                 OfxTime & time) const
{
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}

OfxStatus
OfxInteger3DInstance::getKeyIndex(OfxTime time,
                                  int direction,
                                  int & index) const
{
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}

OfxStatus
OfxInteger3DInstance::deleteKey(OfxTime time)
{
    return OfxKeyFrame::deleteKey(_knob, time);
}

OfxStatus
OfxInteger3DInstance::deleteAllKeys()
{
    return OfxKeyFrame::deleteAllKeys(_knob);
}

OfxStatus
OfxInteger3DInstance::copyFrom(const OFX::Host::Param::Instance &instance,
                               OfxTime offset,
                               const OfxRangeD* range)
{
    const OfxParamToKnob & other = dynamic_cast<const OfxParamToKnob &>(instance);

    return OfxKeyFrame::copyFrom(other.getKnob(), getKnob(), offset, range);
}

void
OfxInteger3DInstance::onKnobAnimationLevelChanged(int,int lvl)
{
    Natron::AnimationLevelEnum l = (Natron::AnimationLevelEnum)lvl;

    assert( l == Natron::eAnimationLevelNone || getCanAnimate() );
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::eAnimationLevelNone);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::eAnimationLevelInterpolatedValue);
}

////////////////////////// OfxGroupInstance /////////////////////////////////////////////////

OfxGroupInstance::OfxGroupInstance(OfxEffectInstance* node,
                                   OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::Param::GroupInstance( descriptor,node->effectInstance() )
      , _groupKnob()
{
    const OFX::Host::Property::Set &properties = getProperties();
    int isTab = properties.getIntProperty(kFnOfxParamPropGroupIsTab);

    _groupKnob = Natron::createKnob<Group_Knob>( node, getParamLabel(this) );
    int opened = properties.getIntProperty(kOfxParamPropGroupOpen);
    if (isTab) {
        _groupKnob->setAsTab();
    }
    _groupKnob->setDefaultValue(opened,0);
}

void
OfxGroupInstance::addKnob(boost::shared_ptr<KnobI> k)
{
    _groupKnob->addKnob(k);
}

boost::shared_ptr<KnobI> OfxGroupInstance::getKnob() const
{
    return _groupKnob;
}

void
OfxGroupInstance::setEnabled()
{
    _groupKnob->setAllDimensionsEnabled( getEnabled() );
}

// callback which should set secret state as appropriate
void
OfxGroupInstance::setSecret()
{
    _groupKnob->setSecret( getSecret() );
}

////////////////////////// OfxPageInstance /////////////////////////////////////////////////


OfxPageInstance::OfxPageInstance(OfxEffectInstance* node,
                                 OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::Param::PageInstance( descriptor,node->effectInstance() )
      , _pageKnob()
{
    _pageKnob = Natron::createKnob<Page_Knob>( node, getParamLabel(this) );
}

// callback which should set enabled state as appropriate
void
OfxPageInstance::setEnabled()
{
    _pageKnob->setAllDimensionsEnabled( getEnabled() );
}

// callback which should set secret state as appropriate
void
OfxPageInstance::setSecret()
{
    _pageKnob->setAllDimensionsEnabled( getSecret() );
}

boost::shared_ptr<KnobI> OfxPageInstance::getKnob() const
{
    return _pageKnob;
}

////////////////////////// OfxStringInstance /////////////////////////////////////////////////


OfxStringInstance::OfxStringInstance(OfxEffectInstance* node,
                                     OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::Param::StringInstance( descriptor,node->effectInstance() )
      , _node(node)
      , _fileKnob()
      , _outputFileKnob()
      , _stringKnob()
      , _pathKnob()
{
    const OFX::Host::Property::Set &properties = getProperties();
    std::string mode = properties.getStringProperty(kOfxParamPropStringMode);
    bool richText = mode == kOfxParamStringIsRichTextFormat;

    if (mode == kOfxParamStringIsFilePath) {
        int fileIsImage = ((node->isReader() ||
                            node->isWriter() ) &&
                           (getScriptName() == kOfxImageEffectFileParamName ||
                            getScriptName() == kOfxImageEffectProxyParamName));
        int fileIsOutput = !properties.getIntProperty(kOfxParamPropStringFilePathExists);
        int filePathSupportsImageSequences = getCanAnimate();


        if (!fileIsOutput) {
            _fileKnob = Natron::createKnob<File_Knob>( node, getParamLabel(this) );
            if (fileIsImage) {
                _fileKnob->setAsInputImage();
            }
            if (!filePathSupportsImageSequences) {
                _fileKnob->setAnimationEnabled(false);
            }
        } else {
            _outputFileKnob = Natron::createKnob<OutputFile_Knob>( node, getParamLabel(this) );
            if (fileIsImage) {
                _outputFileKnob->setAsOutputImageFile();
            } else {
                _outputFileKnob->turnOffSequences();
            }
            if (!filePathSupportsImageSequences) {
                _outputFileKnob->setAnimationEnabled(false);
            }
        }

    } else if (mode == kOfxParamStringIsDirectoryPath) {
        _pathKnob = Natron::createKnob<Path_Knob>( node, getParamLabel(this) );
        _pathKnob->setMultiPath(false);
        
    } else if ( (mode == kOfxParamStringIsSingleLine) || (mode == kOfxParamStringIsLabel) || (mode == kOfxParamStringIsMultiLine) || richText ) {
        _stringKnob = Natron::createKnob<String_Knob>( node, getParamLabel(this) );
        if (mode == kOfxParamStringIsLabel) {
            _stringKnob->setAllDimensionsEnabled(false);
            _stringKnob->setAsLabel();
        }
        if ( (mode == kOfxParamStringIsMultiLine) || richText ) {
            ///only QTextArea support rich text anyway
            _stringKnob->setUsesRichText(richText);
            _stringKnob->setAsMultiLine();
        }
    }
    std::string defaultVal = properties.getStringProperty(kOfxParamPropDefault).c_str();
    if ( !defaultVal.empty() ) {
        if (_fileKnob) {
            projectEnvVar_setProxy(defaultVal);
            _fileKnob->setDefaultValue(defaultVal,0);
        } else if (_outputFileKnob) {
            projectEnvVar_setProxy(defaultVal);
            _outputFileKnob->setDefaultValue(defaultVal,0);
        } else if (_stringKnob) {
            _stringKnob->setDefaultValue(defaultVal,0);
        } else if (_pathKnob) {
            projectEnvVar_setProxy(defaultVal);
            _pathKnob->setDefaultValue(defaultVal,0);
        }
    }
}


void
OfxStringInstance::projectEnvVar_getProxy(std::string& str) const
{
    _node->getApp()->getProject()->canonicalizePath(str);
}

void
OfxStringInstance::projectEnvVar_setProxy(std::string& str) const
{
    _node->getApp()->getProject()->simplifyPath(str);
   
}

OfxStatus
OfxStringInstance::get(std::string &str)
{
    assert( _node->effectInstance() );
    int currentFrame = _node->getApp()->getTimeLine()->currentFrame();
    if (_fileKnob) {
        str = _fileKnob->getFileName(currentFrame);
        projectEnvVar_getProxy(str);
    } else if (_outputFileKnob) {
        str = _outputFileKnob->generateFileNameAtTime(currentFrame).toStdString();
        projectEnvVar_getProxy(str);
    } else if (_stringKnob) {
        str = _stringKnob->getValueAtTime(currentFrame,0);
    } else if (_pathKnob) {
        str = _pathKnob->getValue();
        projectEnvVar_getProxy(str);
    }

    return kOfxStatOK;
}

OfxStatus
OfxStringInstance::get(OfxTime time,
                       std::string & str)
{
    assert( _node->effectInstance() );
    if (_fileKnob) {
        str = _fileKnob->getFileName(std::floor(time + 0.5));
        projectEnvVar_getProxy(str);
    } else if (_outputFileKnob) {
        str = _outputFileKnob->generateFileNameAtTime(time).toStdString();
        projectEnvVar_getProxy(str);
    } else if (_stringKnob) {
        str = _stringKnob->getValueAtTime(std::floor(time + 0.5), 0);
    } else if (_pathKnob) {
        str = _pathKnob->getValue();
        projectEnvVar_getProxy(str);
    }

    return kOfxStatOK;
}

OfxStatus
OfxStringInstance::set(const char* str)
{
    if (_fileKnob) {
        std::string s(str);
        projectEnvVar_setProxy(s);
        _fileKnob->setValueFromPlugin(s,0);
    }
    if (_outputFileKnob) {
        std::string s(str);
        projectEnvVar_setProxy(s);
        _outputFileKnob->setValueFromPlugin(s,0);
    }
    if (_stringKnob) {
        _stringKnob->setValueFromPlugin(str,0);
    }
    if (_pathKnob) {
        std::string s(str);
        projectEnvVar_setProxy(s);
        _pathKnob->setValueFromPlugin(s,0);
    }

    return kOfxStatOK;
}

OfxStatus
OfxStringInstance::set(OfxTime time,
                       const char* str)
{
    if (!getCanAnimate()) {
        qDebug() << "Attempting to call setValueAtTime on a parameter that does not have animation enabled";
        return set(str);
    }
    assert( !String_Knob::canAnimateStatic() );
    if (_fileKnob) {
        std::string s(str);
        projectEnvVar_setProxy(s);
        _fileKnob->setValueAtTimeFromPlugin(time,s,0);
    }
    if (_outputFileKnob) {
        std::string s(str);
        projectEnvVar_setProxy(s);
        _outputFileKnob->setValueAtTimeFromPlugin(time,s,0);
    }
    if (_stringKnob) {
        _stringKnob->setValueAtTimeFromPlugin( (int)time,str,0 );
    }
    if (_pathKnob) {
        std::string s(str);
        projectEnvVar_setProxy(s);
        _pathKnob->setValueAtTimeFromPlugin(time,s,0);
    }

    return kOfxStatOK;
}

OfxStatus
OfxStringInstance::getV(va_list arg)
{
    const char **value = va_arg(arg, const char **);
    OfxStatus stat;

    std::string& tls = _localString.localData();
    stat = get(tls);

    *value = tls.c_str();

    return stat;
}

OfxStatus
OfxStringInstance::getV(OfxTime time,
                        va_list arg)
{
    const char **value = va_arg(arg, const char **);
    OfxStatus stat;

    std::string& tls = _localString.localData();
    stat = get( time,tls);
    *value = tls.c_str();

    return stat;
}

boost::shared_ptr<KnobI>
OfxStringInstance::getKnob() const
{
    if (_fileKnob) {
        return _fileKnob;
    }
    if (_outputFileKnob) {
        return _outputFileKnob;
    }
    if (_stringKnob) {
        return _stringKnob;
    }
    if (_pathKnob) {
        return _pathKnob;
    }

    return boost::shared_ptr<KnobI>();
}

// callback which should set enabled state as appropriate
void
OfxStringInstance::setEnabled()
{
    if (_fileKnob) {
        _fileKnob->setAllDimensionsEnabled( getEnabled() );
    }
    if (_outputFileKnob) {
        _outputFileKnob->setAllDimensionsEnabled( getEnabled() );
    }
    if (_stringKnob) {
        _stringKnob->setAllDimensionsEnabled( getEnabled() );
    }
    if (_pathKnob) {
        _pathKnob->setAllDimensionsEnabled( getEnabled() );
    }
}

// callback which should set secret state as appropriate
void
OfxStringInstance::setSecret()
{
    if (_fileKnob) {
        _fileKnob->setSecret( getSecret() );
    }
    if (_outputFileKnob) {
        _outputFileKnob->setSecret( getSecret() );
    }
    if (_stringKnob) {
        _stringKnob->setSecret( getSecret() );
    }
    if (_pathKnob) {
        _pathKnob->setSecret( getSecret() );
    }
}

void
OfxStringInstance::setEvaluateOnChange()
{
    if (_fileKnob) {
        _fileKnob->setEvaluateOnChange( getEvaluateOnChange() );
    }
    if (_outputFileKnob) {
        _outputFileKnob->setEvaluateOnChange( getEvaluateOnChange() );
    }
    if (_stringKnob) {
        _stringKnob->setEvaluateOnChange( getEvaluateOnChange() );
    }
    if (_pathKnob) {
        _pathKnob->setEvaluateOnChange( getEvaluateOnChange() );
    }
}

OfxStatus
OfxStringInstance::getNumKeys(unsigned int &nKeys) const
{
    boost::shared_ptr<KnobI> knob;

    if (_stringKnob) {
        knob = boost::dynamic_pointer_cast<KnobI>(_stringKnob);
    } else if (_fileKnob) {
        knob = boost::dynamic_pointer_cast<KnobI>(_fileKnob);
    } else {
        return nKeys = 0;
    }

    return OfxKeyFrame::getNumKeys(knob, nKeys);
}

OfxStatus
OfxStringInstance::getKeyTime(int nth,
                              OfxTime & time) const
{
    boost::shared_ptr<KnobI> knob;

    if (_stringKnob) {
        knob = boost::dynamic_pointer_cast<KnobI>(_stringKnob);
    } else if (_fileKnob) {
        knob = boost::dynamic_pointer_cast<KnobI>(_fileKnob);
    } else {
        return kOfxStatErrBadIndex;
    }

    return OfxKeyFrame::getKeyTime(knob, nth, time);
}

OfxStatus
OfxStringInstance::getKeyIndex(OfxTime time,
                               int direction,
                               int & index) const
{
    boost::shared_ptr<KnobI> knob;

    if (_stringKnob) {
        knob = boost::dynamic_pointer_cast<KnobI>(_stringKnob);
    } else if (_fileKnob) {
        knob = boost::dynamic_pointer_cast<KnobI>(_fileKnob);
    } else {
        return kOfxStatFailed;
    }

    return OfxKeyFrame::getKeyIndex(knob, time, direction, index);
}

OfxStatus
OfxStringInstance::deleteKey(OfxTime time)
{
    boost::shared_ptr<KnobI> knob;

    if (_stringKnob) {
        knob = boost::dynamic_pointer_cast<KnobI>(_stringKnob);
    } else if (_fileKnob) {
        knob = boost::dynamic_pointer_cast<KnobI>(_fileKnob);
    } else {
        return kOfxStatErrBadIndex;
    }

    return OfxKeyFrame::deleteKey(knob, time);
}

OfxStatus
OfxStringInstance::deleteAllKeys()
{
    boost::shared_ptr<KnobI> knob;

    if (_stringKnob) {
        knob = boost::dynamic_pointer_cast<KnobI>(_stringKnob);
    } else if (_fileKnob) {
        knob = boost::dynamic_pointer_cast<KnobI>(_fileKnob);
    } else {
        return kOfxStatOK;
    }

    return OfxKeyFrame::deleteAllKeys(knob);
}

OfxStatus
OfxStringInstance::copyFrom(const OFX::Host::Param::Instance &instance,
                            OfxTime offset,
                            const OfxRangeD* range)
{
    const OfxParamToKnob & other = dynamic_cast<const OfxParamToKnob &>(instance);

    return OfxKeyFrame::copyFrom(other.getKnob(), getKnob(), offset, range);
}

void
OfxStringInstance::onKnobAnimationLevelChanged(int,int lvl)
{
    Natron::AnimationLevelEnum l = (Natron::AnimationLevelEnum)lvl;

    ///This assert might crash Natron when reading a project made with a version
    ///of Natron prior to 0.96 when file params still had keyframes.
    //assert(l == Natron::eAnimationLevelNone || getCanAnimate());
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::eAnimationLevelNone);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::eAnimationLevelInterpolatedValue);
}

////////////////////////// OfxCustomInstance /////////////////////////////////////////////////


/*
   http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#kOfxParamTypeCustom

   Custom parameters contain null terminated char * C strings, and may animate. They are designed to provide plugins with a way of storing data that is too complicated or impossible to store in a set of ordinary parameters.

   If a custom parameter animates, it must set its kOfxParamPropCustomInterpCallbackV1 property, which points to a OfxCustomParamInterpFuncV1 function. This function is used to interpolate keyframes in custom params.

   Custom parameters have no interface by default. However,

 * if they animate, the host's animation sheet/editor should present a keyframe/curve representation to allow positioning of keys and control of interpolation. The 'normal' (ie: paged or hierarchical) interface should not show any gui.
 * if the custom param sets its kOfxParamPropInteractV1 property, this should be used by the host in any normal (ie: paged or hierarchical) interface for the parameter.

   Custom parameters are mandatory, as they are simply ASCII C strings. However, animation of custom parameters an support for an in editor interact is optional.
 */


OfxCustomInstance::OfxCustomInstance(OfxEffectInstance* node,
                                     OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::Param::CustomInstance( descriptor,node->effectInstance() )
      , _node(node)
      , _knob()
      , _customParamInterpolationV1Entry(0)
{
    const OFX::Host::Property::Set &properties = getProperties();


    _knob = Natron::createKnob<String_Knob>( node, getParamLabel(this) );
    _knob->setAsCustom();

    _knob->setDefaultValue(properties.getStringProperty(kOfxParamPropDefault),0);

    _customParamInterpolationV1Entry = (customParamInterpolationV1Entry_t)properties.getPointerProperty(kOfxParamPropCustomInterpCallbackV1);
    if (_customParamInterpolationV1Entry) {
        _knob->setCustomInterpolation( _customParamInterpolationV1Entry, (void*)getHandle() );
    }
}

OfxStatus
OfxCustomInstance::get(std::string &str)
{
    assert( _node->effectInstance() );
    int currentFrame = (int)_node->effectInstance()->timeLineGetTime();
    str = _knob->getValueAtTime(currentFrame, 0);

    return kOfxStatOK;
}

OfxStatus
OfxCustomInstance::get(OfxTime time,
                       std::string & str)
{
    assert( String_Knob::canAnimateStatic() );
    // it should call _customParamInterpolationV1Entry
    assert( _node->effectInstance() );
    str = _knob->getValueAtTime(time, 0);

    return kOfxStatOK;
}

OfxStatus
OfxCustomInstance::set(const char* str)
{
    _knob->setValueFromPlugin(str,0);

    return kOfxStatOK;
}

OfxStatus
OfxCustomInstance::set(OfxTime time,
                       const char* str)
{
    if (!getCanAnimate()) {
        qDebug() << "Attempting to call setValueAtTime on a parameter that does not have animation enabled";
        return set(str);
    }
    assert( String_Knob::canAnimateStatic() );
    _knob->setValueAtTimeFromPlugin(time,str,0);

    return kOfxStatOK;
}

OfxStatus
OfxCustomInstance::getV(va_list arg)
{
    const char **value = va_arg(arg, const char **);
    OfxStatus stat = get( _localString.localData() );

    *value = _localString.localData().c_str();

    return stat;
}

OfxStatus
OfxCustomInstance::getV(OfxTime time,
                        va_list arg)
{
    const char **value = va_arg(arg, const char **);
    OfxStatus stat = get( time,_localString.localData() );

    *value = _localString.localData().c_str();

    return stat;
}

boost::shared_ptr<KnobI> OfxCustomInstance::getKnob() const
{
    return _knob;
}

// callback which should set enabled state as appropriate
void
OfxCustomInstance::setEnabled()
{
    _knob->setAllDimensionsEnabled( getEnabled() );
}

// callback which should set secret state as appropriate
void
OfxCustomInstance::setSecret()
{
    _knob->setSecret( getSecret() );
}

void
OfxCustomInstance::setEvaluateOnChange()
{
    _knob->setEvaluateOnChange( getEvaluateOnChange() );
}

OfxStatus
OfxCustomInstance::getNumKeys(unsigned int &nKeys) const
{
    return OfxKeyFrame::getNumKeys(_knob, nKeys);
}

OfxStatus
OfxCustomInstance::getKeyTime(int nth,
                              OfxTime & time) const
{
    return OfxKeyFrame::getKeyTime(_knob, nth, time);
}

OfxStatus
OfxCustomInstance::getKeyIndex(OfxTime time,
                               int direction,
                               int & index) const
{
    return OfxKeyFrame::getKeyIndex(_knob, time, direction, index);
}

OfxStatus
OfxCustomInstance::deleteKey(OfxTime time)
{
    return OfxKeyFrame::deleteKey(_knob, time);
}

OfxStatus
OfxCustomInstance::deleteAllKeys()
{
    return OfxKeyFrame::deleteAllKeys(_knob);
}

OfxStatus
OfxCustomInstance::copyFrom(const OFX::Host::Param::Instance &instance,
                            OfxTime offset,
                            const OfxRangeD* range)
{
    const OfxParamToKnob & other = dynamic_cast<const OfxParamToKnob &>(instance);

    return OfxKeyFrame::copyFrom(other.getKnob(), getKnob(), offset, range);
}

void
OfxCustomInstance::onKnobAnimationLevelChanged(int,int lvl)
{
    Natron::AnimationLevelEnum l = (Natron::AnimationLevelEnum)lvl;

    assert( l == Natron::eAnimationLevelNone || getCanAnimate() );
    getProperties().setIntProperty(kOfxParamPropIsAnimating, l != Natron::eAnimationLevelNone);
    getProperties().setIntProperty(kOfxParamPropIsAutoKeying, l == Natron::eAnimationLevelInterpolatedValue);
}

////////////////////////// OfxParametricInstance /////////////////////////////////////////////////

OfxParametricInstance::OfxParametricInstance(OfxEffectInstance* node,
                                             OFX::Host::Param::Descriptor & descriptor)
    : OFX::Host::ParametricParam::ParametricInstance( descriptor,node->effectInstance() )
      , _descriptor(descriptor)
      , _overlayInteract(NULL)
      , _effect(node)
{
    const OFX::Host::Property::Set &properties = getProperties();
    int parametricDimension = properties.getIntProperty(kOfxParamPropParametricDimension);


    _knob = Natron::createKnob<Parametric_Knob>(node, getParamLabel(this),parametricDimension);

    setLabel(); //set label on all curves

    std::vector<double> color(3 * parametricDimension);
    properties.getDoublePropertyN(kOfxParamPropParametricUIColour, &color[0], 3 * parametricDimension);

    for (int i = 0; i < parametricDimension; ++i) {
        _knob->setCurveColor(i, color[i * 3], color[i * 3 + 1], color[i * 3 + 2]);
    }

    QObject::connect( _knob.get(),SIGNAL( mustInitializeOverlayInteract(OverlaySupport*) ),this,SLOT( initializeInteract(OverlaySupport*) ) );
    QObject::connect( _knob.get(), SIGNAL( mustResetToDefault(QVector<int>) ), this, SLOT( onResetToDefault(QVector<int>) ) );
    setDisplayRange();
}

void
OfxParametricInstance::onResetToDefault(const QVector<int> & dimensions)
{
    for (int i = 0; i < dimensions.size(); ++i) {
        Natron::StatusEnum st = _knob->deleteAllControlPoints( dimensions.at(i) );
        assert(st == Natron::eStatusOK);
        defaultInitializeFromDescriptor(dimensions.at(i),_descriptor);
    }
}

void
OfxParametricInstance::initializeInteract(OverlaySupport* widget)
{
    OfxPluginEntryPoint* interactEntryPoint = (OfxPluginEntryPoint*)getProperties().getPointerProperty(kOfxParamPropParametricInteractBackground);

    if (interactEntryPoint) {
        _overlayInteract = new Natron::OfxOverlayInteract( ( *_effect->effectInstance() ),8,true );
        _overlayInteract->setCallingViewport(widget);
        _overlayInteract->createInstanceAction();
        QObject::connect( _knob.get(), SIGNAL( customBackgroundRequested() ), this, SLOT( onCustomBackgroundDrawingRequested() ) );
    }
}

OfxParametricInstance::~OfxParametricInstance()
{
    if (_overlayInteract) {
        delete _overlayInteract;
    }
}

boost::shared_ptr<KnobI> OfxParametricInstance::getKnob() const
{
    return _knob;
}

// callback which should set enabled state as appropriate
void
OfxParametricInstance::setEnabled()
{
    _knob->setAllDimensionsEnabled( getEnabled() );
}

// callback which should set secret state as appropriate
void
OfxParametricInstance::setSecret()
{
    _knob->setSecret( getSecret() );
}

void
OfxParametricInstance::setEvaluateOnChange()
{
    _knob->setEvaluateOnChange( getEvaluateOnChange() );
}

/// callback which should update label
void
OfxParametricInstance::setLabel()
{
    _knob->setName( getParamLabel(this) );
    for (int i = 0; i < _knob->getDimension(); ++i) {
        const std::string & curveName = getProperties().getStringProperty(kOfxParamPropDimensionLabel,i);
        _knob->setDimensionName(i, curveName);
    }
}

void
OfxParametricInstance::setDisplayRange()
{
    double range_min = getProperties().getDoubleProperty(kOfxParamPropParametricRange,0);
    double range_max = getProperties().getDoubleProperty(kOfxParamPropParametricRange,1);

    assert(range_max > range_min);

    _knob->setParametricRange(range_min, range_max);
}

OfxStatus
OfxParametricInstance::getValue(int curveIndex,
                                OfxTime /*time*/,
                                double parametricPosition,
                                double *returnValue)
{
    Natron::StatusEnum stat = _knob->getValue(curveIndex, parametricPosition, returnValue);

    if (stat == Natron::eStatusOK) {
        return kOfxStatOK;
    } else {
        return kOfxStatFailed;
    }
}

OfxStatus
OfxParametricInstance::getNControlPoints(int curveIndex,
                                         double /*time*/,
                                         int *returnValue)
{
    Natron::StatusEnum stat = _knob->getNControlPoints(curveIndex, returnValue);

    if (stat == Natron::eStatusOK) {
        return kOfxStatOK;
    } else {
        return kOfxStatFailed;
    }
}

OfxStatus
OfxParametricInstance::getNthControlPoint(int curveIndex,
                                          double /*time*/,
                                          int nthCtl,
                                          double *key,
                                          double *value)
{
    Natron::StatusEnum stat = _knob->getNthControlPoint(curveIndex, nthCtl, key, value);

    if (stat == Natron::eStatusOK) {
        return kOfxStatOK;
    } else {
        return kOfxStatFailed;
    }
}

OfxStatus
OfxParametricInstance::setNthControlPoint(int curveIndex,
                                          double /*time*/,
                                          int nthCtl,
                                          double key,
                                          double value,
                                          bool /*addAnimationKey*/)
{
    Natron::StatusEnum stat = _knob->setNthControlPoint(curveIndex, nthCtl, key, value);

    if (stat == Natron::eStatusOK) {
        return kOfxStatOK;
    } else {
        return kOfxStatFailed;
    }
}

OfxStatus
OfxParametricInstance::addControlPoint(int curveIndex,
                                       double time,
                                       double key,
                                       double value,
                                       bool /* addAnimationKey*/)
{
    if (boost::math::isnan(time) ||
        boost::math::isinf(time) ||
        boost::math::isnan(key) ||
        boost::math::isinf(key) ||
        boost::math::isnan(value) ||
        boost::math::isinf(value)) {
        return kOfxStatFailed;
    }

    Natron::StatusEnum stat = _knob->addControlPoint(curveIndex, key, value);

    if (stat == Natron::eStatusOK) {
        return kOfxStatOK;
    } else {
        return kOfxStatFailed;
    }
}

OfxStatus
OfxParametricInstance::deleteControlPoint(int curveIndex,
                                          int nthCtl)
{
    Natron::StatusEnum stat = _knob->deleteControlPoint(curveIndex, nthCtl);

    if (stat == Natron::eStatusOK) {
        return kOfxStatOK;
    } else {
        return kOfxStatFailed;
    }
}

OfxStatus
OfxParametricInstance::deleteAllControlPoints(int curveIndex)
{
    Natron::StatusEnum stat = _knob->deleteAllControlPoints(curveIndex);

    if (stat == Natron::eStatusOK) {
        return kOfxStatOK;
    } else {
        return kOfxStatFailed;
    }
}

void
OfxParametricInstance::onCustomBackgroundDrawingRequested()
{
    if (_overlayInteract) {
        RenderScale s;
        _overlayInteract->getPixelScale(s.x, s.y);
        _overlayInteract->drawAction(_effect->getApp()->getTimeLine()->currentFrame(), s);
    }
}

OfxStatus
OfxParametricInstance::copyFrom(const OFX::Host::Param::Instance &instance,
                                OfxTime offset,
                                const OfxRangeD* range)
{
    const OfxParamToKnob & other = dynamic_cast<const OfxParamToKnob &>(instance);

    return OfxKeyFrame::copyFrom(other.getKnob(), getKnob(), offset, range);
}

