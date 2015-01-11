//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NATRON_GUI_GROUPBOXLABEL_H_
#define NATRON_GUI_GROUPBOXLABEL_H_

// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>

#include "Global/Macros.h"
CLANG_DIAG_OFF(deprecated)
CLANG_DIAG_OFF(uninitialized)
#include <QLabel> // in QtGui on Qt4, in QtWidgets on Qt5
CLANG_DIAG_ON(deprecated)
CLANG_DIAG_ON(uninitialized)

#include "Global/Macros.h"

class GroupBoxLabel
    : public QLabel
{
    Q_OBJECT

public:

    GroupBoxLabel(QWidget *parent = 0);

    virtual ~GroupBoxLabel() OVERRIDE
    {
    }

    bool isChecked() const
    {
        return _checked;
    }

public Q_SLOTS:

    void setChecked(bool);

private:
    virtual void mousePressEvent(QMouseEvent* /*e*/) OVERRIDE FINAL
    {
        if ( isEnabled() ) {
            Q_EMIT checked(!_checked);
        }
    }

Q_SIGNALS:
    void checked(bool);

private:
    bool _checked;
};

#endif // ifndef NATRON_GUI_GROUPBOXLABEL_H_
