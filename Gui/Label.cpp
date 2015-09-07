/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of Natron <http://www.natron.fr/>,
 * Copyright (C) 2015 INRIA and Alexandre Gauthier-Foichat
 *
 * Natron is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Natron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Natron.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

// ***** BEGIN PYTHON BLOCK *****
// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>
// ***** END PYTHON BLOCK *****

#include "Label.h"

#include <QApplication>
#include <QStyle>

using namespace Natron;

Label::Label(const QString &text,
             QWidget *parent,
             Qt::WindowFlags f)
: QLabel(text, parent, f)
, altered(false)

{
    setFont(QApplication::font()); // necessary, or the labels will get the default font size
}

Label::Label(QWidget *parent,
             Qt::WindowFlags f)
: QLabel(parent, f)
, altered(false)
{
    setFont(QApplication::font()); // necessary, or the labels will get the default font size
}


bool
Label::getAltered() const
{
    return altered;
}

void
Label::setAltered(bool a)
{
    altered = a;
    style()->unpolish(this);
    style()->polish(this);
    repaint();
}