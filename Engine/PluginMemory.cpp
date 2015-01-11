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

#include "PluginMemory.h"

#include <stdexcept>
#include <vector>

#include "Global/Macros.h"
CLANG_DIAG_OFF(deprecated)
#include <QMutex>
CLANG_DIAG_ON(deprecated)
#include "Engine/EffectInstance.h"


struct PluginMemory::Implementation
{
    Implementation(Natron::EffectInstance* effect_)
        : data()
          , locked(0)
          , mutex()
          , effect(effect_)
    {
    }

    std::vector<char> data;
    int locked;
    QMutex mutex;
    Natron::EffectInstance* effect;
};

PluginMemory::PluginMemory(Natron::EffectInstance* effect)
    : _imp( new Implementation(effect) )
{
    if (effect) {
        _imp->effect->addPluginMemoryPointer(this);
    }
}

PluginMemory::~PluginMemory()
{
    if (_imp->effect) {
        _imp->effect->removePluginMemoryPointer(this);
    }
}

bool
PluginMemory::alloc(size_t nBytes)
{
    QMutexLocker l(&_imp->mutex);

    if (_imp->locked) {
        return false;
    } else {
        _imp->data.resize(nBytes);
        if (_imp->effect) {
            _imp->effect->registerPluginMemory( _imp->data.size() );
        }

        return true;
    }
}

void
PluginMemory::freeMem()
{
    QMutexLocker l(&_imp->mutex);
    if (_imp->effect) {
        _imp->effect->unregisterPluginMemory( _imp->data.size() );
    }
    _imp->data.clear();
    _imp->locked = 0;
}

void*
PluginMemory::getPtr()
{
    QMutexLocker l(&_imp->mutex);

    assert(_imp->data.size() == 0 || (_imp->data.size() > 0 && &_imp->data.front()));

    return (void*)( &_imp->data.front() );
}

void
PluginMemory::lock()
{
    QMutexLocker l(&_imp->mutex);

    ++_imp->locked;
}

void
PluginMemory::unlock()
{
    QMutexLocker l(&_imp->mutex);

    // http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#OfxImageEffectSuiteV1_imageMemoryUnlock
    // "Also note, if you unlock a completely unlocked handle, it has no effect (ie: the lock count can't be negative)."
    if (_imp->locked > 0) {
        --_imp->locked;
    }
}

