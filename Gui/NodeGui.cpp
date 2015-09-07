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

#include "NodeGui.h"

#include <cassert>
#include <algorithm> // min, max
#include <boost/scoped_array.hpp>

CLANG_DIAG_OFF(deprecated)
CLANG_DIAG_OFF(uninitialized)
#include <QLayout>
#include <QAction>
#include <QtConcurrentRun>
#include <QFontMetrics>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QGridLayout>
#include <QFile>
#include <QDialogButtonBox>
#include <QApplication>
CLANG_DIAG_ON(deprecated)
CLANG_DIAG_ON(uninitialized)

#include <ofxNatron.h>

#include "Engine/BackDrop.h"
#include "Engine/Image.h"
#include "Engine/Knob.h"
#include "Engine/Node.h"
#include "Engine/NodeSerialization.h"
#include "Engine/OfxEffectInstance.h"
#include "Engine/OfxImageEffectInstance.h"
#include "Engine/Plugin.h"
#include "Engine/Project.h"
#include "Engine/Settings.h"
#include "Engine/ViewerInstance.h"

#include "Gui/BackDropGui.h"
#include "Gui/Button.h"
#include "Gui/CurveEditor.h"
#include "Gui/DefaultOverlays.h"
#include "Gui/DockablePanel.h"
#include "Gui/DopeSheetEditor.h"
#include "Gui/Edge.h"
#include "Gui/Gui.h"
#include "Gui/GuiAppInstance.h"
#include "Gui/GuiApplicationManager.h"
#include "Gui/KnobGui.h"
#include "Gui/String_KnobGui.h"
#include "Gui/Label.h"
#include "Gui/LineEdit.h"
#include "Gui/MultiInstancePanel.h"
#include "Gui/NodeGraph.h"
#include "Gui/NodeGraphUndoRedo.h"
#include "Gui/NodeGuiSerialization.h"
#include "Gui/NodeSettingsPanel.h"
#include "Gui/SequenceFileDialog.h"
#include "Gui/SequenceFileDialog.h"
#include "Gui/SpinBox.h"
#include "Gui/Utils.h"
#include "Gui/ViewerGL.h"
#include "Gui/ViewerTab.h"

#define NATRON_STATE_INDICATOR_OFFSET 5

#define NATRON_EDGE_DROP_TOLERANCE 15

#define NATRON_MAGNETIC_GRID_GRIP_TOLERANCE 20

#define NATRON_MAGNETIC_GRID_RELEASE_DISTANCE 30

#define NATRON_ELLIPSE_WARN_DIAMETER 10

#define NODE_WIDTH 80
#define NODE_HEIGHT 30

#define DOT_GUI_DIAMETER 15

#define NATRON_PLUGIN_ICON_SIZE 20
#define PLUGIN_ICON_OFFSET 2

using namespace Natron;

using std::make_pair;

#ifndef M_PI
#define M_PI        3.14159265358979323846264338327950288   /* pi             */
#endif

static QString
replaceLineBreaksWithHtmlParagraph(QString txt)
{
    txt.replace("\n", "<br>");

    return txt;
}

static void getPixmapForMergeOperator(const QString& op,QPixmap* pix)
{
    if (op == "atop") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_ATOP, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "average") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_AVERAGE, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "color-burn") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_COLOR_BURN, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "color-dodge") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_COLOR_DODGE, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "conjoint-over") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_CONJOINT_OVER, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "copy") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_COPY, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "difference") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_DIFFERENCE, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "disjoint-over") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_DISJOINT_OVER, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "divide") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_DIVIDE, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "exclusion") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_EXCLUSION, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "freeze") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_FREEZE, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "from") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_FROM, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "geometric") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_GEOMETRIC, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "hard-light") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_HARD_LIGHT, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "hypot") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_HYPOT, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "in") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_IN, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "interpolated") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_INTERPOLATED, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "mask") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_MASK, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "matte") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_MATTE, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "max") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_MAX, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "min") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_MIN, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "minus") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_MINUS, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "multiply") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_MULTIPLY, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "out") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_OUT, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "over") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_OVER, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "overlay") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_OVERLAY, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "pinlight") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_PINLIGHT, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "plus") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_PLUS, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "reflect") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_REFLECT, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "screen") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_SCREEN, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "soft-light") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_SOFT_LIGHT, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "stencil") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_STENCIL, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "under") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_UNDER, NATRON_PLUGIN_ICON_SIZE, pix);
    } else if (op == "xor") {
        appPTR->getIcon(Natron::NATRON_PIXMAP_MERGE_XOR, NATRON_PLUGIN_ICON_SIZE, pix);
    }
}

NodeGui::NodeGui(QGraphicsItem *parent)
: QObject()
, QGraphicsItem(parent)
, _graph(NULL)
, _internalNode()
, _selected(false)
, _settingNameFromGui(false)
, _panelOpenedBeforeDeactivate(false)
, _pluginIcon(NULL)
, _pluginIconFrame(NULL)
, _mergeIcon(NULL)
, _nameItem(NULL)
, _nameFrame(NULL)
, _resizeHandle(NULL)
, _boundingBox(NULL)
, _channelsPixmap(NULL)
, _previewPixmap(NULL)
, _persistentMessage(NULL)
, _stateIndicator(NULL)
, _mergeHintActive(false)
, _bitDepthWarning(NULL)
, _disabledTopLeftBtmRight(NULL)
, _disabledBtmLeftTopRight(NULL)
, _inputEdges()
, _outputEdge(NULL)
, _settingsPanel(NULL)
, _mainInstancePanel(NULL)
, _panelCreated(false)
, _currentColorMutex()
, _currentColor()
, _clonedColor()
, _wasBeginEditCalled(false)
, positionMutex()
, _slaveMasterLink(NULL)
, _masterNodeGui()
, _knobsLinks()
, _expressionIndicator(NULL)
, _magnecEnabled()
, _magnecDistance()
, _updateDistanceSinceLastMagnec()
, _distanceSinceLastMagnec()
, _magnecStartingPos()
, _nodeLabel()
, _parentMultiInstance()
, _renderingStartedCount(0)
, _optionalInputsVisible(false)
, _mtSafeSizeMutex()
, _mtSafeWidth(0)
, _mtSafeHeight(0)
, _defaultOverlay()
, _undoStack(new QUndoStack())
, _overlayLocked(false)
{
}

NodeGui::~NodeGui()
{
    deleteReferences();

    delete _bitDepthWarning;
    delete _expressionIndicator;
}

void
NodeGui::initialize(NodeGraph* dag,
                    const boost::shared_ptr<Natron::Node> & internalNode)
{
    _internalNode = internalNode;
    assert(internalNode);
    _graph = dag;

    boost::shared_ptr<NodeGui> thisAsShared = shared_from_this();

    internalNode->setNodeGuiPointer(thisAsShared);

    QObject::connect( internalNode.get(), SIGNAL( labelChanged(QString) ), this, SLOT( onInternalNameChanged(QString) ) );
    QObject::connect( internalNode.get(), SIGNAL( refreshEdgesGUI() ),this,SLOT( refreshEdges() ) );
    QObject::connect( internalNode.get(), SIGNAL( knobsInitialized() ),this,SLOT( initializeKnobs() ) );
    QObject::connect( internalNode.get(), SIGNAL( inputsInitialized() ),this,SLOT( initializeInputs() ) );
    QObject::connect( internalNode.get(), SIGNAL( previewImageChanged(int) ), this, SLOT( updatePreviewImage(int) ) );
    QObject::connect( internalNode.get(), SIGNAL( previewRefreshRequested(int) ), this, SLOT( forceComputePreview(int) ) );
    QObject::connect( internalNode.get(), SIGNAL( deactivated(bool) ),this,SLOT( deactivate(bool) ) );
    QObject::connect( internalNode.get(), SIGNAL( activated(bool) ), this, SLOT( activate(bool) ) );
    QObject::connect( internalNode.get(), SIGNAL( inputChanged(int) ), this, SLOT( connectEdge(int) ) );
    QObject::connect( internalNode.get(), SIGNAL( persistentMessageChanged() ),this,SLOT( onPersistentMessageChanged() ) );
    QObject::connect( internalNode.get(), SIGNAL( renderingStarted() ), this, SLOT( onRenderingStarted() ) );
    QObject::connect( internalNode.get(), SIGNAL( renderingEnded() ), this, SLOT( onRenderingFinished() ) );
    QObject::connect( internalNode.get(), SIGNAL( inputNIsRendering(int) ), this, SLOT( onInputNRenderingStarted(int) ) );
    QObject::connect( internalNode.get(), SIGNAL( inputNIsFinishedRendering(int) ), this, SLOT( onInputNRenderingFinished(int) ) );
    QObject::connect( internalNode.get(), SIGNAL( allKnobsSlaved(bool) ), this, SLOT( onAllKnobsSlaved(bool) ) );
    QObject::connect( internalNode.get(), SIGNAL( knobsLinksChanged() ), this, SLOT( onKnobsLinksChanged() ) );
    QObject::connect( internalNode.get(), SIGNAL( outputsChanged() ),this,SLOT( refreshOutputEdgeVisibility() ) );
    QObject::connect( internalNode.get(), SIGNAL( previewKnobToggled() ),this,SLOT( onPreviewKnobToggled() ) );
    QObject::connect( internalNode.get(), SIGNAL( disabledKnobToggled(bool) ),this,SLOT( onDisabledKnobToggled(bool) ) );
    QObject::connect( internalNode.get(), SIGNAL( bitDepthWarningToggled(bool,QString) ),this,SLOT( toggleBitDepthIndicator(bool,QString) ) );
    QObject::connect( internalNode.get(), SIGNAL( nodeExtraLabelChanged(QString) ),this,SLOT( onNodeExtraLabelChanged(QString) ) );

    setCacheMode(DeviceCoordinateCache);

    OutputEffectInstance* isOutput = dynamic_cast<OutputEffectInstance*>(internalNode->getLiveInstance());
    if (isOutput) {
        QObject::connect (isOutput->getRenderEngine(), SIGNAL(refreshAllKnobs()), _graph, SLOT(refreshAllKnobsGui()));
    }

    ViewerInstance* isViewer = dynamic_cast<ViewerInstance*>(isOutput);
    if (isViewer) {
        QObject::connect(isViewer,SIGNAL(refreshOptionalState()),this,SLOT(refreshDashedStateOfEdges()));
    }

    createGui();

    NodePtr parent = internalNode->getParentMultiInstance();
    if (parent) {
        boost::shared_ptr<NodeGuiI> parentNodeGui_i = parent->getNodeGui();
        NodeGui* parentGui = dynamic_cast<NodeGui*>(parentNodeGui_i.get());
        assert(parentGui);
        if (parentGui->isSettingsPanelOpened()) {
            ensurePanelCreated();
            boost::shared_ptr<MultiInstancePanel> panel = parentGui->getMultiInstancePanel();
            assert(panel);
            panel->onChildCreated(internalNode);
        }
    }
    
    if (internalNode->getPluginID() == PLUGINID_OFX_MERGE) {
        boost::shared_ptr<KnobI> knob = internalNode->getKnobByName(kNatronOfxParamStringSublabelName);
        assert(knob);
        String_Knob* strKnob = dynamic_cast<String_Knob*>(knob.get());
        if (strKnob) {
            onNodeExtraLabelChanged(strKnob->getValue().c_str());
        }
    }


    if ( internalNode->makePreviewByDefault() ) {
        ///It calls resize
        togglePreview_internal(false);
    } else {
        int w,h;
        getInitialSize(&w, &h);
        resize(w,h);
    }

    
    _clonedColor.setRgb(200,70,100);

    //QColor defaultColor = getCurrentColor();
    Natron::EffectInstance* iseffect = internalNode->getLiveInstance();
    boost::shared_ptr<Settings> settings = appPTR->getCurrentSettings();
    float r,g,b;
    BackDrop* isBd = dynamic_cast<BackDrop*>(iseffect);
    
    std::list<std::string> grouping;
    iseffect->getPluginGrouping(&grouping);
    std::string majGroup = grouping.empty() ? "" : grouping.front();
    
    if ( iseffect->isReader() ) {
        settings->getReaderColor(&r, &g, &b);
    } else if (isBd) {
        settings->getDefaultBackDropColor(&r, &g, &b);
    } else if ( iseffect->isWriter() ) {
        settings->getWriterColor(&r, &g, &b);
    } else if ( iseffect->isGenerator() ) {
        settings->getGeneratorColor(&r, &g, &b);
    } else if (majGroup == PLUGIN_GROUP_COLOR) {
        settings->getColorGroupColor(&r, &g, &b);
    } else if (majGroup == PLUGIN_GROUP_FILTER) {
        settings->getFilterGroupColor(&r, &g, &b);
    } else if (majGroup == PLUGIN_GROUP_CHANNEL) {
        settings->getChannelGroupColor(&r, &g, &b);
    } else if (majGroup == PLUGIN_GROUP_KEYER) {
        settings->getKeyerGroupColor(&r, &g, &b);
    } else if (majGroup == PLUGIN_GROUP_MERGE) {
        settings->getMergeGroupColor(&r, &g, &b);
    } else if (majGroup == PLUGIN_GROUP_PAINT) {
        settings->getDrawGroupColor(&r, &g, &b);
    } else if (majGroup == PLUGIN_GROUP_TIME) {
        settings->getTimeGroupColor(&r, &g, &b);
    } else if (majGroup == PLUGIN_GROUP_TRANSFORM) {
        settings->getTransformGroupColor(&r, &g, &b);
    } else if (majGroup == PLUGIN_GROUP_MULTIVIEW) {
        settings->getViewsGroupColor(&r, &g, &b);
    } else if (majGroup == PLUGIN_GROUP_DEEP) {
        settings->getDeepGroupColor(&r, &g, &b);
    } else {
        settings->getDefaultNodeColor(&r, &g, &b);
    }
    QColor color;
    color.setRgbF(Natron::clamp<qreal>(r, 0., 1.),
                  Natron::clamp<qreal>(g, 0., 1.),
                  Natron::clamp<qreal>(b, 0., 1.));
    setCurrentColor(color);

    if ( !internalNode->isMultiInstance() ) {
        _nodeLabel = internalNode->getNodeExtraLabel().c_str();
        _nodeLabel = replaceLineBreaksWithHtmlParagraph(_nodeLabel);
    }


    ///Refresh the name in the line edit
    onInternalNameChanged( internalNode->getLabel().c_str() );

    ///Make the output edge
    if ( !isBd && !internalNode->isOutputNode() ) {
        _outputEdge = new Edge( thisAsShared,parentItem() );
    }

    ///Refresh the disabled knob
    if ( internalNode->isNodeDisabled() ) {
        onDisabledKnobToggled(true);
    }

    ///Link the position of the node to the position of the parent multi-instance
    const std::string parentMultiInstanceName = internalNode->getParentMultiInstanceName();
    if ( !parentMultiInstanceName.empty() ) {
        boost::shared_ptr<Natron::Node> parentNode = internalNode->getGroup()->getNodeByName(parentMultiInstanceName);
        boost::shared_ptr<NodeGuiI> parentNodeGui_I = parentNode->getNodeGui();
        assert(parentNode && parentNodeGui_I);
        NodeGui* parentNodeGui = dynamic_cast<NodeGui*>(parentNodeGui_I.get());
        assert(parentNodeGui);
        QObject::connect( parentNodeGui, SIGNAL( positionChanged(int,int) ),this,SLOT( onParentMultiInstancePositionChanged(int,int) ) );
        QPointF p = parentNodeGui->pos();
        refreshPosition(p.x(), p.y(),true);
    }

    getNode()->initializeDefaultOverlays();

} // initialize

void
NodeGui::ensurePanelCreated()
{
    if (_panelCreated) {
        return;
    }
    _panelCreated = true;
    Gui* gui = getDagGui()->getGui();
    QVBoxLayout* propsLayout = gui->getPropertiesLayout();
    assert(propsLayout);
    boost::shared_ptr<NodeGui> thisShared = shared_from_this();
    _settingsPanel = createPanel(propsLayout,thisShared);
    if (_settingsPanel) {
        QObject::connect( _settingsPanel,SIGNAL( nameChanged(QString) ),this,SLOT( setName(QString) ) );
        QObject::connect( _settingsPanel,SIGNAL( closeChanged(bool) ), this, SLOT( onSettingsPanelClosed(bool) ) );
        QObject::connect( _settingsPanel,SIGNAL( colorChanged(QColor) ),this,SLOT( onSettingsPanelColorChanged(QColor) ) );
        if (getNode()->isRotoPaintingNode()) {
            _graph->getGui()->setRotoInterface(this);
        }
        if (getNode()->isTrackerNode()) {
            _graph->getGui()->createNewTrackerInterface(this);
        }
    }
    initializeKnobs();
    beginEditKnobs();
    gui->addNodeGuiToCurveEditor(thisShared);
    gui->addNodeGuiToDopeSheetEditor(thisShared);
    
    //Ensure panel for all children if multi-instance
    
    boost::shared_ptr<MultiInstancePanel> panel = getMultiInstancePanel();
    if (_mainInstancePanel && panel) {
        panel->setRedrawOnSelectionChanged(false);
        
        /*
         * If there are many children, each children may request for a redraw of the viewer which may 
         * very well freeze the UI.
         * We just do one redraw when all children are created
         */
        NodeList children;
        getNode()->getChildrenMultiInstance(&children);
        for (NodeList::iterator it = children.begin() ; it != children.end(); ++it) {
            boost::shared_ptr<NodeGuiI> gui_i = (*it)->getNodeGui();
            assert(gui_i);
            NodeGui* gui = dynamic_cast<NodeGui*>(gui_i.get());
            assert(gui);
            gui->ensurePanelCreated();
            
            panel->onChildCreated(*it);
        }
        panel->setRedrawOnSelectionChanged(true);
        if (!children.empty()) {
            getDagGui()->getGui()->redrawAllViewers();
        }
    }
}

void
NodeGui::onSettingsPanelClosed(bool closed)
{
    QString message;
    int type;
    getNode()->getPersistentMessage(&message, &type);

    if (!message.isEmpty()) {
        const std::list<ViewerTab*>& viewers = getDagGui()->getGui()->getViewersList();
        for (std::list<ViewerTab*>::const_iterator it = viewers.begin(); it != viewers.end(); ++it) {
            (*it)->getViewer()->updatePersistentMessage();
        }
    }
    Q_EMIT settingsPanelClosed(closed);
}

NodeSettingsPanel*
NodeGui::createPanel(QVBoxLayout* container,
                     const boost::shared_ptr<NodeGui>& thisAsShared)
{
    NodeSettingsPanel* panel = 0;

    boost::shared_ptr<Natron::Node> node = getNode();
    ViewerInstance* isViewer = dynamic_cast<ViewerInstance*>( node->getLiveInstance() );

    if (!isViewer) {
        assert(container);
        boost::shared_ptr<MultiInstancePanel> multiPanel;
        if ( node->isTrackerNode() && node->isMultiInstance() && node->getParentMultiInstanceName().empty() ) {
            multiPanel.reset( new TrackerPanel(thisAsShared) );

            ///This is valid only if the node is a multi-instance and this is the main instance.
            ///The "real" panel showed on the gui will be the _settingsPanel, but we still need to create
            ///another panel for the main-instance (hidden) knobs to function properly (and also be showed in the CurveEditor)

            _mainInstancePanel = new NodeSettingsPanel( boost::shared_ptr<MultiInstancePanel>(),_graph->getGui(),
                                                        thisAsShared,container,container->parentWidget() );
            _mainInstancePanel->blockSignals(true);
            _mainInstancePanel->setClosed(true);
            _mainInstancePanel->initializeKnobs();
        }
        panel = new NodeSettingsPanel( multiPanel,_graph->getGui(),thisAsShared,container,container->parentWidget() );

        if (panel) {
            bool isCreatingPythonGroup = getDagGui()->getGui()->getApp()->isCreatingPythonGroup();
            
            std::string pluginID = node->getPluginID();
            if (pluginID == PLUGINID_NATRON_OUTPUT ||
                (isCreatingPythonGroup && pluginID != PLUGINID_NATRON_GROUP) ||
                !node->getParentMultiInstanceName().empty()) {
                panel->setClosed(true);
            } else {
                _graph->getGui()->addVisibleDockablePanel(panel);
            }
        }
    }

    return panel;
}

void
NodeGui::getSizeWithPreview(int *w, int *h) const
{
    getInitialSize(w,h);
    *w = *w -  (NODE_WIDTH / 2.) + NATRON_PREVIEW_WIDTH;
    *h = *h + NATRON_PREVIEW_HEIGHT;
}

void
NodeGui::getInitialSize(int *w, int *h) const
{
    const QString& iconFilePath = getNode()->getPlugin()->getIconFilePath();
    if (!iconFilePath.isEmpty() && QFile::exists(iconFilePath) && appPTR->getCurrentSettings()->isPluginIconActivatedOnNodeGraph()) {
        *w = NODE_WIDTH + NATRON_PLUGIN_ICON_SIZE + PLUGIN_ICON_OFFSET * 2;
    } else {
        *w = NODE_WIDTH;
    }
    *h = NODE_HEIGHT;
}

void
NodeGui::createGui()
{
    int depth = getBaseDepth();
    setZValue(depth);
    _boundingBox = new QGraphicsRectItem(this);
    _boundingBox->setZValue(depth);

    if (mustFrameName()) {
        _nameFrame = new QGraphicsRectItem(this);
        _nameFrame->setZValue(depth + 1);
    }

    if (mustAddResizeHandle()) {
        _resizeHandle = new QGraphicsPolygonItem(this);
        _resizeHandle->setZValue(depth + 1);
    }

    const QString& iconFilePath = getNode()->getPlugin()->getIconFilePath();

    BackDropGui* isBd = dynamic_cast<BackDropGui*>(this);

    if (!isBd && !iconFilePath.isEmpty() && appPTR->getCurrentSettings()->isPluginIconActivatedOnNodeGraph()) {

        QPixmap pix(iconFilePath);
        if (QFile::exists(iconFilePath) && !pix.isNull()) {
            _pluginIcon = new QGraphicsPixmapItem(this);
            _pluginIcon->setZValue(depth + 1);
            _pluginIconFrame = new QGraphicsRectItem(this);
            _pluginIconFrame->setZValue(depth);
            _pluginIconFrame->setBrush(QColor(50,50,50));
            pix = pix.scaled(NATRON_PLUGIN_ICON_SIZE,NATRON_PLUGIN_ICON_SIZE,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
            _pluginIcon->setPixmap(pix);
        }
    }

    if (getNode()->getPlugin()->getPluginID() == QString(PLUGINID_OFX_MERGE)) {
        _mergeIcon = new QGraphicsPixmapItem(this);
        _mergeIcon->setZValue(depth + 1);
    }

    _nameItem = new QGraphicsTextItem(getNode()->getLabel().c_str(),this);
    _nameItem->setDefaultTextColor( QColor(0,0,0,255) );
    //_nameItem->setFont( QFont(appFont,appFontSize) );
    _nameItem->setZValue(depth+ 1);

    _persistentMessage = new QGraphicsTextItem("",this);
    _persistentMessage->setZValue(depth + 3);
    QFont f = _persistentMessage->font();
    f.setPixelSize(25);
    _persistentMessage->setFont(f);
    _persistentMessage->hide();

    _stateIndicator = new QGraphicsRectItem(this);
    _stateIndicator->setZValue(depth -1);
    _stateIndicator->hide();

    QRectF bbox = boundingRect();
    QGradientStops bitDepthGrad;
    bitDepthGrad.push_back( qMakePair( 0., QColor(Qt::white) ) );
    bitDepthGrad.push_back( qMakePair( 0.3, QColor(Qt::yellow) ) );
    bitDepthGrad.push_back( qMakePair( 1., QColor(243,137,0) ) );
    _bitDepthWarning = new NodeGuiIndicator(depth + 2, "C",bbox.topLeft(),NATRON_ELLIPSE_WARN_DIAMETER,NATRON_ELLIPSE_WARN_DIAMETER,
                                            bitDepthGrad,QColor(0,0,0,255),this);
    _bitDepthWarning->setActive(false);


    QGradientStops exprGrad;
    exprGrad.push_back( qMakePair( 0., QColor(Qt::white) ) );
    exprGrad.push_back( qMakePair( 0.3, QColor(Qt::green) ) );
    exprGrad.push_back( qMakePair( 1., QColor(69,96,63) ) );
    _expressionIndicator = new NodeGuiIndicator(depth + 2,"E",bbox.topRight(),NATRON_ELLIPSE_WARN_DIAMETER,NATRON_ELLIPSE_WARN_DIAMETER,
                                                exprGrad,QColor(255,255,255),this);
    _expressionIndicator->setToolTip(Natron::convertFromPlainText(tr("This node has one or several expression(s) involving values of parameters of other "
                                         "nodes in the project. Hover the mouse on the green connections to see what are the effective links."), Qt::WhiteSpaceNormal));
    _expressionIndicator->setActive(false);

    _disabledBtmLeftTopRight = new QGraphicsLineItem(this);
    _disabledBtmLeftTopRight->setZValue(depth + 1);
    _disabledBtmLeftTopRight->hide();
    _disabledTopLeftBtmRight = new QGraphicsLineItem(this);
    _disabledTopLeftBtmRight->hide();
    _disabledTopLeftBtmRight->setZValue(depth + 1);
}

void
NodeGui::onSettingsPanelColorChanged(const QColor & color)
{
    {
        QMutexLocker k(&_currentColorMutex);
        _currentColor = color;
    }
    refreshCurrentBrush();
}

void
NodeGui::beginEditKnobs()
{
    _wasBeginEditCalled = true;
    getNode()->beginEditKnobs();
}

void
NodeGui::togglePreview_internal(bool refreshPreview)
{
    if ( !canMakePreview() ) {
        return;
    }
    if ( getNode()->isPreviewEnabled() ) {
        ensurePreviewCreated();
        if (refreshPreview) {
            getNode()->computePreviewImage( _graph->getGui()->getApp()->getTimeLine()->currentFrame() );
        }
    } else {
        if (_previewPixmap) {
            _previewPixmap->hide();
        }
        int w,h;
        getInitialSize(&w, &h);
        resize(w,h);
    }
}

void
NodeGui::ensurePreviewCreated()
{
    if (!_previewPixmap) {
        QImage prev(NATRON_PREVIEW_WIDTH, NATRON_PREVIEW_HEIGHT, QImage::Format_ARGB32);
        prev.fill(Qt::black);
        QPixmap prev_pixmap = QPixmap::fromImage(prev);
        _previewPixmap = new QGraphicsPixmapItem(prev_pixmap,this);
        _previewPixmap->setZValue(getBaseDepth() + 1);

    }
    QSize size = getSize();
    int w,h;
    getSizeWithPreview(&w,&h);
    if (size.width() < w ||
        size.height() < h) {


        resize(w,h);
        _previewPixmap->stackBefore(_nameItem);
        _previewPixmap->show();
    }

}

void
NodeGui::onPreviewKnobToggled()
{
    togglePreview_internal();
}

void
NodeGui::togglePreview()
{
    getNode()->togglePreview();
    togglePreview_internal();
}

void
NodeGui::removeUndoStack()
{
    if ( _graph && _graph->getGui() && _undoStack ) {
        _graph->getGui()->removeUndoStack( _undoStack.get() );
    }
}

void
NodeGui::discardGraphPointer()
{
    _graph = 0;
}

void
NodeGui::removeSettingsPanel()
{
    //called by DockablePanel when it is deleted by Qt's parenting scheme
    _settingsPanel = NULL;
}


void
NodeGui::refreshSize()
{
    QRectF bbox = boundingRect();
    resize(bbox.width(),bbox.height());
}

int
NodeGui::getFrameNameHeight() const
{
    if (mustFrameName()) {
        return _nameFrame->boundingRect().height();
    } else {
        return boundingRect().height();
    }
}

bool
NodeGui::isNearbyNameFrame(const QPointF& pos) const
{
    if (!mustFrameName()) {
        return _boundingBox->boundingRect().contains(pos);
    } else {
        QRectF headerBbox = _nameFrame->boundingRect();
        headerBbox.adjust(-5, -5, 5, 5);
        return headerBbox.contains(pos);
    }
}

bool
NodeGui::isNearbyResizeHandle(const QPointF& pos) const
{
    if (!mustAddResizeHandle()) {
        return false;
    }

    QPolygonF resizePoly = _resizeHandle->polygon();
    return resizePoly.containsPoint(pos,Qt::OddEvenFill);
}

void
NodeGui::adjustSizeToContent(int* /*w*/,int *h,bool adjustToTextSize)
{
    QRectF labelBbox = _nameItem->boundingRect();
    if (adjustToTextSize) {
        if (_previewPixmap && _previewPixmap->isVisible()) {
            int pw,ph;
            getSizeWithPreview(&pw, &ph);
            *h = ph;
        } else {
            *h = labelBbox.height() * 1.2;
        }
    } else {
        *h = std::max((double)*h, labelBbox.height() * 1.2);
    }
}

void
NodeGui::resize(int width,
                int height,
                bool forceSize,
                bool adjustToTextSize)
{
    if (!canResize()) {
        return;
    }

    QPointF topLeft = mapFromParent( pos() );
    QRectF labelBbox = _nameItem->boundingRect();

    adjustSizeToContent(&width,&height,adjustToTextSize);

    bool hasPluginIcon = _pluginIcon != NULL;

    {
        QMutexLocker k(&_mtSafeSizeMutex);
        _mtSafeWidth = width;
        _mtSafeHeight = height;
    }

    int iconWidth = hasPluginIcon ? NATRON_PLUGIN_ICON_SIZE + PLUGIN_ICON_OFFSET * 2 : 0;

    QRectF bbox(topLeft.x(),topLeft.y(),width,height);

    _boundingBox->setRect(bbox);

    if (hasPluginIcon) {
        _pluginIcon->setX(topLeft.x() + PLUGIN_ICON_OFFSET);
        int iconsOffset = _mergeIcon  && _mergeIcon->isVisible() ? (height - 2 * NATRON_PLUGIN_ICON_SIZE) / 3. : (height - NATRON_PLUGIN_ICON_SIZE) /2.;
        _pluginIcon->setY(topLeft.y() + iconsOffset);
        _pluginIconFrame->setRect(topLeft.x(),topLeft.y(),iconWidth, height);
    }

    if (_mergeIcon && _mergeIcon->isVisible()) {
        int iconsOffset =  (height - 2 * NATRON_PLUGIN_ICON_SIZE) / 3.;
        _mergeIcon->setX(topLeft.x() + PLUGIN_ICON_OFFSET);
        _mergeIcon->setY(topLeft.y() + iconsOffset * 2 + NATRON_PLUGIN_ICON_SIZE);
    }

    QFont f(appFont,appFontSize);
    QFontMetrics metrics(f);
    int nameWidth = labelBbox.width();
    _nameItem->setX( topLeft.x() + iconWidth +  ((width - iconWidth) / 2) - (nameWidth / 2) );

    double mh = labelBbox.height();
    _nameItem->setY(topLeft.y() + mh * 0.1);

    if (mustFrameName()) {
        QRectF nameFrameBox(topLeft.x(),topLeft.y(), width, 1.5 * mh);
        _nameFrame->setRect(nameFrameBox);
        height = std::max((double)height, nameFrameBox.height());
    }

    if (mustAddResizeHandle()) {
        QPolygonF poly;
        QPointF bottomRight(topLeft.x() + width,topLeft.y() + height);
        poly.push_back( QPointF( bottomRight.x() - 20,bottomRight.y() ) );
        poly.push_back(bottomRight);
        poly.push_back( QPointF(bottomRight.x(), bottomRight.y() - 20) );
        _resizeHandle->setPolygon(poly);

    }

    QString persistentMessage = _persistentMessage->toPlainText();
    f.setPixelSize(25);
    metrics = QFontMetrics(f);
    int pMWidth = metrics.width(persistentMessage);
    QPointF bitDepthPos(topLeft.x() + iconWidth + (width - iconWidth) / 2,0);
    _bitDepthWarning->refreshPosition(bitDepthPos);

    _expressionIndicator->refreshPosition( topLeft + QPointF(width,0) );

    _persistentMessage->setPos(topLeft.x() + (width / 2) - (pMWidth / 2), topLeft.y() + height / 2 - metrics.height() / 2);
    _stateIndicator->setRect(topLeft.x() - NATRON_STATE_INDICATOR_OFFSET,topLeft.y() - NATRON_STATE_INDICATOR_OFFSET,
                             width + NATRON_STATE_INDICATOR_OFFSET * 2,height + NATRON_STATE_INDICATOR_OFFSET * 2);
    if (_previewPixmap) {
        _previewPixmap->setPos(topLeft.x() + iconWidth + NODE_WIDTH / 4. ,
                               topLeft.y() + height / 2 - NATRON_PREVIEW_HEIGHT / 2 + 10);
    }

    _disabledBtmLeftTopRight->setLine( QLineF( bbox.bottomLeft(),bbox.topRight() ) );
    _disabledTopLeftBtmRight->setLine( QLineF( bbox.topLeft(),bbox.bottomRight() ) );

    resizeExtraContent(width,height,forceSize);

    refreshPosition( pos().x(), pos().y(), true );
}

void
NodeGui::refreshPositionEnd(double x,
                            double y)
{
    setPos(x, y);
    if (_graph) {
        QRectF bbox = mapRectToScene(boundingRect());
        const NodeGuiList & allNodes = _graph->getAllActiveNodes();

        for (NodeGuiList::const_iterator it = allNodes.begin(); it != allNodes.end(); ++it) {
            if ((*it)->isVisible() && (it->get() != this) && (*it)->intersects(bbox)) {
                setAboveItem( it->get() );
            }
        }
    }
    refreshEdges();
    NodePtr node = getNode();
    if (node) {
        const std::list<Natron::Node* > & outputs = node->getGuiOutputs();

        for (std::list<Natron::Node* >::const_iterator it = outputs.begin(); it != outputs.end(); ++it) {
            assert(*it);
            (*it)->doRefreshEdgesGUI();
        }
    }
    Q_EMIT positionChanged(x,y);
}

void
NodeGui::refreshPosition(double x,
                         double y,
                         bool skipMagnet,
                         const QPointF & mouseScenePos)
{
    if (appPTR->getCurrentSettings()->isSnapToNodeEnabled() && !skipMagnet) {
        QSize size = getSize();
        ///handle magnetic grid
        QPointF middlePos(x + size.width() / 2,y + size.height() / 2);


        if ( _magnecEnabled.x() || _magnecEnabled.y() ) {
            if ( _magnecEnabled.x() ) {
                _magnecDistance.rx() += ( x - _magnecStartingPos.x() );
                if (std::abs( _magnecDistance.x() ) >= NATRON_MAGNETIC_GRID_RELEASE_DISTANCE) {
                    _magnecEnabled.rx() = 0;
                    _updateDistanceSinceLastMagnec.rx() = 1;
                    _distanceSinceLastMagnec.rx() = 0;
                }
            }
            if ( _magnecEnabled.y() ) {
                _magnecDistance.ry() += ( y - _magnecStartingPos.y() );
                if (std::abs( _magnecDistance.y() ) >= NATRON_MAGNETIC_GRID_RELEASE_DISTANCE) {
                    _magnecEnabled.ry() = 0;
                    _updateDistanceSinceLastMagnec.ry() = 1;
                    _distanceSinceLastMagnec.ry() = 0;
                }
            }


            if ( !_magnecEnabled.x() && !_magnecEnabled.y() ) {
                ///When releasing the grip, make sure to follow the mouse
                QPointF newPos = ( mapToParent( mapFromScene(mouseScenePos) ) );
                newPos.rx() -= size.width() / 2;
                newPos.ry() -= size.height() / 2;
                refreshPositionEnd( newPos.x(),newPos.y() );

                return;
            } else if ( _magnecEnabled.x() && !_magnecEnabled.y() ) {
                x = pos().x();
            } else if ( !_magnecEnabled.x() && _magnecEnabled.y() ) {
                y = pos().y();
            } else {
                return;
            }
        }

        bool continueMagnet = true;
        if (_updateDistanceSinceLastMagnec.rx() == 1) {
            _distanceSinceLastMagnec.rx() = x - _magnecStartingPos.x();
            if ( std::abs( _distanceSinceLastMagnec.x() ) > (NATRON_MAGNETIC_GRID_GRIP_TOLERANCE) ) {
                _updateDistanceSinceLastMagnec.rx() = 0;
            } else {
                continueMagnet = false;
            }
        }
        if (_updateDistanceSinceLastMagnec.ry() == 1) {
            _distanceSinceLastMagnec.ry() = y - _magnecStartingPos.y();
            if ( std::abs( _distanceSinceLastMagnec.y() ) > (NATRON_MAGNETIC_GRID_GRIP_TOLERANCE) ) {
                _updateDistanceSinceLastMagnec.ry() = 0;
            } else {
                continueMagnet = false;
            }
        }


        if ( ( !_magnecEnabled.x() || !_magnecEnabled.y() ) && continueMagnet ) {
            for (InputEdges::iterator it = _inputEdges.begin(); it != _inputEdges.end(); ++it) {
                ///For each input try to find if the magnet should be enabled
                boost::shared_ptr<NodeGui> inputSource = (*it)->getSource();
                if (inputSource) {
                    QSize inputSize = inputSource->getSize();
                    QPointF inputScenePos = inputSource->scenePos();
                    QPointF inputPos = inputScenePos + QPointF(inputSize.width() / 2,inputSize.height() / 2);
                    QPointF mapped = mapToParent( mapFromScene(inputPos) );
                    if ( !contains(mapped) ) {
                        if ( !_magnecEnabled.x() && ( ( mapped.x() >= (middlePos.x() - NATRON_MAGNETIC_GRID_GRIP_TOLERANCE) ) &&
                                                      ( mapped.x() <= (middlePos.x() + NATRON_MAGNETIC_GRID_GRIP_TOLERANCE) ) ) ) {
                            _magnecEnabled.rx() = 1;
                            _magnecDistance.rx() = 0;
                            x = mapped.x() - size.width() / 2;
                            _magnecStartingPos.setX(x);
                        } else if ( !_magnecEnabled.y() && ( ( mapped.y() >= (middlePos.y() - NATRON_MAGNETIC_GRID_GRIP_TOLERANCE) ) &&
                                                             ( mapped.y() <= (middlePos.y() + NATRON_MAGNETIC_GRID_GRIP_TOLERANCE) ) ) ) {
                            _magnecEnabled.ry() = 1;
                            _magnecDistance.ry() = 0;
                            y = mapped.y() - size.height() / 2;
                            _magnecStartingPos.setY(y);
                        }
                    }
                }
            }

            if ( ( !_magnecEnabled.x() || !_magnecEnabled.y() ) ) {
                ///check now the outputs
                const std::list<Natron::Node* > & outputs = getNode()->getGuiOutputs();
                for (std::list<Natron::Node* >::const_iterator it = outputs.begin(); it != outputs.end(); ++it) {
                    boost::shared_ptr<NodeGuiI> node_gui_i = (*it)->getNodeGui();
                    if (!node_gui_i) {
                        continue;
                    }
                    NodeGui* node = dynamic_cast<NodeGui*>(node_gui_i.get());
                    assert(node);
                    QSize outputSize = node->getSize();
                    QPointF nodeScenePos = node->scenePos();
                    QPointF outputPos = nodeScenePos  + QPointF(outputSize.width() / 2,outputSize.height() / 2);
                    QPointF mapped = mapToParent( mapFromScene(outputPos) );
                    if ( !contains(mapped) ) {
                        if ( !_magnecEnabled.x() && ( ( mapped.x() >= (middlePos.x() - NATRON_MAGNETIC_GRID_GRIP_TOLERANCE) ) &&
                                                      ( mapped.x() <= (middlePos.x() + NATRON_MAGNETIC_GRID_GRIP_TOLERANCE) ) ) ) {
                            _magnecEnabled.rx() = 1;
                            _magnecDistance.rx() = 0;
                            x = mapped.x() - size.width() / 2;
                            _magnecStartingPos.setX(x);
                        } else if ( !_magnecEnabled.y() && ( ( mapped.y() >= (middlePos.y() - NATRON_MAGNETIC_GRID_GRIP_TOLERANCE) ) &&
                                                             ( mapped.y() <= (middlePos.y() + NATRON_MAGNETIC_GRID_GRIP_TOLERANCE) ) ) ) {
                            _magnecEnabled.ry() = 1;
                            _magnecDistance.ry() = 0;
                            y = mapped.y() - size.height() / 2;
                            _magnecStartingPos.setY(y);
                        }
                    }
                }
            }
        }
    }

    refreshPositionEnd(x, y);
} // refreshPosition

void
NodeGui::setAboveItem(QGraphicsItem* item)
{
    if (!isVisible() || dynamic_cast<BackDropGui*>(this) || dynamic_cast<BackDropGui*>(item)) {
        return;
    }
    item->stackBefore(this);
    for (InputEdges::iterator it = _inputEdges.begin(); it != _inputEdges.end(); ++it) {
        boost::shared_ptr<NodeGui> inputSource = (*it)->getSource();
        if (inputSource.get() != item) {
            item->stackBefore((*it));
        }
    }
    if (_outputEdge) {
        item->stackBefore(_outputEdge);
    }
}

void
NodeGui::changePosition(double dx,
                        double dy)
{
    QPointF p = pos();

    refreshPosition(p.x() + dx, p.y() + dy,true);
}

void
NodeGui::refreshDashedStateOfEdges()
{
    ViewerInstance* viewer = dynamic_cast<ViewerInstance*>(getNode()->getLiveInstance());
    if (viewer) {
        int activeInputs[2];
        viewer->getActiveInputs(activeInputs[0], activeInputs[1]);

        int nbInputsConnected = 0;

        for (U32 i = 0; i < _inputEdges.size() ; ++i) {
            if ((int)i == activeInputs[0] || (int)i == activeInputs[1]) {
                _inputEdges[i]->setDashed(false);
            } else {
                _inputEdges[i]->setDashed(true);
            }
            if (_inputEdges[i]->getSource()) {
                ++nbInputsConnected;
            }
        }
        if (nbInputsConnected == 0 && !_inputEdges.empty()) {
            if (_inputEdges[0]) {
                _inputEdges[0]->setDashed(false);
            }
        }
    }
}

void
NodeGui::refreshEdges()
{
    const std::vector<boost::shared_ptr<Natron::Node> > & nodeInputs = getNode()->getGuiInputs();
    if (_inputEdges.size() != nodeInputs.size()) {
        return;
    }

    for (U32 i = 0; i < _inputEdges.size(); ++i) {
        assert(i < nodeInputs.size());
        assert(_inputEdges[i]);
        if (nodeInputs[i]) {
            boost::shared_ptr<NodeGuiI> nodeInputGui_i = nodeInputs[i]->getNodeGui();
            if (!nodeInputGui_i) {
                continue;
            }
            boost::shared_ptr<NodeGui> node = boost::dynamic_pointer_cast<NodeGui>(nodeInputGui_i);
            if (_inputEdges[i]->getSource() != node) {
                _inputEdges[i]->setSource(node);
            } else {
                _inputEdges[i]->initLine();
            }
        } else {
            _inputEdges[i]->initLine();
        }

    }
    if (_outputEdge) {
        _outputEdge->initLine();
    }
}

void
NodeGui::refreshKnobLinks()
{
    for (KnobGuiLinks::iterator it = _knobsLinks.begin(); it != _knobsLinks.end(); ++it) {
        it->second.arrow->refreshPosition();
    }
    if (_slaveMasterLink) {
        _slaveMasterLink->refreshPosition();
    }
}

void
NodeGui::markInputNull(Edge* e)
{
    for (U32 i = 0; i < _inputEdges.size(); ++i) {
        if (_inputEdges[i] == e) {
            _inputEdges[i] = 0;
        }
    }
}

void
NodeGui::updatePreviewImage(int time)
{
    NodePtr node = getNode();
    if ( isVisible() && node->isPreviewEnabled()  && node->getApp()->getProject()->isAutoPreviewEnabled() ) {

        if (node->getScriptName().find(NATRON_FILE_DIALOG_PREVIEW_READER_NAME) != std::string::npos ||
            node->getScriptName().find(NATRON_FILE_DIALOG_PREVIEW_VIEWER_NAME) != std::string::npos) {
            return;
        }

        ensurePreviewCreated();

        QtConcurrent::run(this,&NodeGui::computePreviewImage,time);
    }
}

void
NodeGui::forceComputePreview(int time)
{
    NodePtr node = getNode();
    if (!node) {
        return;
    }
    if ( isVisible() && node->isPreviewEnabled() && !node->getApp()->getProject()->isLoadingProject()) {

        if (node->getScriptName().find(NATRON_FILE_DIALOG_PREVIEW_READER_NAME) != std::string::npos ||
            node->getScriptName().find(NATRON_FILE_DIALOG_PREVIEW_VIEWER_NAME) != std::string::npos) {
            return;
        }

        ensurePreviewCreated();

        QtConcurrent::run(this,&NodeGui::computePreviewImage,time);
    }
}

void
NodeGui::computePreviewImage(int time)
{
    NodePtr node = getNode();
    if ( node->isRenderingPreview() ) {
        return;
    }


    int w = NATRON_PREVIEW_WIDTH;
    int h = NATRON_PREVIEW_HEIGHT;
    size_t dataSize = 4 * w * h;
    {
#ifndef __NATRON_WIN32__
        unsigned int* buf = (unsigned int*)calloc(dataSize,1);
#else
        unsigned int* buf = (unsigned int*)malloc(dataSize);
        for (int i = 0; i < w * h; ++i) {
            buf[i] = qRgba(0,0,0,255);
        }
#endif
        bool success = node->makePreviewImage(time, &w, &h, buf);

        if (success) {
            QImage img(reinterpret_cast<const uchar*>(buf), w, h, QImage::Format_ARGB32_Premultiplied);
            QPixmap prev_pixmap = QPixmap::fromImage(img);
            _previewPixmap->setPixmap(prev_pixmap);
            QPointF topLeft = mapFromParent( pos() );
            QRectF bbox = boundingRect();

            int iconWidth = _pluginIcon ? NATRON_PLUGIN_ICON_SIZE + PLUGIN_ICON_OFFSET * 2 : 0;
            _previewPixmap->setPos(topLeft.x() + iconWidth + NODE_WIDTH / 4. ,
                                   topLeft.y() + bbox.height() / 2 - NATRON_PREVIEW_HEIGHT / 2 + 10);
        }
        free(buf);
    }
}

bool
NodeGui::getOverlayColor(double* r, double* g, double* b) const
{
    if (!getSettingPanel()) {
        return false;
    }
    if (_overlayLocked) {
        *r = 0.5;
        *g = 0.5;
        *b = 0.5;
        return true;
    }
    if (!getSettingPanel()->hasOverlayColor()) {
        return false;
    }
    QColor c = getSettingPanel()->getOverlayColor();
    *r = c.redF();
    *g = c.greenF();
    *b = c.blueF();
    return true;
}

void
NodeGui::initializeInputsForInspector()
{
    NodePtr node = getNode();
    assert(node);

    ///If the node is a viewer, display 1 input and another one aside and hide all others.
    ///If the node is something else (switch, merge) show 2 inputs and another one aside an hide all others.

    bool isViewer = dynamic_cast<ViewerInstance*>(node->getLiveInstance()) != 0;

    int maxInitiallyOnTopVisibleInputs = isViewer ? 1 : 2;

    double piDividedbyX = M_PI / (maxInitiallyOnTopVisibleInputs + 1);

    double angle =  piDividedbyX;
    bool maskAside = false;
    for (U32 i = 0; i < _inputEdges.size(); ++i) {
        bool isMask = node->getLiveInstance()->isInputMask(i);

        if ((int)i < maxInitiallyOnTopVisibleInputs || (isMask && maskAside)) {
            _inputEdges[i]->setAngle(angle);
            angle += piDividedbyX;
        } else if (isMask && !maskAside) {
            _inputEdges[i]->setAngle(0);
            maskAside = true;
        } else {
            _inputEdges[i]->setAngle(M_PI);
        }

        if (!_inputEdges[i]->hasSource()) {
            _inputEdges[i]->initLine();
        }
    }


    bool inputAsideDisplayed = false;
    for (U32 i = 0; i < _inputEdges.size(); ++i) {
        if (_inputEdges[i]->hasSource() || node->getLiveInstance()->isInputMask(i)) {
            _inputEdges[i]->setVisible(true);
        } else {
            if ((int)i < maxInitiallyOnTopVisibleInputs) {
                _inputEdges[i]->setVisible(true);
            } else {
                if (!inputAsideDisplayed) {
                    _inputEdges[i]->setVisible(true);
                    inputAsideDisplayed = true;
                } else {
                    _inputEdges[i]->setVisible(false);
                }
            }
        }
    }

}

void
NodeGui::initializeInputs()
{
    ///Also refresh the output position
    if (_outputEdge) {
        _outputEdge->initLine();
    }

    NodePtr node = getNode();

    ///The actual numbers of inputs of the internal node
    const std::vector<NodePtr>& inputs = node->getGuiInputs();

    ///Delete all  inputs that may exist
    for (InputEdges::iterator it = _inputEdges.begin(); it != _inputEdges.end(); ++it) {
        delete *it;
    }
    _inputEdges.clear();


    ///Make new edge for all non existing inputs
    boost::shared_ptr<NodeGui> thisShared = shared_from_this();

    int inputsCount = 0;
    int emptyInputsCount = 0;
    for (U32 i = 0; i < inputs.size(); ++i) {
        Edge* edge = new Edge( i,0.,thisShared,parentItem());
        if ( node->getLiveInstance()->isInputRotoBrush(i) || !isVisible()) {
            edge->setActive(false);
            edge->hide();
        }
        if (inputs[i]) {
            boost::shared_ptr<NodeGuiI> gui_i = inputs[i]->getNodeGui();
            assert(gui_i);
            boost::shared_ptr<NodeGui> gui = boost::dynamic_pointer_cast<NodeGui>(gui_i);
            assert(gui);
            edge->setSource(gui);
        }
        if (!node->getLiveInstance()->isInputMask(i) &&
            !node->getLiveInstance()->isInputRotoBrush(i)) {
            if (!inputs[i]) {
                ++emptyInputsCount;
            }
            ++inputsCount;
        }
        _inputEdges.push_back(edge);

    }



    refreshDashedStateOfEdges();

    InspectorNode* isInspector = dynamic_cast<InspectorNode*>( node.get() );
    if (isInspector) {
        initializeInputsForInspector();
    } else {
        double piDividedbyX = M_PI / (inputsCount + 1);

        double angle =  piDividedbyX;

        int maskIndex = 0;
        for (U32 i = 0; i < _inputEdges.size(); ++i) {
            if (!node->getLiveInstance()->isInputRotoBrush(i)) {
                double edgeAngle;
                bool incrAngle = true;
                if (node->getLiveInstance()->isInputMask(i)) {
                    if (maskIndex == 0) {
                        edgeAngle = 0;
                        incrAngle = false;
                        ++maskIndex;
                    } else if (maskIndex == 1) {
                        edgeAngle = M_PI;
                        incrAngle = false;
                        ++maskIndex;
                    } else {
                        edgeAngle = angle;
                    }
                } else {
                    edgeAngle = angle;
                }
                _inputEdges[i]->setAngle(edgeAngle);
                if (incrAngle) {
                    angle += piDividedbyX;
                }
                if (!_inputEdges[i]->hasSource()) {
                    _inputEdges[i]->initLine();
                }
            }
        }

    }
} // initializeInputs

bool
NodeGui::contains(const QPointF &point) const
{
    QRectF bbox = boundingRect();

    bbox.adjust(-5, -5, 5, 5);

    return bbox.contains(point);
}

bool
NodeGui::intersects(const QRectF & rect) const
{
    QRectF mapped = mapRectFromScene(rect);

    return boundingRect().intersects(mapped);
}

QPainterPath
NodeGui::shape() const
{
    return _boundingBox->shape();
}

QRectF
NodeGui::boundingRect() const
{
    QTransform t;
    QRectF bbox = _boundingBox->boundingRect();
    QPointF center = bbox.center();

    t.translate( center.x(), center.y() );
    t.scale( scale(), scale() );
    t.translate( -center.x(), -center.y() );

    return t.mapRect(bbox);
}

void
NodeGui::checkOptionalEdgesVisibility()
{
    QPointF mousePos = mapFromScene(_graph->mapToScene(_graph->mapFromGlobal(QCursor::pos())));
    if (contains(mousePos) || getIsSelected()) {
        _optionalInputsVisible = true;
    } else {
        _optionalInputsVisible = false;
    }
    
    NodePtr node = getNode();
    bool isReader = node->getLiveInstance()->isReader();
    for (U32 i = 0; i < _inputEdges.size() ; ++i) {
        if (isReader || (node->getLiveInstance()->isInputOptional(i) &&
                         node->getLiveInstance()->isInputMask(i) &&
                         !_inputEdges[i]->isRotoEdge())) {
            
            bool nodeVisible = _optionalInputsVisible;
            if (!_optionalInputsVisible && node->getRealInput(i) ) {
                nodeVisible = true;
            }
            
            _inputEdges[i]->setVisible(nodeVisible);
            
        }
    }
}


void
NodeGui::setOptionalInputsVisible(bool visible)
{
    ///Don't do this for inspectors
    NodePtr node = getNode();

    //InspectorNode* isInspector = dynamic_cast<InspectorNode*>(node.get());
   

    if (visible != _optionalInputsVisible) {
        _optionalInputsVisible = visible;
        
        bool isReader = node->getLiveInstance()->isReader();
        for (U32 i = 0; i < _inputEdges.size() ; ++i) {
            if (isReader || (node->getLiveInstance()->isInputOptional(i) &&
                node->getLiveInstance()->isInputMask(i) &&
                !_inputEdges[i]->isRotoEdge())) {
                
                bool nodeVisible = visible;
                if (!visible && node->getRealInput(i) ) {
                    nodeVisible = true;
                }
                
                _inputEdges[i]->setVisible(nodeVisible);
                
            }
        }
    }
}

QRectF
NodeGui::boundingRectWithEdges() const
{
    QRectF ret;
    QRectF bbox = boundingRect();

    ret = mapToScene(bbox).boundingRect();
    for (InputEdges::const_iterator it = _inputEdges.begin(); it != _inputEdges.end(); ++it) {
        if (!(*it)->hasSource()) {
            ret = ret.united( (*it)->mapToScene( (*it)->boundingRect() ).boundingRect() );
        }
    }

    return ret;
}

bool
NodeGui::isNearby(QPointF &point)
{
    QPointF p = mapFromScene(point);
    QRectF bbox = boundingRect();
    QRectF r(bbox.x() - NATRON_EDGE_DROP_TOLERANCE,bbox.y() - NATRON_EDGE_DROP_TOLERANCE,
             bbox.width() + NATRON_EDGE_DROP_TOLERANCE,bbox.height() + NATRON_EDGE_DROP_TOLERANCE);

    return r.contains(p);
}




Edge*
NodeGui::firstAvailableEdge()
{
    for (U32 i = 0; i < _inputEdges.size(); ++i) {
        Edge* a = _inputEdges[i];
        if ( !a->hasSource() ) {
            if ( getNode()->getLiveInstance()->isInputOptional(i) ) {
                continue;
            }
        }

        return a;
    }

    return NULL;
}

void
NodeGui::applyBrush(const QBrush & brush)
{
    _boundingBox->setBrush(brush);
    if (mustFrameName()) {
        _nameFrame->setBrush(brush);
    }
    if (mustAddResizeHandle()) {
        _resizeHandle->setBrush(brush);
    }
}

void
NodeGui::refreshCurrentBrush()
{

    if (_slaveMasterLink) {
        applyBrush(_clonedColor);
    } else {
        applyBrush(_currentColor);
    }

}

void
NodeGui::setUserSelected(bool b)
{
    {
        QMutexLocker l(&_selectedMutex);
        _selected = b;
    }
    if (_settingsPanel) {
        _settingsPanel->setSelected(b);
        _settingsPanel->update();
        if ( b && isSettingsPanelVisible() && (getNode()->isRotoPaintingNode())) {
            _graph->getGui()->setRotoInterface(this);
        }
    }

    bool optionalInputsAutoHidden = _graph->areOptionalInputsAutoHidden();
    if (optionalInputsAutoHidden) {
        if (!b) {
            QPointF evpt = mapFromScene(_graph->mapToScene(_graph->mapFromGlobal(QCursor::pos())));
            QRectF bbox = boundingRect();
            if (!bbox.contains(evpt)) {
                setOptionalInputsVisible(false);
            }
        } else {
            setOptionalInputsVisible(true);
        }
    }

    refreshStateIndicator();



}

bool
NodeGui::getIsSelected() const
{
    QMutexLocker l(&_selectedMutex); return _selected;
}


Edge*
NodeGui::findConnectedEdge(NodeGui* parent)
{
    for (U32 i = 0; i < _inputEdges.size(); ++i) {
        Edge* e = _inputEdges[i];

        if ( e && (e->getSource().get() == parent) ) {
            return e;
        }
    }

    return NULL;
}

bool
NodeGui::connectEdge(int edgeNumber)
{
    const std::vector<boost::shared_ptr<Natron::Node> > & inputs = getNode()->getGuiInputs();

    if ( (edgeNumber < 0) || ( edgeNumber >= (int)inputs.size() ) || _inputEdges.size() != inputs.size() ) {
        return false;
    }

    boost::shared_ptr<NodeGui> src;
    if (inputs[edgeNumber]) {
        boost::shared_ptr<NodeGuiI> ngi = inputs[edgeNumber]->getNodeGui();
        src = boost::dynamic_pointer_cast<NodeGui>(ngi);
    }

    _inputEdges[edgeNumber]->setSource(src);

    NodePtr node = getNode();
    assert(node);
    if (dynamic_cast<InspectorNode*>(node.get())) {
        initializeInputsForInspector();
    }

    return true;

}

Edge*
NodeGui::hasEdgeNearbyPoint(const QPointF & pt)
{
    for (InputEdges::const_iterator it = _inputEdges.begin(); it != _inputEdges.end(); ++it) {
        if ( (*it) && (*it)->contains( (*it)->mapFromScene(pt) ) ) {
            return (*it);
        }
    }
    if ( _outputEdge && _outputEdge->contains( _outputEdge->mapFromScene(pt) ) ) {
        return _outputEdge;
    }

    return NULL;
}

Edge*
NodeGui::hasBendPointNearbyPoint(const QPointF & pt)
{
    for (InputEdges::const_iterator it = _inputEdges.begin(); it != _inputEdges.end(); ++it) {
        if ( (*it) && (*it)->hasSource() && (*it)->isBendPointVisible() ) {
            if ( (*it)->isNearbyBendPoint(pt) ) {
                return (*it);
            }
        }
    }

    return NULL;
}

Edge*
NodeGui::hasEdgeNearbyRect(const QRectF & rect)
{
    ///try with all 4 corners

    QLineF rectEdges[4] =
    {
        QLineF( rect.topLeft(),rect.topRight() ),
        QLineF( rect.topRight(),rect.bottomRight() ),
        QLineF( rect.bottomRight(),rect.bottomLeft() ),
        QLineF( rect.bottomLeft(),rect.topLeft() )
    };
    QPointF intersection;
    QPointF middleRect = rect.center();
    Edge* closest = 0;
    double closestSquareDist = 0;

    for (InputEdges::const_iterator it = _inputEdges.begin(); it != _inputEdges.end(); ++it) {
        QLineF edgeLine = (*it)->line();
        edgeLine.setP1((*it)->mapToScene(edgeLine.p1()));
        edgeLine.setP2((*it)->mapToScene(edgeLine.p2()));
        for (int j = 0; j < 4; ++j) {
            if (edgeLine.intersect(rectEdges[j], &intersection) == QLineF::BoundedIntersection) {
                if (!closest) {
                    closest = *it;
                    closestSquareDist = ( intersection.x() - middleRect.x() ) * ( intersection.x() - middleRect.x() )
                                        + ( intersection.y() - middleRect.y() ) * ( intersection.y() - middleRect.y() );
                } else {
                    double dist = ( intersection.x() - middleRect.x() ) * ( intersection.x() - middleRect.x() )
                                  + ( intersection.y() - middleRect.y() ) * ( intersection.y() - middleRect.y() );
                    if (dist < closestSquareDist) {
                        closestSquareDist = dist;
                        closest = *it;
                    }
                }
                break;
            }
        }
    }
    if (closest) {
        return closest;
    }

    if (_outputEdge) {
        if (_outputEdge->isVisible()) {
            QLineF edgeLine = _outputEdge->line();
            edgeLine.setP1((_outputEdge)->mapToScene(edgeLine.p1()));
            edgeLine.setP2((_outputEdge)->mapToScene(edgeLine.p2()));
            for (int j = 0; j < 4; ++j) {
                if (edgeLine.intersect(rectEdges[j], &intersection) == QLineF::BoundedIntersection) {
                    return _outputEdge;
                }
            }
        }
    }

    return NULL;
} // hasEdgeNearbyRect

void
NodeGui::showGui()
{
    show();
    setActive(true);
    NodePtr node = getNode();
    for (U32 i = 0; i < _inputEdges.size() ; ++i) {
        _graph->scene()->addItem(_inputEdges[i]);
        _inputEdges[i]->setParentItem( parentItem() );
        if ( !node->getLiveInstance()->isInputRotoBrush(i) ) {
            _inputEdges[i]->setActive(true);
        }
    }
    if (_outputEdge) {
        _graph->scene()->addItem(_outputEdge);
        _outputEdge->setParentItem( parentItem() );
        _outputEdge->setActive(true);
    }
    refreshEdges();
    const std::list<Natron::Node* > & outputs = node->getGuiOutputs();
    for (std::list<Natron::Node* >::const_iterator it = outputs.begin(); it != outputs.end(); ++it) {
        assert(*it);
        (*it)->doRefreshEdgesGUI();
    }
    ViewerInstance* viewer = dynamic_cast<ViewerInstance*>( node->getLiveInstance() );
    if (viewer) {
        _graph->getGui()->activateViewerTab(viewer);
    } else {
        if (_panelOpenedBeforeDeactivate) {
            setVisibleSettingsPanel(true);
        }
        if (node->isRotoPaintingNode()) {
            _graph->getGui()->setRotoInterface(this);
        }
        OfxEffectInstance* ofxNode = dynamic_cast<OfxEffectInstance*>( node->getLiveInstance() );
        if (ofxNode) {
            ofxNode->effectInstance()->beginInstanceEditAction();
        }
    }

    if (_slaveMasterLink) {
        if ( !node->getMasterNode() ) {
            onAllKnobsSlaved(false);
        } else {
            _slaveMasterLink->show();
        }
    }
    for (KnobGuiLinks::iterator it = _knobsLinks.begin(); it != _knobsLinks.end(); ++it) {
        it->second.arrow->show();
    }
}

void
NodeGui::activate(bool triggerRender)
{
    ///first activate all child instance if any
    NodePtr node = getNode();

    bool isMultiInstanceChild = !node->getParentMultiInstanceName().empty();

    if (!isMultiInstanceChild) {
        showGui();
    } else {
        ///don't show gui if it is a multi instance child, but still Q_EMIT the begin edit action
        OfxEffectInstance* ofxNode = dynamic_cast<OfxEffectInstance*>( node->getLiveInstance() );
        if (ofxNode) {
            ofxNode->effectInstance()->beginInstanceEditAction();
        }
    }
    _graph->restoreFromTrash(this);
    //_graph->getGui()->getCurveEditor()->addNode(shared_from_this());

    if (!isMultiInstanceChild && triggerRender) {
        std::list<ViewerInstance* > viewers;
        getNode()->hasViewersConnected(&viewers);
        for (std::list<ViewerInstance* >::iterator it = viewers.begin(); it != viewers.end(); ++it) {
            (*it)->renderCurrentFrame(true);
        }
    }
}

void
NodeGui::hideGui()
{
    if ( !_graph || !_graph->getGui() ) {
        return;
    }
    hide();
    setActive(false);
    for (InputEdges::const_iterator it = _inputEdges.begin(); it != _inputEdges.end(); ++it) {
        if ( (*it)->scene() ) {
            (*it)->scene()->removeItem((*it));
        }
        (*it)->setActive(false);
        (*it)->setSource( boost::shared_ptr<NodeGui>() );
    }
    if (_outputEdge) {
        if ( _outputEdge->scene() ) {
            _outputEdge->scene()->removeItem(_outputEdge);
        }
        _outputEdge->setActive(false);
    }

    if (_slaveMasterLink) {
        _slaveMasterLink->hide();
    }
    for (KnobGuiLinks::iterator it = _knobsLinks.begin(); it != _knobsLinks.end(); ++it) {
        it->second.arrow->hide();
    }
    NodePtr node = getNode();
    ViewerInstance* isViewer = dynamic_cast<ViewerInstance*>( node->getLiveInstance() );
    if (isViewer) {
        ViewerGL* viewerGui = dynamic_cast<ViewerGL*>( isViewer->getUiContext() );
        if (viewerGui) {
            viewerGui->clearLastRenderedTexture();
            _graph->getGui()->deactivateViewerTab(isViewer);
        }
    } else {
        _panelOpenedBeforeDeactivate = isSettingsPanelVisible();
        if (_panelOpenedBeforeDeactivate) {
            setVisibleSettingsPanel(false);
        }

        if ( node->isRotoPaintingNode() ) {
            _graph->getGui()->removeRotoInterface(this, false);
        }
        if ( node->isPointTrackerNode() && node->getParentMultiInstanceName().empty() ) {
            _graph->getGui()->removeTrackerInterface(this, false);
        }

        NodeGroup* isGrp = dynamic_cast<NodeGroup*>(node->getLiveInstance());
        if (isGrp) {
            NodeGraphI* graph_i = isGrp->getNodeGraph();
            assert(graph_i);
            NodeGraph* graph = dynamic_cast<NodeGraph*>(graph_i);
            assert(graph);
            if (graph) {
                _graph->getGui()->removeGroupGui(graph, false);
            }
        }
    }
} // hideGui

void
NodeGui::deactivate(bool triggerRender)
{
    ///first deactivate all child instance if any
    NodePtr node = getNode();

    bool isMultiInstanceChild = node->getParentMultiInstance().get() != NULL;
    if (!isMultiInstanceChild) {
        hideGui();
    }
    OfxEffectInstance* ofxNode = dynamic_cast<OfxEffectInstance*>( node->getLiveInstance() );
    if (ofxNode) {
        ofxNode->effectInstance()->endInstanceEditAction();
    }
    if (_graph) {
        _graph->moveToTrash(this);
        if ( _graph->getGui() ) {
            _graph->getGui()->getCurveEditor()->removeNode(this);
            _graph->getGui()->getDopeSheetEditor()->removeNode(this);
        }
    }

    if (!isMultiInstanceChild && triggerRender) {
        std::list<ViewerInstance* > viewers;
        getNode()->hasViewersConnected(&viewers);
        for (std::list<ViewerInstance* >::iterator it = viewers.begin(); it != viewers.end(); ++it) {
            (*it)->renderCurrentFrame(true);
        }
    }
}

void
NodeGui::initializeKnobs()
{
    if (_settingsPanel) {
        _settingsPanel->initializeKnobs();
    }
}

void
NodeGui::setVisibleSettingsPanel(bool b)
{
    if (!_panelCreated) {
        ensurePanelCreated();
    }
    if (_settingsPanel) {
        _settingsPanel->setClosed(!b);
    }
}

bool
NodeGui::isSettingsPanelVisible() const
{
    if (_settingsPanel) {
        return !_settingsPanel->isClosed();
    } else {
        return false;
    }
}

bool
NodeGui::isSettingsPanelMinimized() const
{
    return _settingsPanel ? _settingsPanel->isMinimized() : false;
}

void
NodeGui::onPersistentMessageChanged()
{
    //keep type in synch with this enum:
    //enum MessageTypeEnum{eMessageTypeInfo = 0,eMessageTypeError = 1,eMessageTypeWarning = 2,eMessageTypeQuestion = 3};

    ///don't do anything if the last persistent message is the same
    if (!_persistentMessage || !_stateIndicator || !_graph || !_graph->getGui()) {
        return;
    }

    QString message;
    int type;
    getNode()->getPersistentMessage(&message, &type);

    _persistentMessage->setVisible(!message.isEmpty());

    if (message.isEmpty()) {


        setToolTip(QString());

    } else {

        if (type == 1) {
            _persistentMessage->setPlainText(tr("ERROR"));
            QColor errColor(128,0,0,255);
            _persistentMessage->setDefaultTextColor(errColor);
        } else if (type == 2) {
            _persistentMessage->setPlainText(tr("WARNING"));
            QColor warColor(180,180,0,255);
            _persistentMessage->setDefaultTextColor(warColor);
        } else {
            return;
        }

        setToolTip(message);

        refreshSize();
    }
    refreshStateIndicator();

    const std::list<ViewerTab*>& viewers = getDagGui()->getGui()->getViewersList();
    for (std::list<ViewerTab*>::const_iterator it = viewers.begin(); it != viewers.end(); ++it) {
        (*it)->getViewer()->updatePersistentMessage();
    }
}


QVBoxLayout*
NodeGui::getDockContainer() const
{
    return _settingsPanel->getContainer();
}

void
NodeGui::paint(QPainter* /*painter*/,
               const QStyleOptionGraphicsItem* /*options*/,
               QWidget* /*parent*/)
{
    //nothing special
}

const std::map<boost::weak_ptr<KnobI>,KnobGui*> &
NodeGui::getKnobs() const
{
    assert(_settingsPanel);
    if (_mainInstancePanel) {
        return _mainInstancePanel->getKnobs();
    }

    return _settingsPanel->getKnobs();
}

void
NodeGui::serialize(NodeGuiSerialization* serializationObject) const
{
    serializationObject->initialize(this);
}

void
NodeGui::serializeInternal(std::list<boost::shared_ptr<NodeSerialization> >& internalSerialization) const
{
    NodePtr node = getNode();
    boost::shared_ptr<NodeSerialization> thisSerialization(new NodeSerialization(node,false));
    internalSerialization.push_back(thisSerialization);

    ///For multi-instancs, serialize children too
    if (node->isMultiInstance()) {
        assert(_settingsPanel);
        boost::shared_ptr<MultiInstancePanel> panel = _settingsPanel->getMultiInstancePanel();
        assert(panel);

        const std::list<std::pair<boost::weak_ptr<Natron::Node>,bool> >& instances = panel->getInstances();
        for (std::list<std::pair<boost::weak_ptr<Natron::Node>,bool> >::const_iterator it = instances.begin();
             it != instances.end(); ++it) {
            boost::shared_ptr<NodeSerialization> childSerialization(new NodeSerialization(it->first.lock(),false));
            internalSerialization.push_back(childSerialization);
        }
    }
}

void
NodeGui::restoreInternal(const boost::shared_ptr<NodeGui>& thisShared,
                         const std::list<boost::shared_ptr<NodeSerialization> >& internalSerialization)
{
    assert(internalSerialization.size() >= 1);

    getSettingPanel()->pushUndoCommand(new LoadNodePresetsCommand(thisShared,internalSerialization));
}

void
NodeGui::copyFrom(const NodeGuiSerialization & obj)
{
    setPos_mt_safe( QPointF( obj.getX(),obj.getY() ) );
    if ( getNode()->isPreviewEnabled() != obj.isPreviewEnabled() ) {
        togglePreview();
    }
    double w,h;
    obj.getSize(&w,&h);
    resize(w,h);
}

boost::shared_ptr<QUndoStack>
NodeGui::getUndoStack() const
{
    return _undoStack;
}

void
NodeGui::onRenderingStarted()
{
    if (!_renderingStartedCount) {
        if (!_stateIndicator->isVisible()) {
            _stateIndicator->setBrush(Qt::yellow);
            _stateIndicator->show();
            update();
        }
    }
    ++_renderingStartedCount;

}

void
NodeGui::onRenderingFinished()
{
    --_renderingStartedCount;
    if (!_renderingStartedCount) {
        refreshStateIndicator();
    }
}

void
NodeGui::refreshStateIndicator()
{
    if (!_stateIndicator) {
        return;
    }
    QString message;
    int type;
    if (!getNode()) {
        return;
    }
    getNode()->getPersistentMessage(&message, &type);

    bool showIndicator = true;
    if (_mergeHintActive) {

        _stateIndicator->setBrush(Qt::green);

    } else if (getIsSelected()) {

        _stateIndicator->setBrush(Qt::white);

    } else if (!message.isEmpty() && (type == 1 || type == 2)) {
        if (type == 1) {
            _stateIndicator->setBrush(QColor(128,0,0,255)); //< error
        } else if ( type == 2) {
            _stateIndicator->setBrush(QColor(80,180,0,255)); //< warning
        }

    } else {
        showIndicator = false;
    }

    if (showIndicator && !_stateIndicator->isVisible()) {
        _stateIndicator->show();
    } else if (!showIndicator && _stateIndicator->isVisible()) {
        _stateIndicator->hide();
    } else {
        update();
    }
}

void
NodeGui::setMergeHintActive(bool active)
{
    if (active == _mergeHintActive) {
        return;
    }
    _mergeHintActive = active;
    refreshStateIndicator();

}

void
NodeGui::setVisibleDetails(bool visible)
{
    if (!isVisible()) {
        return;
    }
    if (_nameItem) {
        _nameItem->setVisible(visible);
    }
    for (InputEdges::iterator it = _inputEdges.begin(); it != _inputEdges.end(); ++it) {
        (*it)->setVisibleDetails(visible);
    }
}

void
NodeGui::onInputNRenderingStarted(int input)
{
    assert(input >= 0 && input < (int)_inputEdges.size());
    std::map<int,int>::iterator itC = _inputNRenderingStartedCount.find(input);
    if (itC == _inputNRenderingStartedCount.end()) {
        _inputEdges[input]->turnOnRenderingColor();
        _inputNRenderingStartedCount.insert(std::make_pair(input,1));
    }

}

void
NodeGui::onInputNRenderingFinished(int input)
{
    std::map<int,int>::iterator itC = _inputNRenderingStartedCount.find(input);
    if (itC != _inputNRenderingStartedCount.end()) {

        --itC->second;
        if (!itC->second) {
            _inputEdges[input]->turnOffRenderingColor();
            _inputNRenderingStartedCount.erase(itC);
        }
    }
}

void
NodeGui::moveBelowPositionRecursively(const QRectF & r)
{
    QRectF sceneRect = mapToScene( boundingRect() ).boundingRect();

    if ( r.intersects(sceneRect) ) {
        changePosition(0, r.height() + NodeGui::DEFAULT_OFFSET_BETWEEN_NODES);
        const std::list<Natron::Node* > & outputs = getNode()->getGuiOutputs();
        for (std::list<Natron::Node* >::const_iterator it = outputs.begin(); it != outputs.end(); ++it) {
            assert(*it);
            boost::shared_ptr<NodeGuiI> outputGuiI = (*it)->getNodeGui();
            if (!outputGuiI) {
                continue;
            }
            NodeGui* gui = dynamic_cast<NodeGui*>(outputGuiI.get());
            assert(gui);
            sceneRect = mapToScene( boundingRect() ).boundingRect();
            gui->moveBelowPositionRecursively(sceneRect);
        }
    }
}

void
NodeGui::moveAbovePositionRecursively(const QRectF & r)
{
    QRectF sceneRect = mapToScene( boundingRect() ).boundingRect();

    if ( r.intersects(sceneRect) ) {
        changePosition(0,-r.height() - NodeGui::DEFAULT_OFFSET_BETWEEN_NODES);
        for (U32 i = 0; i < _inputEdges.size(); ++i) {
            if ( _inputEdges[i]->hasSource() ) {
                sceneRect = mapToScene( boundingRect() ).boundingRect();
                _inputEdges[i]->getSource()->moveAbovePositionRecursively(sceneRect);
            }
        }
    }
}

QPointF
NodeGui::getPos_mt_safe() const
{
    QMutexLocker l(&positionMutex);
    
    return pos();
}

void
NodeGui::setPos_mt_safe(const QPointF & pos)
{
    QMutexLocker l(&positionMutex);

    setPos(pos);
}

void
NodeGui::centerGraphOnIt()
{
    _graph->centerOnItem(this);
}

void
NodeGui::onAllKnobsSlaved(bool b)
{
    NodePtr node = getNode();
    if (b) {
        boost::shared_ptr<Natron::Node> masterNode = node->getMasterNode();
        assert(masterNode);
        boost::shared_ptr<NodeGuiI> masterNodeGui_i = masterNode->getNodeGui();
        assert(masterNodeGui_i);
        boost::shared_ptr<NodeGui> masterNodeGui = boost::dynamic_pointer_cast<NodeGui>(masterNodeGui_i);
        _masterNodeGui = masterNodeGui;
        assert(!_slaveMasterLink);

        if (masterNode->getGroup() == node->getGroup()) {
            _slaveMasterLink = new LinkArrow( masterNodeGui.get(),this,parentItem() );
            _slaveMasterLink->setColor( QColor(200,100,100) );
            _slaveMasterLink->setArrowHeadColor( QColor(243,137,20) );
            _slaveMasterLink->setWidth(3);
        }
        if ( !node->isNodeDisabled() ) {
            if ( !isSelected() ) {
                applyBrush(_clonedColor);
            }
        }
    } else {
        if (_slaveMasterLink) {
            delete _slaveMasterLink;
            _slaveMasterLink = 0;
        }
        _masterNodeGui.reset();
        if ( !node->isNodeDisabled() ) {
            if ( !isSelected() ) {
                applyBrush(_currentColor);
            }
        }
    }
    update();
}

static QString makeLinkString(Natron::Node* masterNode,KnobI* master,Natron::Node* slaveNode,KnobI* slave)
{
    QString tt("<br>");
    tt.append(masterNode->getLabel().c_str());
    tt.append(".");
    tt.append(master->getName().c_str());


    tt.append(" (master) ");

    tt.append("------->");

    tt.append(slaveNode->getLabel().c_str());
    tt.append(".");
    tt.append(slave->getName().c_str());


    tt.append(" (slave)</br>");
    return tt;
}

void
NodeGui::onKnobsLinksChanged()
{
    NodePtr node = getNode();

    typedef std::list<Natron::Node::KnobLink> InternalLinks;
    InternalLinks links;
    node->getKnobsLinks(links);

    ///1st pass: remove the no longer needed links
    KnobGuiLinks newLinks;
    for (KnobGuiLinks::iterator it = _knobsLinks.begin(); it != _knobsLinks.end(); ++it) {
        bool found = false;
        for (InternalLinks::iterator it2 = links.begin(); it2 != links.end(); ++it2) {
            if (it2->masterNode == it->first) {
                found = true;
                break;
            }
        }
        if (!found) {
            delete it->second.arrow;
        } else {
            newLinks.insert(*it);
        }
    }
    _knobsLinks = newLinks;

    ///2nd pass: create the new links

    for (InternalLinks::iterator it = links.begin(); it != links.end(); ++it) {

        KnobGuiLinks::iterator foundGuiLink = _knobsLinks.find(it->masterNode);
        if (foundGuiLink != _knobsLinks.end()) {

            //We already have a link to the master node
            bool found = false;

            for (std::list<std::pair<KnobI*,KnobI*> >::iterator it2 = foundGuiLink->second.knobs.begin(); it2 != foundGuiLink->second.knobs.end(); ++it2) {
                if (it2->first == it->slave && it2->second == it->master) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                ///There's no link for this knob, add info to the tooltip of the link arrow

                foundGuiLink->second.knobs.push_back(std::make_pair(it->slave,it->master));
                QString fullTooltip;
                for (std::list<std::pair<KnobI*,KnobI*> >::iterator it2 = foundGuiLink->second.knobs.begin(); it2 != foundGuiLink->second.knobs.end(); ++it2) {
                    QString tt = makeLinkString(it->masterNode.get(),it2->second,node.get(),it2->first);
                    fullTooltip.append(tt);
                }
            }
        } else {

            ///There's no link to the master node yet
            if (it->masterNode->getNodeGui().get() != this && it->masterNode->getGroup() == getNode()->getGroup()) {
                boost::shared_ptr<NodeGuiI> master_i = it->masterNode->getNodeGui();
                boost::shared_ptr<NodeGui> master = boost::dynamic_pointer_cast<NodeGui>(master_i);
                assert(master);
                LinkArrow* arrow = new LinkArrow( master.get(),this,parentItem() );
                arrow->setWidth(2);
                arrow->setColor( QColor(143,201,103) );
                arrow->setArrowHeadColor( QColor(200,255,200) );

                QString tt = makeLinkString(it->masterNode.get(),it->master,node.get(),it->slave);
                arrow->setToolTip(tt);
                if ( !getDagGui()->areKnobLinksVisible() ) {
                    arrow->setVisible(false);
                }
                LinkedDim guilink;
                guilink.knobs.push_back(std::make_pair(it->slave,it->master));
                guilink.arrow = arrow;
                _knobsLinks.insert(std::make_pair(it->masterNode,guilink));
            }
        }

    }

    if (links.size() > 0) {
        if ( !_expressionIndicator->isActive() ) {
            _expressionIndicator->setActive(true);
        }
    } else {
        if ( _expressionIndicator->isActive() ) {
            _expressionIndicator->setActive(false);
        }
    }
} // onKnobsLinksChanged

void
NodeGui::refreshOutputEdgeVisibility()
{
    if (_outputEdge) {
        if ( getNode()->getGuiOutputs().empty() ) {
            if ( !_outputEdge->isVisible() ) {
                _outputEdge->setActive(true);
                _outputEdge->show();
            }
        } else {
            if ( _outputEdge->isVisible() ) {
                _outputEdge->setActive(false);
                _outputEdge->hide();
            }
        }
    }
}

void
NodeGui::deleteReferences()
{
    removeUndoStack();
    for (InputEdges::const_iterator it = _inputEdges.begin(); it != _inputEdges.end(); ++it) {

        QGraphicsScene* scene = (*it)->scene();
        if (scene) {
            scene->removeItem((*it));
        }
        (*it)->setParentItem(NULL);
        delete *it;
    }
    _inputEdges.clear();

    if (_outputEdge) {
        QGraphicsScene* scene = _outputEdge->scene();
        if (scene) {
            scene->removeItem(_outputEdge);
        }
        _outputEdge->setParentItem(NULL);
        delete _outputEdge;
        _outputEdge = NULL;
    }

    if (_settingsPanel) {
        delete _settingsPanel;
        _settingsPanel = NULL;
    }
}

QSize
NodeGui::getSize() const
{
    if (QThread::currentThread() == qApp->thread()) {
        QRectF bbox = boundingRect();
        return QSize( bbox.width(),bbox.height() );
    } else {
        QMutexLocker k(&_mtSafeSizeMutex);
        return QSize(_mtSafeWidth,_mtSafeHeight);
    }
}

void
NodeGui::setSize(double w, double h)
{
    resize(w, h);
}

void
NodeGui::onDisabledKnobToggled(bool disabled)
{
    if (!_nameItem) {
        return;
    }

    NodePtr node = getNode();
    ///When received whilst the node is under a multi instance, let the MultiInstancePanel call this slot instead.
    if ( ( sender() == node.get() ) && node->isMultiInstance() ) {
        return;
    }

    _disabledTopLeftBtmRight->setVisible(disabled);
    _disabledBtmLeftTopRight->setVisible(disabled);
    update();
}

void
NodeGui::toggleBitDepthIndicator(bool on,
                                 const QString & tooltip)
{
    if (on) {
        QString arrangedTt = Natron::convertFromPlainText(tooltip.trimmed(), Qt::WhiteSpaceNormal);
        setToolTip(arrangedTt);
        _bitDepthWarning->setToolTip(arrangedTt);
    } else {
        setToolTip("");
        _bitDepthWarning->setToolTip("");
    }
    _bitDepthWarning->setActive(on);
}

////////////////////////////////////////// NodeGuiIndicator ////////////////////////////////////////////////////////

struct NodeGuiIndicatorPrivate
{
    QGraphicsEllipseItem* ellipse;
    QGraphicsTextItem* textItem;
    QGradientStops gradStops;

    NodeGuiIndicatorPrivate(int depth,
                            const QString & text,
                            const QPointF & topLeft,
                            int width,
                            int height,
                            const QGradientStops & gradient,
                            const QColor & textColor,
                            QGraphicsItem* parent)
        : ellipse(NULL)
          , textItem(NULL)
          , gradStops(gradient)
    {
        ellipse = new QGraphicsEllipseItem(parent);
        int ellipseRad = width / 2;
        QPoint ellipsePos(topLeft.x() + (width / 2) - ellipseRad, -ellipseRad);
        QRectF ellipseRect(ellipsePos.x(),ellipsePos.y(),width,height);
        ellipse->setRect(ellipseRect);
        ellipse->setZValue(depth);

        QPointF ellipseCenter = ellipseRect.center();
        QRadialGradient radialGrad(ellipseCenter,ellipseRad);
        radialGrad.setStops(gradStops);
        ellipse->setBrush(radialGrad);


        textItem = new QGraphicsTextItem(text,parent);
        //QFont font(appFont,appFontSize);
        QFontMetrics fm(textItem->font());
        textItem->setPos(topLeft.x()  - 2 * width / 3, topLeft.y() - 2 * fm.height() / 3);
        //textItem->setFont(font);
        textItem->setDefaultTextColor(textColor);
        textItem->setZValue(depth);
#if QT_VERSION < 0x050000
        textItem->scale(0.8, 0.8);
#else
        textItem->setScale(0.8);
#endif
    }
};

NodeGuiIndicator::NodeGuiIndicator(int depth,
                                   const QString & text,
                                   const QPointF & topLeft,
                                   int width,
                                   int height,
                                   const QGradientStops & gradient,
                                   const QColor & textColor,
                                   QGraphicsItem* parent)
    : _imp( new NodeGuiIndicatorPrivate(depth,text,topLeft,width,height,gradient,textColor,parent) )
{
}

NodeGuiIndicator::~NodeGuiIndicator()
{
}

void
NodeGuiIndicator::setToolTip(const QString & tooltip)
{
    _imp->ellipse->setToolTip(tooltip);
}

void
NodeGuiIndicator::setActive(bool active)
{
    _imp->ellipse->setActive(active);
    _imp->textItem->setActive(active);
    _imp->ellipse->setVisible(active);
    _imp->textItem->setVisible(active);
}

bool
NodeGuiIndicator::isActive() const
{
    return _imp->ellipse->isVisible();
}

void
NodeGuiIndicator::refreshPosition(const QPointF & topLeft)
{
    QRectF r = _imp->ellipse->rect();
    int ellipseRad = r.width() / 2;
    QPoint ellipsePos(topLeft.x() - ellipseRad, topLeft.y() - ellipseRad);
    QRectF ellipseRect( ellipsePos.x(), ellipsePos.y(), r.width(), r.height() );

    _imp->ellipse->setRect(ellipseRect);

    QRadialGradient radialGrad(ellipseRect.center(),ellipseRad);
    radialGrad.setStops(_imp->gradStops);
    _imp->ellipse->setBrush(radialGrad);

    QFont font = _imp->textItem->font();
    QFontMetrics fm(font);
    _imp->textItem->setPos(topLeft.x()  - 2 * r.width() / 3, topLeft.y() - 2 * fm.height() / 3);
}

///////////////////

void
NodeGui::setScale_natron(double scale)
{
    setScale(scale);
    for (InputEdges::iterator it = _inputEdges.begin(); it != _inputEdges.end(); ++it) {
        (*it)->setScale(scale);
    }

    if (_outputEdge) {
        _outputEdge->setScale(scale);
    }
    refreshEdges();
    const std::list<Natron::Node* > & outputs = getNode()->getGuiOutputs();
    for (std::list<Natron::Node* >::const_iterator it = outputs.begin(); it != outputs.end(); ++it) {
        assert(*it);
        (*it)->doRefreshEdgesGUI();
    }
    update();
}

void
NodeGui::removeHighlightOnAllEdges()
{
    for (InputEdges::iterator it = _inputEdges.begin(); it != _inputEdges.end(); ++it) {
        (*it)->setUseHighlight(false);
    }
    if (_outputEdge) {
        _outputEdge->setUseHighlight(false);
    }
}

Edge*
NodeGui::getInputArrow(int inputNb) const
{
    if (inputNb == -1) {
        return _outputEdge;
    }
    if (inputNb >= (int)_inputEdges.size()) {
        return 0;
    }
    return _inputEdges[inputNb];
}

Edge*
NodeGui::getOutputArrow() const
{
    return _outputEdge;
}

void
NodeGui::setNameItemHtml(const QString & name,
                         const QString & label)
{
    if (!_nameItem) {
        return;
    }
    QString textLabel;
    textLabel.append("<div align=\"center\">");
    bool hasFontData = true;
    if ( !label.isEmpty() ) {
        QString labelCopy = label;

        ///remove any custom data tag natron might have added
        QString startCustomTag(NATRON_CUSTOM_HTML_TAG_START);
        int startCustomData = labelCopy.indexOf(startCustomTag);
        if (startCustomData != -1) {
            labelCopy.remove( startCustomData, startCustomTag.size() );

            QString endCustomTag(NATRON_CUSTOM_HTML_TAG_END);
            int endCustomData = labelCopy.indexOf(endCustomTag,startCustomData);
            assert(endCustomData != -1);
            labelCopy.remove( endCustomData, endCustomTag.size() );
            labelCopy.insert(endCustomData, "<br>");
        }

        ///add the node name into the html encoded label
        int startFontTag = labelCopy.indexOf("<font size=");
        hasFontData = startFontTag != -1;
        if (hasFontData) {
            QString toFind("\">");
            int endFontTag = labelCopy.indexOf(toFind,startFontTag);
            int i = endFontTag += toFind.size();
            labelCopy.insert(i == -1 ? 0 : i, name + "<br>");
        } else {
            labelCopy.prepend(name + "<br>");
        }
        textLabel.append(labelCopy);

    } else {
        ///Default to something not too bad
        QString fontTag = (QString("<font size=\"%1\" color=\"%2\" face=\"%3\">")
                           .arg(6)
                           .arg( QColor(Qt::black).name() )
                           .arg(QApplication::font().family()));
        textLabel.append(fontTag);
        textLabel.append(name);
        textLabel.append("</font>");
    }
    textLabel.append("</div>");
    _nameItem->setHtml(textLabel);
    _nameItem->adjustSize();


    QFont f;
    QColor color;
    if (hasFontData) {
        String_KnobGui::parseFont(textLabel, &f, &color);
    }
    _nameItem->setFont(f);

    QRectF bbox = boundingRect();
    resize(bbox.width(),bbox.height(),false,!label.isEmpty());
//    QRectF currentBbox = boundingRect();
//    QRectF labelBbox = _nameItem->boundingRect();
//    resize( currentBbox.width(), std::max( currentBbox.height(),labelBbox.height() ) );
} // setNameItemHtml

void
NodeGui::onNodeExtraLabelChanged(const QString & label)
{
    if (!_graph->getGui()) {
        return;
    }
    NodePtr node = getNode();
    _nodeLabel = label;
    if ( node->isMultiInstance() ) {
        ///The multi-instances store in the kNatronOfxParamStringSublabelName knob the name of the instance
        ///Since the "main-instance" is the one displayed on the node-graph we don't want it to display its name
        ///hence we remove it
        _nodeLabel = String_KnobGui::removeNatronHtmlTag(_nodeLabel);
    }
    _nodeLabel = replaceLineBreaksWithHtmlParagraph(_nodeLabel); ///< maybe we should do this in the knob itself when the user writes ?
    setNameItemHtml(node->getLabel().c_str(),_nodeLabel);
    
    //For the merge node, set its operator icon
    if (getNode()->getPlugin()->getPluginID() == QString(PLUGINID_OFX_MERGE)) {
        assert(_mergeIcon);
        QString op = String_KnobGui::getNatronHtmlTagContent(label);
        //Remove surrounding parenthesis
        if (op[0] == QChar('(')) {
            op.remove(0, 1);
        }
        if (op[op.size() - 1] == QChar(')')) {
            op.remove(op.size() - 1,1);
        }
        QPixmap pix;
        getPixmapForMergeOperator(op, &pix);
        if (pix.isNull()) {
            _mergeIcon->setVisible(false);
        } else {
            _mergeIcon->setVisible(true);
            _mergeIcon->setPixmap(pix);
        }
        refreshSize();
    }
}

QColor
NodeGui::getCurrentColor() const
{
    QMutexLocker k(&_currentColorMutex);
    return _currentColor;
}

void
NodeGui::setCurrentColor(const QColor & c)
{
    onSettingsPanelColorChanged(c);
    if (_settingsPanel) {
        _settingsPanel->setCurrentColor(c);
    }
}

void
NodeGui::setOverlayColor(const QColor& c)
{
    if (_settingsPanel) {
        _settingsPanel->setOverlayColor(c);
    }
}

void
NodeGui::onSwitchInputActionTriggered()
{
    NodePtr node = getNode();
    if (node->getMaxInputCount() >= 2) {
        node->switchInput0And1();
        std::list<ViewerInstance* > viewers;
        node->hasViewersConnected(&viewers);
        for (std::list<ViewerInstance* >::iterator it = viewers.begin(); it != viewers.end(); ++it) {
            (*it)->renderCurrentFrame(true);
        }
        update();
        node->getApp()->triggerAutoSave();
    }
}

///////////////////

TextItem::TextItem(QGraphicsItem* parent )
    : QGraphicsTextItem(parent)
      , _alignement(Qt::AlignCenter)
{
    init();
}

TextItem::TextItem(const QString & text,
                   QGraphicsItem* parent)
    : QGraphicsTextItem(text,parent)
      , _alignement(Qt::AlignCenter)
{
    init();
}

void
TextItem::setAlignment(Qt::Alignment alignment)
{
    _alignement = alignment;
    QTextBlockFormat format;
    format.setAlignment(alignment);
    QTextCursor cursor = textCursor();      // save cursor position
    int position = textCursor().position();
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(format);
    cursor.clearSelection();
    cursor.setPosition(position);           // restore cursor position
    setTextCursor(cursor);
}

int
TextItem::type() const
{
    return Type;
}

void
TextItem::updateGeometry(int,
                         int,
                         int)
{
    updateGeometry();
}

void
TextItem::updateGeometry()
{
    QPointF topRightPrev = boundingRect().topRight();

    setTextWidth(-1);
    setTextWidth( boundingRect().width() );
    setAlignment(_alignement);
    QPointF topRight = boundingRect().topRight();

    if (_alignement & Qt::AlignRight) {
        setPos( pos() + (topRightPrev - topRight) );
    }
}

void
TextItem::init()
{
    updateGeometry();
    connect( document(), SIGNAL( contentsChange(int, int, int) ),
             this, SLOT( updateGeometry(int, int, int) ) );
}

void
NodeGui::refreshKnobsAfterTimeChange(SequenceTime time)
{
    NodePtr node = getNode();
    if ( ( _settingsPanel && !_settingsPanel->isClosed() ) ) {
        node->getLiveInstance()->refreshAfterTimeChange(time);
    } else if ( !node->getParentMultiInstanceName().empty() ) {
        node->getLiveInstance()->refreshInstanceSpecificKnobsOnly(time);
    }
}

void
NodeGui::onGuiFrozenChanged(bool frozen)
{
    if ( ( _settingsPanel ) ) {

        getNode()->getLiveInstance()->onGuiFrozenChange(frozen);
    }
}

void
NodeGui::onSettingsPanelClosedChanged(bool closed)
{
    if (!_settingsPanel) {
        return;
    }

    DockablePanel* panel = dynamic_cast<DockablePanel*>( sender() );
    assert(panel);
    if (panel == _settingsPanel) {
        ///if it is a multiinstance, notify the multi instance panel
        if (_mainInstancePanel) {
            _settingsPanel->getMultiInstancePanel()->onSettingsPanelClosed(closed);
        } else {
            if (!closed) {
                NodePtr node = getNode();
                SequenceTime time = node->getApp()->getTimeLine()->currentFrame();
                node->getLiveInstance()->refreshAfterTimeChange(time);
            }
        }
    }
}

boost::shared_ptr<MultiInstancePanel> NodeGui::getMultiInstancePanel() const
{
    if (_settingsPanel) {
        return _settingsPanel->getMultiInstancePanel();
    } else {
        return boost::shared_ptr<MultiInstancePanel>();
    }
}



void
NodeGui::setParentMultiInstance(const boost::shared_ptr<NodeGui> & node)
{
    _parentMultiInstance = node;
}

void
NodeGui::setKnobLinksVisible(bool visible)
{
    for (KnobGuiLinks::iterator it = _knobsLinks.begin(); it != _knobsLinks.end(); ++it) {
        it->second.arrow->setVisible(visible);
    }
}

void
NodeGui::onParentMultiInstancePositionChanged(int x,
                                              int y)
{
    refreshPosition(x, y,true);
}

//////////Dot node gui
DotGui::DotGui(QGraphicsItem* parent)
: NodeGui(parent)
, diskShape(NULL)
, ellipseIndicator(NULL)
{
}

void
DotGui::createGui()
{
    double depth = getBaseDepth();
    setZValue(depth);

    diskShape = new QGraphicsEllipseItem(this);
    diskShape->setZValue(depth);
    QPointF topLeft = mapFromParent( pos() );
    diskShape->setRect( QRectF(topLeft.x(),topLeft.y(),DOT_GUI_DIAMETER,DOT_GUI_DIAMETER) );

    ellipseIndicator = new QGraphicsEllipseItem(this);
    ellipseIndicator->setRect(QRectF(topLeft.x() - NATRON_STATE_INDICATOR_OFFSET,
                                     topLeft.y() - NATRON_STATE_INDICATOR_OFFSET,
                                     DOT_GUI_DIAMETER + NATRON_STATE_INDICATOR_OFFSET * 2,
                                     DOT_GUI_DIAMETER + NATRON_STATE_INDICATOR_OFFSET * 2));
    ellipseIndicator->hide();
}

void
DotGui::refreshStateIndicator()
{
    bool showIndicator = true;
    if (getIsSelected()) {
        ellipseIndicator->setBrush(QColor(255,255,255,128));
    } else {
        showIndicator = false;
    }

    if (showIndicator && !ellipseIndicator->isVisible()) {
        ellipseIndicator->show();
    } else if (!showIndicator && ellipseIndicator->isVisible()) {
        ellipseIndicator->hide();
    } else {
        update();
    }

}

void
DotGui::applyBrush(const QBrush & brush)
{
    diskShape->setBrush(brush);
}

NodeSettingsPanel*
DotGui::createPanel(QVBoxLayout* container,
                    const boost::shared_ptr<NodeGui> & thisAsShared)
{
    NodeSettingsPanel* panel = new NodeSettingsPanel( boost::shared_ptr<MultiInstancePanel>(),
                                                      getDagGui()->getGui(),
                                                      thisAsShared,
                                                      container,container->parentWidget() );

    ///Always close the panel by default for Dots
    panel->setClosed(true);

    return panel;
}

QRectF
DotGui::boundingRect() const
{
    QTransform t;
    QRectF bbox = diskShape->boundingRect();
    QPointF center = bbox.center();

    t.translate( center.x(), center.y() );
    t.scale( scale(), scale() );
    t.translate( -center.x(), -center.y() );

    return t.mapRect(bbox);
}

QPainterPath
DotGui::shape() const
{
    return diskShape->shape();
}

void
NodeGui::onInternalNameChanged(const QString & s)
{
    if (_settingNameFromGui) {
        return;
    }

    setNameItemHtml(s,_nodeLabel);

    if (_settingsPanel) {
        _settingsPanel->setName(s);
    }
    scene()->update();
}

void
NodeGui::setName(const QString & newName)
{
    _settingNameFromGui = true;
    getNode()->setLabel(newName.toStdString());
    _settingNameFromGui = false;

    onInternalNameChanged(newName);
}

bool
NodeGui::isSettingsPanelOpened() const
{
    return isSettingsPanelVisible();
}

bool
NodeGui::shouldDrawOverlay() const
{
    NodePtr internalNode = getNode();
    if (!internalNode) {
        return false;
    }

    NodePtr parentMultiInstance = internalNode->getParentMultiInstance();


    if (parentMultiInstance) {
        boost::shared_ptr<NodeGuiI> gui_i = parentMultiInstance->getNodeGui();
        assert(gui_i);
        NodeGui *parentGui = dynamic_cast<NodeGui*>(gui_i.get());
        assert(parentGui);

        boost::shared_ptr<MultiInstancePanel> multiInstance = parentGui->getMultiInstancePanel();
        assert(multiInstance);

        const std::list< std::pair<boost::weak_ptr<Natron::Node>,bool > >& instances = multiInstance->getInstances();
        for (std::list< std::pair<boost::weak_ptr<Natron::Node>,bool > >::const_iterator it = instances.begin(); it != instances.end(); ++it) {
            NodePtr instance = it->first.lock();

            if (instance == internalNode) {

                if (parentGui->isSettingsPanelVisible() &&
                    !parentGui->isSettingsPanelMinimized() &&
                    instance->isActivated() &&
                    it->second &&
                    !instance->isNodeDisabled()) {
                    return true;
                } else {
                    return false;
                }

            }

        }

    } else {
        if (!internalNode->isNodeDisabled() &&
            internalNode->isActivated() &&
            isSettingsPanelVisible() &&
            !isSettingsPanelMinimized() ) {

            return true;
        }
    }
    return false;
}

void
NodeGui::setPosition(double x,double y)
{
    refreshPosition(x, y, true);
}

void
NodeGui::getPosition(double *x, double* y) const
{
    QPointF pos = getPos_mt_safe();
    *x = pos.x();
    *y = pos.y();
}

void
NodeGui::getSize(double* w, double* h) const
{
    QSize s = getSize();
    *w = s.width();
    *h = s.height();
}

struct ExportGroupTemplateDialogPrivate
{
    Gui* gui;
    NodeCollection* group;
    QGridLayout* mainLayout;

    Natron::Label* labelLabel;
    LineEdit* labelEdit;

    Natron::Label* idLabel;
    LineEdit* idEdit;

    Natron::Label* groupingLabel;
    LineEdit* groupingEdit;

    Natron::Label* fileLabel;
    LineEdit* fileEdit;
    Button* openButton;

    Natron::Label* iconPathLabel;
    LineEdit* iconPath;

    Natron::Label* descriptionLabel;
    LineEdit* descriptionEdit;

    QDialogButtonBox *buttons;

    ExportGroupTemplateDialogPrivate(NodeCollection* group,Gui* gui)
    : gui(gui)
    , group(group)
    , mainLayout(0)
    , labelLabel(0)
    , labelEdit(0)
    , idLabel(0)
    , idEdit(0)
    , groupingLabel(0)
    , groupingEdit(0)
    , fileLabel(0)
    , fileEdit(0)
    , openButton(0)
    , iconPathLabel(0)
    , iconPath(0)
    , descriptionLabel(0)
    , descriptionEdit(0)
    , buttons(0)
    {

    }
};

ExportGroupTemplateDialog::ExportGroupTemplateDialog(NodeCollection* group,Gui* gui,QWidget* parent)
: QDialog(parent)
, _imp(new ExportGroupTemplateDialogPrivate(group,gui))
{
    _imp->mainLayout = new QGridLayout(this);


    _imp->idLabel = new Natron::Label(tr("Unique ID"),this);
    QString idTt = Natron::convertFromPlainText(tr("The unique ID is used by " NATRON_APPLICATION_NAME "to identify the plug-in in various "
                                               "places in the application. Generally this contains domain and sub-domains names "
                                               "such as fr.inria.group.XXX. If 2 plug-ins happen to have the same ID they will be "
                                               "gathered by version. If 2 plug-ins have the same ID and version, the first loaded in the"
                                               " search-paths will take precedence over the other."), Qt::WhiteSpaceNormal);
    _imp->idEdit = new LineEdit(this);
    _imp->idEdit->setPlaceholderText("org.organization.pyplugs.XXX");
    _imp->idEdit->setToolTip(idTt);


    _imp->labelLabel = new Natron::Label(tr("Label"),this);
    QString labelTt = Natron::convertFromPlainText(tr("Set the label of the group as the user will see it in the user interface."), Qt::WhiteSpaceNormal);
    _imp->labelLabel->setToolTip(labelTt);
    _imp->labelEdit = new LineEdit(this);
    _imp->labelEdit->setPlaceholderText("MyPlugin");
    QObject::connect(_imp->labelEdit,SIGNAL(editingFinished()), this , SLOT(onLabelEditingFinished()));
    _imp->labelEdit->setToolTip(labelTt);


    _imp->groupingLabel = new Natron::Label(tr("Grouping"),this);
    QString groupingTt = Natron::convertFromPlainText(tr("The grouping of the plug-in specifies where the plug-in will be located in the menus. "
                                                     "E.g: Color/Transform, or Draw. Each sub-level must be separated by a '/'."), Qt::WhiteSpaceNormal);
    _imp->groupingLabel->setToolTip(groupingTt);

    _imp->groupingEdit = new LineEdit(this);
    _imp->groupingEdit->setPlaceholderText("Color/Transform");
    _imp->groupingEdit->setToolTip(groupingTt);


    _imp->iconPathLabel = new Natron::Label(tr("Icon relative path"),this);
    QString iconTt = Natron::convertFromPlainText(tr("Set here the file path of an optional icon to identify the plug-in. "
                                                 "The path is relative to the Python script."), Qt::WhiteSpaceNormal);
    _imp->iconPathLabel->setToolTip(iconTt);
    _imp->iconPath = new LineEdit(this);
    _imp->iconPath->setPlaceholderText("Label.png");
    _imp->iconPath->setToolTip(iconTt);

    _imp->descriptionLabel = new Natron::Label(tr("Description"),this);
    QString descTt =  Natron::convertFromPlainText(tr("Set here the (optional) plug-in description that the user will see when clicking the "
                                                  " \"?\" button on the settings panel of the node."), Qt::WhiteSpaceNormal);
    _imp->descriptionEdit = new LineEdit(this);
    _imp->descriptionEdit->setToolTip(descTt);
    _imp->descriptionEdit->setPlaceholderText(tr("This plug-in can be used to produce XXX effect..."));

    _imp->fileLabel = new Natron::Label(tr("Directory"),this);
    QString fileTt  = Natron::convertFromPlainText(tr("Specify here the directory where to export the Python script."), Qt::WhiteSpaceNormal);
    _imp->fileLabel->setToolTip(fileTt);
    _imp->fileEdit = new LineEdit(this);


    _imp->fileEdit->setToolTip(fileTt);


    QPixmap openPix;
    appPTR->getIcon(Natron::NATRON_PIXMAP_OPEN_FILE, NATRON_MEDIUM_BUTTON_ICON_SIZE, &openPix);
    _imp->openButton = new Button(QIcon(openPix),"",this);
    _imp->openButton->setFocusPolicy(Qt::NoFocus);
    _imp->openButton->setFixedSize(NATRON_MEDIUM_BUTTON_SIZE, NATRON_MEDIUM_BUTTON_SIZE);
    QObject::connect( _imp->openButton, SIGNAL( clicked() ), this, SLOT( onButtonClicked() ) );

    _imp->buttons = new QDialogButtonBox(QDialogButtonBox::StandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel),
                                         Qt::Horizontal,this);
    QObject::connect(_imp->buttons, SIGNAL(accepted()), this, SLOT(onOkClicked()));
    QObject::connect(_imp->buttons, SIGNAL(rejected()), this, SLOT(reject()));


    _imp->mainLayout->addWidget(_imp->idLabel, 0, 0 , 1 , 1);
    _imp->mainLayout->addWidget(_imp->idEdit, 0, 1,  1 , 2);
    _imp->mainLayout->addWidget(_imp->labelLabel, 1, 0 , 1 , 1);
    _imp->mainLayout->addWidget(_imp->labelEdit, 1, 1,  1 , 2);
    _imp->mainLayout->addWidget(_imp->groupingLabel, 2, 0,  1 , 1);
    _imp->mainLayout->addWidget(_imp->groupingEdit, 2, 1,  1 , 2);
    _imp->mainLayout->addWidget(_imp->iconPathLabel, 3, 0 , 1 , 1);
    _imp->mainLayout->addWidget(_imp->iconPath, 3, 1 , 1 , 2);
    _imp->mainLayout->addWidget(_imp->descriptionLabel, 4, 0 , 1 , 1);
    _imp->mainLayout->addWidget(_imp->descriptionEdit, 4, 1 , 1 , 2);
    _imp->mainLayout->addWidget(_imp->fileLabel, 5, 0 , 1 , 1);
    _imp->mainLayout->addWidget(_imp->fileEdit, 5, 1, 1 , 1);
    _imp->mainLayout->addWidget(_imp->openButton, 5, 2, 1, 1);
    _imp->mainLayout->addWidget(_imp->buttons, 6, 0, 1, 3);

    resize(400,sizeHint().height());


}

ExportGroupTemplateDialog::~ExportGroupTemplateDialog()
{

}

void
ExportGroupTemplateDialog::onButtonClicked()
{
    std::vector<std::string> filters;

    const QString& path = _imp->gui->getLastPluginDirectory();
    SequenceFileDialog dialog(this,filters,false,SequenceFileDialog::eFileDialogModeDir,path.toStdString(),_imp->gui,false);
    if (dialog.exec()) {
        std::string selection = dialog.selectedFiles();
        _imp->fileEdit->setText(selection.c_str());
        QDir d = dialog.currentDirectory();
        _imp->gui->updateLastPluginDirectory(d.absolutePath());
    }
}

void
ExportGroupTemplateDialog::onLabelEditingFinished()
{
    if (_imp->idEdit->text().isEmpty()) {
        _imp->idEdit->setText(_imp->labelEdit->text());
    }
}

void
ExportGroupTemplateDialog::onOkClicked()
{
    QString dirPath = _imp->fileEdit->text();

    if (!dirPath.isEmpty() && dirPath[dirPath.size() - 1] == QChar('/')) {
        dirPath.remove(dirPath.size() - 1, 1);
    }
    QDir d(dirPath);

    if (!d.exists()) {
        Natron::errorDialog(tr("Error").toStdString(), tr("You must specify a directory to save the script").toStdString());
        return;
    }
    QString pluginLabel = _imp->labelEdit->text();
    if (pluginLabel.isEmpty()) {
        Natron::errorDialog(tr("Error").toStdString(), tr("You must specify a label to name the script").toStdString());
        return;
    }

    QString pluginID = _imp->idEdit->text();
    if (pluginID.isEmpty()) {
        Natron::errorDialog(tr("Error").toStdString(), tr("You must specify a unique ID to identify the script").toStdString());
        return;
    }

    QString iconPath = _imp->iconPath->text();
    QString grouping = _imp->groupingEdit->text();
    QString description = _imp->descriptionEdit->text();

    QString filePath = d.absolutePath() + "/" + pluginLabel + ".py";

    QStringList filters;
    filters.push_back(QString(pluginLabel + ".py"));
    if (!d.entryList(filters,QDir::Files | QDir::NoDotAndDotDot).isEmpty()) {
        Natron::StandardButtonEnum rep = Natron::questionDialog(tr("Existing plug-in").toStdString(),
                                                                tr("A group plug-in with the same name already exists "
                                                                   "would you like to "
                                                                   "override it?").toStdString(), false);
        if  (rep == Natron::eStandardButtonNo) {
            return;
        }
    }

    bool foundInPath = false;
    QStringList groupSearchPath = appPTR->getAllNonOFXPluginsPaths();
    for (QStringList::iterator it = groupSearchPath.begin(); it != groupSearchPath.end(); ++it) {
        if (!it->isEmpty() && it->at(it->size() - 1) == QChar('/')) {
            it->remove(it->size() - 1, 1);
        }
        if (*it == dirPath) {
            foundInPath = true;
        }
    }

    if (!foundInPath) {

        QString message = dirPath + tr(" does not exist in the group plug-in search path, would you like to add it?");
        Natron::StandardButtonEnum rep = Natron::questionDialog(tr("Plug-in path").toStdString(),
                                                                message.toStdString(), false);

        if  (rep == Natron::eStandardButtonYes) {
            appPTR->getCurrentSettings()->appendPythonGroupsPath(dirPath.toStdString());
        }

    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        Natron::errorDialog(tr("Error").toStdString(), QString(tr("Cannot open ") + filePath).toStdString());
        return;
    }

    QTextStream ts(&file);
    QString content;
    _imp->group->exportGroupToPython(pluginID, pluginLabel, description, iconPath, grouping, content);
    ts << content;

    accept();
}

void
NodeGui::exportGroupAsPythonScript()
{
    NodePtr node = getNode();
    if (!node) {
        return;
    }
    NodeGroup* isGroup = dynamic_cast<NodeGroup*>(node->getLiveInstance());
    if (!isGroup) {
        qDebug() << "Attempting to export a non-group as a python script.";
        return;
    }
    getDagGui()->getGui()->exportGroupAsPythonScript(isGroup);
}


void
NodeGui::getColor(double* r,double *g, double* b) const
{
    QColor c = getCurrentColor();
    *r = c.redF();
    *g = c.greenF();
    *b = c.blueF();
}

void
NodeGui::setColor(double r, double g, double b)
{
    QColor c;
    c.setRgbF(r,g,b);
    setCurrentColor(c);
}

void
NodeGui::addDefaultPositionInteract(const boost::shared_ptr<Double_Knob>& point)
{
    assert(QThread::currentThread() == qApp->thread());
    if (!_defaultOverlay) {
        _defaultOverlay.reset(new DefaultOverlay(shared_from_this()));
    }
    if (_defaultOverlay->addPositionParam(point)) {
        getDagGui()->getGui()->redrawAllViewers();
    }
}


boost::shared_ptr<DefaultOverlay>
NodeGui::getDefaultOverlay() const
{
    return _defaultOverlay;
}

bool
NodeGui::hasDefaultOverlay() const
{
    if (_defaultOverlay) {
        return true;
    }
    return false;
}

void
NodeGui::setCurrentViewportForDefaultOverlays(OverlaySupport* viewPort)
{
    if (_defaultOverlay) {
        _defaultOverlay->setCallingViewport(viewPort);
    }
}

void
NodeGui::drawDefaultOverlay(double time, double scaleX,double scaleY)
{
    if (_defaultOverlay) {
        RenderScale rs;
        rs.x = scaleX;
        rs.y = scaleY;
        NatronOverlayInteractSupport::OGLContextSaver s(_defaultOverlay->getLastCallingViewport());
        _defaultOverlay->draw(time , rs);
    }
}

bool
NodeGui::onOverlayPenDownDefault(double scaleX,double scaleY,const QPointF & viewportPos, const QPointF & pos, double pressure)
{
    if (_defaultOverlay) {
        RenderScale rs;
        rs.x = scaleX;
        rs.y = scaleY;

       return _defaultOverlay->penDown(getNode()->getLiveInstance()->getCurrentTime(), rs, pos, viewportPos.toPoint(), pressure);
    }
    return false;
}

bool
NodeGui::onOverlayPenMotionDefault(double scaleX, double scaleY, const QPointF & viewportPos, const QPointF & pos, double pressure)
{
    if (_defaultOverlay) {
        RenderScale rs;
        rs.x = scaleX;
        rs.y = scaleY;
        
        return _defaultOverlay->penMotion(getNode()->getLiveInstance()->getCurrentTime(), rs, pos, viewportPos.toPoint(), pressure);
    }
    return false;
}

bool
NodeGui::onOverlayPenUpDefault(double scaleX,double scaleY,const QPointF & viewportPos, const QPointF & pos, double pressure)
{
    if (_defaultOverlay) {
        RenderScale rs;
        rs.x = scaleX;
        rs.y = scaleY;
        
        return _defaultOverlay->penUp(getNode()->getLiveInstance()->getCurrentTime(), rs, pos, viewportPos.toPoint(), pressure);
    }
    return false;
}

bool
NodeGui::onOverlayKeyDownDefault(double scaleX,double scaleY,Natron::Key key,Natron::KeyboardModifiers /*modifiers*/)
{
    if (_defaultOverlay) {
        RenderScale rs;
        rs.x = scaleX;
        rs.y = scaleY;
        QByteArray keyStr;
        return _defaultOverlay->keyDown(getNode()->getLiveInstance()->getCurrentTime(), rs,(int)key,keyStr.data());
    }
    return false;
}

bool
NodeGui::onOverlayKeyUpDefault(double scaleX,double scaleY,Natron::Key key,Natron::KeyboardModifiers /*modifiers*/)
{
    if (_defaultOverlay) {
        RenderScale rs;
        rs.x = scaleX;
        rs.y = scaleY;
        QByteArray keyStr;
        return _defaultOverlay->keyUp(getNode()->getLiveInstance()->getCurrentTime(), rs,(int)key,keyStr.data());

    }
    return false;
}

bool
NodeGui::onOverlayKeyRepeatDefault(double scaleX,double scaleY,Natron::Key key,Natron::KeyboardModifiers /*modifiers*/)
{
    if (_defaultOverlay) {
        RenderScale rs;
        rs.x = scaleX;
        rs.y = scaleY;
        QByteArray keyStr;
        return _defaultOverlay->keyRepeat(getNode()->getLiveInstance()->getCurrentTime(), rs,(int)key,keyStr.data());

    }
    return false;
}

bool
NodeGui::onOverlayFocusGainedDefault(double scaleX,double scaleY)
{
    if (_defaultOverlay) {
        RenderScale rs;
        rs.x = scaleX;
        rs.y = scaleY;
        QByteArray keyStr;
        return _defaultOverlay->gainFocus(getNode()->getLiveInstance()->getCurrentTime(), rs);

    }
    return false;
}

bool
NodeGui::onOverlayFocusLostDefault(double scaleX,double scaleY)
{
    if (_defaultOverlay) {
        RenderScale rs;
        rs.x = scaleX;
        rs.y = scaleY;
        QByteArray keyStr;
        return _defaultOverlay->loseFocus(getNode()->getLiveInstance()->getCurrentTime(), rs);
    }
    return false;
}

bool
NodeGui::hasDefaultOverlayForParam(const KnobI* param)
{
    if (_defaultOverlay) {
        return _defaultOverlay->hasDefaultOverlayForParam(param);
    }
    return false;
}

void
NodeGui::removeDefaultOverlay(KnobI* knob)
{
    if (_defaultOverlay) {
        _defaultOverlay->removeDefaultOverlay(knob);
        if (_defaultOverlay->isEmpty()) {
            _defaultOverlay.reset();
        }
    }
}

void
NodeGui::setPluginIconFilePath(const std::string& filePath)
{
    boost::shared_ptr<Settings> currentSettings = appPTR->getCurrentSettings();

    QPixmap p(filePath.c_str());
    if (p.isNull() || !currentSettings->isPluginIconActivatedOnNodeGraph()) {
        return;
    }
    p = p.scaled(NATRON_PLUGIN_ICON_SIZE,NATRON_PLUGIN_ICON_SIZE,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);

    if (getSettingPanel()) {
        getSettingPanel()->setPluginIcon(p);
    }

    if (!_pluginIcon) {
        _pluginIcon = new QGraphicsPixmapItem(this);
        _pluginIcon->setZValue(getBaseDepth() + 1);
        _pluginIconFrame = new QGraphicsRectItem(this);
        _pluginIconFrame->setZValue(getBaseDepth());

        int r, g, b;
        currentSettings->getPluginIconFrameColor(&r, &g, &b);
        _pluginIconFrame->setBrush(QColor(r, g, b));
    }

    if (_pluginIcon) {

        _pluginIcon->setPixmap(p);
        if (!_pluginIcon->isVisible()) {
            _pluginIcon->show();
            _pluginIconFrame->show();
        }
        double w,h;
        getSize(&w, &h);
        w = NODE_WIDTH + NATRON_PLUGIN_ICON_SIZE + PLUGIN_ICON_OFFSET * 2;
        resize(w,h);

        double x,y;
        getPosition(&x, &y);
        x -= (NATRON_PLUGIN_ICON_SIZE) / 2. + PLUGIN_ICON_OFFSET;
        setPosition(x, y);

    }
}

void
NodeGui::setPluginIDAndVersion(const std::string& pluginLabel,const std::string& pluginID,unsigned int version)
{
    if (getSettingPanel()) {
        getSettingPanel()->setPluginIDAndVersion(pluginLabel,pluginID, version);
    }
}

void
NodeGui::setPluginDescription(const std::string& description)
{
    if (getSettingPanel()) {
        getSettingPanel()->setPluginDescription(description);
    }
}

void
NodeGui::setOverlayLocked(bool locked)
{
    assert(QThread::currentThread() == qApp->thread());
    _overlayLocked = locked;
}