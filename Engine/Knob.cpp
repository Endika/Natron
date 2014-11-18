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
#include "Knob.h"
#include "KnobImpl.h"

#include <QtCore/QDataStream>
#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QThread>
#include <QtCore/QDebug>

#include "Global/GlobalDefines.h"
#include "Engine/Node.h"
#include "Engine/ViewerInstance.h"
#include "Engine/TimeLine.h"
#include "Engine/Curve.h"
#include "Engine/KnobFile.h"
#include "Engine/KnobTypes.h"
#include "Engine/Project.h"
#include "Engine/KnobSerialization.h"
#include "Engine/ThreadStorage.h"

#include "Engine/AppManager.h"
#include "Engine/LibraryBinary.h"
#include "Engine/AppInstance.h"
#include "Engine/Hash64.h"
#include "Engine/StringAnimationManager.h"


using namespace Natron;
using std::make_pair; using std::pair;

KnobSignalSlotHandler::KnobSignalSlotHandler(boost::shared_ptr<KnobI> knob)
: QObject()
, k(knob)
{
    QObject::connect( this, SIGNAL( evaluateValueChangedInMainThread(int,int) ), this,
                     SLOT( onEvaluateValueChangedInOtherThread(int,int) ) );
}

void
KnobSignalSlotHandler::onAnimationRemoved(int dimension)
{
    k->onAnimationRemoved(dimension);
}

void
KnobSignalSlotHandler::onMasterChanged(int dimension)
{
    KnobSignalSlotHandler* handler = qobject_cast<KnobSignalSlotHandler*>( sender() );
    
    assert(handler);
    k->onMasterChanged(handler->getKnob().get(), dimension);
}

void
KnobSignalSlotHandler::onExprDependencyChanged(int dimension)
{
    KnobSignalSlotHandler* handler = qobject_cast<KnobSignalSlotHandler*>( sender() );
    
    assert(handler);
    k->onExprDependencyChanged(handler->getKnob().get(), dimension);
    
}

void
KnobSignalSlotHandler::onMasterKeyFrameSet(SequenceTime time,int dimension,int reason,bool added)
{
    KnobSignalSlotHandler* handler = qobject_cast<KnobSignalSlotHandler*>( sender() );
    assert(handler);
    boost::shared_ptr<KnobI> master = handler->getKnob();
    
    k->clone(master.get(), dimension);
    emit keyFrameSet(time, dimension, reason, added);
}

void
KnobSignalSlotHandler::onMasterKeyFrameRemoved(SequenceTime time,int dimension,int reason)
{
    KnobSignalSlotHandler* handler = qobject_cast<KnobSignalSlotHandler*>( sender() );
    assert(handler);
    boost::shared_ptr<KnobI> master = handler->getKnob();
    
    k->clone(master.get(), dimension);
    emit keyFrameRemoved(time, dimension, reason);
}

void
KnobSignalSlotHandler::onMasterKeyFrameMoved(int dimension,int oldTime,int newTime)
{
    KnobSignalSlotHandler* handler = qobject_cast<KnobSignalSlotHandler*>( sender() );
    assert(handler);
    boost::shared_ptr<KnobI> master = handler->getKnob();
    
    k->clone(master.get(), dimension);
    emit keyFrameMoved(dimension, oldTime, newTime);
}

void
KnobSignalSlotHandler::onMasterAnimationRemoved(int dimension)
{
    KnobSignalSlotHandler* handler = qobject_cast<KnobSignalSlotHandler*>( sender() );
    assert(handler);
    boost::shared_ptr<KnobI> master = handler->getKnob();
    
    k->clone(master.get(), dimension);
    emit animationRemoved(dimension);
}

void
KnobSignalSlotHandler::onEvaluateValueChangedInOtherThread(int dimension,
                                                           int reason)
{
    assert( QThread::currentThread() == qApp->thread() );
    k->evaluateValueChange(dimension, (Natron::ValueChangedReasonEnum)reason);
}

/***************** KNOBI**********************/

bool
KnobI::slaveTo(int dimension,
               const boost::shared_ptr<KnobI> & other,
               int otherDimension,
               bool ignoreMasterPersistence)
{
    return slaveTo(dimension, other, otherDimension, Natron::eValueChangedReasonPluginEdited,ignoreMasterPersistence);
}

void
KnobI::onKnobSlavedTo(int dimension,
                      const boost::shared_ptr<KnobI> &  other,
                      int otherDimension)
{
    slaveTo(dimension, other, otherDimension, Natron::eValueChangedReasonUserEdited);
}

void
KnobI::unSlave(int dimension,
               bool copyState)
{
    unSlave(dimension, Natron::eValueChangedReasonPluginEdited,copyState);
}

void
KnobI::onKnobUnSlaved(int dimension)
{
    unSlave(dimension, Natron::eValueChangedReasonUserEdited,true);
}

void
KnobI::deleteValueAtTime(int time,
                         int dimension)
{
    deleteValueAtTime(time, dimension, Natron::eValueChangedReasonPluginEdited);
}

void
KnobI::removeAnimation(int dimension)
{
    removeAnimation(dimension, Natron::eValueChangedReasonPluginEdited);
}


void
KnobI::onAnimationRemoved(int dimension)
{
    removeAnimation(dimension, Natron::eValueChangedReasonUserEdited);
}

/***********************************KNOB HELPER******************************************/

///for each dimension, the dimension of the master this dimension is linked to, and a pointer to the master
typedef std::vector< std::pair< int,boost::shared_ptr<KnobI> > > MastersMap;

///a curve for each dimension
typedef std::vector< boost::shared_ptr<Curve> > CurvesMap;

struct Expr
{
    std::string expression; //< the one modified by Natron
    std::string originalExpression; //< the one input by the user
    
    bool hasRet;
    std::list<KnobI*> dependencies;
    
    PyCodeObject* code;
    PyObject* global_dict;
    
    Expr() : expression(), originalExpression(), hasRet(false),  code(0), global_dict(0) {}
};


struct KnobHelperPrivate
{
    KnobHelper* publicInterface;
    KnobHolder* holder;
    std::string description; //< the text label that will be displayed  on the GUI
    bool descriptionVisible;
    std::string name; //< the knob can have a name different than the label displayed on GUI.
    //By default this is the same as _description but can be set by calling setName().
    bool newLine;
    int itemSpacing;
    boost::shared_ptr<KnobI> parentKnob;
    bool IsSecret;
    std::vector<bool> enabled;
    bool CanUndo;
    bool EvaluateOnChange; //< if true, a value change will never trigger an evaluation
    bool IsPersistant; //will it be serialized?
    std::string tooltipHint;
    bool isAnimationEnabled;
    int dimension;
    /* the keys for a specific dimension*/
    CurvesMap curves;
    
    ////curve links
    ///A slave link CANNOT be master at the same time (i.e: if _slaveLinks[i] != NULL  then _masterLinks[i] == NULL )
    mutable QReadWriteLock mastersMutex; //< protects _masters & ignoreMasterPersistence & listeners
    MastersMap masters; //from what knob is slaved each curve if any
    bool ignoreMasterPersistence; //< when true masters will not be serialized
    
    ///This is a list of all the knobs that have expressions/links to this knob. It could be named "slaves" but
    ///in the future we will also add expressions.
    std::list<KnobI*> listeners;
    
    mutable QMutex animationLevelMutex;
    std::vector<Natron::AnimationLevelEnum> animationLevel; //< indicates for each dimension whether it is static/interpolated/onkeyframe
    bool declaredByPlugin; //< was the knob declared by a plug-in or added by Natron
    
    ///Pointer to the ofx param overlay interact
    boost::shared_ptr<OfxParamOverlayInteract> customInteract;
    
    ///Pointer to the knobGui interface if it has any
    KnobGuiI* gui;
    
    mutable QMutex mustCloneGuiCurvesMutex;
    /// Set to true if gui curves were modified by the user instead of the real internal curves.
    /// If true then when finished rendering, the knob should clone the guiCurves into the internal curves.
    std::vector<bool> mustCloneGuiCurves;
    std::vector<bool> mustCloneInternalCurves;
    
    ///A blind handle to the ofx param, needed for custom overlay interacts
    void* ofxParamHandle;
    
    ///This is to deal with multi-instance effects such as the Tracker: instance specifics knobs are
    ///not shared between instances whereas non instance specifics are shared.
    bool isInstanceSpecific;
    
    std::vector<std::string> dimensionNames;
    
    mutable QMutex expressionMutex;
    std::vector<Expr> expressions;
    
    KnobHelperPrivate(KnobHelper* publicInterface_,
                      KnobHolder*  holder_,
                      int dimension_,
                      const std::string & description_,
                      bool declaredByPlugin_)
    : publicInterface(publicInterface_)
    , holder(holder_)
    , description(description_)
    , descriptionVisible(true)
    , name( description_.c_str() )
    , newLine(true)
    , itemSpacing(0)
    , parentKnob()
    , IsSecret(false)
    , enabled(dimension_)
    , CanUndo(true)
    , EvaluateOnChange(true)
    , IsPersistant(true)
    , tooltipHint()
    , isAnimationEnabled(true)
    , dimension(dimension_)
    , curves(dimension_)
    , mastersMutex()
    , masters(dimension_)
    , ignoreMasterPersistence(false)
    , listeners()
    , animationLevelMutex()
    , animationLevel(dimension_)
    , declaredByPlugin(declaredByPlugin_)
    , customInteract()
    , gui(0)
    , mustCloneGuiCurvesMutex()
    , mustCloneGuiCurves()
    , ofxParamHandle(0)
    , isInstanceSpecific(false)
    , dimensionNames(dimension_)
    , expressionMutex()
    , expressions()
    {
        mustCloneGuiCurves.resize(dimension);
        mustCloneInternalCurves.resize(dimension);
        expressions.resize(dimension);
        for (int i = 0; i < dimension_; ++i) {
            mustCloneGuiCurves[i] = false;
            mustCloneInternalCurves[i] = false;
        }
    }
    
    void parseListenersFromExpression(int dimension);
};



KnobHelper::KnobHelper(KnobHolder* holder,
                       const std::string & description,
                       int dimension,
                       bool declaredByPlugin)
: _signalSlotHandler()
, _expressionsRecursionLevel(0)
, _expressionRecursionLevelMutex(QMutex::Recursive)
, _imp( new KnobHelperPrivate(this,holder,dimension,description,declaredByPlugin) )
{
}

KnobHelper::~KnobHelper()
{
    if (_signalSlotHandler) {
        _signalSlotHandler->s_deleted();
    }
    if (_imp->holder) {
        _imp->holder->removeKnob(this);
    }
}

void
KnobHelper::setKnobGuiPointer(KnobGuiI* ptr)
{
    assert( QThread::currentThread() == qApp->thread() );
    _imp->gui = ptr;
}

KnobGuiI*
KnobHelper::getKnobGuiPointer() const
{
    return _imp->gui;
}

bool
KnobHelper::isDeclaredByPlugin() const
{
    return _imp->declaredByPlugin;
}

void
KnobHelper::setAsInstanceSpecific()
{
    _imp->isInstanceSpecific = true;
}

bool
KnobHelper::isInstanceSpecific() const
{
    return _imp->isInstanceSpecific;
}

void
KnobHelper::populate()
{
    Color_Knob* isColor = dynamic_cast<Color_Knob*>(this);
    for (int i = 0; i < _imp->dimension; ++i) {
        _imp->enabled[i] = true;
        _imp->curves[i] = boost::shared_ptr<Curve>( new Curve(this,i) );
        _imp->animationLevel[i] = Natron::eAnimationLevelNone;
        
        
        if (!isColor) {
            switch (i) {
                case 0:
                    _imp->dimensionNames[i] = "x";
                    break;
                case 1:
                    _imp->dimensionNames[i] = "y";
                    break;
                case 2:
                    _imp->dimensionNames[i] = "z";
                    break;
                case 3:
                    _imp->dimensionNames[i] = "w";
                    break;
                default:
                    break;
            }
            
        } else {
            switch (i) {
                case 0:
                    _imp->dimensionNames[i] = "r";
                    break;
                case 1:
                    _imp->dimensionNames[i] = "g";
                    break;
                case 2:
                    _imp->dimensionNames[i] = "b";
                    break;
                case 3:
                    _imp->dimensionNames[i] = "a";
                    break;
                default:
                    break;
            }
        }
        
    }
    
}

std::string
KnobHelper::getDimensionName(int dimension) const
{
    assert( dimension < (int)_imp->dimensionNames.size() && dimension >= 0);
    return _imp->dimensionNames[dimension];
    
}

void
KnobHelper::setDimensionName(int dim,const std::string & name)
{
    assert(QThread::currentThread() == qApp->thread());
    _imp->dimensionNames[dim] = name;
}

template <typename T>
const std::string &
Knob<T>::typeName() const {
    static std::string knobNoTypeName("NoType");
    return knobNoTypeName;
}

template <typename T>
bool
Knob<T>::canAnimate() const {
    return false;
}

void
KnobHelper::setSignalSlotHandler(const boost::shared_ptr<KnobSignalSlotHandler> & handler)
{
    _signalSlotHandler = handler;
}

double
KnobHelper::getDerivativeAtTime(double time,
                                int dimension) const
{
    if ( dimension > (int)_imp->curves.size() ) {
        throw std::invalid_argument("KnobHelper::getDerivativeAtTime(): Dimension out of range");
    }
    
    ///if the knob is slaved to another knob, returns the other knob value
    std::pair<int,boost::shared_ptr<KnobI> > master = getMaster(dimension);
    if (master.second) {
        return master.second->getDerivativeAtTime(time,master.first);
    }
    
    boost::shared_ptr<Curve> curve  = _imp->curves[dimension];
    if (curve->getKeyFramesCount() > 0) {
        return curve->getDerivativeAt(time);
    } else {
        /*if the knob as no keys at this dimension, the derivative is 0.*/
        return 0.;
    }
}


void
KnobHelper::deleteValueAtTime(int time,
                              int dimension,
                              Natron::ValueChangedReasonEnum reason)
{
    if ( dimension > (int)_imp->curves.size() || dimension < 0) {
        throw std::invalid_argument("KnobHelper::deleteValueAtTime(): Dimension out of range");
    }
    
    
    KnobHolder* holder = getHolder();
    boost::shared_ptr<Curve> curve;
    
    bool useGuiCurve = (!holder || !holder->canSetValue()) && _imp->gui;
    
    if (!useGuiCurve) {
        curve = _imp->curves[dimension];
    } else {
        curve = _imp->gui->getCurve(dimension);
        setGuiCurveHasChanged(dimension,true);
    }
    
    try {
        curve->removeKeyFrameWithTime( (double)time );
    } catch (const std::exception & e) {
        qDebug() << e.what();
    }
    
    //virtual portion
    keyframeRemoved_virtual(dimension, time);
    

    if (_imp->holder) {
        _imp->holder->updateHasAnimation();
    }


    if (!useGuiCurve) {
        
        if (_signalSlotHandler) {
            _signalSlotHandler->s_updateDependencies(dimension);
        }
        checkAnimationLevel(dimension);
        guiCurveCloneInternalCurve(dimension);
        evaluateValueChange(dimension,reason);
    }
    
    if (_signalSlotHandler) {
        _signalSlotHandler->s_keyFrameRemoved(time,dimension,(int)reason);
    }
    
}

void
KnobHelper::onKeyFrameRemoved(SequenceTime time,int dimension)
{
    deleteValueAtTime(time,dimension,Natron::eValueChangedReasonUserEdited);
}

bool
KnobHelper::moveValueAtTime(int time,int dimension,double dt,double dv,KeyFrame* newKey)
{
    if ( dimension > (int)_imp->curves.size() || dimension < 0) {
        throw std::invalid_argument("KnobHelper::moveValueAtTime(): Dimension out of range");
    }
    
    KnobHolder* holder = getHolder();
    
    boost::shared_ptr<Curve> curve;
    
    bool useGuiCurve = (!holder || !holder->canSetValue()) && _imp->gui;
    
    if (!useGuiCurve) {
        curve = _imp->curves[dimension];
    } else {
        curve = _imp->gui->getCurve(dimension);
        setGuiCurveHasChanged(dimension,true);
    }
    
    assert(curve);
    
    std::pair<double,double> curveYRange = curve->getCurveYRange();
    
    KeyFrame k;
    int keyindex = curve->keyFrameIndex(time);
    if (keyindex == -1) {
        return false;
    }
    
    bool gotKey = curve->getKeyFrameWithIndex(keyindex, &k);
    if (!gotKey) {
        return false;
    }
    
    double newX = k.getTime() + dt;
    double newY = k.getValue() + dv;
    
    if ( curve->areKeyFramesValuesClampedToIntegers() ) {
        newY = std::floor(newY + 0.5);
    } else if ( curve->areKeyFramesValuesClampedToBooleans() ) {
        newY = newY < 0.5 ? 0 : 1;
    }
    
    if (newY > curveYRange.second) {
        newY = k.getValue();
    } else if (newY < curveYRange.first) {
        newY = k.getValue();
    }
    
    ///Make sure string animation follows up
    AnimatingString_KnobHelper* isString = dynamic_cast<AnimatingString_KnobHelper*>(this);
    std::string v;
    if (isString) {
        isString->stringFromInterpolatedValue(k.getValue(), &v);
    }
    keyframeRemoved_virtual(dimension,time);
    if (isString) {
        double ret;
        isString->stringToKeyFrameValue(newX, v, &ret);
    }
    
    
    try {
        *newKey = curve->setKeyFrameValueAndTime(newX,newY, keyindex, NULL);
    } catch (...) {
        return false;
    }
    
    if (_signalSlotHandler) {
        _signalSlotHandler->s_keyFrameMoved(dimension,time,newX);
    }
    
    if (!useGuiCurve) {
        evaluateValueChange(dimension, Natron::eValueChangedReasonPluginEdited);
        guiCurveCloneInternalCurve(dimension);
    }
    return true;
    
}

bool
KnobHelper::setInterpolationAtTime(int dimension,int time,Natron::KeyframeTypeEnum interpolation,KeyFrame* newKey)
{
    if ( dimension > (int)_imp->curves.size() || dimension < 0) {
        throw std::invalid_argument("KnobHelper::setInterpolationAtTime(): Dimension out of range");
    }
    
    KnobHolder* holder = getHolder();
    boost::shared_ptr<Curve> curve;
    
    bool useGuiCurve = (!holder || !holder->canSetValue()) && _imp->gui;
    
    if (!useGuiCurve) {
        curve = _imp->curves[dimension];
    } else {
        curve = _imp->gui->getCurve(dimension);
        setGuiCurveHasChanged(dimension,true);
    }
    
    int keyIndex = curve->keyFrameIndex(time);
    if (keyIndex == -1) {
        return false;
    }
    
    *newKey = curve->setKeyFrameInterpolation(interpolation, keyIndex);
    
    if (!useGuiCurve) {
        evaluateValueChange(dimension, Natron::eValueChangedReasonPluginEdited);
        guiCurveCloneInternalCurve(dimension);
    } else {
        if (_signalSlotHandler) {
            _signalSlotHandler->s_refreshGuiCurve(dimension);
        }
    }
    if (_signalSlotHandler) {
        _signalSlotHandler->s_keyFrameInterpolationChanged(time, dimension);
    }
    return true;
}

bool
KnobHelper::moveDerivativesAtTime(int dimension,int time,double left,double right)
{
    if ( dimension > (int)_imp->curves.size() || dimension < 0) {
        throw std::invalid_argument("KnobHelper::setInterpolationAtTime(): Dimension out of range");
    }
    
    KnobHolder* holder = getHolder();
    boost::shared_ptr<Curve> curve;
    
    bool useGuiCurve = (!holder || !holder->canSetValue()) && _imp->gui;
    
    if (!useGuiCurve) {
        curve = _imp->curves[dimension];
    } else {
        curve = _imp->gui->getCurve(dimension);
        setGuiCurveHasChanged(dimension,true);
    }
    
    int keyIndex = curve->keyFrameIndex(time);
    if (keyIndex == -1) {
        return false;
    }
    
    curve->setKeyFrameInterpolation(eKeyframeTypeFree, keyIndex);
    curve->setKeyFrameDerivatives(left, right, keyIndex);
    
    if (!useGuiCurve) {
        evaluateValueChange(dimension, Natron::eValueChangedReasonPluginEdited);
        guiCurveCloneInternalCurve(dimension);
    } else {
        if (_signalSlotHandler) {
            _signalSlotHandler->s_refreshGuiCurve(dimension);
        }
    }
    if (_signalSlotHandler) {
        _signalSlotHandler->s_derivativeMoved(time, dimension);
    }
    return true;
}

bool
KnobHelper::moveDerivativeAtTime(int dimension,int time,double derivative,bool isLeft)
{
    if ( dimension > (int)_imp->curves.size() || dimension < 0) {
        throw std::invalid_argument("KnobHelper::setInterpolationAtTime(): Dimension out of range");
    }
    
    KnobHolder* holder = getHolder();
    boost::shared_ptr<Curve> curve;
    
    bool useGuiCurve = (!holder || !holder->canSetValue()) && _imp->gui;
    
    if (!useGuiCurve) {
        curve = _imp->curves[dimension];
    } else {
        curve = _imp->gui->getCurve(dimension);
        setGuiCurveHasChanged(dimension,true);
    }
    
    int keyIndex = curve->keyFrameIndex(time);
    if (keyIndex == -1) {
        return false;
    }
    
    curve->setKeyFrameInterpolation(eKeyframeTypeBroken, keyIndex);
    if (isLeft) {
        curve->setKeyFrameLeftDerivative(derivative, keyIndex);
    } else {
        curve->setKeyFrameRightDerivative(derivative, keyIndex);
    }
    
    if (!useGuiCurve) {
        evaluateValueChange(dimension, Natron::eValueChangedReasonPluginEdited);
        guiCurveCloneInternalCurve(dimension);
    } else {
        if (_signalSlotHandler) {
            _signalSlotHandler->s_refreshGuiCurve(dimension);
        }
    }
    if (_signalSlotHandler) {
        _signalSlotHandler->s_derivativeMoved(time, dimension);
    }
    return true;
}

void
KnobHelper::removeAnimation(int dimension,
                            Natron::ValueChangedReasonEnum reason)
{
    assert(0 <= dimension);
    if ( (dimension < 0) || ( (int)_imp->curves.size() <= dimension ) ) {
        throw std::invalid_argument("KnobHelper::removeAnimation(): Dimension out of range");
    }
    
    KnobHolder* holder = getHolder();
    
    boost::shared_ptr<Curve> curve;
    
    bool useGuiCurve = (!holder || !holder->canSetValue()) && _imp->gui;
    
    if (!useGuiCurve) {
        curve = _imp->curves[dimension];
    } else {
        curve = _imp->gui->getCurve(dimension);
        setGuiCurveHasChanged(dimension,true);
    }
    
    if ( _signalSlotHandler && (reason != Natron::eValueChangedReasonUserEdited) ) {
        _signalSlotHandler->s_animationAboutToBeRemoved(dimension);
    }
    
    curve->clearKeyFrames();
    
    if ( _signalSlotHandler && (reason != Natron::eValueChangedReasonUserEdited) ) {
        _signalSlotHandler->s_animationRemoved(dimension);
    }
    
    animationRemoved_virtual(dimension);

    if (_imp->holder) {
        _imp->holder->updateHasAnimation();
    }
    
    
    if (!useGuiCurve) {
        //virtual portion
        evaluateValueChange(dimension, reason);
        guiCurveCloneInternalCurve(dimension);
    } else {
        if (_signalSlotHandler) {
            _signalSlotHandler->s_refreshGuiCurve(dimension);
        }
    }
}

void
KnobHelper::cloneInternalCurvesIfNeeded(std::set<int>& modifiedDimensions)
{
    QMutexLocker k(&_imp->mustCloneGuiCurvesMutex);
    for (int i = 0; i < getDimension(); ++i) {
        if (_imp->mustCloneInternalCurves[i]) {
            guiCurveCloneInternalCurve(i);
            _imp->mustCloneInternalCurves[i] = false;
            modifiedDimensions.insert(i);
        }
    }
}

void
KnobHelper::setInternalCurveHasChanged(int dimension, bool changed)
{
    QMutexLocker k(&_imp->mustCloneGuiCurvesMutex);
    _imp->mustCloneInternalCurves[dimension] = changed;
}

void
KnobHelper::cloneGuiCurvesIfNeeded(std::set<int>& modifiedDimensions)
{
    QMutexLocker k(&_imp->mustCloneGuiCurvesMutex);
    for (int i = 0; i < getDimension(); ++i) {
        if (_imp->mustCloneGuiCurves[i]) {
            getCurve(i)->clone(*_imp->gui->getCurve(i));
            _imp->mustCloneGuiCurves[i] = false;
            
            modifiedDimensions.insert(i);
        }
    }
    if (_imp->holder) {
        _imp->holder->updateHasAnimation();
    }
}

void
KnobHelper::guiCurveCloneInternalCurve(int dimension)
{
    if (_imp->gui) {
        _imp->gui->getCurve(dimension)->clone(*(_imp->curves[dimension]));
        if (_signalSlotHandler) {
            _signalSlotHandler->s_refreshGuiCurve(dimension);
        }
    }
}

boost::shared_ptr<Curve>
KnobHelper::getGuiCurve(int dimension) const
{
    if (_imp->gui) {
        return _imp->gui->getCurve(dimension);
    } else {
        return boost::shared_ptr<Curve>();
    }
}

void
KnobHelper::setGuiCurveHasChanged(int dimension,bool changed)
{
    QMutexLocker k(&_imp->mustCloneGuiCurvesMutex);
    _imp->mustCloneGuiCurves[dimension] = changed;
}

boost::shared_ptr<Curve> KnobHelper::getCurve(int dimension,bool byPassMaster) const
{
    assert( 0 <= dimension && dimension < (int)_imp->curves.size() );
    
    std::pair<int,boost::shared_ptr<KnobI> > master = getMaster(dimension);
    if (!byPassMaster && master.second) {
        return master.second->getCurve(master.first);
    }
    
    return _imp->curves[dimension];
}

bool
KnobHelper::isAnimated(int dimension) const
{
    return getCurve(dimension)->isAnimated();
}

const std::vector<boost::shared_ptr<Curve> > &
KnobHelper::getCurves() const
{
    return _imp->curves;
}

int
KnobHelper::getDimension() const
{
    return _imp->dimension;
}

void
KnobHelper::blockEvaluation()
{
    if (_imp->holder) {
        _imp->holder->blockEvaluation();
    }
}

void
KnobHelper::unblockEvaluation()
{
    if (_imp->holder) {
        _imp->holder->unblockEvaluation();
    }
}

void
KnobHelper::evaluateValueChange(int dimension,
                                Natron::ValueChangedReasonEnum reason)
{
    
    if ( QThread::currentThread() != qApp->thread() ) {
        _signalSlotHandler->s_evaluateValueChangedInMainThread(dimension, reason);
        
        return;
    }
    
    AppInstance* app = 0;
    if (_imp->holder) {
        app = _imp->holder->getApp();
    }
    
    bool guiFrozen = app && _imp->gui && _imp->gui->isGuiFrozenForPlayback();
    
    
    /// For eValueChangedReasonTimeChanged we never call the instanceChangedAction and evaluate otherwise it would just throttle down
    /// the application responsiveness
    if (reason != Natron::eValueChangedReasonTimeChanged && _imp->holder) {
        int time;
        if (app) {
            time = app->getTimeLine()->currentFrame();
        } else {
            time = 0;
        }
        if ( ( app && !app->getProject()->isLoadingProject() ) || !app ) {
            
            ///Notify that a value has changed, this may lead to this function being called recursively because it calls the plugin's
            ///instance changed action.
            _imp->holder->onKnobValueChanged_public(this, reason, time);
            
            
            if (reason != Natron::eValueChangedReasonSlaveRefresh && !guiFrozen) {
                ///Evaluate the change only if the reason is not time changed or slave refresh
                _imp->holder->evaluate_public(this, _imp->EvaluateOnChange, reason);
            }
            
            
            
        }
    }
    
    if (!guiFrozen  && _signalSlotHandler) {
        _signalSlotHandler->s_valueChanged(dimension,(int)reason);
        _signalSlotHandler->s_updateDependencies(dimension);
        checkAnimationLevel(dimension);
    }
}

void
KnobHelper::turnOffNewLine()
{
    _imp->newLine = false;
}

bool
KnobHelper::isNewLineTurnedOff() const
{
    return !_imp->newLine;
}

void
KnobHelper::setSpacingBetweenItems(int spacing)
{
    _imp->itemSpacing = spacing;
}

void
KnobHelper::setEnabled(int dimension,
                       bool b)
{
    _imp->enabled[dimension] = b;
    if (_signalSlotHandler) {
        _signalSlotHandler->s_enabledChanged();
    }
}

void
KnobHelper::setAllDimensionsEnabled(bool b)
{
    for (U32 i = 0; i < _imp->enabled.size(); ++i) {
        _imp->enabled[i] = b;
    }
    if (_signalSlotHandler) {
        _signalSlotHandler->s_enabledChanged();
    }
}

void
KnobHelper::setSecret(bool b)
{
    _imp->IsSecret = b;
    
    ///the knob was revealed , refresh its gui to the current time
    if ( !b && _imp->holder && _imp->holder->getApp() ) {
        onTimeChanged( _imp->holder->getApp()->getTimeLine()->currentFrame() );
    }
    if (_signalSlotHandler) {
        _signalSlotHandler->s_secretChanged();
    }
}

int
KnobHelper::determineHierarchySize() const
{
    int ret = 0;
    boost::shared_ptr<KnobI> current = getParentKnob();
    
    while (current) {
        ++ret;
        current = current->getParentKnob();
    }
    
    return ret;
}

const std::string &
KnobHelper::getDescription() const
{
    return _imp->description;
}

void
KnobHelper::hideDescription()
{
    _imp->descriptionVisible = false;
}

bool
KnobHelper::isDescriptionVisible() const
{
    return _imp->descriptionVisible;
}

bool
KnobHelper::hasAnimation() const
{
#pragma message WARN("Return true if the knob has an expression in Python branch")
    for (int i = 0; i < getDimension(); ++i) {
        if (getKeyFramesCount(i) > 0) {
            return true;
        }
    }
    
    return false;
}

std::size_t
KnobI::declareCurrentKnobVariable_Python(KnobI* knob,int dimension,std::string& script)
{
    KnobHolder* holder = knob->getHolder();
    if (!holder) {
        return std::string::npos;
    }
    
    EffectInstance* effect = dynamic_cast<EffectInstance*>(holder);
    if (effect) {
        
        //import the math module
        Natron::ensureScriptHasModuleImport("math", script);
        
        std::size_t firstLine = effect->getNode()->declareCurrentNodeVariable_Python(script);
        ///Now define the app variable
        std::stringstream ss;
        ss << "thisParam = thisNode.getParam(\"" << knob->getName() << "\") \n";
        ss << "frame = thisParam.getCurrentTime() \n";
        if (dimension != -1) {
            ss << "dimension = " << dimension << "\n";
        }
        std::string toInsert = ss.str();
        script.insert(firstLine, toInsert);
        return firstLine + toInsert.size();
    } else {
        return std::string::npos;
    }
}

static std::size_t getMatchingParenthesisPosition(std::size_t openingParenthesisPos,const std::string& str) {
    assert(openingParenthesisPos < str.size() && str.at(openingParenthesisPos) == '(');
    
    int noOpeningParenthesisFound = 0;
    int i = openingParenthesisPos + 1;
    
    while (i < (int)str.size()) {
        if (str.at(i) == ')') {
            if (noOpeningParenthesisFound == 0) {
                break;
            } else {
                --noOpeningParenthesisFound;
            }
        } else if (str.at(i) == '(') {
            ++noOpeningParenthesisFound;
        }
        ++i;
    }
    if (i >= (int)str.size()) {
        return std::string::npos;
    }
    return i;
}

static bool parseTokenFrom(const std::string& str,const std::string& token,std::size_t inputPos,std::size_t* tokenStart,std::size_t* tokenSize)
{
    std::size_t pos;
    bool foundMatchingToken = false;
    while (!foundMatchingToken) {
        
        pos = str.find(token,inputPos);
        if (pos == std::string::npos) {
            return false;
        }
        
        *tokenStart = pos;
        
        pos += token.size();
        
        ///Find nearest opening parenthesis
        for (; pos < str.size(); ++pos) {
            if (str.at(pos) == '(') {
                foundMatchingToken = true;
                break;
            } else if (str.at(pos) != ' ') {
                //We didn't find a good token
                break;
            }
        }
        
        if (pos >= str.size()) {
            throw std::invalid_argument("Invalid expr");
        }
        
        if (!foundMatchingToken) {
            inputPos = pos;
        }

    }
    
    std::size_t endingParenthesis = getMatchingParenthesisPosition(pos, str);
    if (endingParenthesis == std::string::npos) {
        throw std::invalid_argument("Invalid expr");
    }
    
    *tokenSize = endingParenthesis - *tokenStart + 1;
    assert(*tokenSize > 0);
    return true;
}

static bool replaceAllOcurrencesOfToken(std::string& str,const std::string& token,bool findDotAfterParenthesis,int fromDim)
{
    
    std::size_t tokenStart,tokenSize;
    bool couldFindToken;
    try {
        couldFindToken = parseTokenFrom(str, token, 0, &tokenStart, &tokenSize);
    } catch (...) {
        return false;
    }
    
    
    std::stringstream ss;
    ss << "addAsDependencyOf(" << fromDim << ",thisParam)";
    std::string toInsert = ss.str();
    
    while (couldFindToken) {
        
        if (findDotAfterParenthesis ) {
            if (tokenStart + tokenSize >= str.size() || str.at(tokenStart + tokenSize) != '.') {
                //We should anyway never get here because the expression should've been compiled and tested before
                throw std::invalid_argument("A '.' must follow the ending parenthesis because this function returns a Tuple.");
            } else {
                //remove the 2 characters, e.g: ".x" of the tuple
                str.erase(tokenStart + tokenSize, 2);
            }
        }

        str.replace(tokenStart, tokenSize, toInsert);
        try {
            couldFindToken = parseTokenFrom(str, token, tokenStart, &tokenStart, &tokenSize);
        } catch (...) {
            return false;
        }
    }
    return true;
}

void
KnobHelperPrivate::parseListenersFromExpression(int dimension)
{
    
    //Extract pointers to knobs referred to by the expression
    //Our heuristic is quite simple: we replace in the python code all calls to:
    // - getValue
    // - getValueAtTime
    // - getDerivativeAtTime
    // - getIntegrateFromTimeToTime
    // - get
    // - getAt
    // And replace them by addAsDependencyOf(thisParam) which will register the parameters as a dependency of this parameter
    
    std::string expressionCopy;
    
    {
        QMutexLocker k(&expressionMutex);
        expressionCopy = expressions[dimension].expression;
    }
    
    if  (!replaceAllOcurrencesOfToken(expressionCopy, "getValue", false,dimension)) {
        return ;
    }
    
    if (!replaceAllOcurrencesOfToken(expressionCopy, "getValueAtTime", false, dimension)) {
        return;
    }
    
    if (!replaceAllOcurrencesOfToken(expressionCopy, "getDerivativeAtTime", false, dimension)) {
        return;
    }
    
    if (!replaceAllOcurrencesOfToken(expressionCopy, "getIntegrateFromTimeToTime", false, dimension)) {
        return;
    }
    
    if (!replaceAllOcurrencesOfToken(expressionCopy, "get", true, dimension)) {
        return;
    }
    
    if (!replaceAllOcurrencesOfToken(expressionCopy, "getAt", true, dimension)) {
        return;
    }
    
    ///This will register the listeners
    std::string error;
    bool success = Natron::interpretPythonScript(expressionCopy, &error);
    if (!error.empty()) {
        qDebug() << error.c_str();
    }
    assert(success);
    
    
}

static void compileExpression(const std::string& expression,PyCodeObject** code,PyObject** globalDict)
{
    *globalDict = PyModule_GetDict(PyImport_AddModule("__main__"));
    *code = (PyCodeObject*)Py_CompileString(expression.c_str(), "<string>", Py_file_input);
    if (PyErr_Occurred() || !*code) {
#ifdef DEBUG
        PyErr_Print();
#endif
        //In theory it should not happen since in validateExpression we ran the interpreter using PyRun_SimpleString
        throw std::runtime_error("failed to compile the expression");
    }
}

std::string
KnobHelper::validateExpression(const std::string& expression,int dimension,bool hasRetVariable,std::string* resultAsString)
{
    if (expression.empty()) {
        throw std::invalid_argument("empty expression");;
    }
    std::string exprCpy = expression;
    
    //if !hasRetVariable the expression is expected to be single-line
    if (!hasRetVariable) {
        std::size_t foundNewLine = expression.find("\n");
        if (foundNewLine != std::string::npos) {
            throw std::invalid_argument("unexpected new line character \'\\n\'");
        }
        //preprend the line with "ret = ..."
        std::string toInsert("ret = ");
        exprCpy.insert(0, toInsert);
    }
    
    declareCurrentKnobVariable_Python(this,dimension,exprCpy);
    
    ///Try to compile the expression and evaluate it, if it doesn't have a good syntax, throw an exception
    ///with the error.
    std::string error;
    
    {
        QMutexLocker k(&_expressionRecursionLevelMutex);
        assert(_expressionsRecursionLevel == 0);
        ++_expressionsRecursionLevel;
        
        //this is python code to redirect stdout/stderr
        std::string stdOutErr =
        "import sys \n"
        "class CatchOutErr:\n"
        "   def __init__(self):\n"
        "       self.value = ''\n"
        "   def write(self, txt):\n"
        "       self.value += txt\n"
        "catchOutErr = CatchOutErr()\n"
        "sys.stdout = catchOutErr\n"
        "sys.stderr = catchOutErr\n";
        
        PyObject *pModule = PyImport_AddModule("__main__"); //create main module , borrowed ref
        PyRun_SimpleString(stdOutErr.c_str()); //invoke code to redirect
        PyRun_SimpleString(exprCpy.c_str());
        PyObject *catcher = PyObject_GetAttrString(pModule,"catchOutErr"); //get our catchOutErr created above, new ref
        assert(catcher);
        
        PyErr_Print(); //make python print any errors
        
        PyObject *output = PyObject_GetAttrString(catcher,"value"); //get the stdout and stderr from our catchOutErr object, new ref
        
        PyObject *ret = PyObject_GetAttrString(pModule,"ret"); //get our ret variable created above
        
        if (!ret || PyErr_Occurred()) {
#ifdef DEBUG
            PyErr_Print();
#endif
            --_expressionsRecursionLevel;
            throw std::runtime_error("return value must be assigned to the \"ret\" variable");
        }
        
        error = std::string(PyString_AsString(output));
        
        Py_DECREF(catcher);
        Py_DECREF(output);
        
        
        
        --_expressionsRecursionLevel;
        
        if (!error.empty()) {
            throw std::runtime_error(error);
        }
        
        
        /// execute
        Knob<double>* isDouble = dynamic_cast<Knob<double>*>(this);
        Knob<int>* isInt = dynamic_cast<Knob<int>*>(this);
        Knob<bool>* isBool = dynamic_cast<Knob<bool>*>(this);
        Knob<std::string>* isString = dynamic_cast<Knob<std::string>*>(this);
        if (isDouble) {
            double r = isDouble->pyObjectToType(ret);
            *resultAsString = QString::number(r).toStdString();
        } else if (isInt) {
            int r = isInt->pyObjectToType(ret);
            *resultAsString = QString::number(r).toStdString();
        } else if (isBool) {
            bool r = isBool->pyObjectToType(ret);
            *resultAsString = r ? "True" : "False";
        } else {
            assert(isString);
            *resultAsString = isString->pyObjectToType(ret);
        }

        
    }
 
    
    return exprCpy;
}

void
KnobHelper::setExpression(int dimension,const std::string& expression,bool hasRetVariable)
{
    ///Clear previous expr
    clearExpression(dimension);
    
    if (expression.empty()) {
        return ;
    }
    
    std::string exprResult;
    std::string exprCpy = validateExpression(expression, dimension, hasRetVariable,&exprResult);
    
    
    ///Compile the expression
    {
        QMutexLocker k(&_imp->expressionMutex);
        _imp->expressions[dimension].hasRet = hasRetVariable;
        
        ///This may throw an exception upon failure
        compileExpression(exprCpy, &_imp->expressions[dimension].code, &_imp->expressions[dimension].global_dict);
    }
    
    
    //Try to execute the expression at least once to check that it works.
    {
        QMutexLocker k(&_expressionRecursionLevelMutex);
        assert(_expressionsRecursionLevel == 0);
        ++_expressionsRecursionLevel;
        
        try {
        
            PyObject* ret = executeExpression(dimension);
            Py_DECREF(ret); //< new ref

        } catch (const std::runtime_error& e) {
            --_expressionsRecursionLevel;
            clearExpression(dimension);
            throw e;
        }
        --_expressionsRecursionLevel;

    }
    
    
    //Set internal fields
    {
        QMutexLocker k(&_imp->expressionMutex);
        
        ///The expression is good so far, register it and add listeners (dependencies)
        
        _imp->expressions[dimension].expression = exprCpy;
        _imp->expressions[dimension].originalExpression = expression;
    }
    
    //Parse listeners of the expression, to keep track of dependencies to indicate them to the user.
    {
        QMutexLocker k(&_expressionRecursionLevelMutex);
        assert(_expressionsRecursionLevel == 0);
        ++_expressionsRecursionLevel;
        _imp->parseListenersFromExpression(dimension);
        --_expressionsRecursionLevel;
        
    }
    
    //Notify the expr. has changed
    expressionChanged(dimension);
}

bool
KnobHelper::isExpressionUsingRetVariable(int dimension) const
{
    QMutexLocker k(&_imp->expressionMutex);
    return _imp->expressions[dimension].hasRet;
}

void
KnobHelper::clearExpression(int dimension)
{
    {
        QMutexLocker k(&_imp->expressionMutex);
        _imp->expressions[dimension].expression.clear();
        _imp->expressions[dimension].originalExpression.clear();
        Py_XDECREF(_imp->expressions[dimension].code); //< new ref
        _imp->expressions[dimension].code = 0;
        _imp->expressions[dimension].global_dict = 0; //< python borrowed ref!
    }
    {
        std::list<KnobI*> dependencies;
        {
            QWriteLocker kk(&_imp->mastersMutex);
            dependencies = _imp->expressions[dimension].dependencies;
            _imp->expressions[dimension].dependencies.clear();
        }
        for (std::list<KnobI*>::iterator it = dependencies.begin();
             it != dependencies.end(); ++it) {
            
            KnobHelper* other = dynamic_cast<KnobHelper*>(*it);
            assert(other);
            
            std::list<KnobI*> otherListeners;
            {
                QReadLocker otherMastersLocker(&other->_imp->mastersMutex);
                otherListeners = other->_imp->listeners;
            }
            std::list<KnobI*>::iterator  found = std::find(otherListeners.begin(), otherListeners.end(), this);
            if (found != otherListeners.end()) {
                
                if ( (*found)->getHolder() && (*found)->getSignalSlotHandler() ) {
                    ///hackish way to get a shared ptr to this knob
                    getHolder()->onKnobSlaved(this, other,dimension,false );
                }
                QObject::disconnect(other->getSignalSlotHandler().get(), SIGNAL(updateDependencies(int)), _signalSlotHandler.get(),
                                    SLOT(onExprDependencyChanged(int)));
                otherListeners.erase(found);
                
            }
            
            {
                QWriteLocker otherMastersLocker(&other->_imp->mastersMutex);
                other->_imp->listeners = otherListeners;
            }
        }
    }
    expressionChanged(dimension);
    
}

void
KnobHelper::expressionChanged(int dimension)
{
    
    if (_signalSlotHandler) {
        _signalSlotHandler->s_expressionChanged(dimension);
    }
    
}

PyObject*
KnobHelper::executeExpression(int dimension) const
{
    Expr exp;
    {
        QMutexLocker k(&_imp->expressionMutex);
        exp = _imp->expressions[dimension];
    }
    //returns a new ref, this function's documentation is not clear onto what it returns...
    //https://docs.python.org/2/c-api/veryhigh.html
    PyObject* evalRet = PyEval_EvalCode(exp.code, exp.global_dict, exp.global_dict);
    Py_XDECREF(evalRet);
    
    if (PyErr_Occurred()) {
#ifdef DEBUG
        PyErr_Print();
#endif
        throw std::runtime_error("Failed to execute compiled Python code");
    }
    PyObject* mainModule = PyImport_ImportModule("__main__");
    PyObject *ret = PyObject_GetAttrString(mainModule,"ret"); //get our ret variable created above
    
    if (!ret || PyErr_Occurred()) {
#ifdef DEBUG
        PyErr_Print();
#endif
        throw std::runtime_error("return value must be assigned to the \"ret\" variable");
    }
    return ret;
    
}

std::string
KnobHelper::getExpression(int dimension) const
{
    QMutexLocker k(&_imp->expressionMutex);
    return _imp->expressions[dimension].originalExpression;
}

KnobHolder*
KnobHelper::getHolder() const
{
    return _imp->holder;
}

void
KnobHelper::setAnimationEnabled(bool val)
{
    _imp->isAnimationEnabled = val;
}

bool
KnobHelper::isAnimationEnabled() const
{
    return canAnimate() && _imp->isAnimationEnabled;
}

void
KnobHelper::setName(const std::string & name)
{
    _imp->name = name;
}

const std::string &
KnobHelper::getName() const
{
    return _imp->name;
}

void
KnobHelper::setParentKnob(boost::shared_ptr<KnobI> knob)
{
    _imp->parentKnob = knob;
}

boost::shared_ptr<KnobI> KnobHelper::getParentKnob() const
{
    return _imp->parentKnob;
}

bool
KnobHelper::getIsSecret() const
{
    return _imp->IsSecret;
}

void
KnobHelper::setIsFrozen(bool frozen)
{
    if (_signalSlotHandler) {
        _signalSlotHandler->s_setFrozen(frozen);
    }
}

bool
KnobHelper::isEnabled(int dimension) const
{
    assert( 0 <= dimension && dimension < getDimension() );
    
    return _imp->enabled[dimension];
}

void
KnobHelper::setDirty(bool d)
{
    _signalSlotHandler->s_setDirty(d);
}

void
KnobHelper::setEvaluateOnChange(bool b)
{
    assert( QThread::currentThread() == qApp->thread() );
    _imp->EvaluateOnChange = b;
}

bool
KnobHelper::getIsPersistant() const
{
    return _imp->IsPersistant;
}

void
KnobHelper::setIsPersistant(bool b)
{
    _imp->IsPersistant = b;
}

void
KnobHelper::setCanUndo(bool val)
{
    _imp->CanUndo = val;
}

bool
KnobHelper::getCanUndo() const
{
    return _imp->CanUndo;
}

bool
KnobHelper::getEvaluateOnChange() const
{
    return _imp->EvaluateOnChange;
}

void
KnobHelper::setHintToolTip(const std::string & hint)
{
    _imp->tooltipHint = hint;
    if (_signalSlotHandler) {
        _signalSlotHandler->s_helpChanged();
    }
    
}

const std::string &
KnobHelper::getHintToolTip() const
{
    return _imp->tooltipHint;
}

void
KnobHelper::setCustomInteract(const boost::shared_ptr<OfxParamOverlayInteract> & interactDesc)
{
    assert( QThread::currentThread() == qApp->thread() );
    _imp->customInteract = interactDesc;
}

boost::shared_ptr<OfxParamOverlayInteract> KnobHelper::getCustomInteract() const
{
    assert( QThread::currentThread() == qApp->thread() );
    
    return _imp->customInteract;
}

void
KnobHelper::swapOpenGLBuffers()
{
}

void
KnobHelper::redraw()
{
    if (_imp->gui) {
        _imp->gui->redraw();
    }
}

void
KnobHelper::getViewportSize(double &width,
                            double &height) const
{
    if (_imp->gui) {
        _imp->gui->getViewportSize(width, height);
    } else {
        width = 0;
        height = 0;
    }
}

void
KnobHelper::getPixelScale(double & xScale,
                          double & yScale) const
{
    if (_imp->gui) {
        _imp->gui->getPixelScale(xScale, yScale);
    } else {
        xScale = 0;
        yScale = 0;
    }
}

void
KnobHelper::getBackgroundColour(double &r,
                                double &g,
                                double &b) const
{
    if (_imp->gui) {
        _imp->gui->getBackgroundColour(r, g, b);
    } else {
        r = 0;
        g = 0;
        b = 0;
    }
}

void
KnobHelper::saveOpenGLContext()
{
    if (_imp->gui) {
        _imp->gui->saveOpenGLContext();
    }
}

void
KnobHelper::restoreOpenGLContext()
{
    if (_imp->gui) {
        _imp->gui->restoreOpenGLContext();
    }
}

void
KnobHelper::setOfxParamHandle(void* ofxParamHandle)
{
    assert( QThread::currentThread() == qApp->thread() );
    _imp->ofxParamHandle = ofxParamHandle;
}

void*
KnobHelper::getOfxParamHandle() const
{
    assert( QThread::currentThread() == qApp->thread() );
    
    return _imp->ofxParamHandle;
}

bool
KnobHelper::isMastersPersistenceIgnored() const
{
    QReadLocker l(&_imp->mastersMutex);
    
    return _imp->ignoreMasterPersistence;
}

void
KnobHelper::copyAnimationToClipboard() const
{
    if (_imp->gui) {
        _imp->gui->copyAnimationToClipboard();
    }
}

bool
KnobHelper::slaveTo(int dimension,
                    const boost::shared_ptr<KnobI> & other,
                    int otherDimension,
                    Natron::ValueChangedReasonEnum reason,
                    bool ignoreMasterPersistence)
{
    assert( 0 <= dimension && dimension < (int)_imp->masters.size() );
    assert( !other->isSlave(otherDimension) );
    
    {
        QWriteLocker l(&_imp->mastersMutex);
        if (_imp->masters[dimension].second) {
            return false;
        }
        _imp->ignoreMasterPersistence = ignoreMasterPersistence;
        _imp->masters[dimension].second = other;
        _imp->masters[dimension].first = otherDimension;
    }
    
    KnobHelper* helper = dynamic_cast<KnobHelper*>( other.get() );
    assert(helper);
    
    if (helper->_signalSlotHandler && _signalSlotHandler) {
        QObject::connect( helper->_signalSlotHandler.get(), SIGNAL( updateSlaves(int) ), _signalSlotHandler.get(), SLOT( onMasterChanged(int) ) );
        QObject::connect( helper->_signalSlotHandler.get(), SIGNAL( keyFrameSet(SequenceTime,int,int,bool) ),
                         _signalSlotHandler.get(), SLOT( onMasterKeyFrameSet(SequenceTime,int,int,bool) ) );
        QObject::connect( helper->_signalSlotHandler.get(), SIGNAL( keyFrameRemoved(SequenceTime,int,int) ),
                         _signalSlotHandler.get(), SLOT( onMasterKeyFrameRemoved(SequenceTime,int,int)) );
        
        QObject::connect( helper->_signalSlotHandler.get(), SIGNAL( keyFrameMoved(int,int,int) ),
                         _signalSlotHandler.get(), SLOT( onMasterKeyFrameMoved(int,int,int) ) );
        QObject::connect( helper->_signalSlotHandler.get(), SIGNAL(animationRemoved(int) ),
                         _signalSlotHandler.get(), SLOT(onMasterAnimationRemoved(int)) );
        
    }
    
    clone(other,dimension);
    
    if (_signalSlotHandler) {
        ///Notify we want to refresh
        if (reason == Natron::eValueChangedReasonPluginEdited) {
            _signalSlotHandler->s_knobSlaved(dimension,true);
        }
    }
    evaluateValueChange(dimension, reason);
    
    ///Register this as a listener of the master
    helper->addListener(false,dimension,this);
    
    return true;
}

std::pair<int,boost::shared_ptr<KnobI> > KnobHelper::getMaster(int dimension) const
{
    assert(dimension >= 0 && dimension < (int)_imp->masters.size());
    QReadLocker l(&_imp->mastersMutex);
    
    return _imp->masters[dimension];
}

void
KnobHelper::resetMaster(int dimension)
{
    assert(dimension >= 0);
    _imp->masters[dimension].second.reset();
    _imp->masters[dimension].first = -1;
    _imp->ignoreMasterPersistence = false;
}

bool
KnobHelper::isSlave(int dimension) const
{
    assert(dimension >= 0);
    QReadLocker l(&_imp->mastersMutex);
    
    return bool(_imp->masters[dimension].second);
}

std::vector< std::pair<int,boost::shared_ptr<KnobI> > > KnobHelper::getMasters_mt_safe() const
{
    QReadLocker l(&_imp->mastersMutex);
    
    return _imp->masters;
}

void
KnobHelper::checkAnimationLevel(int dimension)
{
    AnimationLevelEnum level = Natron::eAnimationLevelNone;
    
    if ( getHolder() && getHolder()->getApp() ) {
        boost::shared_ptr<Curve> c = getCurve(dimension);
        SequenceTime time = getHolder()->getApp()->getTimeLine()->currentFrame();
        if (c->getKeyFramesCount() > 0) {
            KeyFrame kf;
            bool found = c->getKeyFrameWithTime(time, &kf);;
            if (found) {
                level = Natron::eAnimationLevelOnKeyframe;
            } else {
                level = Natron::eAnimationLevelInterpolatedValue;
            }
        } else {
            level = Natron::eAnimationLevelNone;
        }
    }
    setAnimationLevel(dimension,level);
}

void
KnobHelper::setAnimationLevel(int dimension,
                              Natron::AnimationLevelEnum level)
{
    {
        QMutexLocker l(&_imp->animationLevelMutex);
        assert( dimension < (int)_imp->animationLevel.size() );
        _imp->animationLevel[dimension] = level;
    }
    if ( _signalSlotHandler && _imp->gui && !_imp->gui->isGuiFrozenForPlayback() ) {
        _signalSlotHandler->s_animationLevelChanged( dimension,(int)level );
    }
}

Natron::AnimationLevelEnum
KnobHelper::getAnimationLevel(int dimension) const
{
    ///if the knob is slaved to another knob, returns the other knob value
    std::pair<int,boost::shared_ptr<KnobI> > master = getMaster(dimension);
    
    if (master.second) {
        return master.second->getAnimationLevel(master.first);
    }
    
    QMutexLocker l(&_imp->animationLevelMutex);
    if ( dimension > (int)_imp->animationLevel.size() ) {
        throw std::invalid_argument("Knob::getAnimationLevel(): Dimension out of range");
    }
    
    return _imp->animationLevel[dimension];
}

void
KnobHelper::deleteAnimationBeforeTime(int time,
                                      int dimension,
                                      Natron::ValueChangedReasonEnum reason)
{
    assert( 0 <= dimension && dimension < getDimension() );
    KeyFrame k;
    bool ok = _imp->curves[dimension]->getPreviousKeyframeTime(time, &k);
    while (ok) {
        deleteValueAtTime(k.getTime(), dimension, reason);
        ok = _imp->curves[dimension]->getPreviousKeyframeTime(time, &k);
    }
}

void
KnobHelper::deleteAnimationAfterTime(int time,
                                     int dimension,
                                     Natron::ValueChangedReasonEnum reason)
{
    assert( 0 <= dimension && dimension < getDimension() );
    KeyFrame k;
    bool ok = _imp->curves[dimension]->getNextKeyframeTime(time, &k);
    while (ok) {
        deleteValueAtTime(k.getTime(), dimension, reason);
        ok = _imp->curves[dimension]->getNextKeyframeTime(time, &k);
    }
}

bool
KnobHelper::getKeyFrameTime(int index,
                            int dimension,
                            double* time) const
{
    assert( 0 <= dimension && dimension < getDimension() );
    if ( !isAnimated(dimension) ) {
        return false;
    }
    boost::shared_ptr<Curve> curve = getCurve(dimension); //< getCurve will return the master's curve if any
    assert(curve);
    KeyFrame kf;
    bool ret = curve->getKeyFrameWithIndex(index, &kf);
    if (ret) {
        *time = kf.getTime();
    }
    
    return ret;
}

bool
KnobHelper::getLastKeyFrameTime(int dimension,
                                double* time) const
{
    assert( 0 <= dimension && dimension < getDimension() );
    if ( !isAnimated(dimension) ) {
        return false;
    }
    
    boost::shared_ptr<Curve> curve = getCurve(dimension);  //< getCurve will return the master's curve if any
    assert(curve);
    *time = curve->getMaximumTimeCovered();
    
    return true;
}

bool
KnobHelper::getFirstKeyFrameTime(int dimension,
                                 double* time) const
{
    return getKeyFrameTime(0, dimension, time);
}

int
KnobHelper::getKeyFramesCount(int dimension) const
{
    return getCurve(dimension)->getKeyFramesCount();   //< getCurve will return the master's curve if any
}

bool
KnobHelper::getNearestKeyFrameTime(int dimension,
                                   double time,
                                   double* nearestTime) const
{
    assert( 0 <= dimension && dimension < getDimension() );
    if ( !isAnimated(dimension) ) {
        return false;
    }
    
    boost::shared_ptr<Curve> curve = getCurve(dimension);  //< getCurve will return the master's curve if any
    assert(curve);
    KeyFrame kf;
    bool ret = curve->getNearestKeyFrameWithTime(time, &kf);
    if (ret) {
        *nearestTime = kf.getTime();
    }
    
    return ret;
}

int
KnobHelper::getKeyFrameIndex(int dimension,
                             double time) const
{
    assert( 0 <= dimension && dimension < getDimension() );
    if ( !isAnimated(dimension) ) {
        return -1;
    }
    
    boost::shared_ptr<Curve> curve = getCurve(dimension);  //< getCurve will return the master's curve if any
    assert(curve);
    
    return curve->keyFrameIndex(time);
}

void
KnobHelper::onMasterChanged(KnobI* master,
                            int masterDimension)
{
    ///Map to the good dimension
    
    MastersMap masters;
    {
        QReadLocker l(&_imp->mastersMutex);
        masters = _imp->masters;
    }
    for (U32 i = 0; i < masters.size(); ++i) {
        if (masters[i].second.get() == master && masters[i].first == masterDimension) {
            
            ///We still want to clone the master's dimension because otherwise we couldn't edit the curve e.g in the curve editor
            ///For example we use it for roto knobs where selected beziers have their knobs slaved to the gui knobs
            clone(master,i);
            
            evaluateValueChange(i, Natron::eValueChangedReasonSlaveRefresh);
            
            return;
        }
    }
    
    ///The master must exist.
    assert(false);
}


void
KnobHelper::onExprDependencyChanged(KnobI* knob,int /*dimension*/)
{
    int dim = -1;
    
    {
        QMutexLocker k(&_imp->expressionMutex);
        for (int i = 0; i < _imp->dimension; ++i) {
            std::list<KnobI*>::iterator found = std::find(_imp->expressions[i].dependencies.begin(),_imp->expressions[i].dependencies.end(),knob);
            if (found != _imp->expressions[i].dependencies.end()) {
                dim = i;
                break;
            }
        }
    }
    assert(dim != -1);
    evaluateValueChange(dim, Natron::eValueChangedReasonSlaveRefresh);
    
}

void
KnobHelper::cloneExpressions(KnobI* other)
{
    assert((int)_imp->expressions.size() == getDimension());
    try {
        for (int i = 0; i < getDimension(); ++i) {
            std::string expr = other->getExpression(i);
            bool hasRet = other->isExpressionUsingRetVariable(i);
            (void)setExpression(i, expr,hasRet);
        }
    } catch(...) {
        ///ignore errors
    }
}

void
KnobHelper::addListener(bool isExpression,int fromExprDimension,KnobI* knob)
{
    assert(fromExprDimension != -1);
    KnobHelper* slave = dynamic_cast<KnobHelper*>(knob);
    
    if ( getHolder() && slave->getSignalSlotHandler() && getSignalSlotHandler() ) {
        ///hackish way to get a shared ptr to this knob
        slave->getHolder()->onKnobSlaved(slave, this,fromExprDimension,true );
    }
    
    if (slave->_signalSlotHandler && _signalSlotHandler) {
        if (!isExpression) {
            QObject::connect(_signalSlotHandler.get() , SIGNAL( updateSlaves(int) ),slave->_signalSlotHandler.get() , SLOT( onMasterChanged(int) ) );
        } else {
            QObject::connect(_signalSlotHandler.get() , SIGNAL( updateDependencies(int) ),slave->_signalSlotHandler.get() , SLOT( onExprDependencyChanged(int) ) );
            
            QMutexLocker k(&slave->_imp->expressionMutex);
            slave->_imp->expressions[fromExprDimension].dependencies.push_back(this);
        }
    }
    if (knob != this) {
        QWriteLocker l(&_imp->mastersMutex);
        
        _imp->listeners.push_back(knob);
        
    }
    
    
}

void
KnobHelper::removeListener(KnobI* knob)
{
    KnobHelper* other = dynamic_cast<KnobHelper*>(knob);
    
    if (other->_signalSlotHandler && _signalSlotHandler) {
        QObject::disconnect( other->_signalSlotHandler.get(), SIGNAL( updateSlaves(int) ), _signalSlotHandler.get(), SLOT( onMasterChanged(int) ) );
    }
    
    QWriteLocker l(&_imp->mastersMutex);
    std::list<KnobI*>::iterator found = std::find(_imp->listeners.begin(), _imp->listeners.end(), knob);
    
    if ( found != _imp->listeners.end() ) {
        _imp->listeners.erase(found);
    }
}


void
KnobHelper::getListeners(std::list<KnobI*> & listeners) const
{
    QReadLocker l(&_imp->mastersMutex);
    
    listeners = _imp->listeners;
}

SequenceTime
KnobHelper::getCurrentTime() const
{
    KnobHolder* holder = getHolder();
    return holder && holder->getApp() ? holder->getCurrentTime() : 0;
}

/***************************KNOB HOLDER******************************************/

struct KnobHolder::KnobHolderPrivate
{
    AppInstance* app;
    std::vector< boost::shared_ptr<KnobI> > knobs;
    bool knobsInitialized;
    bool isSlave;
    
    ///Use to count the recursion in the function calls
    /* The image effect actions which may trigger a recursive action call on a single instance are...
     
     kOfxActionBeginInstanceChanged
     kOfxActionInstanceChanged
     kOfxActionEndInstanceChanged
     The interact actions which may trigger a recursive action to be called on the associated plugin instance are...
     
     kOfxInteractActionGainFocus
     kOfxInteractActionKeyDown
     kOfxInteractActionKeyRepeat
     kOfxInteractActionKeyUp
     kOfxInteractActionLoseFocus
     kOfxInteractActionPenDown
     kOfxInteractActionPenMotion
     kOfxInteractActionPenUp
     
     The image effect actions which may be called recursively are...
     
     kOfxActionBeginInstanceChanged
     kOfxActionInstanceChanged
     kOfxActionEndInstanceChanged
     kOfxImageEffectActionGetClipPreferences
     The interact actions which may be called recursively are...
     
     kOfxInteractActionDraw
     
     */
    ThreadStorage<int> actionsRecursionLevel;
    
    ///If true, when the actionsRecursionLevel hit 0, it will trigger an evaluation.
    struct EvaluationRequest
    {
        KnobI* requester; //< the last requester
        bool isSignificant; //< is it a significant evaluation ?
        
        EvaluationRequest()
        : requester(0), isSignificant(false)
        {
        }
    };
    
    EvaluationRequest evaluateQueue;
    mutable QMutex paramsEditLevelMutex;
    KnobHolder::MultipleParamsEditLevel paramsEditLevel;
    mutable QMutex evaluationBlockedMutex;
    int evaluationBlocked;
    
    QMutex knobsFrozenMutex;
    bool knobsFrozen;
    
    mutable QMutex hasAnimationMutex;
    bool hasAnimation;
    
    KnobHolderPrivate(AppInstance* appInstance_)
        : app(appInstance_)
          , knobs()
          , knobsInitialized(false)
          , isSlave(false)
          , actionsRecursionLevel()
          , evaluateQueue()
          , paramsEditLevel(PARAM_EDIT_OFF)
          , evaluationBlockedMutex(QMutex::Recursive)
          , evaluationBlocked(0)
          , knobsFrozenMutex()
          , knobsFrozen(false)
          , hasAnimationMutex()
          , hasAnimation(false)
    {
        // Initialize local data on the main-thread
        ///Don't remove the if condition otherwise this will crash because QApp is not initialized yet for Natron settings.
        if (appInstance_) {
            actionsRecursionLevel.setLocalData(0);
        }
    }
};

KnobHolder::KnobHolder(AppInstance* appInstance)
: _imp( new KnobHolderPrivate(appInstance) )
{
}

KnobHolder::~KnobHolder()
{
    for (U32 i = 0; i < _imp->knobs.size(); ++i) {
        KnobHelper* helper = dynamic_cast<KnobHelper*>( _imp->knobs[i].get() );
        assert(helper);
        helper->_imp->holder = 0;
    }
}

void
KnobHolder::unblockEvaluation()
{
    QMutexLocker l(&_imp->evaluationBlockedMutex);
    
    --_imp->evaluationBlocked;
    assert(_imp->evaluationBlocked >= 0);
}

void
KnobHolder::blockEvaluation()
{
    QMutexLocker l(&_imp->evaluationBlockedMutex);
    
    ++_imp->evaluationBlocked;
}

bool
KnobHolder::isEvaluationBlocked() const
{
    QMutexLocker l(&_imp->evaluationBlockedMutex);
    
    return _imp->evaluationBlocked > 0;
}

KnobHolder::MultipleParamsEditLevel
KnobHolder::getMultipleParamsEditLevel() const
{
    QMutexLocker l(&_imp->paramsEditLevelMutex);
    
    return _imp->paramsEditLevel;
}

void
KnobHolder::setMultipleParamsEditLevel(KnobHolder::MultipleParamsEditLevel level)
{
    QMutexLocker l(&_imp->paramsEditLevelMutex);
    
    _imp->paramsEditLevel = level;
}

AppInstance*
KnobHolder::getApp() const
{
    return _imp->app;
}

void
KnobHolder::initializeKnobsPublic()
{
    initializeKnobs();
    _imp->knobsInitialized = true;
}

void
KnobHolder::addKnob(boost::shared_ptr<KnobI> k)
{
    _imp->knobs.push_back(k);
}

void
KnobHolder::removeKnob(KnobI* knob)
{
    for (U32 i = 0; i < _imp->knobs.size(); ++i) {
        if (_imp->knobs[i].get() == knob) {
            _imp->knobs.erase(_imp->knobs.begin() + i);
            break;
        }
    }
}

void
KnobHolder::onGuiFrozenChange(bool frozen)
{
    ///The issue with this is if the user toggles off the global frozen mode
    ///and the knobs are already frozen because for instance they are already rendering something
    ///that would unfrozen them, though this is very unlikely that the user does it.
    setKnobsFrozen(frozen);
}

void
KnobHolder::refreshAfterTimeChange(SequenceTime time)
{
    if (getApp()->isGuiFrozen()) {
        return;
    }
    for (U32 i = 0; i < _imp->knobs.size(); ++i) {
        _imp->knobs[i]->onTimeChanged(time);
    }
}

void
KnobHolder::refreshInstanceSpecificKnobsOnly(SequenceTime time)
{
    if (getApp()->isGuiFrozen()) {
        return;
    }
    for (U32 i = 0; i < _imp->knobs.size(); ++i) {
        if ( _imp->knobs[i]->isInstanceSpecific() ) {
            _imp->knobs[i]->onTimeChanged(time);
        }
    }
}

boost::shared_ptr<KnobI> KnobHolder::getKnobByName(const std::string & name) const
{
    const std::vector<boost::shared_ptr<KnobI> > & knobs = getKnobs();
    
    for (U32 i = 0; i < knobs.size(); ++i) {
        if (knobs[i]->getName() == name) {
            return knobs[i];
        }
    }
    
    return boost::shared_ptr<KnobI>();
}

const std::vector< boost::shared_ptr<KnobI> > &
KnobHolder::getKnobs() const
{
    ///MT-safe since it never changes
    return _imp->knobs;
}

void
KnobHolder::slaveAllKnobs(KnobHolder* other)
{
    if (_imp->isSlave) {
        return;
    }
    ///Call it prior to slaveTo: it will set the master pointer as pointing to other
    onAllKnobsSlaved(true,other);
    
    blockEvaluation();
    
    const std::vector<boost::shared_ptr<KnobI> > & otherKnobs = other->getKnobs();
    const std::vector<boost::shared_ptr<KnobI> > & thisKnobs = getKnobs();
    for (U32 i = 0; i < otherKnobs.size(); ++i) {
        
        if (otherKnobs[i]->isDeclaredByPlugin()) {
            boost::shared_ptr<KnobI> foundKnob;
            for (U32 j = 0; j < thisKnobs.size(); ++j) {
                if ( thisKnobs[j]->getName() == otherKnobs[i]->getName() ) {
                    foundKnob = thisKnobs[j];
                    break;
                }
            }
            assert(foundKnob);
            int dims = foundKnob->getDimension();
            for (int j = 0; j < dims; ++j) {
                foundKnob->slaveTo(j, otherKnobs[i], j);
            }
        }
    }
    unblockEvaluation();
    evaluate_public(NULL, true, Natron::eValueChangedReasonUserEdited);
    _imp->isSlave = true;
}

bool
KnobHolder::isSlave() const
{
    return _imp->isSlave;
}

void
KnobHolder::unslaveAllKnobs()
{
    if (!_imp->isSlave) {
        return;
    }
    const std::vector<boost::shared_ptr<KnobI> > & thisKnobs = getKnobs();
    blockEvaluation();
    for (U32 i = 0; i < thisKnobs.size(); ++i) {
        int dims = thisKnobs[i]->getDimension();
        for (int j = 0; j < dims; ++j) {
            if ( (i == thisKnobs.size() - 1) && (j == dims - 1) ) {
                unblockEvaluation();
            }
            if ( thisKnobs[i]->isSlave(j) ) {
                thisKnobs[i]->unSlave(j,true);
            }
        }
    }
    _imp->isSlave = false;
    onAllKnobsSlaved(false,(KnobHolder*)NULL);
}

void
KnobHolder::beginKnobsValuesChanged_public(Natron::ValueChangedReasonEnum reason)
{
    ///cannot run in another thread.
    assert( QThread::currentThread() == qApp->thread() );
    
    RECURSIVE_ACTION();
    beginKnobsValuesChanged(reason);
}

void
KnobHolder::endKnobsValuesChanged_public(Natron::ValueChangedReasonEnum reason)
{
    ///cannot run in another thread.
    assert( QThread::currentThread() == qApp->thread() );
    
    RECURSIVE_ACTION();
    endKnobsValuesChanged(reason);
}

void
KnobHolder::onKnobValueChanged_public(KnobI* k,
                                      Natron::ValueChangedReasonEnum reason,
                                      SequenceTime time)
{
    ///cannot run in another thread.
    assert( QThread::currentThread() == qApp->thread() );
    if (isEvaluationBlocked() || !_imp->knobsInitialized) {
        return;
    }
    RECURSIVE_ACTION();
    onKnobValueChanged(k, reason,time);
}

void
KnobHolder::evaluate_public(KnobI* knob,
                            bool isSignificant,
                            Natron::ValueChangedReasonEnum reason)
{
    ///cannot run in another thread.
    assert( QThread::currentThread() == qApp->thread() );
    if ( isEvaluationBlocked() ) {
        return;
    }
    _imp->evaluateQueue.isSignificant |= isSignificant;
    if (!_imp->evaluateQueue.requester) {
        _imp->evaluateQueue.requester = knob;
    }
    if (getRecursionLevel() == 0) {
        evaluate(_imp->evaluateQueue.requester, _imp->evaluateQueue.isSignificant,reason);
        _imp->evaluateQueue.requester = NULL;
        _imp->evaluateQueue.isSignificant = false;
        if ( isSignificant && getApp() ) {
            ///Don't trigger autosaves for buttons
            Button_Knob* isButton = dynamic_cast<Button_Knob*>(knob);
            if (!isButton) {
                getApp()->triggerAutoSave();
            }
        }
    }
}

void
KnobHolder::checkIfRenderNeeded()
{
    ///cannot run in another thread.
    assert( QThread::currentThread() == qApp->thread() );
    if ( (getRecursionLevel() == 1) && (_imp->evaluateQueue.requester != NULL) ) {
        evaluate(_imp->evaluateQueue.requester, _imp->evaluateQueue.isSignificant,Natron::eValueChangedReasonUserEdited);
        _imp->evaluateQueue.requester = NULL;
        _imp->evaluateQueue.isSignificant = false;
    }
}

void
KnobHolder::assertActionIsNotRecursive() const
{
# ifdef DEBUG
    
    ///Only check recursions which are on a render threads, because we do authorize recursions in getRegionOfDefinition and such which
    ///always happen in the main thread.
    if (QThread::currentThread() != qApp->thread()) {
        int recursionLvl = getRecursionLevel();
        
        if ( getApp() && getApp()->isShowingDialog() ) {
            return;
        }
        if (recursionLvl != 0) {
            qDebug() << "A non-recursive action has been called recursively.";
        }
    }
# endif // DEBUG
}

void
KnobHolder::incrementRecursionLevel()
{
    if ( !_imp->actionsRecursionLevel.hasLocalData() ) {
        _imp->actionsRecursionLevel.setLocalData(1);
    } else {
        _imp->actionsRecursionLevel.localData() += 1;
    }
    
    /*NamedKnobHolder* named = dynamic_cast<NamedKnobHolder*>(this);
     if (named) {
     std::cout << named->getName_mt_safe() <<  " INCR: " << _imp->actionsRecursionLevel.localData() <<  " ( "<<
     QThread::currentThread() <<
     " ) main-thread = " << (QThread::currentThread() == qApp->thread()) << std::endl;
     }
     */
}

void
KnobHolder::decrementRecursionLevel()
{
    assert( _imp->actionsRecursionLevel.hasLocalData() );
    _imp->actionsRecursionLevel.localData() -= 1;
    /*NamedKnobHolder* named = dynamic_cast<NamedKnobHolder*>(this);
     if (named) {
     std::cout << named->getName_mt_safe() << " DECR: "<< _imp->actionsRecursionLevel.localData() <<  " ( "<<QThread::currentThread() <<
     " ) main-thread = " << (QThread::currentThread() == qApp->thread()) << std::endl;
     }
     */
}

int
KnobHolder::getRecursionLevel() const
{
    
    if ( _imp->actionsRecursionLevel.hasLocalData() ) {
        /* const NamedKnobHolder* named = dynamic_cast<const NamedKnobHolder*>(this);
         if (named) {
         std::cout << named->getName_mt_safe() << " GET: "<< _imp->actionsRecursionLevel.localData() <<  " ( "<<
         QThread::currentThread() <<
         " ) main-thread = " << (QThread::currentThread() == qApp->thread()) << std::endl;
         }*/
        return _imp->actionsRecursionLevel.localData();
    } else {
        /*const NamedKnobHolder* named = dynamic_cast<const NamedKnobHolder*>(this);
         if (named) {
         std::cout << named->getName_mt_safe() << "GET: "<< 0 <<  "( "<< QThread::currentThread() <<
         " ) main-thread = " << (QThread::currentThread() == qApp->thread()) << std::endl;
         }
         */
        return 0;
    }
}

void
KnobHolder::restoreDefaultValues()
{
    aboutToRestoreDefaultValues();
    
    for (U32 i = 0; i < _imp->knobs.size(); ++i) {
        Button_Knob* isBtn = dynamic_cast<Button_Knob*>( _imp->knobs[i].get() );
        Page_Knob* isPage = dynamic_cast<Page_Knob*>( _imp->knobs[i].get() );
        Group_Knob* isGroup = dynamic_cast<Group_Knob*>( _imp->knobs[i].get() );
        Separator_Knob* isSeparator = dynamic_cast<Separator_Knob*>( _imp->knobs[i].get() );
        
        ///Don't restore buttons and the node label
        if ( !isBtn && !isPage && !isGroup && !isSeparator && (_imp->knobs[i]->getName() != kUserLabelKnobName) ) {
            _imp->knobs[i]->blockEvaluation();
            for (int d = 0; d < _imp->knobs[i]->getDimension(); ++d) {
                _imp->knobs[i]->resetToDefaultValue(d);
            }
            _imp->knobs[i]->unblockEvaluation();
        }
    }
    evaluate_public(NULL, true, Natron::eValueChangedReasonUserEdited);
}

void
KnobHolder::setKnobsFrozen(bool frozen)
{
    {
        QMutexLocker l(&_imp->knobsFrozenMutex);
        if (frozen == _imp->knobsFrozen) {
            return;
        }
        _imp->knobsFrozen = frozen;
    }
    const std::vector<boost::shared_ptr<KnobI> > & knobs = getKnobs();
    
    for (U32 i = 0; i < knobs.size(); ++i) {
        knobs[i]->setIsFrozen(frozen);
    }
}

void
KnobHolder::dequeueValuesSet()
{
    assert(QThread::currentThread() == qApp->thread());
    
    for (U32 i = 0; i < _imp->knobs.size(); ++i) {
        _imp->knobs[i]->dequeueValuesSet(false);
    }
}

SequenceTime
KnobHolder::getCurrentTime() const
{
    return getApp()->getTimeLine()->currentFrame();
}

void
KnobHolder::discardAppPointer()
{
    _imp->app = 0;
}


bool
KnobHolder::getHasAnimation() const
{
    QMutexLocker k(&_imp->hasAnimationMutex);
    return _imp->hasAnimation;
}


void
KnobHolder::setHasAnimation(bool hasAnimation)
{
    QMutexLocker k(&_imp->hasAnimationMutex);
    _imp->hasAnimation = hasAnimation;
}


void
KnobHolder::updateHasAnimation()
{
    bool hasAnimation = false;
    for (std::vector<boost::shared_ptr<KnobI> >::const_iterator it = _imp->knobs.begin(); it != _imp->knobs.end(); ++it) {
        if ((*it)->hasAnimation()) {
            hasAnimation = true;
            break;
        }
    }
    QMutexLocker k(&_imp->hasAnimationMutex);
    _imp->hasAnimation = hasAnimation;
}

/***************************STRING ANIMATION******************************************/
void
AnimatingString_KnobHelper::cloneExtraData(KnobI* other,int /*dimension*/ )
{
    AnimatingString_KnobHelper* isAnimatedString = dynamic_cast<AnimatingString_KnobHelper*>(other);
    
    if (isAnimatedString) {
        _animation->clone( isAnimatedString->getAnimation() );
    }
}

void
AnimatingString_KnobHelper::cloneExtraData(KnobI* other,
                                           SequenceTime offset,
                                           const RangeD* range,
                                           int /*dimension*/)
{
    AnimatingString_KnobHelper* isAnimatedString = dynamic_cast<AnimatingString_KnobHelper*>(other);
    
    if (isAnimatedString) {
        _animation->clone(isAnimatedString->getAnimation(), offset, range);
    }
}

AnimatingString_KnobHelper::AnimatingString_KnobHelper(KnobHolder* holder,
                                                       const std::string &description,
                                                       int dimension,
                                                       bool declaredByPlugin)
: Knob<std::string>(holder,description,dimension,declaredByPlugin)
, _animation( new StringAnimationManager(this) )
{
}

AnimatingString_KnobHelper::~AnimatingString_KnobHelper()
{
    delete _animation;
}

void
AnimatingString_KnobHelper::stringToKeyFrameValue(int time,
                                                  const std::string & v,
                                                  double* returnValue)
{
    _animation->insertKeyFrame(time, v, returnValue);
}

void
AnimatingString_KnobHelper::stringFromInterpolatedValue(double interpolated,
                                                        std::string* returnValue) const
{
    _animation->stringFromInterpolatedIndex(interpolated, returnValue);
}

void
AnimatingString_KnobHelper::animationRemoved_virtual(int /*dimension*/)
{
    _animation->clearKeyFrames();
}

void
AnimatingString_KnobHelper::keyframeRemoved_virtual(int /*dimension*/,
                                                    double time)
{
    _animation->removeKeyFrame(time);
}

std::string
AnimatingString_KnobHelper::getStringAtTime(double time,
                                            int dimension) const
{
    std::string ret;
    
    if ( _animation->hasCustomInterp() ) {
        bool succeeded = _animation->customInterpolation(time, &ret);
        if (!succeeded) {
            return getValue(dimension);
        } else {
            return ret;
        }
    }
    
    return ret;
}

void
AnimatingString_KnobHelper::setCustomInterpolation(customParamInterpolationV1Entry_t func,
                                                   void* ofxParamHandle)
{
    _animation->setCustomInterpolation(func, ofxParamHandle);
}

void
AnimatingString_KnobHelper::loadAnimation(const std::map<int,std::string> & keyframes)
{
    _animation->load(keyframes);
}

void
AnimatingString_KnobHelper::saveAnimation(std::map<int,std::string>* keyframes) const
{
    _animation->save(keyframes);
}

/***************************KNOB EXPLICIT TEMPLATE INSTANTIATION******************************************/


template class Knob<int>;
template class Knob<double>;
template class Knob<bool>;
template class Knob<std::string>;

