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

#include "ViewerInstancePrivate.h"

#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>

CLANG_DIAG_OFF(deprecated)
#include <QtCore/QtGlobal>
#include <QtConcurrentMap> // QtCore on Qt4, QtConcurrent on Qt<<(
#include <QtCore/QFutureWatcher>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QCoreApplication>
CLANG_DIAG_ON(deprecated)

#include "Global/MemoryInfo.h"
#include "Engine/Node.h"
#include "Engine/ImageInfo.h"
#include "Engine/AppManager.h"
#include "Engine/AppInstance.h"
#include "Engine/MemoryFile.h"
#include "Engine/OfxEffectInstance.h"
#include "Engine/ImageInfo.h"
#include "Engine/TimeLine.h"
#include "Engine/Cache.h"
#include "Engine/Log.h"
#include "Engine/Lut.h"
#include "Engine/Settings.h"
#include "Engine/Project.h"
#include "Engine/OpenGLViewerI.h"
#include "Engine/Image.h"
#include "Engine/OutputSchedulerThread.h"

#ifndef M_LN2
#define M_LN2       0.693147180559945309417232121458176568  /* loge(2)        */
#endif

using namespace Natron;
using std::make_pair;
using boost::shared_ptr;


static void scaleToTexture8bits(std::pair<int,int> yRange,
                                const RenderViewerArgs & args,
                                ViewerInstance* viewer,
                                U32* output);
static void scaleToTexture32bits(std::pair<int,int> yRange,
                                 const RenderViewerArgs & args,
                                 ViewerInstance* viewer,
                                 float *output);
static std::pair<double, double>
findAutoContrastVminVmax(boost::shared_ptr<const Natron::Image> inputImage,
                         ViewerInstance::DisplayChannels channels,
                         const RectI & rect);
static void renderFunctor(std::pair<int,int> yRange,
                          const RenderViewerArgs & args,
                          ViewerInstance* viewer,
                          void *buffer);

/**
 *@brief Actually converting to ARGB... but it is called BGRA by
   the texture format GL_UNSIGNED_INT_8_8_8_8_REV
 **/
static unsigned int toBGRA(unsigned char r, unsigned char g, unsigned char b, unsigned char a) WARN_UNUSED_RETURN;
unsigned int
toBGRA(unsigned char r,
       unsigned char g,
       unsigned char b,
       unsigned char a)
{
    return (a << 24) | (r << 16) | (g << 8) | b;
}

const Natron::Color::Lut*
ViewerInstance::lutFromColorspace(Natron::ViewerColorSpaceEnum cs)
{
    const Natron::Color::Lut* lut;

    switch (cs) {
    case Natron::eViewerColorSpaceSRGB:
        lut = Natron::Color::LutManager::sRGBLut();
        break;
    case Natron::eViewerColorSpaceRec709:
        lut = Natron::Color::LutManager::Rec709Lut();
        break;
    case Natron::eViewerColorSpaceLinear:
    default:
        lut = 0;
        break;
    }
    if (lut) {
        lut->validate();
    }

    return lut;
}


class ViewerRenderingStarted_RAII
{
    ViewerInstance* _node;
    bool _didEmit;
public:
    
    ViewerRenderingStarted_RAII(ViewerInstance* node)
    : _node(node)
    {
        _didEmit = node->getNode()->notifyRenderingStarted();
        if (_didEmit) {
            _node->s_viewerRenderingStarted();
        }
    }
    
    ~ViewerRenderingStarted_RAII()
    {
        if (_didEmit) {
            _node->getNode()->notifyRenderingEnded();
            _node->s_viewerRenderingEnded();
        }
    }
};


Natron::EffectInstance*
ViewerInstance::BuildEffect(boost::shared_ptr<Natron::Node> n)
{
    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );

    return new ViewerInstance(n);
}

ViewerInstance::ViewerInstance(boost::shared_ptr<Node> node)
    : Natron::OutputEffectInstance(node)
      , _imp( new ViewerInstancePrivate(this) )
{
    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );

    if (node) {
        connect( node.get(),SIGNAL( nameChanged(QString) ),this,SLOT( onNodeNameChanged(QString) ) );
    }
    QObject::connect( this,SIGNAL( disconnectTextureRequest(int) ),this,SLOT( executeDisconnectTextureRequestOnMainThread(int) ) );
    QObject::connect( _imp.get(),SIGNAL( mustRedrawViewer() ),this,SLOT( redrawViewer() ) );
    QObject::connect( this,SIGNAL( s_callRedrawOnMainThread() ), this, SLOT( redrawViewer() ) );
}

ViewerInstance::~ViewerInstance()
{
    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );

    
    // If _imp->updateViewerRunning is true, that means that the next updateViewer call was
    // not yet processed. Since we're in the main thread and it is processed in the main thread,
    // there is no way to wait for it (locking the mutex would cause a deadlock).
    // We don't care, after all.
    //{
    //    QMutexLocker locker(&_imp->updateViewerMutex);
    //    assert(!_imp->updateViewerRunning);
    //}
    if (_imp->uiContext) {
        _imp->uiContext->removeGUI();
    }
}

RenderEngine*
ViewerInstance::createRenderEngine()
{
    return new ViewerRenderEngine(this);
}

void
ViewerInstance::getPluginGrouping(std::list<std::string>* grouping) const
{
    grouping->push_back(PLUGIN_GROUP_IMAGE);
}

OpenGLViewerI*
ViewerInstance::getUiContext() const
{
    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );

    return _imp->uiContext;
}

void
ViewerInstance::forceFullComputationOnNextFrame()
{
    // this is called by the GUI when the user presses the "Refresh" button.
    // It set the flag forceRender to true, meaning no cache will be used.

    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );

    QMutexLocker forceRenderLocker(&_imp->forceRenderMutex);
    _imp->forceRender = true;
}

void
ViewerInstance::clearLastRenderedImage()
{
    EffectInstance::clearLastRenderedImage();
    if (_imp->uiContext) {
        _imp->uiContext->clearLastRenderedImage();
    }
}

void
ViewerInstance::setUiContext(OpenGLViewerI* viewer)
{
    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );
    
    _imp->uiContext = viewer;
}

void
ViewerInstance::invalidateUiContext()
{
    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );
    _imp->uiContext = NULL;
}

void
ViewerInstance::onNodeNameChanged(const QString & name)
{
    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );

    ///update the gui tab name
    if (_imp->uiContext) {
        _imp->uiContext->onViewerNodeNameChanged(name);
    }
}


int
ViewerInstance::getMaxInputCount() const
{
    // runs in the render thread or in the main thread
    
    //MT-safe
    return getNode()->getMaxInputCount();
}

void
ViewerInstance::getFrameRange(SequenceTime *first,
                              SequenceTime *last)
{

    SequenceTime inpFirst = 1,inpLast = 1;
    int activeInputs[2];
    getActiveInputs(activeInputs[0], activeInputs[1]);
    EffectInstance* n1 = getInput(activeInputs[0]);
    if (n1) {
        n1->getFrameRange_public(n1->getRenderHash(),&inpFirst,&inpLast);
    }
    *first = inpFirst;
    *last = inpLast;

    inpFirst = 1;
    inpLast = 1;

    EffectInstance* n2 = getInput(activeInputs[1]);
    if (n2) {
        n2->getFrameRange_public(n2->getRenderHash(),&inpFirst,&inpLast);
        if (inpFirst < *first) {
            *first = inpFirst;
        }

        if (inpLast > *last) {
            *last = inpLast;
        }
    }
}

void
ViewerInstance::executeDisconnectTextureRequestOnMainThread(int index)
{
    assert( QThread::currentThread() == qApp->thread() );
    if (_imp->uiContext) {
        _imp->uiContext->disconnectInputTexture(index);
    }
}

Natron::StatusEnum
ViewerInstance::renderViewer(int view,
                             bool singleThreaded,
                             bool isSequentialRender,
                             U64 viewerHash,
                             bool canAbort,
                             boost::shared_ptr<ViewerInstance::ViewerArgs> args[2])
{
    if (!_imp->uiContext) {
        return eStatusFailed;
    }
    Natron::StatusEnum ret[2] = {
        eStatusReplyDefault, eStatusReplyDefault
    };
    for (int i = 0; i < 2; ++i) {
        if ( (i == 1) && (_imp->uiContext->getCompositingOperator() == Natron::eViewerCompositingOperatorNone) ) {
            break;
        }
        
        if (args[i] && args[i]->params) {
            assert(args[i]->params->textureIndex == i);
            ret[i] = renderViewer_internal(view, singleThreaded, isSequentialRender, viewerHash, canAbort,*args[i]);
            if (ret[i] == eStatusReplyDefault) {
                args[i].reset();
            }
        }
    }
    

    if ( (ret[0] == eStatusFailed) && (ret[1] == eStatusFailed) ) {
        return eStatusFailed;
    }

    return eStatusOK;
}

static void checkTreeCanRender_internal(Node* node,bool* ret)
{
    const std::vector<boost::shared_ptr<Node> > inputs = node->getInputs_copy();
    for (U32 i = 0; i < inputs.size(); ++i) {
        if (!inputs[i]) {
            if (!node->getLiveInstance()->isInputOptional(i)) {
                *ret = false;
                return;
            }
            
        } else {
            checkTreeCanRender_internal(inputs[i].get(), ret);
            if (!ret) {
                return;
            }
        }
    }
}

/**
 * @brief Returns false if the tree has unconnected mandatory inputs
 **/
static bool checkTreeCanRender(Node* node)
{
    bool ret = true;
    checkTreeCanRender_internal(node,&ret);
    return ret;
}


Natron::StatusEnum
ViewerInstance::getRenderViewerArgsAndCheckCache(SequenceTime time, int view, int textureIndex, U64 viewerHash,
                                                 ViewerArgs* outArgs)
{
    if (textureIndex == 0) {
        QMutexLocker l(&_imp->activeInputsMutex);
        outArgs->activeInputIndex =  _imp->activeInputs[0];
    } else {
        QMutexLocker l(&_imp->activeInputsMutex);
        outArgs->activeInputIndex =  _imp->activeInputs[1];
    }
    
    
    outArgs->activeInputToRender = getInput(outArgs->activeInputIndex);
    
    if (outArgs->activeInputToRender) {
        outArgs->activeInputToRender = outArgs->activeInputToRender->getNearestNonDisabled();
    }
    
    if (!outArgs->activeInputToRender || !checkTreeCanRender(outArgs->activeInputToRender->getNode().get())) {
        emit disconnectTextureRequest(textureIndex);
        return eStatusFailed;
    }
    
    {
        QMutexLocker forceRenderLocker(&_imp->forceRenderMutex);
        outArgs->forceRender = _imp->forceRender;
        _imp->forceRender = false;
    }
    
    ///instead of calling getRegionOfDefinition on the active input, check the image cache
    ///to see whether the result of getRegionOfDefinition is already present. A cache lookup
    ///might be much cheaper than a call to getRegionOfDefinition.
    ///
    ///Note that we can't yet use the texture cache because we would need the TextureRect identifyin
    ///the texture in order to retrieve from the cache, but to make the TextureRect we need the RoD!
    RenderScale scale;
    int mipMapLevel;
    {
        QMutexLocker l(&_imp->viewerParamsMutex);
        scale.x = Natron::Image::getScaleFromMipMapLevel(_imp->viewerMipMapLevel);
        scale.y = scale.x;
        mipMapLevel = _imp->viewerMipMapLevel;
    }
    
    assert(_imp->uiContext);
    int zoomMipMapLevel = getMipMapLevelFromZoomFactor();
    mipMapLevel = std::max( (double)mipMapLevel, (double)zoomMipMapLevel );
    
    // If it's eSupportsMaybe and mipMapLevel!=0, don't forget to update
    // this after the first call to getRegionOfDefinition().
    RenderScale scaleOne;
    scaleOne.x = scaleOne.y = 1.;
    EffectInstance::SupportsEnum supportsRS = outArgs->activeInputToRender->supportsRenderScaleMaybe();
    scale.x = scale.y = Natron::Image::getScaleFromMipMapLevel(mipMapLevel);
    
    
    int closestPowerOf2 = 1 << mipMapLevel;
    
    
    
    //The hash of the node to render
    outArgs->activeInputHash = outArgs->activeInputToRender->getHash();
    
    //The RoD returned by the plug-in
    RectD rod;
    
    //The bounds of the image in pixel coords.
    
    //Is this image originated from a generator which is dependent of the size of the project ? If true we should then discard the image from the cache
    //when the project's format changes.
    bool isRodProjectFormat = false;
    
    
    const double par = outArgs->activeInputToRender->getPreferredAspectRatio();
    
    ///Get the RoD here to be able to figure out what is the RoI of the Viewer.
    StatusEnum stat = outArgs->activeInputToRender->getRegionOfDefinition_public(outArgs->activeInputHash,time,
                                                                        supportsRS ==  eSupportsNo ? scaleOne : scale,
                                                                        view, &rod, &isRodProjectFormat);
    if (stat == eStatusFailed) {
        emit disconnectTextureRequest(textureIndex);
        return stat;
    }
    // update scale after the first call to getRegionOfDefinition
    if ( (supportsRS == eSupportsMaybe) && (mipMapLevel != 0) ) {
        supportsRS = (outArgs->activeInputToRender)->supportsRenderScaleMaybe();
        if (supportsRS == eSupportsNo) {
            scale.x = scale.y = 1.;
        }
    }
    
    isRodProjectFormat = ifInfiniteclipRectToProjectDefault(&rod);
    
    assert(_imp->uiContext);
    
    /*computing the RoI*/
    
    ////Texrect is the coordinates of the 4 corners of the texture in the bounds with the current zoom
    ////factor taken into account.
    RectI roi = _imp->uiContext->getImageRectangleDisplayedRoundedToTileSize(rod, par, mipMapLevel);
    
    if ( (roi.width() == 0) || (roi.height() == 0) ) {
        emit disconnectTextureRequest(textureIndex);
        outArgs->params.reset();
        return eStatusReplyDefault;
    }
    
    
    /////////////////////////////////////
    // start UpdateViewerParams scope
    //
    outArgs->params.reset(new UpdateViewerParams);
    outArgs->params->srcPremult = outArgs->activeInputToRender->getOutputPremultiplication();
    
    ///Texture rect contains the pixel coordinates in the image to be rendered
    outArgs->params->textureRect.x1 = roi.x1;
    outArgs->params->textureRect.x2 = roi.x2;
    outArgs->params->textureRect.y1 = roi.y1;
    outArgs->params->textureRect.y2 = roi.y2;
    outArgs->params->textureRect.w = roi.width();
    outArgs->params->textureRect.h = roi.height();
    outArgs->params->textureRect.closestPo2 = closestPowerOf2;
    outArgs->params->textureRect.par = par;
    
    outArgs->params->bytesCount = outArgs->params->textureRect.w * outArgs->params->textureRect.h * 4;
    assert(outArgs->params->bytesCount > 0);
    
    assert(_imp->uiContext);
    OpenGLViewerI::BitDepth bitDepth = _imp->uiContext->getBitDepth();
    
    //half float is not supported yet so it is the same as float
    if ( (bitDepth == OpenGLViewerI::FLOAT) || (bitDepth == OpenGLViewerI::HALF_FLOAT) ) {
        outArgs->params->bytesCount *= sizeof(float);
    }
    
    outArgs->params->time = time;
    outArgs->params->rod = rod;
    outArgs->params->mipMapLevel = (unsigned int)mipMapLevel;
    outArgs->params->textureIndex = textureIndex;

    
    
    bool autoContrast;
    ViewerInstance::DisplayChannels channels;
    {
        QMutexLocker locker(&_imp->viewerParamsMutex);
        outArgs->params->gain = _imp->viewerParamsGain;
        outArgs->params->lut = _imp->viewerParamsLut;
        autoContrast = _imp->viewerParamsAutoContrast;
        channels = _imp->viewerParamsChannels;
    }
    std::string inputToRenderName = outArgs->activeInputToRender->getNode()->getName_mt_safe();
    
    outArgs->key.reset(new FrameKey(time,
                 viewerHash,
                 outArgs->params->gain,
                 outArgs->params->lut,
                 (int)bitDepth,
                 channels,
                 view,
                 outArgs->params->textureRect,
                 scale,
                 inputToRenderName));
    
    bool isCached = false;
    
    ///we never use the texture cache when the user RoI is enabled, otherwise we would have
    ///zillions of textures in the cache, each a few pixels different.
    assert(_imp->uiContext);
    boost::shared_ptr<Natron::FrameParams> cachedFrameParams;
    
    if (!_imp->uiContext->isUserRegionOfInterestEnabled() && !autoContrast) {
        isCached = Natron::getTextureFromCache(*(outArgs->key), &outArgs->params->cachedFrame);
        
        ///if we want to force a refresh, we by-pass the cache
        if (outArgs->forceRender && outArgs->params->cachedFrame) {
            appPTR->removeFromViewerCache(outArgs->params->cachedFrame);
            isCached = false;
            outArgs->params->cachedFrame.reset();
        }
        
        if (isCached) {
            assert(outArgs->params->cachedFrame);
            cachedFrameParams = outArgs->params->cachedFrame->getParams();
        }
        
        ///The user changed a parameter or the tree, just clear the cache
        ///it has no point keeping the cache because we will never find these entries again.
        U64 lastRenderHash;
        bool lastRenderedHashValid;
        {
            QMutexLocker l(&_imp->lastRenderedHashMutex);
            lastRenderHash = _imp->lastRenderedHash;
            lastRenderedHashValid = _imp->lastRenderedHashValid;
        }
        if ( lastRenderedHashValid && (lastRenderHash != viewerHash) ) {
            appPTR->removeAllTexturesFromCacheWithMatchingKey(lastRenderHash);
            {
                QMutexLocker l(&_imp->lastRenderedHashMutex);
                _imp->lastRenderedHashValid = false;
            }
        }
    }
    
    if (isCached) {
        
        /// make sure we have the lock on the texture because it may be in the cache already
        ///but not yet allocated.
        FrameEntryLocker entryLocker(_imp.get());
        if (!entryLocker.tryLock(outArgs->params->cachedFrame)) {
            outArgs->params->cachedFrame.reset();
            ///Another thread is rendering it, just return it is not useful to keep this thread waiting.
            //return eStatusReplyDefault;
            return eStatusOK;
        }
        
        if (outArgs->params->cachedFrame->getAborted()) {
            ///The thread rendering the frame entry might have been aborted and the entry removed from the cache
            ///but another thread might successfully have found it in the cache. This flag is to notify it the frame
            ///is invalid.
            isCached = false;
        }
        
        // how do you make sure cachedFrame->data() is not freed after this line?
        ///It is not freed as long as the cachedFrame shared_ptr has a used_count greater than 1.
        ///Since it is used during the whole function scope it is guaranteed not to be freed before
        ///The viewer is actually done with it.
        /// @see Cache::clearInMemoryPortion and Cache::clearDiskPortion and LRUHashTable::evict
        ///
        
        
        outArgs->params->ramBuffer = outArgs->params->cachedFrame->data();
        
        {
            QMutexLocker l(&_imp->lastRenderedHashMutex);
            _imp->lastRenderedHash = viewerHash;
            _imp->lastRenderedHashValid = true;
        }
        
        
    }
    return eStatusOK;
}

//if render was aborted, remove the frame from the cache as it contains only garbage
#define abortCheck(input) if ( input->aborted() ) { \
                                if (inArgs.params->cachedFrame) { \
                                    inArgs.params->cachedFrame->setAborted(true); \
                                    appPTR->removeFromViewerCache(inArgs.params->cachedFrame); \
                                } \
                                return eStatusReplyDefault; \
                            }

Natron::StatusEnum
ViewerInstance::renderViewer_internal(int view,
                                      bool singleThreaded,
                                      bool isSequentialRender,
                                      U64 viewerHash,
                                      bool canAbort,
                                      const ViewerArgs& inArgs)
{
    assert(!inArgs.params->ramBuffer);
    
    RectI roi;
    roi.x1 = inArgs.params->textureRect.x1;
    roi.y1 = inArgs.params->textureRect.y1;
    roi.x2 = inArgs.params->textureRect.x2;
    roi.y2 = inArgs.params->textureRect.y2;
    
    bool autoContrast;
    ViewerInstance::DisplayChannels channels;
    {
        QMutexLocker locker(&_imp->viewerParamsMutex);
        autoContrast = _imp->viewerParamsAutoContrast;
        channels = _imp->viewerParamsChannels;
    }
    
    ///Check that we were not aborted already
    if ( !isSequentialRender && (inArgs.activeInputToRender->getHash() != inArgs.activeInputHash ||
                                 inArgs.params->time != getTimeline()->currentFrame()) ) {
        return eStatusReplyDefault;
    }
    
    ///Notify the gui we're rendering.
    ViewerRenderingStarted_RAII renderingNotifier(this);
    
    ///Don't allow different threads to write the texture entry
    FrameEntryLocker entryLocker(_imp.get());
    
    ///If the user RoI is enabled, the odds that we find a texture containing exactly the same portion
    ///is very low, we better render again (and let the NodeCache do the work) rather than just
    ///overload the ViewerCache which may become slowe
    assert(_imp->uiContext);
    if (inArgs.forceRender || _imp->uiContext->isUserRegionOfInterestEnabled() || autoContrast) {
        
        assert(!inArgs.params->cachedFrame);
        inArgs.params->mustFreeRamBuffer = true;
        inArgs.params->ramBuffer =  (unsigned char*)malloc(inArgs.params->bytesCount);
        
    } else {
        
        // For the viewer, we need the enclosing rectangle to avoid black borders.
        // Do this here to avoid infinity values.
        RectI bounds;
        inArgs.params->rod.toPixelEnclosing(inArgs.params->mipMapLevel, inArgs.params->textureRect.par, &bounds);
        
        
        boost::shared_ptr<Natron::FrameParams> cachedFrameParams =
        FrameEntry::makeParams(bounds,inArgs.key->getBitDepth(), inArgs.params->textureRect.w, inArgs.params->textureRect.h);
        bool textureIsCached = Natron::getTextureFromCacheOrCreate(*(inArgs.key), cachedFrameParams, &entryLocker,
                                                                   &inArgs.params->cachedFrame);
        if (!inArgs.params->cachedFrame) {
            std::stringstream ss;
            ss << "Failed to allocate a texture of ";
            ss << printAsRAM( cachedFrameParams->getElementsCount() * sizeof(FrameEntry::data_t) ).toStdString();
            Natron::errorDialog( QObject::tr("Out of memory").toStdString(),ss.str() );
            
            return eStatusFailed;
        }
        
        if (textureIsCached) {
            //entryLocker.lock(inArgs.params->cachedFrame);
            if (!entryLocker.tryLock(inArgs.params->cachedFrame)) {
                ///Another thread is rendering it, just return it is not useful to keep this thread waiting.
                inArgs.params->cachedFrame.reset();
                return eStatusOK;
            }
        } else {
            ///The entry has already been locked by the cache
            inArgs.params->cachedFrame->allocateMemory();
        }
        
        assert(inArgs.params->cachedFrame);
        // how do you make sure cachedFrame->data() is not freed after this line?
        ///It is not freed as long as the cachedFrame shared_ptr has a used_count greater than 1.
        ///Since it is used during the whole function scope it is guaranteed not to be freed before
        ///The viewer is actually done with it.
        /// @see Cache::clearInMemoryPortion and Cache::clearDiskPortion and LRUHashTable::evict
        inArgs.params->ramBuffer = inArgs.params->cachedFrame->data();
        
        {
            QMutexLocker l(&_imp->lastRenderedHashMutex);
            _imp->lastRenderedHashValid = true;
            _imp->lastRenderedHash = viewerHash;
        }
    }
    assert(inArgs.params->ramBuffer);
    
    
    ImageComponentsEnum components;
    ImageBitDepthEnum imageDepth;
    inArgs.activeInputToRender->getPreferredDepthAndComponents(-1, &components, &imageDepth);
    
    {
        
        EffectInstance::NotifyInputNRenderingStarted_RAII inputNIsRendering_RAII(_node.get(),inArgs.activeInputIndex);
        
        Node::ParallelRenderArgsSetter frameRenderArgs(inArgs.activeInputToRender->getNode().get(),
                                                       inArgs.params->time,
                                                       view,
                                                       !isSequentialRender,  // is this render due to user interaction ?
                                                       isSequentialRender, // is this sequential ?
                                                       canAbort,
                                                       inArgs.activeInputHash,
                                                       false,
                                                       getTimeline().get());
        
        
        
        // If an exception occurs here it is probably fatal, since
        // it comes from Natron itself. All exceptions from plugins are already caught
        // by the HostSupport library.
        // We catch it  and rethrow it just to notify the rendering is done.
        try {
            
            inArgs.params->image = inArgs.activeInputToRender->renderRoI(EffectInstance::RenderRoIArgs(inArgs.params->time,
                                                                                         inArgs.key->getScale(),
                                                                                         inArgs.params->mipMapLevel,
                                                                                         view,
                                                                                         inArgs.forceRender,
                                                                                         roi,
                                                                                         inArgs.params->rod,
                                                                                         components,
                                                                                         imageDepth) );
            
            if (!inArgs.params->image) {
                if (inArgs.params->cachedFrame) {
                    inArgs.params->cachedFrame->setAborted(true);
                    appPTR->removeFromViewerCache(inArgs.params->cachedFrame);
                }
                return eStatusReplyDefault;
            }
            
            
        } catch (...) {
            ///If the plug-in was aborted, this is probably not a failure due to render but because of abortion.
            ///Don't forward the exception in that case.
            abortCheck(inArgs.activeInputToRender);
            throw;
        }
        
        
        
    } // EffectInstance::NotifyInputNRenderingStarted_RAII inputNIsRendering_RAII(_node.get(),activeInputIndex);
    
    ///Re-check the RoI of the viewer after we rendered. If the user was zooming in/out, it might have changed
    ///and we don't want to erase all the texture for nothing. E.g: when the user is currently zoomed in on a few
    ///pixels of the image, when dezooming, several renders will be triggered, but we don't want the intermediate renders
    ///to clear portions of the texture, just the last one.
    RectI roiAfterRender = _imp->uiContext->getImageRectangleDisplayedRoundedToTileSize(inArgs.params->rod,
                                                                                        inArgs.params->textureRect.par,
                                                                                        inArgs.params->mipMapLevel);
    if (roi != roiAfterRender) {
        inArgs.params->cachedFrame->setAborted(true);
        appPTR->removeFromViewerCache(inArgs.params->cachedFrame);
        inArgs.params->cachedFrame.reset();
        return eStatusReplyDefault;
    }
    
    abortCheck(inArgs.activeInputToRender);
    
    
    ViewerColorSpaceEnum srcColorSpace = getApp()->getDefaultColorSpaceForBitDepth( inArgs.params->image->getBitDepth() );
    
    
    if (singleThreaded) {
        if (autoContrast) {
            double vmin, vmax;
            std::pair<double,double> vMinMax = findAutoContrastVminVmax(inArgs.params->image, channels, roi);
            vmin = vMinMax.first;
            vmax = vMinMax.second;
            
            ///if vmax - vmin is greater than 1 the gain will be really small and we won't see
            ///anything in the image
            if (vmin == vmax) {
                vmin = vmax - 1.;
            }
            inArgs.params->gain = 1 / (vmax - vmin);
            inArgs.params->offset = -vmin / ( vmax - vmin);
        }
        
        const RenderViewerArgs args( inArgs.params->image,
                                    inArgs.params->textureRect,
                                    channels,
                                    inArgs.params->srcPremult,
                                    1,
                                    inArgs.key->getBitDepth(),
                                    inArgs.params->gain,
                                    inArgs.params->offset,
                                    lutFromColorspace(srcColorSpace),
                                    lutFromColorspace(inArgs.params->lut) );
        
        renderFunctor(std::make_pair(roi.y1,roi.y2),
                      args,
                      this,
                      inArgs.params->ramBuffer);
    } else {
        
        int rowsPerThread = std::ceil( (double)(roi.x2 - roi.x1) / appPTR->getHardwareIdealThreadCount() );
        // group of group of rows where first is image coordinate, second is texture coordinate
        QList< std::pair<int, int> > splitRows;
        
        bool runInCurrentThread = QThreadPool::globalInstance()->activeThreadCount() >= QThreadPool::globalInstance()->maxThreadCount();
        
        if (!runInCurrentThread) {
            int k = roi.y1;
            while (k < roi.y2) {
                int top = k + rowsPerThread;
                int realTop = top > roi.y2 ? roi.y2 : top;
                splitRows.push_back( std::make_pair(k,realTop) );
                k += rowsPerThread;
            }
        }
        
        ///if autoContrast is enabled, find out the vmin/vmax before rendering and mapping against new values
        if (autoContrast) {
            rowsPerThread = std::ceil( (double)( roi.width() ) / (double)appPTR->getHardwareIdealThreadCount() );
            
            std::vector<RectI> splitRects;
            
            double vmin = std::numeric_limits<double>::infinity();
            double vmax = -std::numeric_limits<double>::infinity();
            
            if (!runInCurrentThread) {
                int k = roi.y1;
                while (k < roi.y2) {
                    int top = k + rowsPerThread;
                    int realTop = top > roi.top() ? roi.top() : top;
                    splitRects.push_back( RectI(roi.left(), k, roi.right(), realTop) );
                    k += rowsPerThread;
                }
                
                
                QFuture<std::pair<double,double> > future = QtConcurrent::mapped( splitRects,
                                                                                 boost::bind(findAutoContrastVminVmax,
                                                                                             inArgs.params->image,
                                                                                             channels,
                                                                                             _1) );
                future.waitForFinished();
                
                std::pair<double,double> vMinMax;
                foreach ( vMinMax, future.results() ) {
                    if (vMinMax.first < vmin) {
                        vmin = vMinMax.first;
                    }
                    if (vMinMax.second > vmax) {
                        vmax = vMinMax.second;
                    }
                }
            } else { //!runInCurrentThread
                std::pair<double,double> vMinMax = findAutoContrastVminVmax(inArgs.params->image, channels, roi);
                vmin = vMinMax.first;
                vmax = vMinMax.second;
            }
            
            if (vmax == vmin) {
                vmin = vmax - 1.;
            }
            
            inArgs.params->gain = 1 / (vmax - vmin);
            inArgs.params->offset =  -vmin / (vmax - vmin);
        }
        
        const RenderViewerArgs args(inArgs.params->image,
                                    inArgs.params->textureRect,
                                    channels,
                                    inArgs.params->srcPremult,
                                    1,
                                    inArgs.key->getBitDepth(),
                                    inArgs.params->gain,
                                    inArgs.params->offset,
                                    lutFromColorspace(srcColorSpace),
                                    lutFromColorspace(inArgs.params->lut));
        if (runInCurrentThread) {
            renderFunctor(std::make_pair(inArgs.params->textureRect.y1,inArgs.params->textureRect.y2),
                          args, this, inArgs.params->ramBuffer);
        } else {
            QtConcurrent::map( splitRows,
                              boost::bind(&renderFunctor,
                                          _1,
                                          args,
                                          this,
                                          inArgs.params->ramBuffer) ).waitForFinished();
        }
        
        
    }
    abortCheck(inArgs.activeInputToRender);

    return eStatusOK;
} // renderViewer_internal


void
ViewerInstance::updateViewer(boost::shared_ptr<UpdateViewerParams> & frame)
{
    _imp->updateViewer(boost::dynamic_pointer_cast<UpdateViewerParams>(frame));
}

void
renderFunctor(std::pair<int,int> yRange,
              const RenderViewerArgs & args,
              ViewerInstance* viewer,
              void *buffer)
{
    assert(args.texRect.y1 <= yRange.first && yRange.first <= yRange.second && yRange.second <= args.texRect.y2);

    if ( (args.bitDepth == OpenGLViewerI::FLOAT) || (args.bitDepth == OpenGLViewerI::HALF_FLOAT) ) {
        // image is stored as linear, the OpenGL shader with do gamma/sRGB/Rec709 decompression, as well as gain and offset
        scaleToTexture32bits(yRange, args,viewer, (float*)buffer);
    } else {
        // texture is stored as sRGB/Rec709 compressed 8-bit RGBA
        scaleToTexture8bits(yRange, args,viewer, (U32*)buffer);
    }
}

template <int nComps>
std::pair<double, double>
findAutoContrastVminVmax_internal(boost::shared_ptr<const Natron::Image> inputImage,
                         ViewerInstance::DisplayChannels channels,
                         const RectI & rect)
{
    double localVmin = std::numeric_limits<double>::infinity();
    double localVmax = -std::numeric_limits<double>::infinity();
    
    for (int y = rect.bottom(); y < rect.top(); ++y) {
        const float* src_pixels = (const float*)inputImage->pixelAt(rect.left(),y);
        ///we fill the scan-line with all the pixels of the input image
        for (int x = rect.left(); x < rect.right(); ++x) {
            
            double r,g,b,a;
            switch (nComps) {
                case 4:
                    r = src_pixels[0];
                    g = src_pixels[1];
                    b = src_pixels[2];
                    a = src_pixels[3];
                    break;
                case 3:
                    r = src_pixels[0];
                    g = src_pixels[1];
                    b = src_pixels[2];
                    a = 1.;
                    break;
                case 1:
                    a = src_pixels[0];
                    r = g = b = 0.;
                    break;
                default:
                    r = g = b = 0.;
            }
            
            double mini, maxi;
            switch (channels) {
                case ViewerInstance::RGB:
                    mini = std::min(std::min(r,g),b);
                    maxi = std::max(std::max(r,g),b);
                    break;
                case ViewerInstance::LUMINANCE:
                    mini = r = 0.299 * r + 0.587 * g + 0.114 * b;
                    maxi = mini;
                    break;
                case ViewerInstance::R:
                    mini = r;
                    maxi = mini;
                    break;
                case ViewerInstance::G:
                    mini = g;
                    maxi = mini;
                    break;
                case ViewerInstance::B:
                    mini = b;
                    maxi = mini;
                    break;
                case ViewerInstance::A:
                    mini = a;
                    maxi = mini;
                    break;
                default:
                    mini = 0.;
                    maxi = 0.;
                    break;
            }
            if (mini < localVmin) {
                localVmin = mini;
            }
            if (maxi > localVmax) {
                localVmax = maxi;
            }
            
            src_pixels += nComps;
        }
    }
    
    return std::make_pair(localVmin, localVmax);
}


std::pair<double, double>
findAutoContrastVminVmax(boost::shared_ptr<const Natron::Image> inputImage,
                         ViewerInstance::DisplayChannels channels,
                         const RectI & rect)
{
    switch (inputImage->getComponents()) {
        case Natron::eImageComponentRGBA:
            return findAutoContrastVminVmax_internal<4>(inputImage, channels, rect);
        case Natron::eImageComponentRGB:
            return findAutoContrastVminVmax_internal<3>(inputImage, channels, rect);
        case Natron::eImageComponentAlpha:
            return findAutoContrastVminVmax_internal<1>(inputImage, channels, rect);
        default:
            return std::make_pair(0,1);
    }
} // findAutoContrastVminVmax

template <typename PIX,int maxValue,int nComps,bool opaque,int rOffset,int gOffset,int bOffset>
void
scaleToTexture8bits_internal(const std::pair<int,int> & yRange,
                             const RenderViewerArgs & args,
                             ViewerInstance* /*viewer*/,
                             U32* output)
{
    size_t pixelSize = sizeof(PIX);
    
    const bool luminance = (args.channels == ViewerInstance::LUMINANCE);
    
    ///offset the output buffer at the starting point
    output += ( (yRange.first - args.texRect.y1) / args.closestPowerOf2 ) * args.texRect.w;

    ///iterating over the scan-lines of the input image
    int dstY = 0;
    for (int y = yRange.first; y < yRange.second; y += args.closestPowerOf2) {
        
        int start = (int)( rand() % std::max( ( (args.texRect.x2 - args.texRect.x1) / args.closestPowerOf2 ),1 ) );
        const PIX* src_pixels = (const PIX*)args.inputImage->pixelAt(args.texRect.x1, y);
        U32* dst_pixels = output + dstY * args.texRect.w;
        
        /* go fowards from starting point to end of line: */
        for (int backward = 0; backward < 2; ++backward) {
            int dstIndex = backward ? start - 1 : start;
            int srcIndex = dstIndex * args.closestPowerOf2; //< offset from src_pixels
            assert( backward == 1 || ( srcIndex >= 0 && srcIndex < (args.texRect.x2 - args.texRect.x1) ) );

            unsigned error_r = 0x80;
            unsigned error_g = 0x80;
            unsigned error_b = 0x80;

            while (dstIndex < args.texRect.w && dstIndex >= 0) {
                if ( srcIndex >= ( args.texRect.x2 - args.texRect.x1) ) {
                    break;
                } else {
                    double r,g,b;
                    int a;
                    switch (nComps) {
                        case 4:
                            r = (src_pixels ? src_pixels[srcIndex * nComps + rOffset] : 0.);
                            g = (src_pixels ? src_pixels[srcIndex * nComps + gOffset] : 0.);
                            b = (src_pixels ? src_pixels[srcIndex * nComps + bOffset] : 0.);
                            if (opaque) {
                                a = 255;
                            } else {
                                a = (src_pixels ? Color::floatToInt<256>(src_pixels[srcIndex * nComps + 3]) : 0);
                            }
                            break;
                        case 3:
                            r = (src_pixels && rOffset < nComps) ? src_pixels[srcIndex * nComps + rOffset] : 0.;
                            g = (src_pixels && gOffset < nComps) ? src_pixels[srcIndex * nComps + gOffset] : 0.;
                            b = (src_pixels && bOffset < nComps) ? src_pixels[srcIndex * nComps + bOffset] : 0.;
                            a = (src_pixels ? 255 : 0);
                            break;
                        case 1:
                            r = src_pixels ? src_pixels[srcIndex] : 0.;
                            g = b = r;
                            a = src_pixels ? 255 : 0;
                            break;
                        default:
                            assert(false);
                            break;
                    }
                    
                    
                    switch ( pixelSize ) {
                    case sizeof(unsigned char): //byte
                        if (args.srcColorSpace) {
                            r = args.srcColorSpace->fromColorSpaceUint8ToLinearFloatFast( (unsigned char)r );
                            g = args.srcColorSpace->fromColorSpaceUint8ToLinearFloatFast( (unsigned char)g );
                            b = args.srcColorSpace->fromColorSpaceUint8ToLinearFloatFast( (unsigned char)b );
                        } else {
                            r = (double)convertPixelDepth<unsigned char, float>( (unsigned char)r );
                            g = (double)convertPixelDepth<unsigned char, float>( (unsigned char)g );
                            b = (double)convertPixelDepth<unsigned char, float>( (unsigned char)b );
                        }
                        break;
                    case sizeof(unsigned short): //short
                        if (args.srcColorSpace) {
                            r = args.srcColorSpace->fromColorSpaceUint16ToLinearFloatFast( (unsigned short)r );
                            g = args.srcColorSpace->fromColorSpaceUint16ToLinearFloatFast( (unsigned short)g );
                            b = args.srcColorSpace->fromColorSpaceUint16ToLinearFloatFast( (unsigned short)b );
                        } else {
                            r = (double)convertPixelDepth<unsigned short, float>( (unsigned char)r );
                            g = (double)convertPixelDepth<unsigned short, float>( (unsigned char)g );
                            b = (double)convertPixelDepth<unsigned short, float>( (unsigned char)b );
                        }
                        break;
                    case sizeof(float): //float
                        if (args.srcColorSpace) {
                            r = args.srcColorSpace->fromColorSpaceFloatToLinearFloat(r);
                            g = args.srcColorSpace->fromColorSpaceFloatToLinearFloat(g);
                            b = args.srcColorSpace->fromColorSpaceFloatToLinearFloat(b);
                        }
                        break;
                    default:
                        break;
                    }

                    r =  r * args.gain + args.offset;
                    g =  g * args.gain + args.offset;
                    b =  b * args.gain + args.offset;

                    if (luminance) {
                        r = 0.299 * r + 0.587 * g + 0.114 * b;
                        g = r;
                        b = r;
                    }

                    if (!args.colorSpace) {
                        dst_pixels[dstIndex] = toBGRA(Color::floatToInt<256>(r),
                                                      Color::floatToInt<256>(g),
                                                      Color::floatToInt<256>(b),
                                                      a);
                    } else {
                        error_r = (error_r & 0xff) + args.colorSpace->toColorSpaceUint8xxFromLinearFloatFast(r);
                        error_g = (error_g & 0xff) + args.colorSpace->toColorSpaceUint8xxFromLinearFloatFast(g);
                        error_b = (error_b & 0xff) + args.colorSpace->toColorSpaceUint8xxFromLinearFloatFast(b);
                        assert(error_r < 0x10000 && error_g < 0x10000 && error_b < 0x10000);
                        dst_pixels[dstIndex] = toBGRA( (U8)(error_r >> 8),
                                                       (U8)(error_g >> 8),
                                                       (U8)(error_b >> 8),
                                                       a );
                    }
                }
                if (backward) {
                    --dstIndex;
                    srcIndex -= args.closestPowerOf2;
                } else {
                    ++dstIndex;
                    srcIndex += args.closestPowerOf2;
                }
            }
        }
        ++dstY;
    }
} // scaleToTexture8bits_internal


template <typename PIX,int maxValue,int nComps,int rOffset,int gOffset,int bOffset>
void
scaleToTexture8bitsForPremult(const std::pair<int,int> & yRange,
                             const RenderViewerArgs & args,
                             ViewerInstance* viewer,
                             U32* output)
{
    switch (args.srcPremult) {
        case Natron::eImagePremultiplicationOpaque:
            scaleToTexture8bits_internal<PIX, maxValue, nComps, true, rOffset, gOffset, bOffset>(yRange, args,viewer, output);
            break;
        case Natron::eImagePremultiplicationPremultiplied:
        case Natron::eImagePremultiplicationUnPremultiplied:
        default:
            scaleToTexture8bits_internal<PIX, maxValue, nComps, false, rOffset, gOffset, bOffset>(yRange, args,viewer, output);
            break;
            
    }
}

template <typename PIX,int maxValue,int nComps>
void
scaleToTexture8bitsForDepthForComponents(const std::pair<int,int> & yRange,
                            const RenderViewerArgs & args,
                            ViewerInstance* viewer,
                            U32* output)
{
    switch (args.channels) {
        case ViewerInstance::RGB:
        case ViewerInstance::LUMINANCE:

            scaleToTexture8bitsForPremult<PIX, maxValue, nComps, 0, 1, 2>(yRange, args,viewer, output);
            break;
        case ViewerInstance::G:
            scaleToTexture8bitsForPremult<PIX, maxValue, nComps, 1, 1, 1>(yRange, args,viewer, output);
            break;
        case ViewerInstance::B:
            scaleToTexture8bitsForPremult<PIX, maxValue, nComps, 2, 2, 2>(yRange, args,viewer, output);
            break;
        case ViewerInstance::A:
            scaleToTexture8bitsForPremult<PIX, maxValue, nComps, 3, 3, 3>(yRange, args,viewer, output);
            break;
        case ViewerInstance::R:
        default:
            scaleToTexture8bitsForPremult<PIX, maxValue, nComps, 0, 0, 0>(yRange, args,viewer, output);

            break;
    }
}

template <typename PIX,int maxValue>
void
scaleToTexture8bitsForDepth(const std::pair<int,int> & yRange,
                            const RenderViewerArgs & args,
                            ViewerInstance* viewer,
                            U32* output)
{
    Natron::ImageComponentsEnum comps = args.inputImage->getComponents();
    switch (comps) {
        case Natron::eImageComponentRGBA:
            scaleToTexture8bitsForDepthForComponents<PIX,maxValue,4>(yRange,args,viewer,output);
            break;
        case Natron::eImageComponentRGB:
            scaleToTexture8bitsForDepthForComponents<PIX,maxValue,3>(yRange,args,viewer,output);
            break;
        case Natron::eImageComponentAlpha:
            scaleToTexture8bitsForDepthForComponents<PIX,maxValue,1>(yRange,args,viewer,output);
            break;
        default:
            break;
    }
}

void
scaleToTexture8bits(std::pair<int,int> yRange,
                    const RenderViewerArgs & args,
                    ViewerInstance* viewer,
                    U32* output)
{
    assert(output);
    switch ( args.inputImage->getBitDepth() ) {
        case Natron::eImageBitDepthFloat:
            scaleToTexture8bitsForDepth<float, 1>(yRange, args,viewer, output);
            break;
        case Natron::eImageBitDepthByte:
            scaleToTexture8bitsForDepth<unsigned char, 255>(yRange, args,viewer, output);
            break;
        case Natron::eImageBitDepthShort:
            scaleToTexture8bitsForDepth<unsigned short, 65535>(yRange, args,viewer,output);
            break;
            
        case Natron::eImageBitDepthNone:
            break;
    }
} // scaleToTexture8bits

template <typename PIX,int maxValue,int nComps,bool opaque,int rOffset,int gOffset,int bOffset>
void
scaleToTexture32bitsInternal(const std::pair<int,int> & yRange,
                             const RenderViewerArgs & args,
                             ViewerInstance* viewer,
                             float *output)
{
    size_t pixelSize = sizeof(PIX);
    const bool luminance = (args.channels == ViewerInstance::LUMINANCE);

    ///the width of the output buffer multiplied by the channels count
    int dst_width = args.texRect.w * 4;

    ///offset the output buffer at the starting point
    output += ( (yRange.first - args.texRect.y1) / args.closestPowerOf2 ) * dst_width;

    ///iterating over the scan-lines of the input image
    int dstY = 0;
    for (int y = yRange.first; y < yRange.second; y += args.closestPowerOf2) {
        
        if (viewer->aborted()) {
            return;
        }
        
        const float* src_pixels = (const float*)args.inputImage->pixelAt(args.texRect.x1, y);
        float* dst_pixels = output + dstY * dst_width;

        ///we fill the scan-line with all the pixels of the input image
        for (int x = args.texRect.x1; x < args.texRect.x2; x += args.closestPowerOf2) {
            double r,g,b,a;
            
            switch (nComps) {
                case 4:
                    r = (double)(src_pixels[rOffset]);
                    g = (double)src_pixels[gOffset];
                    b = (double)src_pixels[bOffset];
                    if (opaque) {
                        a = 1.;
                    } else {
                        a = (nComps < 4) ? 1. : src_pixels[3];
                    }
                    break;
                case 3:
                    r = (double)(src_pixels[rOffset]);
                    g = (double)src_pixels[gOffset];
                    b = (double)src_pixels[bOffset];
                    a = 1.;
                    break;
                case 1:
                    a = (nComps < 4) ? 1. : *src_pixels;
                    r = g = b = a;
                    a = 1.;
                    break;
                default:
                    assert(false);
                    r = g = b = a = 0.;
                    break;
            }
            
            switch ( pixelSize ) {
            case sizeof(unsigned char):
                if (args.srcColorSpace) {
                    r = args.srcColorSpace->fromColorSpaceUint8ToLinearFloatFast( (unsigned char)r );
                    g = args.srcColorSpace->fromColorSpaceUint8ToLinearFloatFast( (unsigned char)g );
                    b = args.srcColorSpace->fromColorSpaceUint8ToLinearFloatFast( (unsigned char)b );
                } else {
                    r = (double)convertPixelDepth<unsigned char, float>( (unsigned char)r );
                    g = (double)convertPixelDepth<unsigned char, float>( (unsigned char)g );
                    b = (double)convertPixelDepth<unsigned char, float>( (unsigned char)b );
                }
                break;
            case sizeof(unsigned short):
                if (args.srcColorSpace) {
                    r = args.srcColorSpace->fromColorSpaceUint16ToLinearFloatFast( (unsigned short)r );
                    g = args.srcColorSpace->fromColorSpaceUint16ToLinearFloatFast( (unsigned short)g );
                    b = args.srcColorSpace->fromColorSpaceUint16ToLinearFloatFast( (unsigned short)b );
                } else {
                    r = (double)convertPixelDepth<unsigned short, float>( (unsigned char)r );
                    g = (double)convertPixelDepth<unsigned short, float>( (unsigned char)g );
                    b = (double)convertPixelDepth<unsigned short, float>( (unsigned char)b );
                }
                break;
            case sizeof(float):
                if (args.srcColorSpace) {
                    r = args.srcColorSpace->fromColorSpaceFloatToLinearFloat(r);
                    g = args.srcColorSpace->fromColorSpaceFloatToLinearFloat(g);
                    b = args.srcColorSpace->fromColorSpaceFloatToLinearFloat(b);
                }
                break;
            default:
                break;
            }


            if (luminance) {
                r = 0.299 * r + 0.587 * g + 0.114 * b;
                g = r;
                b = r;
            }
            *dst_pixels++ = r;
            *dst_pixels++ = g;
            *dst_pixels++ = b;
            *dst_pixels++ = a;

            src_pixels += args.closestPowerOf2 * 4;
        }
        ++dstY;
    }
} // scaleToTexture32bitsInternal


template <typename PIX,int maxValue,int nComps,int rOffset,int gOffset,int bOffset>
void
scaleToTexture32bitsForPremult(const std::pair<int,int> & yRange,
                             const RenderViewerArgs & args,
                            ViewerInstance* viewer,
                             float *output)
{
    switch (args.srcPremult) {
        case Natron::eImagePremultiplicationOpaque:
            scaleToTexture32bitsInternal<PIX, maxValue, nComps, true, rOffset, gOffset, bOffset>(yRange, args,viewer, output);
            break;
        case Natron::eImagePremultiplicationPremultiplied:
        case Natron::eImagePremultiplicationUnPremultiplied:
        default:
            scaleToTexture32bitsInternal<PIX, maxValue, nComps, false, rOffset, gOffset, bOffset>(yRange, args,viewer, output);
            break;
        
    }
}

template <typename PIX,int maxValue,int nComps>
void
scaleToTexture32bitsForDepthForComponents(const std::pair<int,int> & yRange,
                             const RenderViewerArgs & args,
                            ViewerInstance* viewer,
                             float *output)
{
    switch (args.channels) {
        case ViewerInstance::RGB:
        case ViewerInstance::LUMINANCE:
            switch (nComps) {
                case 1:
                    scaleToTexture32bitsForPremult<PIX, maxValue, nComps, 0, 0, 0>(yRange, args,viewer, output);
                    break;
                case 3:
                case 4:
                    scaleToTexture32bitsForPremult<PIX, maxValue, nComps, 0, 1, 2>(yRange, args,viewer, output);
                    break;
                default:
                    break;
            }
            break;
        case ViewerInstance::G:
            switch (nComps) {
                case 1:
                    scaleToTexture32bitsForPremult<PIX, maxValue, nComps, 0, 0, 0>(yRange, args,viewer, output);
                    break;
                case 3:
                case 4:
                    scaleToTexture32bitsForPremult<PIX, maxValue, nComps, 1, 1, 1>(yRange, args,viewer, output);
                    break;
                default:
                    break;
            }
            break;
        case ViewerInstance::B:
            switch (nComps) {
                case 1:
                    scaleToTexture32bitsForPremult<PIX, maxValue, nComps, 0, 0, 0>(yRange, args,viewer, output);
                    break;
                case 3:
                case 4:
                    scaleToTexture32bitsForPremult<PIX, maxValue, nComps, 2, 2, 2>(yRange, args,viewer, output);
                    break;
                default:
                    break;
            }
            break;
        case ViewerInstance::A:
            switch (nComps) {
                case 1:
                case 3:
                    scaleToTexture32bitsForPremult<PIX, maxValue, nComps, 0, 0, 0>(yRange, args,viewer, output);
                    break;
                case 4:
                    scaleToTexture32bitsForPremult<PIX, maxValue, nComps, 3, 3, 3>(yRange, args,viewer, output);
                    break;
                default:
                    break;
            }
            break;
        case ViewerInstance::R:
        default:
            scaleToTexture32bitsForPremult<PIX, maxValue, nComps, 0, 0, 0>(yRange, args,viewer, output);
            break;
    }

}

template <typename PIX,int maxValue>
void
scaleToTexture32bitsForDepth(const std::pair<int,int> & yRange,
                             const RenderViewerArgs & args,
                             ViewerInstance* viewer,
                             float *output)
{
    Natron::ImageComponentsEnum comps = args.inputImage->getComponents();
    switch (comps) {
        case Natron::eImageComponentRGBA:
            scaleToTexture32bitsForDepthForComponents<PIX,maxValue,4>(yRange,args,viewer,output);
            break;
        case Natron::eImageComponentRGB:
            scaleToTexture32bitsForDepthForComponents<PIX,maxValue,3>(yRange,args,viewer,output);
            break;
        case Natron::eImageComponentAlpha:
            scaleToTexture32bitsForDepthForComponents<PIX,maxValue,1>(yRange,args,viewer,output);
            break;
        default:
            break;
    }
}

void
scaleToTexture32bits(std::pair<int,int> yRange,
                     const RenderViewerArgs & args,
                     ViewerInstance* viewer,
                     float *output)
{
    assert(output);

    switch ( args.inputImage->getBitDepth() ) {
        case Natron::eImageBitDepthFloat:
            scaleToTexture32bitsForDepth<float, 1>(yRange, args,viewer, output);
            break;
        case Natron::eImageBitDepthByte:
            scaleToTexture32bitsForDepth<unsigned char, 255>(yRange, args,viewer, output);
            break;
        case Natron::eImageBitDepthShort:
            scaleToTexture32bitsForDepth<unsigned short, 65535>(yRange, args,viewer, output);
            break;
        case Natron::eImageBitDepthNone:
            break;
    }
} // scaleToTexture32bits


void
ViewerInstance::ViewerInstancePrivate::updateViewer(boost::shared_ptr<UpdateViewerParams> params)
{
    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );

    // QMutexLocker locker(&updateViewerMutex);
    // if (updateViewerRunning) {
    uiContext->makeOpenGLcontextCurrent();
    
    // how do you make sure params->ramBuffer is not freed during this operation?
    /// It is not freed as long as the cachedFrame shared_ptr in renderViewer has a used_count greater than 1.
    /// i.e. until renderViewer exits.
    /// Since updateViewer() is in the scope of cachedFrame, and renderViewer waits for the completion
    /// of updateViewer(), it is guaranteed not to be freed before the viewer is actually done with it.
    /// @see Cache::clearInMemoryPortion and Cache::clearDiskPortion and LRUHashTable::evict
    
    assert(params->ramBuffer);
    
    uiContext->transferBufferFromRAMtoGPU(params->ramBuffer,
                                          params->image,
                                          params->time,
                                          params->rod,
                                          params->bytesCount,
                                          params->textureRect,
                                          params->gain,
                                          params->offset,
                                          params->lut,
                                          updateViewerPboIndex,
                                          params->mipMapLevel,
                                          params->srcPremult,
                                          params->textureIndex);
    updateViewerPboIndex = (updateViewerPboIndex + 1) % 2;
    
    
    uiContext->updateColorPicker(params->textureIndex);
    //
    //        updateViewerRunning = false;
    //    }
    //    updateViewerCond.wakeOne();
}

bool
ViewerInstance::isInputOptional(int n) const
{
    //activeInput() is MT-safe
    int activeInputs[2];

    getActiveInputs(activeInputs[0], activeInputs[1]);
    
    if ( (n == 0) && (activeInputs[0] == -1) && (activeInputs[1] == -1) ) {
        return false;
    }

    return n != activeInputs[0] && n != activeInputs[1];
}

void
ViewerInstance::onGainChanged(double exp)
{
    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );

    {
        QMutexLocker l(&_imp->viewerParamsMutex);
        _imp->viewerParamsGain = exp;
    }
    assert(_imp->uiContext);
    if ( ( (_imp->uiContext->getBitDepth() == OpenGLViewerI::BYTE) || !_imp->uiContext->supportsGLSL() )
         && !getApp()->getProject()->isLoadingProject() ) {
        renderCurrentFrame(true);
    } else {
        _imp->uiContext->redraw();
    }
}

void
ViewerInstance::onMipMapLevelChanged(int level)
{
    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );
    {
        QMutexLocker l(&_imp->viewerParamsMutex);
        if (_imp->viewerMipMapLevel == (unsigned int)level) {
            return;
        }
        _imp->viewerMipMapLevel = level;
    }
    if (!getApp()->getProject()->isLoadingProject()) {
        renderCurrentFrame(true);
    }
}

void
ViewerInstance::onAutoContrastChanged(bool autoContrast,
                                      bool refresh)
{
    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );

    {
        QMutexLocker l(&_imp->viewerParamsMutex);
        _imp->viewerParamsAutoContrast = autoContrast;
    }
    if ( refresh && !getApp()->getProject()->isLoadingProject() ) {
        renderCurrentFrame(true);
    }
}

bool
ViewerInstance::isAutoContrastEnabled() const
{
    // MT-safe
    QMutexLocker l(&_imp->viewerParamsMutex);

    return _imp->viewerParamsAutoContrast;
}

void
ViewerInstance::onColorSpaceChanged(Natron::ViewerColorSpaceEnum colorspace)
{
    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );
    
    {
        QMutexLocker l(&_imp->viewerParamsMutex);
        
        _imp->viewerParamsLut = colorspace;
    }
    assert(_imp->uiContext);
    if ( ( (_imp->uiContext->getBitDepth() == OpenGLViewerI::BYTE) || !_imp->uiContext->supportsGLSL() )
        && !getApp()->getProject()->isLoadingProject() ) {
        renderCurrentFrame(true);
    } else {
        _imp->uiContext->redraw();
    }
}

void
ViewerInstance::setDisplayChannels(DisplayChannels channels)
{
    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );

    {
        QMutexLocker l(&_imp->viewerParamsMutex);
        _imp->viewerParamsChannels = channels;
    }
    if ( !getApp()->getProject()->isLoadingProject() ) {
        renderCurrentFrame(true);
    }
}

void
ViewerInstance::disconnectViewer()
{
    // always running in the render thread

    //_lastRenderedImage.reset(); // if you uncomment this, _lastRenderedImage is not set back when you reconnect the viewer immediately after disconnecting
    if (_imp->uiContext) {
        emit viewerDisconnected();
    }
}


bool
ViewerInstance::supportsGLSL() const
{
    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );

    ///This is a short-cut, this is primarily used when the user switch the
    /// texture mode in the preferences menu. If the hardware doesn't support GLSL
    /// it returns false, true otherwise. @see Settings::onKnobValueChanged
    assert(_imp->uiContext);

    return _imp->uiContext->supportsGLSL();
}

void
ViewerInstance::redrawViewer()
{
    // always running in the main thread
    assert( qApp && qApp->thread() == QThread::currentThread() );
    assert(_imp->uiContext);
    _imp->uiContext->redraw();
}


int
ViewerInstance::getLutType() const
{
    // MT-SAFE: called from main thread and Serialization (pooled) thread

    QMutexLocker l(&_imp->viewerParamsMutex);

    return _imp->viewerParamsLut;
}

double
ViewerInstance::getGain() const
{
    // MT-SAFE: called from main thread and Serialization (pooled) thread

    QMutexLocker l(&_imp->viewerParamsMutex);

    return _imp->viewerParamsGain;
}

int
ViewerInstance::getMipMapLevel() const
{
    // MT-SAFE: called from main thread and Serialization (pooled) thread

    QMutexLocker l(&_imp->viewerParamsMutex);

    return _imp->viewerMipMapLevel;
}


ViewerInstance::DisplayChannels
ViewerInstance::getChannels() const
{
    // MT-SAFE: called from main thread and Serialization (pooled) thread

    QMutexLocker l(&_imp->viewerParamsMutex);

    return _imp->viewerParamsChannels;
}

void
ViewerInstance::addAcceptedComponents(int /*inputNb*/,
                                      std::list<Natron::ImageComponentsEnum>* comps)
{
    ///Viewer only supports RGBA for now.
    comps->push_back(Natron::eImageComponentRGBA);
}

int
ViewerInstance::getCurrentView() const
{
    return _imp->uiContext ? _imp->uiContext->getCurrentView() : 0;
}

void
ViewerInstance::onInputChanged(int inputNb)
{
    assert( QThread::currentThread() == qApp->thread() );
    EffectInstance* inp = getInput(inputNb);
    {
        QMutexLocker l(&_imp->activeInputsMutex);
        if (!inp) {
            ///check if the input was one of the active ones if so set to -1
            if (_imp->activeInputs[0] == inputNb) {
                _imp->activeInputs[0] = -1;
            } else if (_imp->activeInputs[1] == inputNb) {
                _imp->activeInputs[1] = -1;
            }
        } else {
            bool autoWipeEnabled = appPTR->getCurrentSettings()->isAutoWipeEnabled();
            if (_imp->activeInputs[0] == -1 || !autoWipeEnabled) {
                _imp->activeInputs[0] = inputNb;
            } else {
                _imp->activeInputs[1] = inputNb;
            }
        }
    }
    emit activeInputsChanged();
    emit refreshOptionalState();
    emit clipPreferencesChanged();
}

void
ViewerInstance::restoreClipPreferences()
{
    emit clipPreferencesChanged();
}

void
ViewerInstance::checkOFXClipPreferences(double /*time*/,
                             const RenderScale & /*scale*/,
                             const std::string & /*reason*/,
                             bool /*forceGetClipPrefAction*/)
{
    emit clipPreferencesChanged();
}

void
ViewerInstance::addSupportedBitDepth(std::list<Natron::ImageBitDepthEnum>* depths) const
{
    depths->push_back(eImageBitDepthFloat);
    depths->push_back(eImageBitDepthShort);
    depths->push_back(eImageBitDepthByte);
}

void
ViewerInstance::getActiveInputs(int & a,
                                int &b) const
{
    QMutexLocker l(&_imp->activeInputsMutex);

    a = _imp->activeInputs[0];
    b = _imp->activeInputs[1];
}

void
ViewerInstance::setInputA(int inputNb)
{
    assert( QThread::currentThread() == qApp->thread() );
    {
        QMutexLocker l(&_imp->activeInputsMutex);
        _imp->activeInputs[0] = inputNb;
    }
    emit refreshOptionalState();
}

void
ViewerInstance::setInputB(int inputNb)
{
    assert( QThread::currentThread() == qApp->thread() );
    {
        QMutexLocker l(&_imp->activeInputsMutex);
        _imp->activeInputs[1] = inputNb;
    }
    emit refreshOptionalState();
}

bool
ViewerInstance::isFrameRangeLocked() const
{
    return _imp->uiContext ? _imp->uiContext->isFrameRangeLocked() : true;
}

int
ViewerInstance::getLastRenderedTime() const
{
    return _imp->uiContext ? _imp->uiContext->getCurrentlyDisplayedTime() : getApp()->getTimeLine()->currentFrame();
}

boost::shared_ptr<TimeLine>
ViewerInstance::getTimeline() const
{
    return _imp->uiContext ? _imp->uiContext->getTimeline() : getApp()->getTimeLine();
}


int
ViewerInstance::getMipMapLevelFromZoomFactor() const
{
    double zoomFactor = _imp->uiContext->getZoomFactor();
    double closestPowerOf2 = zoomFactor >= 1 ? 1 : std::pow( 2,-std::ceil(std::log(zoomFactor) / M_LN2) );
    return std::log(closestPowerOf2) / M_LN2;
}

