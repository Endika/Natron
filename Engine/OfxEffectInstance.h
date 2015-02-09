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

#ifndef NATRON_ENGINE_OFXNODE_H_
#define NATRON_ENGINE_OFXNODE_H_

#include "Global/Macros.h"
#include <map>
#include <string>
#ifndef Q_MOC_RUN
#include <boost/shared_ptr.hpp>
#endif
CLANG_DIAG_OFF(deprecated)
#include <QtCore/QMutex>
#include <QtCore/QString>
#include <QtCore/QObject>
CLANG_DIAG_ON(deprecated)
#include <QtCore/QStringList>
//ofx
#include "ofxhImageEffect.h"

#include "Engine/EffectInstance.h"

#ifdef DEBUG
#include "Engine/ThreadStorage.h"
#endif

class QReadWriteLock;
class OfxClipInstance;
class Button_Knob;
class OverlaySupport;
class NodeSerialization;
class KnobSerialization;
namespace Natron {
class Node;
class OfxImageEffectInstance;
class OfxOverlayInteract;
}

class AbstractOfxEffectInstance
    : public Natron::OutputEffectInstance
{
public:

    AbstractOfxEffectInstance(boost::shared_ptr<Natron::Node> node)
        : Natron::OutputEffectInstance(node)
    {
    }

    virtual ~AbstractOfxEffectInstance()
    {
    }

    virtual void createOfxImageEffectInstance(OFX::Host::ImageEffect::ImageEffectPlugin* plugin,
                                              const std::string & context,const NodeSerialization* serialization,
                                               const std::list<boost::shared_ptr<KnobSerialization> >& paramValues,
                                              bool allowFileDialogs,
                                              bool disableRenderScaleSupport) = 0;
    static QStringList makePluginGrouping(const std::string & pluginIdentifier,
                                          int versionMajor, int versionMinor,
                                          const std::string & pluginLabel,
                                          const std::string & grouping) WARN_UNUSED_RETURN;
    static std::string makePluginLabel(const std::string & shortLabel,
                                       const std::string & label,
                                       const std::string & longLabel) WARN_UNUSED_RETURN;
    
};

class OfxEffectInstance
    : public AbstractOfxEffectInstance
{
    Q_OBJECT

public:
    OfxEffectInstance(boost::shared_ptr<Natron::Node> node);

    virtual ~OfxEffectInstance();

    void createOfxImageEffectInstance(OFX::Host::ImageEffect::ImageEffectPlugin* plugin,
                                      const std::string & context,const NodeSerialization* serialization,
                                       const std::list<boost::shared_ptr<KnobSerialization> >& paramValues,
                                      bool allowFileDialogs,
                                      bool disableRenderScaleSupport) OVERRIDE FINAL;

    Natron::OfxImageEffectInstance* effectInstance() WARN_UNUSED_RETURN
    {
        return _effect;
    }

    const Natron::OfxImageEffectInstance* effectInstance() const WARN_UNUSED_RETURN
    {
        return _effect;
    }

    void setAsOutputNode()
    {
        _isOutput = true;
    }

    const std::string & getShortLabel() const WARN_UNUSED_RETURN;

    typedef std::vector<OFX::Host::ImageEffect::ClipDescriptor*> MappedInputV;
    MappedInputV inputClipsCopyWithoutOutput() const WARN_UNUSED_RETURN;

    bool isCreated() const
    {
        return _created;
    }

    bool isInitialized() const
    {
        return _initialized;
    }

    const std::string & ofxGetOutputPremultiplication() const;

    /**
     * @brief Calls syncPrivateDataAction from another thread than the main thread. The actual
     * call of the action will take place in the main-thread.
     **/
    void syncPrivateData_other_thread()
    {
        emit syncPrivateDataRequested();
    }

public:
    /********OVERRIDEN FROM EFFECT INSTANCE*************/
    virtual int getMajorVersion() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual int getMinorVersion() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual bool isGenerator() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual bool isReader() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual bool isWriter() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual bool isOutput() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual bool isGeneratorAndFilter() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual bool isOpenFX() const OVERRIDE FINAL WARN_UNUSED_RETURN
    {
        return true;
    }
    virtual bool isEffectCreated() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual bool makePreviewByDefault() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual int getMaxInputCount() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual std::string getPluginID() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual std::string getPluginLabel() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual void getPluginGrouping(std::list<std::string>* grouping) const OVERRIDE FINAL;
    virtual std::string getDescription() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual std::string getInputLabel (int inputNb) const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual bool isInputOptional(int inputNb) const OVERRIDE WARN_UNUSED_RETURN;
    virtual bool isInputMask(int inputNb) const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual bool isInputRotoBrush(int inputNb) const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual int getRotoBrushInputIndex() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual Natron::StatusEnum getRegionOfDefinition(U64 hash,SequenceTime time, const RenderScale & scale, int view, RectD* rod) OVERRIDE WARN_UNUSED_RETURN;

    /// calculate the default rod for this effect instance
    virtual void calcDefaultRegionOfDefinition(U64 hash,SequenceTime time,int view, const RenderScale & scale, RectD *rod)  OVERRIDE;
    virtual void getRegionsOfInterest(SequenceTime time,
                                  const RenderScale & scale,
                                  const RectD & outputRoD, //!< full RoD in canonical coordinates
                                  const RectD & renderWindow, //!< the region to be rendered in the output image, in Canonical Coordinates
                                  int view,
                                Natron::EffectInstance::RoIMap* ret) OVERRIDE FINAL;
    virtual Natron::EffectInstance::FramesNeededMap getFramesNeeded(SequenceTime time) WARN_UNUSED_RETURN;
    virtual void getFrameRange(SequenceTime *first,SequenceTime *last) OVERRIDE;
    virtual void initializeOverlayInteract() OVERRIDE FINAL;
    virtual bool hasOverlay() const OVERRIDE FINAL;
    virtual void drawOverlay(double scaleX, double scaleY) OVERRIDE FINAL;
    virtual bool onOverlayPenDown(double scaleX, double scaleY, const QPointF & viewportPos, const QPointF & pos) OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual bool onOverlayPenMotion(double scaleX, double scaleY,
                                    const QPointF & viewportPos, const QPointF & pos) OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual bool onOverlayPenUp(double scaleX, double scaleY, const QPointF & viewportPos, const QPointF & pos) OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual bool onOverlayKeyDown(double scaleX, double scaleY, Natron::Key key, Natron::KeyboardModifiers modifiers) OVERRIDE FINAL;
    virtual bool onOverlayKeyUp(double scaleX, double scaleY, Natron::Key key,Natron::KeyboardModifiers modifiers) OVERRIDE FINAL;
    virtual bool onOverlayKeyRepeat(double scaleX, double scaleY, Natron::Key key,Natron::KeyboardModifiers modifiers) OVERRIDE FINAL;
    virtual bool onOverlayFocusGained(double scaleX, double scaleY) OVERRIDE FINAL;
    virtual bool onOverlayFocusLost(double scaleX, double scaleY) OVERRIDE FINAL;
    virtual void setCurrentViewportForOverlays(OverlaySupport* viewport) OVERRIDE FINAL;
    virtual void beginKnobsValuesChanged(Natron::ValueChangedReasonEnum reason) OVERRIDE;
    virtual void endKnobsValuesChanged(Natron::ValueChangedReasonEnum reason) OVERRIDE;
    virtual void knobChanged(KnobI* k, Natron::ValueChangedReasonEnum reason, int view, SequenceTime time,
                             bool originatedFromMainThread) OVERRIDE;
    virtual void beginEditKnobs() OVERRIDE;
    virtual Natron::StatusEnum render(SequenceTime time,
                                      const RenderScale& originalScale,
                                      const RenderScale & mappedScale,
                                      const RectI & roi, //!< renderWindow in pixel coordinates
                                      int view,
                                      bool isSequentialRender,
                                      bool isRenderResponseToUserInteraction,
                                      boost::shared_ptr<Natron::Image> output) OVERRIDE WARN_UNUSED_RETURN;
    virtual bool isIdentity(SequenceTime time,
                            const RenderScale & scale,
                            const RectD & rod, //!< image rod in canonical coordinates
                            const double par,
                            int view,
                            SequenceTime* inputTime,
                            int* inputNb) OVERRIDE;
    virtual Natron::EffectInstance::RenderSafetyEnum renderThreadSafety() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual void purgeCaches() OVERRIDE;

    /**
     * @brief Does this effect supports tiling ?
     * http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#kOfxImageEffectPropSupportsTiles
     * If a clip or plugin does not support tiled images, then the host should supply
     * full RoD images to the effect whenever it fetches one.
     **/
    virtual bool supportsTiles() const OVERRIDE FINAL WARN_UNUSED_RETURN;

    virtual bool doesTemporalClipAccess() const OVERRIDE FINAL WARN_UNUSED_RETURN;

    /**
     * @brief Does this effect supports multiresolution ?
     * http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#kOfxImageEffectPropSupportsMultiResolution
     * Multiple resolution images mean...
     * input and output images can be of any size
     * input and output images can be offset from the origin
     **/
    virtual bool supportsMultiResolution() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual bool supportsMultipleClipsPAR() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual void onInputChanged(int inputNo) OVERRIDE FINAL;
    virtual void restoreClipPreferences() OVERRIDE FINAL;
    virtual std::vector<std::string> supportedFileFormats() const OVERRIDE FINAL;
    virtual Natron::StatusEnum beginSequenceRender(SequenceTime first,
                                               SequenceTime last,
                                               SequenceTime step,
                                               bool interactive,
                                               const RenderScale & scale,
                                               bool isSequentialRender,
                                               bool isRenderResponseToUserInteraction,
                                               int view) OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual Natron::StatusEnum endSequenceRender(SequenceTime first,
                                             SequenceTime last,
                                             SequenceTime step,
                                             bool interactive,
                                             const RenderScale & scale,
                                             bool isSequentialRender,
                                             bool isRenderResponseToUserInteraction,
                                             int view) OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual void addAcceptedComponents(int inputNb, std::list<Natron::ImageComponentsEnum>* comps) OVERRIDE FINAL;
    virtual void addSupportedBitDepth(std::list<Natron::ImageBitDepthEnum>* depths) const OVERRIDE FINAL;
    virtual void getPreferredDepthAndComponents(int inputNb, Natron::ImageComponentsEnum* comp, Natron::ImageBitDepthEnum* depth) const OVERRIDE FINAL;
    virtual Natron::SequentialPreferenceEnum getSequentialPreference() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual Natron::ImagePremultiplicationEnum getOutputPremultiplication() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual void checkOFXClipPreferences(double time,
                                     const RenderScale & scale,
                                     const std::string & reason,
                                         bool forceGetClipPrefAction) OVERRIDE FINAL;

public:

    virtual double getPreferredAspectRatio() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual double getPreferredFrameRate() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual bool getCanTransform() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual bool getCanApplyTransform(Natron::EffectInstance** effect) const  OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual Natron::StatusEnum getTransform(SequenceTime time,
                                            const RenderScale& renderScale,
                                            int view,
                                            Natron::EffectInstance** inputToTransform,
                                            Transform::Matrix3x3* transform) OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual void rerouteInputAndSetTransform(int inputNb,Natron::EffectInstance* newInput,
                                             int newInputNb,const Transform::Matrix3x3& m) OVERRIDE FINAL;
    virtual void clearTransform(int inputNb) OVERRIDE FINAL;

    virtual bool isFrameVarying() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    /********OVERRIDEN FROM EFFECT INSTANCE: END*************/

    OfxClipInstance* getClipCorrespondingToInput(int inputNo) const;



public slots:

    void onSyncPrivateDataRequested();


signals:

    void syncPrivateDataRequested();

private:
    /** @brief Enumerates the contexts a plugin can be used in */
    enum ContextEnum
    {
        eContextNone,
        eContextGenerator,
        eContextFilter,
        eContextTransition,
        eContextPaint,
        eContextGeneral,
        eContextRetimer,
        eContextReader,
        eContextWriter,
    };

    ContextEnum mapToContextEnum(const std::string &s);


    void tryInitializeOverlayInteracts();

    void initializeContextDependentParams();

#ifdef DEBUG
/*
    Debug helper to track plug-in that do setValue calls that are forbidden
 
 http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#SettingParams
 Officially, setValue calls are allowed during the following actions:
 
 The Create Instance Action
 The The Begin Instance Changed Action
 The The Instance Changed Action
 The The End Instance Changed Action
 The The Sync Private Data Action

 
 */

    void setCanSetValue(bool can)
    {
        _canSetValue.localData() = can;
    }

    void invalidateCanSetValueFlag()
    {
        _canSetValue.localData() = true;
    }


    bool isDuringActionThatCanSetValue() const
    {
        if (_canSetValue.hasLocalData()) {
            return _canSetValue.localData();
        } else {
            ///Not during an action
            return true;
        }
    }

    class CanSetSetValueFlag_RAII
    {
        OfxEffectInstance* effect;
        
        public:
        
        CanSetSetValueFlag_RAII(OfxEffectInstance* effect,bool canSetValue)
        : effect(effect)
        {
            effect->setCanSetValue(canSetValue);
        }
        
        ~CanSetSetValueFlag_RAII()
        {
            effect->invalidateCanSetValueFlag();
        }
    };

    virtual bool checkCanSetValue() const { return isDuringActionThatCanSetValue(); }

#define SET_CAN_SET_VALUE(canSetValue) OfxEffectInstance::CanSetSetValueFlag_RAII canSetValueSetter(this,canSetValue)

#else

#define SET_CAN_SET_VALUE(canSetValue) ( (void)0 )

#endif


private:
    Natron::OfxImageEffectInstance* _effect;
    std::string _natronPluginID; //< small cache to avoid calls to generateImageEffectClassName
    bool _isOutput; //if the OfxNode can output a file somehow
    bool _penDown; // true when the overlay trapped a penDow action
    Natron::OfxOverlayInteract* _overlayInteract; // ptr to the overlay interact if any
    std::list< void* > _overlaySlaves; //void* to actually a KnobI* but stored as void to avoid dereferencing

    bool _created; // true after the call to createInstance
    bool _initialized; //true when the image effect instance has been created and populated
    boost::shared_ptr<Button_Knob> _renderButton; //< render button for writers
    mutable EffectInstance::RenderSafetyEnum _renderSafety;
    mutable bool _wasRenderSafetySet;
    mutable QReadWriteLock* _renderSafetyLock;
    ContextEnum _context;
    mutable QReadWriteLock* _preferencesLock;
#ifdef DEBUG
    Natron::ThreadStorage<bool> _canSetValue;
#endif
};

#endif // NATRON_ENGINE_OFXNODE_H_
