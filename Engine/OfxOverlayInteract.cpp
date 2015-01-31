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
#include "OfxOverlayInteract.h"


#include "Engine/OfxImageEffectInstance.h"
#include "Engine/OfxEffectInstance.h"
#include "Engine/Format.h"
#include "Engine/OverlaySupport.h"
#include "Engine/Knob.h"


using namespace Natron;

namespace {
    // an RAII class to save OpenGL context
    class OGLContextSaver {
    public:
        OGLContextSaver(OverlaySupport* viewport)
        : _viewport(viewport)
        {
            assert(_viewport);
            _viewport->saveOpenGLContext();
        }

        ~OGLContextSaver() {
            _viewport->restoreOpenGLContext();
        }

    private:
        OverlaySupport* const _viewport;
    };
}

OfxOverlayInteract::OfxOverlayInteract(OfxImageEffectInstance &v,
                                       int bitDepthPerComponent,
                                       bool hasAlpha)
    : OFX::Host::ImageEffect::OverlayInteract(v,bitDepthPerComponent,hasAlpha)
      , NatronOverlayInteractSupport()
{
}

////////////////////////////////////////////////////////////////////////////////////////////
// protect all OpenGL attribs from anything wrong that could be done in interact functions
// Should this be done in the GUI?
// Probably not: the fact that interacts are OpenGL
OfxStatus
OfxOverlayInteract::createInstanceAction()
{
    //OGLContextSaver s(_viewport);
    return OFX::Host::ImageEffect::OverlayInteract::createInstanceAction();
}

OfxStatus
OfxOverlayInteract::drawAction(OfxTime time,
                               const OfxPointD &renderScale)
{
    OGLContextSaver s(_viewport);
    OfxStatus stat = OFX::Host::ImageEffect::OverlayInteract::drawAction(time, renderScale);
    return stat;
}

OfxStatus
OfxOverlayInteract::penMotionAction(OfxTime time,
                                    const OfxPointD &renderScale,
                                    const OfxPointD &penPos,
                                    const OfxPointI &penPosViewport,
                                    double  pressure)
{
    //OGLContextSaver s(_viewport);
    return OFX::Host::ImageEffect::OverlayInteract::penMotionAction(time, renderScale, penPos, penPosViewport, pressure);

}

OfxStatus
OfxOverlayInteract::penUpAction(OfxTime time,
                                const OfxPointD &renderScale,
                                const OfxPointD &penPos,
                                const OfxPointI &penPosViewport,
                                double pressure)
{
    return OFX::Host::ImageEffect::OverlayInteract::penUpAction(time, renderScale, penPos, penPosViewport, pressure);

}

OfxStatus
OfxOverlayInteract::penDownAction(OfxTime time,
                                  const OfxPointD &renderScale,
                                  const OfxPointD &penPos,
                                  const OfxPointI &penPosViewport,
                                  double pressure)
{
    return OFX::Host::ImageEffect::OverlayInteract::penDownAction(time, renderScale, penPos, penPosViewport, pressure);

}

OfxStatus
OfxOverlayInteract::keyDownAction(OfxTime time,
                                  const OfxPointD &renderScale,
                                  int     key,
                                  char*   keyString)
{
    return OFX::Host::ImageEffect::OverlayInteract::keyDownAction(time, renderScale, key, keyString);

}

OfxStatus
OfxOverlayInteract::keyUpAction(OfxTime time,
                                const OfxPointD &renderScale,
                                int     key,
                                char*   keyString)
{
    return OFX::Host::ImageEffect::OverlayInteract::keyUpAction(time, renderScale, key, keyString);

}

OfxStatus
OfxOverlayInteract::keyRepeatAction(OfxTime time,
                                    const OfxPointD &renderScale,
                                    int     key,
                                    char*   keyString)
{
    return OFX::Host::ImageEffect::OverlayInteract::keyRepeatAction(time, renderScale, key, keyString);
}

OfxStatus
OfxOverlayInteract::gainFocusAction(OfxTime time,
                                    const OfxPointD &renderScale)
{
    return OFX::Host::ImageEffect::OverlayInteract::gainFocusAction(time, renderScale);

}

OfxStatus
OfxOverlayInteract::loseFocusAction(OfxTime  time,
                                    const OfxPointD &renderScale)
{
    return OFX::Host::ImageEffect::OverlayInteract::loseFocusAction(time, renderScale);
}




Natron::OfxParamOverlayInteract::OfxParamOverlayInteract(KnobI* knob,
                                                         OFX::Host::Interact::Descriptor &desc,
                                                         void *effectInstance)
    : OFX::Host::Interact::Instance(desc,effectInstance)
      , NatronOverlayInteractSupport()
{
    setCallingViewport(knob);
}

NatronOverlayInteractSupport::NatronOverlayInteractSupport()
    : _viewport(NULL)
{
}

NatronOverlayInteractSupport::~NatronOverlayInteractSupport()
{
}

void
NatronOverlayInteractSupport::setCallingViewport(OverlaySupport* viewport)
{
    _viewport = viewport;
}

OverlaySupport*
NatronOverlayInteractSupport::getLastCallingViewport() const
{
    return _viewport;
}

OfxStatus
NatronOverlayInteractSupport::n_swapBuffers()
{
    if (_viewport) {
        _viewport->swapOpenGLBuffers();
    }

    return kOfxStatOK;
}

OfxStatus
NatronOverlayInteractSupport::n_redraw()
{
    if (_viewport) {
        _viewport->redraw();
    }

    return kOfxStatOK;
}

void
NatronOverlayInteractSupport::n_getViewportSize(double &width,
                                                double &height) const
{
    if (_viewport) {
        _viewport->getViewportSize(width,height);
    }
}

void
NatronOverlayInteractSupport::n_getPixelScale(double & xScale,
                                              double & yScale) const
{
    if (_viewport) {
        _viewport->getPixelScale(xScale,yScale);
    }
}

void
NatronOverlayInteractSupport::n_getBackgroundColour(double &r,
                                                    double &g,
                                                    double &b) const
{
    if (_viewport) {
        _viewport->getBackgroundColour(r,g,b);
    }
}

bool
NatronOverlayInteractSupport::n_getSuggestedColour(double &r,
                                                   double &g,
                                                   double &b) const
{
    return false;
    // TODO
    //r = g = b = ...;
    //return true;
}

void
Natron::OfxParamOverlayInteract::getMinimumSize(double & minW,
                                                double & minH) const
{
    minW = _descriptor.getProperties().getDoubleProperty(kOfxParamPropInteractMinimumSize,0);
    minH = _descriptor.getProperties().getDoubleProperty(kOfxParamPropInteractMinimumSize,1);
}

void
Natron::OfxParamOverlayInteract::getPreferredSize(int & pW,
                                                  int & pH) const
{
    pW = _descriptor.getProperties().getIntProperty(kOfxParamPropInteractPreferedSize,0);
    pH = _descriptor.getProperties().getIntProperty(kOfxParamPropInteractPreferedSize,1);
}

void
Natron::OfxParamOverlayInteract::getSize(int &w,
                                         int &h) const
{
    w = _descriptor.getProperties().getIntProperty(kOfxParamPropInteractSize,0);
    h = _descriptor.getProperties().getIntProperty(kOfxParamPropInteractSize,1);
}

void
Natron::OfxParamOverlayInteract::setSize(int w,
                                         int h)
{
    _descriptor.getProperties().setIntProperty(kOfxParamPropInteractSize,w,0);
    _descriptor.getProperties().setIntProperty(kOfxParamPropInteractSize,h,1);
}

void
Natron::OfxParamOverlayInteract::getPixelAspectRatio(double & par) const
{
    par = _descriptor.getProperties().getDoubleProperty(kOfxParamPropInteractSizeAspect);
}

