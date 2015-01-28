//  Natron
//
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
//
//  Created by Frédéric Devernay on 20/03/2014.
//
//

#ifndef Natron_Engine_ViewerInstancePrivate_h
#define Natron_Engine_ViewerInstancePrivate_h

#include "ViewerInstance.h"

#include <map>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QThread>
#include <QtCore/QCoreApplication>

#include "Engine/OutputSchedulerThread.h"
#include "Engine/FrameEntry.h"
#include "Engine/Settings.h"
#include "Engine/TextureRect.h"

namespace Natron {
class FrameEntry;
class FrameParams;
}

//namespace Natron {

struct RenderViewerArgs
{
    RenderViewerArgs(boost::shared_ptr<const Natron::Image> inputImage_,
                     const TextureRect & texRect_,
                     ViewerInstance::DisplayChannelsEnum channels_,
                      Natron::ImagePremultiplicationEnum srcPremult_,
                     int closestPowerOf2_,
                     int bitDepth_,
                     double gain_,
                     double offset_,
                     const Natron::Color::Lut* srcColorSpace_,
                     const Natron::Color::Lut* colorSpace_)
        : inputImage(inputImage_)
          , texRect(texRect_)
          , channels(channels_)
          , srcPremult(srcPremult_)
          , closestPowerOf2(closestPowerOf2_)
          , bitDepth(bitDepth_)
          , gain(gain_)
          , offset(offset_)
          , srcColorSpace(srcColorSpace_)
          , colorSpace(colorSpace_)
    {
    }

    boost::shared_ptr<const Natron::Image> inputImage;
    TextureRect texRect;
    ViewerInstance::DisplayChannelsEnum channels;
    Natron::ImagePremultiplicationEnum srcPremult;
    int closestPowerOf2;
    int bitDepth;
    double gain;
    double offset;
    const Natron::Color::Lut* srcColorSpace;
    const Natron::Color::Lut* colorSpace;
};

/// parameters send from the scheduler thread to updateViewer() (which runs in the main thread)
class UpdateViewerParams : public BufferableObject
{
    
public:
    
    UpdateViewerParams()
        : ramBuffer(NULL)
          , mustFreeRamBuffer(false)
          , textureIndex(0)
          , time(0)
          , textureRect()
          , srcPremult(Natron::eImagePremultiplicationOpaque)
          , bytesCount(0)
          , gain(1.)
          , offset(0.)
          , mipMapLevel(0)
          , premult(Natron::eImagePremultiplicationOpaque)
          , lut(Natron::eViewerColorSpaceSRGB)
          , cachedFrame()
          , image()
          , rod()
          , renderAge(0)
          , isSequential(false)
    {
    }
    
    virtual ~UpdateViewerParams() {
        if (mustFreeRamBuffer) {
            free(ramBuffer);
        }
    }
    
    virtual std::size_t sizeInRAM() const OVERRIDE FINAL
    {
        return bytesCount;
    }

    unsigned char* ramBuffer;
    bool mustFreeRamBuffer; //< set to true when !cachedFrame
    int textureIndex;
    int time;
    TextureRect textureRect;
    Natron::ImagePremultiplicationEnum srcPremult;
    size_t bytesCount;
    double gain;
    double offset;
    unsigned int mipMapLevel;
    Natron::ImagePremultiplicationEnum premult;
    Natron::ViewerColorSpaceEnum lut;
    
    // put a shared_ptr here, so that the cache entry is never released before the end of updateViewer()
    boost::shared_ptr<Natron::FrameEntry> cachedFrame;
    boost::shared_ptr<Natron::Image> image;
    RectD rod;
    U64 renderAge;
    bool isSequential;
};

struct ViewerInstance::ViewerInstancePrivate
: public QObject, public LockManagerI<Natron::FrameEntry>
{
    Q_OBJECT

public:
    
    ViewerInstancePrivate(const ViewerInstance* parent)
        : instance(parent)
    , uiContext(NULL)
    , forceRenderMutex()
    , forceRender(false)
    , updateViewerPboIndex(0)
    , viewerParamsMutex()
    , viewerParamsGain(1.)
    , viewerParamsLut(Natron::eViewerColorSpaceSRGB)
    , viewerParamsAutoContrast(false)
    , viewerParamsChannels(eDisplayChannelsRGB)
    , viewerMipMapLevel(0)
    , activeInputsMutex()
    , activeInputs()
    , lastRenderedHashMutex()
    , lastRenderedHash(0)
    , lastRenderedHashValid(false)
    , renderAge(0)
    , renderAgeMutex()
    {

        activeInputs[0] = -1;
        activeInputs[1] = -1;
    }
    
    

    void redrawViewer()
    {
        emit mustRedrawViewer();
    }
    
public:
    
    virtual void lock(const boost::shared_ptr<Natron::FrameEntry>& entry) OVERRIDE FINAL
    {


        QMutexLocker l(&textureBeingRenderedMutex);
        std::list<boost::shared_ptr<Natron::FrameEntry> >::iterator it =
                std::find(textureBeingRendered.begin(), textureBeingRendered.end(), entry);
        while ( it != textureBeingRendered.end() ) {
            textureBeingRenderedCond.wait(&textureBeingRenderedMutex);
            it = std::find(textureBeingRendered.begin(), textureBeingRendered.end(), entry);
        }
        ///Okay the image is not used by any other thread, claim that we want to use it
        assert( it == textureBeingRendered.end() );
        textureBeingRendered.push_back(entry);
    }
    
    virtual bool tryLock(const boost::shared_ptr<Natron::FrameEntry>& entry) OVERRIDE FINAL
    {
        QMutexLocker l(&textureBeingRenderedMutex);
        std::list<boost::shared_ptr<Natron::FrameEntry> >::iterator it =
        std::find(textureBeingRendered.begin(), textureBeingRendered.end(), entry);
        if ( it != textureBeingRendered.end() ) {
            return false;
        }
        ///Okay the image is not used by any other thread, claim that we want to use it
        assert( it == textureBeingRendered.end() );
        textureBeingRendered.push_back(entry);
        return true;
    }
    
    virtual void unlock(const boost::shared_ptr<Natron::FrameEntry>& entry) OVERRIDE FINAL
    {

        QMutexLocker l(&textureBeingRenderedMutex);
        std::list<boost::shared_ptr<Natron::FrameEntry> >::iterator it =
                std::find(textureBeingRendered.begin(), textureBeingRendered.end(), entry);
        ///The image must exist, otherwise this is a bug
        assert( it != textureBeingRendered.end() );
        textureBeingRendered.erase(it);
        ///Notify all waiting threads that we're finished
        textureBeingRenderedCond.wakeAll();
    }

    U64 getRenderAge()
    {
        QMutexLocker k(&renderAgeMutex);
        
        return renderAge;
    }
    
    bool checkAgeNoUpdate(U64 age)
    {
        QMutexLocker k(&renderAgeMutex);
        assert(age <= renderAge);
        return age == renderAge;
    }
    
    bool checkAndUpdateRenderAge(U64 age)
    {
        QMutexLocker k(&renderAgeMutex);
        assert(age <= renderAge);
        if (age < renderAge) {
            return false;
        }
        if (renderAge == std::numeric_limits<U64>::max()) {
            renderAge = 0;
        } else {
            ++renderAge;
        }
        return true;
    }

    
    
public slots:
    /**
     * @brief Slot called internally by the renderViewer() function when it wants to refresh the OpenGL viewer.
     * Do not call this yourself.
     **/
    void updateViewer(boost::shared_ptr<UpdateViewerParams> params);

signals:
   
    void mustRedrawViewer();

public:
    const ViewerInstance* const instance;
    OpenGLViewerI* uiContext; // written in the main thread before render thread creation, accessed from render thread
    mutable QMutex forceRenderMutex;
    bool forceRender; /*!< true when we want to by-pass the cache*/


    // updateViewer: stuff for handling the execution of updateViewer() in the main thread, @see UpdateViewerParams
    //is always called on the main thread, but the thread running renderViewer MUST
    //wait the entire time. This flag is here to make the renderViewer() thread wait
    //until the texture upload is finished by the main thread.
    int updateViewerPboIndex;                // always accessed in the main thread: initialized in the constructor, then always accessed and modified by updateViewer()


    // viewerParams: The viewer parameters that may be accessed from the GUI
    mutable QMutex viewerParamsMutex;   //< protects viewerParamsGain, viewerParamsLut, viewerParamsAutoContrast, viewerParamsChannels
    double viewerParamsGain;           /*!< Current gain setting in the GUI. Not affected by autoContrast. */
    Natron::ViewerColorSpaceEnum viewerParamsLut; /*!< a value coding the current color-space used to render.
                                                 0 = sRGB ,  1 = linear , 2 = Rec 709*/
    bool viewerParamsAutoContrast;
    DisplayChannelsEnum viewerParamsChannels;
    unsigned int viewerMipMapLevel; //< the mipmap level the viewer should render at (0 == no downscaling)
    
    ////Commented-out: Now that the VideoEngine is gone, there can be several threads running  the render function
    ////and we have no way to identify the threads since they belong to a thread pool.
    // store the threadId of the VideoEngine thread - used for debugging purposes
    //mutable QMutex threadIdMutex;
    //QThread *threadIdVideoEngine;
    
    mutable QMutex activeInputsMutex;
    int activeInputs[2]; //< indexes of the inputs used for the wipe
    
    QMutex lastRenderedHashMutex;
    U64 lastRenderedHash;
    bool lastRenderedHashValid;
    
    mutable QMutex textureBeingRenderedMutex;
    QWaitCondition textureBeingRenderedCond;
    std::list<boost::shared_ptr<Natron::FrameEntry> > textureBeingRendered; ///< a list of all the texture being rendered simultaneously
    
private:
    
    QMutex renderAgeMutex;
    U64 renderAge;
};


//} // namespace Natron


#endif // ifndef Natron_Engine_ViewerInstancePrivate_h
