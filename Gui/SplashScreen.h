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

#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>

#include "Global/Macros.h"
CLANG_DIAG_OFF(deprecated)
CLANG_DIAG_OFF(uninitialized)
#include <QWidget>
#include <QPixmap>
#include <QString>
CLANG_DIAG_ON(deprecated)
CLANG_DIAG_ON(uninitialized)

class SplashScreen
    : public QWidget
{
    QPixmap _pixmap;
    QString _text;
    QString _versionString;

public:

    SplashScreen(const QString & filePath);

    virtual ~SplashScreen()
    {
    }

    void updateText(const QString & text);

private:

    virtual void paintEvent(QPaintEvent* e) OVERRIDE FINAL;
};

#endif // SPLASHSCREEN_H
