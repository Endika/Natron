//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
//
//  Created by Frédéric Devernay on 03/09/13.
//
//

#ifndef NATRON_ENGINE_OFXIMAGEEFFECTINSTANCE_H_
#define NATRON_ENGINE_OFXIMAGEEFFECTINSTANCE_H_

#include <string>
#include <cstdarg>
#ifndef Q_MOC_RUN
#include <boost/shared_ptr.hpp>
#endif
#include <ofxhImageEffect.h>

#include "Global/GlobalDefines.h"

class OfxClipInstance;
class OfxEffectInstance;
class RectD;

namespace OFX {
    namespace Host {
        namespace Property
        {
            class Set;
        }
    }
}

namespace Natron {
class Image;

class OfxImageEffectInstance
    : public OFX::Host::ImageEffect::Instance
{
public:
    OfxImageEffectInstance(OFX::Host::ImageEffect::ImageEffectPlugin* plugin,
                           OFX::Host::ImageEffect::Descriptor & desc,
                           const std::string & context,
                           bool interactive);

    virtual ~OfxImageEffectInstance();

    void setOfxEffectInstance(OfxEffectInstance* node)
    {
        _ofxEffectInstance = node;
    }

    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    // overridden for ImageEffect::Instance
    
    /// call the effect entry point
    virtual OfxStatus mainEntry(const char *action,
                                const void *handle,
                                OFX::Host::Property::Set *inArgs,
                                OFX::Host::Property::Set *outArgs) OVERRIDE FINAL WARN_UNUSED_RETURN;

    virtual OFX::Host::Memory::Instance* newMemoryInstance(size_t nBytes) OVERRIDE FINAL WARN_UNUSED_RETURN;

    /// get default output fielding. This is passed into the clip prefs action
    /// and  might be mapped (if the host allows such a thing)
    virtual const std::string &getDefaultOutputFielding() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    /// make a clip
    virtual OFX::Host::ImageEffect::ClipInstance* newClipInstance(OFX::Host::ImageEffect::Instance* plugin,
                                                                  OFX::Host::ImageEffect::ClipDescriptor* descriptor,
                                                                  int index) OVERRIDE FINAL WARN_UNUSED_RETURN;
    virtual OfxStatus vmessage(const char* type,
                               const char* id,
                               const char* format,
                               va_list args) OVERRIDE FINAL;
    virtual OfxStatus setPersistentMessage(const char* type,
                                           const char* id,
                                           const char* format,
                                           va_list args) OVERRIDE FINAL;
    virtual OfxStatus clearPersistentMessage() OVERRIDE FINAL;

    //
    // live parameters
    //

    // The size of the current project in canonical coordinates.
    // The size of a project is a sub set of the kOfxImageEffectPropProjectExtent. For example a
    // project may be a PAL SD project, but only be a letter-box within that. The project size is
    // the size of this sub window.
    virtual void getProjectSize(double & xSize, double & ySize) const OVERRIDE FINAL;

    // The offset of the current project in canonical coordinates.
    // The offset is related to the kOfxImageEffectPropProjectSize and is the offset from the origin
    // of the project 'subwindow'. For example for a PAL SD project that is in letterbox form, the
    // project offset is the offset to the bottom left hand corner of the letter box. The project
    // offset is in canonical coordinates.
    virtual void getProjectOffset(double & xOffset, double & yOffset) const OVERRIDE FINAL;

    // The extent of the current project in canonical coordinates.
    // The extent is the size of the 'output' for the current project. See ProjectCoordinateSystems
    // for more infomation on the project extent. The extent is in canonical coordinates and only
    // returns the top right position, as the extent is always rooted at 0,0. For example a PAL SD
    // project would have an extent of 768, 576.
    virtual void getProjectExtent(double & xSize, double & ySize) const OVERRIDE FINAL;

    // The pixel aspect ratio of the current project
    virtual double getProjectPixelAspectRatio() const OVERRIDE FINAL WARN_UNUSED_RETURN;

    // The duration of the effect
    // This contains the duration of the plug-in effect, in frames.
    virtual double getEffectDuration() const OVERRIDE FINAL WARN_UNUSED_RETURN;

    // For an instance, this is the frame rate of the project the effect is in.
    virtual double getFrameRate() const OVERRIDE FINAL WARN_UNUSED_RETURN;

    /// This is called whenever a param is changed by the plugin so that
    /// the recursive instanceChangedAction will be fed the correct frame
    virtual double getFrameRecursive() const OVERRIDE FINAL WARN_UNUSED_RETURN;

    /// This is called whenever a param is changed by the plugin so that
    /// the recursive instanceChangedAction will be fed the correct
    /// renderScale
    virtual void getRenderScaleRecursive(double &x, double &y) const OVERRIDE FINAL;
    
    /// Run the clip preferences action from the effect.
    ///
    /// This will look into the input clips and output clip
    /// and set the following properties that the effect should
    /// fetch the image at.
    ///     - pixel depth
    ///     - components
    ///     - pixel aspect ratio
    /// It will also set on the effect itselff
    ///     - whether it is continuously samplable
    ///     - the premult state of the output
    ///     - whether the effect is frame varying
    ///     - the fielding of the output clip
    ///
    /// This will be run automatically by the effect in the following situations...
    ///     - an input clip is changed
    ///     - a clip preferences slave param is changed
    ///
    /// The host still needs to call this explicitly just after the effect is wired
    /// up.
    struct ClipPrefs {
        std::string components;
        std::string bitdepth;
        double par;
    };
    
    struct EffectPrefs {
        double frameRate;
        std::string fielding;
        std::string premult;
        bool continuous;
        bool frameVarying;
    };
    
    /**
     * We add some output parameters to the function so that we can delay the actual setting of the clip preferences
     **/
    bool getClipPreferences_safe(std::map<OfxClipInstance*, ClipPrefs>& clipPrefs,EffectPrefs& effectPrefs);

    /**
     * @brief To be called once no action is currently being run after getClipPreferences_safe was called.
     * Caller maintains a lock around this call to prevent race conditions.
     **/
    void updatePreferences_safe(double frameRate,const std::string& fielding,const std::string& premult,
                                bool continuous,bool frameVarying);
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    // overridden for Param::SetInstance

    /// make a parameter instance
    ///
    /// Client host code needs to implement this
    virtual OFX::Host::Param::Instance* newParam(const std::string & name, OFX::Host::Param::Descriptor & Descriptor) OVERRIDE FINAL WARN_UNUSED_RETURN;


    /// Triggered when the plug-in calls OfxParameterSuiteV1::paramEditBegin
    ///
    /// Client host code needs to implement this
    virtual OfxStatus editBegin(const std::string & name) OVERRIDE FINAL WARN_UNUSED_RETURN;

    /// Triggered when the plug-in calls OfxParameterSuiteV1::paramEditEnd
    ///
    /// Client host code needs to implement this
    virtual OfxStatus editEnd() OVERRIDE FINAL;

    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    // overridden for Progress::ProgressI

    /// Start doing progress.
    virtual void progressStart(const std::string &message) OVERRIDE FINAL;

    /// finish yer progress
    virtual void progressEnd() OVERRIDE FINAL;

    /// set the progress to some level of completion, returns
    /// false if you should abandon processing, true to continue
    virtual bool progressUpdate(double t) OVERRIDE FINAL WARN_UNUSED_RETURN;

    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    // overridden for TimeLine::TimeLineI

    /// get the current time on the timeline. This is not necessarily the same
    /// time as being passed to an action (eg render)
    virtual double timeLineGetTime() OVERRIDE FINAL WARN_UNUSED_RETURN;

    /// set the timeline to a specific time
    virtual void timeLineGotoTime(double t) OVERRIDE FINAL;

    /// get the first and last times available on the effect's timeline
    virtual void timeLineGetBounds(double &t1, double &t2) OVERRIDE FINAL;
    virtual int abort() OVERRIDE FINAL;

    //
    // END of OFX::Host::ImageEffect::Instance methods
    //
    OfxEffectInstance* getOfxEffectInstance() const WARN_UNUSED_RETURN
    {
        return _ofxEffectInstance;
    }

    /// to be called right away after populate() is called. It adds to their group all the params.
    /// This is done in a deferred manner as some params can sometimes not be defined in a good order.
    void addParamsToTheirParents();

    bool areAllNonOptionalClipsConnected() const;


    
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////
    //////////////////////////////////////// THREAD-LOCAL-STORAGE
    /*       These functions below set and unset data on the clip thread-local storage.
     See EffectInstance::Implementation::ScopedRenderArgs for an explanation on what is thread storage,
     why we use it and why some are on the clips on some aren't.
     */
    void setClipsView(int view);
    void discardClipsView();
    void setClipsMipMapLevel(unsigned int mipMapLevel);
    void discardClipsMipMapLevel();
    ////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    bool getCanApplyTransform(OfxClipInstance** clip) const;
    
private:
    OfxEffectInstance* _ofxEffectInstance; /* FIXME: OfxImageEffectInstance should be able to work without the node_ //
                                              Not easy since every Knob need a valid pointer to a node when
                                              KnobFactory::createKnob() is called. That's why we need to pass a pointer
                                              to an OfxParamInstance. Without this pointer we would be unable
                                              to track the knobs that have been created for 1 Node since OfxParamInstance
                                              is totally dissociated from Node.*/

    /*Use this to re-create parenting between effect's params.
       The key is the name of a param and the Instance a pointer to the associated effect.
       This has nothing to do with the base class _params member! */
    std::map<OFX::Host::Param::Instance*,std::string> _parentingMap;
};
} // namespace Natron

#endif // ifndef NATRON_ENGINE_OFXIMAGEEFFECTINSTANCE_H_
