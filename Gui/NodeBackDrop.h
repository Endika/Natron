//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef NODEBACKDROP_H
#define NODEBACKDROP_H
#ifndef Q_MOC_RUN
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#endif
#include "Global/Macros.h"

CLANG_DIAG_OFF(deprecated)
CLANG_DIAG_OFF(uninitialized)
#include <QGraphicsRectItem>
CLANG_DIAG_ON(deprecated)
CLANG_DIAG_ON(uninitialized)

#include "Engine/Knob.h"

class QVBoxLayout;

class String_Knob;

class NodeGraph;
class DockablePanel;
class NodeBackDropSerialization;
struct NodeBackDropPrivate;
class NodeBackDrop
    : public NamedKnobHolder, public QGraphicsRectItem
{
    Q_OBJECT

public:

    NodeBackDrop(NodeGraph* dag,
                 QGraphicsItem* parent = 0);

    void initialize(const QString & name,bool requestedByLoad,const NodeBackDropSerialization & serialization,QVBoxLayout *dockContainer);

    virtual ~NodeBackDrop();


    ///We should only ever use these functions that makes it thread safe for the serialization
    void setPos_mt_safe(const QPointF & pos);
    QPointF getPos_mt_safe() const;

    boost::shared_ptr<String_Knob> getLabelKnob() const;

    ///Mt-safe
    QColor getCurrentColor() const;
    void setCurrentColor(const QColor & color);

    ///Mt-safe
    QString getName() const;
    void setName(const QString & str);
    void trySetName(const QString& str);
    virtual std::string getName_mt_safe() const OVERRIDE FINAL WARN_UNUSED_RETURN;

    ///Mt-safe
    void resize(int w,int h);
    void getSize(int & w,int & h) const;

    double getHeaderHeight() const;

    bool isNearbyHeader(const QPointF & scenePos);
    bool isNearbyResizeHandle(const QPointF & scenePos);

    bool isSettingsPanelClosed() const;

    DockablePanel* getSettingsPanel() const;

    void refreshTextLabelFromKnob();

    void slaveTo(NodeBackDrop* master);
    void unslave();

    bool isSlave() const;

    NodeBackDrop* getMaster() const;

    void deactivate();
    void activate();

    ///MT-Safe
    bool getIsSelected() const;
    
    void centerOnIt();

    void setVisibleDetails(bool visible);
signals:

    void positionChanged();

public slots:

    void onNameChanged(const QString & name);
    void onColorChanged(const QColor & color);

    void setSettingsPanelClosed(bool closed);

    void setUserSelected(bool selected);

    void refreshSlaveMasterLinkPosition();

private:

    virtual void onKnobValueChanged(KnobI* k,Natron::ValueChangedReasonEnum reason,SequenceTime time,
                                    bool originatedFromMainThread) OVERRIDE FINAL;
    virtual void evaluate(KnobI* /*knob*/,
                          bool /*isSignificant*/,
                          Natron::ValueChangedReasonEnum /*reason*/) OVERRIDE FINAL
    {
    }

    virtual void initializeKnobs() OVERRIDE FINAL;
    boost::scoped_ptr<NodeBackDropPrivate> _imp;
};

#endif // NODEBACKDROP_H
