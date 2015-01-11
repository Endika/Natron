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

#include "NonKeyParams.h"

using namespace Natron;


NonKeyParams::NonKeyParams()
    : _cost(0)
      , _elementsCount(0)
{
}

NonKeyParams::NonKeyParams(int cost,
                           U64 elementsCount)
    : _cost(cost)
      , _elementsCount(elementsCount)
{
}

NonKeyParams::NonKeyParams(const NonKeyParams & other)
    : _cost(other._cost)
      , _elementsCount(other._elementsCount)
{
}

///the number of elements the associated cache entry should allocate (relative to the datatype of the entry)
U64
NonKeyParams::getElementsCount() const
{
    return _elementsCount;
}

void
NonKeyParams::setElementsCount(U64 count)
{
    _elementsCount = count;
}

int
NonKeyParams::getCost() const
{
    return _cost;
}



