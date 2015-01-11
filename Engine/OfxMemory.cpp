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

#include "OfxMemory.h"

#include "Global/Macros.h"
CLANG_DIAG_OFF(deprecated)
#include <QMutex>
CLANG_DIAG_ON(deprecated)

#include "Engine/PluginMemory.h"

OfxMemory::OfxMemory(Natron::EffectInstance* effect)
    : OFX::Host::Memory::Instance()
      , _memory( new PluginMemory(effect) )
{
}

OfxMemory::~OfxMemory()
{
    delete _memory;
}

void*
OfxMemory::getPtr()
{
    return _memory->getPtr();
}

bool
OfxMemory::alloc(size_t nBytes)
{
    bool ret = false;

    try {
        ret = _memory->alloc(nBytes);
    } catch (const std::bad_alloc &) {
        return false;
    }

    return ret;
}

void
OfxMemory::freeMem()
{
    _memory->freeMem();
}

void
OfxMemory::lock()
{
    _memory->lock();
}

void
OfxMemory::unlock()
{
    _memory->unlock();
}

