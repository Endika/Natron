//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef NODEBACKDROP_H
#define NODEBACKDROP_H

#include "Gui/NodeGui.h"

class QVBoxLayout;

class String_Knob;

class NodeGraph;
class DockablePanel;
class NodeBackDropSerialization;
struct BackDropGuiPrivate;
class BackDropGui : public NodeGui
{
    Q_OBJECT

public:

    BackDropGui(QGraphicsItem* parent = 0);
    
    virtual ~BackDropGui();
    
    void refreshTextLabelFromKnob();

public Q_SLOTS:
    
    void onLabelChanged(const QString& label);
        
private:
    
    virtual int getBaseDepth() const OVERRIDE FINAL { return -10; }
    
    virtual void createGui() OVERRIDE FINAL;
    
    virtual bool canMakePreview() OVERRIDE FINAL WARN_UNUSED_RETURN
    {
        return false;
    }
    
    virtual void resizeExtraContent(int w,int h,bool forceResize) OVERRIDE FINAL;
    
    virtual bool mustFrameName() const OVERRIDE FINAL WARN_UNUSED_RETURN
    {
        return true;
    }
    
    virtual bool mustAddResizeHandle() const OVERRIDE FINAL WARN_UNUSED_RETURN
    {
        return true;
    }
    
    virtual void adjustSizeToContent(int *w,int *h) OVERRIDE FINAL;
        
    virtual void getInitialSize(int *w, int *h) OVERRIDE FINAL;
    

    
private:
    

    boost::scoped_ptr<BackDropGuiPrivate> _imp;
};

#endif // NODEBACKDROP_H
