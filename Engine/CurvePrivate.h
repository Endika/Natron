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
#ifndef NATRON_ENGINE_CURVEPRIVATE_H_
#define NATRON_ENGINE_CURVEPRIVATE_H_

#include <boost/shared_ptr.hpp>
#include <QReadWriteLock>

#include "Engine/Rect.h"
#include "Engine/Variant.h"
#include "Engine/Knob.h"
#include "Engine/KnobTypes.h"
#include "Engine/KnobFile.h"

class Curve;
class KeyFrame;
class KnobI;

struct CurvePrivate
{
    enum CurveTypeEnum
    {
        eCurveTypeDouble = 0, //< the values held by the keyframes can be any real
        eCurveTypeInt, //< the values held by the keyframes can only be integers
        eCurveTypeIntConstantInterp, //< same as eCurveTypeInt but interpolation is restricted to eKeyframeTypeConstant
        eCurveTypeBool, //< the values held by the keyframes can be either 0 or 1
        eCurveTypeString //< the values held by the keyframes can only be integers and keyframes are ordered by increasing values
                     // and times
    };

    KeyFrameSet keyFrames;
    KnobI* owner;
    int dimensionInOwner;
    bool isParametric;
    CurveTypeEnum type;
    double xMin, xMax;
    double yMin, yMax;
    bool hasYRange;
    mutable QReadWriteLock _lock; //< the plug-ins can call getValueAt at any moment and we must make sure the user is not playing around


    CurvePrivate()
        : keyFrames()
          , owner(NULL)
          , dimensionInOwner(-1)
          , isParametric(false)
          , type(eCurveTypeDouble)
          , xMin(INT_MIN)
          , xMax(INT_MAX)
          , yMin(INT_MIN)
          , yMax(INT_MAX)
          , hasYRange(false)
          , _lock(QReadWriteLock::Recursive)
    {
    }

    CurvePrivate(const CurvePrivate & other)
        : _lock(QReadWriteLock::Recursive)
    {
        *this = other;
    }

    void operator=(const CurvePrivate & other)
    {
        keyFrames = other.keyFrames;
        owner = other.owner;
        dimensionInOwner = other.dimensionInOwner;
        isParametric = other.isParametric;
        type = other.type;
        xMin = other.xMin;
        xMax = other.xMax;
        yMin = other.yMin;
        yMax = other.yMax;
        hasYRange = other.hasYRange;
    }
};


#endif // NATRON_ENGINE_CURVEPRIVATE_H_
