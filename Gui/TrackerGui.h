//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef TRACKERGUI_H
#define TRACKERGUI_H

#ifndef Q_MOC_RUN
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#endif
#include "Global/Macros.h"
CLANG_DIAG_OFF(deprecated)
CLANG_DIAG_OFF(uninitialized)
#include <QObject>
CLANG_DIAG_ON(deprecated)
CLANG_DIAG_ON(uninitialized)

class QWidget;
class ViewerTab;
class TrackerPanel;
class QKeyEvent;
class QPointF;
class QMouseEvent;

struct TrackerGuiPrivate;
class TrackerGui
    : public QObject
{
    Q_OBJECT

public:

    TrackerGui(const boost::shared_ptr<TrackerPanel> & panel,
               ViewerTab* parent);

    virtual ~TrackerGui();

    /**
     * @brief Return the horizontal buttons bar.
     **/
    QWidget* getButtonsBar() const;


    void drawOverlays(double scaleX, double scaleY) const;

    bool penDown(double scaleX, double scaleY, const QPointF & viewportPos, const QPointF & pos, QMouseEvent* e);

    bool penDoubleClicked(double scaleX, double scaleY, const QPointF & viewportPos, const QPointF & pos, QMouseEvent* e);

    bool penMotion(double scaleX, double scaleY, const QPointF & viewportPos, const QPointF & pos, QMouseEvent* e);

    bool penUp(double scaleX, double scaleY, const QPointF & viewportPos, const QPointF & pos, QMouseEvent* e);

    bool keyDown(double scaleX, double scaleY, QKeyEvent* e);

    bool keyUp(double scaleX, double scaleY, QKeyEvent* e);

    bool loseFocus(double scaleX, double scaleY);

public slots:

    void onAddTrackClicked(bool clicked);

    void onTrackBwClicked();

    void onTrackPrevClicked();

    void onStopButtonClicked();

    void onTrackNextClicked();

    void onTrackFwClicked();

    void onUpdateViewerClicked(bool clicked);

    void onClearAllAnimationClicked();

    void onClearBwAnimationClicked();

    void onClearFwAnimationClicked();

    void updateSelectionFromSelectionRectangle(bool onRelease);

    void onSelectionCleared();

    void onTrackingEnded();
private:

    boost::scoped_ptr<TrackerGuiPrivate> _imp;
};

#endif // TRACKERGUI_H
