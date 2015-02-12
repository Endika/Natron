#include "GuiAppWrapper.h"

#include "Global/Macros.h"
CLANG_DIAG_OFF(deprecated)
CLANG_DIAG_OFF(uninitialized)
#include <QColorDialog>
CLANG_DIAG_ON(deprecated)
CLANG_DIAG_ON(uninitialized)

#include "Gui/PythonPanels.h"
#include "Gui/Gui.h"
#include "Gui/TabWidget.h"
#include "Gui/SequenceFileDialog.h"
#include "Gui/NodeGraph.h"
#include "Gui/NodeGui.h"
#include "Gui/ViewerTab.h"
#include "Gui/ViewerGL.h"

#include "Engine/NodeGroupWrapper.h"
#include "Engine/NodeWrapper.h"
#include "Engine/NodeGroup.h"
#include "Engine/Node.h"
#include "Engine/ViewerInstance.h"
#include "Engine/TimeLine.h"
#include "Engine/ScriptObject.h"

GuiApp::GuiApp(AppInstance* app)
: App(app)
, _app(dynamic_cast<GuiAppInstance*>(app))
{
    assert(_app);
}


GuiApp::~GuiApp()
{
    
}

Gui*
GuiApp::getGui() const
{
    return _app->getGui();
}

PyModalDialog*
GuiApp::createModalDialog()
{
    PyModalDialog* ret = new PyModalDialog(_app->getGui());
    return ret;
}


PyTabWidget*
GuiApp::getTabWidget(const std::string& name) const
{
    const std::list<TabWidget*>& tabs = _app->getGui()->getPanes();
    for ( std::list<TabWidget*>::const_iterator it = tabs.begin(); it != tabs.end(); ++it) {
        if ((*it)->objectName_mt_safe().toStdString() == name) {
            return new PyTabWidget(*it);
        }
    }
    return 0;
}

bool
GuiApp::moveTab(const std::string& scriptName,PyTabWidget* pane)
{
    QWidget* w;
    ScriptObject* o;
    _app->getGui()->findExistingTab(scriptName, &w, &o);
    if (!w || !o) {
        return false;
    }
    
    return TabWidget::moveTab(w, o, pane->getInternalTabWidget());
}

void
GuiApp::registerPythonPanel(PyPanel* panel,const std::string& pythonFunction)
{
    _app->getGui()->registerPyPanel(panel,pythonFunction);
}

void
GuiApp::unregisterPythonPanel(PyPanel* panel)
{
    _app->getGui()->unregisterPyPanel(panel);
}

std::string
GuiApp::getFilenameDialog(const std::vector<std::string>& filters,
                          const std::string& location) const
{
    Gui* gui = _app->getGui();
    
    SequenceFileDialog dialog(gui,
                              filters,
                              false,
                              SequenceFileDialog::eFileDialogModeOpen,
                              location,
                              gui,
                              false);
    if (dialog.exec()) {
        std::string ret = dialog.selectedFiles();
        return ret;
    }
    return std::string();
}

std::string
GuiApp::getSequenceDialog(const std::vector<std::string>& filters,
                          const std::string& location) const
{
    Gui* gui = _app->getGui();
    
    SequenceFileDialog dialog(gui,
                              filters,
                              true,
                              SequenceFileDialog::eFileDialogModeOpen,
                              location,
                              gui,
                              false);
    if (dialog.exec()) {
        std::string ret = dialog.selectedFiles();
        return ret;
    }
    return std::string();
}

std::string
GuiApp::getDirectoryDialog(const std::string& location) const
{
    Gui* gui = _app->getGui();
    std::vector<std::string> filters;
    SequenceFileDialog dialog(gui,
                              filters,
                              false,
                              SequenceFileDialog::eFileDialogModeDir,
                              location,
                              gui,
                              false);
    if (dialog.exec()) {
        std::string ret = dialog.selectedDirectory();
        return ret;
    }
    return std::string();

}

std::string
GuiApp::saveFilenameDialog(const std::vector<std::string>& filters,
                           const std::string& location) const
{
    Gui* gui = _app->getGui();
    
    SequenceFileDialog dialog(gui,
                              filters,
                              false,
                              SequenceFileDialog::eFileDialogModeSave,
                              location,
                              gui,
                              false);
    if (dialog.exec()) {
        std::string ret = dialog.filesToSave();
        return ret;
    }
    return std::string();

}

std::string
GuiApp::saveSequenceDialog(const std::vector<std::string>& filters,
                           const std::string& location) const
{
    Gui* gui = _app->getGui();
    
    SequenceFileDialog dialog(gui,
                              filters,
                              true,
                              SequenceFileDialog::eFileDialogModeSave,
                              location,
                              gui,
                              false);
    if (dialog.exec()) {
        std::string ret = dialog.filesToSave();
        return ret;
    }
    return std::string();

}

ColorTuple
GuiApp::getRGBColorDialog() const
{
    ColorTuple ret;
    
    QColorDialog dialog;
    if (dialog.exec()) {
        QColor color = dialog.currentColor();
        
        ret.r = color.redF();
        ret.g = color.greenF();
        ret.b = color.blueF();
        ret.a = 1.;
        
    } else {
        ret.r = ret.g = ret.b = ret.a = 0.;
    }
    return ret;
}

std::list<Effect*>
GuiApp::getSelectedNodes(Group* group) const
{
    std::list<Effect*> ret;
    
    NodeGraph* graph = 0;
    if (group) {
        Effect* isEffect = dynamic_cast<Effect*>(group);
        if (isEffect) {
            NodeGroup* nodeGrp = dynamic_cast<NodeGroup*>(isEffect->getInternalNode()->getLiveInstance());
            if (nodeGrp) {
                NodeGraphI* graph_i  = nodeGrp->getNodeGraph();
                if (graph_i) {
                    graph = dynamic_cast<NodeGraph*>(graph_i);
                    assert(graph);
                }
            }
        }
    }
    if (!graph) {
        graph = _app->getGui()->getNodeGraph();
    }
    assert(graph);
    if (graph) {
        const std::list<boost::shared_ptr<NodeGui> >& nodes = graph->getSelectedNodes();
        for (std::list<boost::shared_ptr<NodeGui> >::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
            NodePtr node = (*it)->getNode();
            if (node->isActivated() && !node->getParentMultiInstance()) {
                ret.push_back(new Effect(node));
            }
        }
    }
    return ret;
}

PyViewer*
GuiApp::getViewer(const std::string& scriptName) const
{
    NodePtr ptr = _app->getNodeByFullySpecifiedName(scriptName);
    if (!ptr || !ptr->isActivated()) {
        return 0;
    }
    
    ViewerInstance* viewer = dynamic_cast<ViewerInstance*>(ptr->getLiveInstance());
    if (!viewer) {
        return 0;
    }
    
    return new PyViewer(ptr);
}


PyPanel*
GuiApp::getUserPanel(const std::string& scriptName) const
{
    QWidget* w = _app->getGui()->findExistingTab(scriptName);
    if (!w) {
        return 0;
    }
    return dynamic_cast<PyPanel*>(w);
}

PyViewer::PyViewer(const boost::shared_ptr<Natron::Node>& node)
: _node(node)
{
    ViewerInstance* viewer = dynamic_cast<ViewerInstance*>(node->getLiveInstance());
    assert(viewer);
    ViewerGL* viewerGL = dynamic_cast<ViewerGL*>(viewer->getUiContext());
    _viewer = viewerGL ? viewerGL->getViewerTab() : NULL;
    assert(_viewer);
}

PyViewer::~PyViewer()
{
    
}

void
PyViewer::seek(int frame)
{
    if (!_node->isActivated()) {
        return;
    }
    _viewer->seek(frame);
}

int
PyViewer::getCurrentFrame()
{
    if (!_node->isActivated()) {
        return 0;
    }
    return _node->getApp()->getTimeLine()->currentFrame();
}

void
PyViewer::startForward()
{
    if (!_node->isActivated()) {
        return;
    }
    _viewer->startPause(true);
}

void
PyViewer::startBackward()
{
    if (!_node->isActivated()) {
        return;
    }
    _viewer->startBackward(true);
}

void
PyViewer::pause()
{
    if (!_node->isActivated()) {
        return;
    }
    _viewer->abortRendering();
}

void
PyViewer::redraw()
{
    if (!_node->isActivated()) {
        return;
    }
    _viewer->redrawGLWidgets();
}

void
PyViewer::renderCurrentFrame(bool useCache)
{
    if (!_node->isActivated()) {
        return;
    }
    if (useCache) {
        _viewer->getInternalNode()->renderCurrentFrame(false);
    } else {
        _viewer->refresh();
    }
}

void
PyViewer::setFrameRange(int firstFrame,int lastFrame)
{
    if (!_node->isActivated()) {
        return;
    }
    _viewer->setFrameRange(firstFrame, lastFrame);
}

void
PyViewer::getFrameRange(int* firstFrame,int* lastFrame) const
{
    if (!_node->isActivated()) {
        return;
    }
    _viewer->getTimelineBounds(firstFrame, lastFrame);
}

void
PyViewer::setPlaybackMode(Natron::PlaybackModeEnum mode)
{
    if (!_node->isActivated()) {
        return;
    }
    _viewer->setPlaybackMode(mode);
}

Natron::PlaybackModeEnum
PyViewer::getPlaybackMode() const
{
    if (!_node->isActivated()) {
        return Natron::ePlaybackModeLoop;
    }
    return _viewer->getPlaybackMode();
}

Natron::ViewerCompositingOperatorEnum
PyViewer::getCompositingOperator() const
{
    if (!_node->isActivated()) {
        return Natron::eViewerCompositingOperatorNone;
    }
    return _viewer->getCompositingOperator();
}

void
PyViewer::setCompositingOperator(Natron::ViewerCompositingOperatorEnum op)
{
    if (!_node->isActivated()) {
        return;
    }
    _viewer->setCompositingOperator(op);
}

int
PyViewer::getAInput() const
{
    if (!_node->isActivated()) {
        return -1;
    }
    int a,b;
    _viewer->getInternalNode()->getActiveInputs(a, b);
    return a;
}

void
PyViewer::setAInput(int index)
{
    if (!_node->isActivated()) {
        return;
    }
    Natron::EffectInstance* input = _viewer->getInternalNode()->getInput(index);
    if (!input) {
        return;
    }
    _viewer->setInputA(index);
}

int
PyViewer::getBInput() const
{
    if (!_node->isActivated()) {
        return -1;
    }
    int a,b;
    _viewer->getInternalNode()->getActiveInputs(a, b);
    return b;
}

void
PyViewer::setBInput(int index)
{
    if (!_node->isActivated()) {
        return;
    }
    Natron::EffectInstance* input = _viewer->getInternalNode()->getInput(index);
    if (!input) {
        return;
    }
    _viewer->setInputB(index);

}

void
PyViewer::setChannels(Natron::DisplayChannelsEnum channels)
{
    if (!_node->isActivated()) {
        return;
    }
    std::string c = ViewerTab::getChannelsString(channels);
    _viewer->setChannels(c);
}

Natron::DisplayChannelsEnum
PyViewer::getChannels() const
{
    if (!_node->isActivated()) {
        return Natron::eDisplayChannelsRGB;
    }
    return _viewer->getChannels();
}

void
PyViewer::setProxyModeEnabled(bool enabled)
{
    if (!_node->isActivated()) {
        return;
    }
    _viewer->setRenderScaleActivated(enabled);
}

bool
PyViewer::isProxyModeEnabled() const
{
    
    if (!_node->isActivated()) {
        return false;
    }
    
    return _viewer->getRenderScaleActivated();
}

void
PyViewer::setProxyIndex(int index)
{
    if (!_node->isActivated()) {
        return;
    }
    _viewer->setMipMapLevel(index + 1);
}

int
PyViewer::getProxyIndex() const
{
    if (!_node->isActivated()) {
        return 0;
    }
    return _viewer->getMipMapLevel() - 1;
}


void
PyViewer::setCurrentView(int index)
{
    if (!_node->isActivated()) {
        return;
    }
    _viewer->setCurrentView(index);
}

int
PyViewer::getCurrentView() const
{
    if (!_node->isActivated()) {
        return 0;
    }
    return _viewer->getCurrentView();
}
