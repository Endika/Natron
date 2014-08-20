//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 *Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 *contact: immarespond at gmail dot com
 *
 */
#ifndef PROJECTGUI_H
#define PROJECTGUI_H

#include "Global/Macros.h"
CLANG_DIAG_OFF(deprecated)
CLANG_DIAG_OFF(uninitialized)
#include <QtCore/QObject>
#include <QDialog>
CLANG_DIAG_ON(deprecated)
CLANG_DIAG_ON(uninitialized)

#include <boost/shared_ptr.hpp>

#include "Engine/Format.h"

class Button;
class QWidget;
class QHBoxLayout;
class QVBoxLayout;
class QLabel;
class ComboBox;
class SpinBox;
class LineEdit;
class Color_Knob;
class DockablePanel;
class ProjectGuiSerialization;
class Gui;
class NodeGui;
class NodeGuiSerialization;
namespace boost {
    namespace archive {
        class xml_archive;
    }
}

namespace Natron{
    class Project;
}

class ProjectGui : public QObject
{
    Q_OBJECT
    
public:
    
    ProjectGui(Gui* gui);
    virtual ~ProjectGui() OVERRIDE;
        
    void create(boost::shared_ptr<Natron::Project> projectInternal,QVBoxLayout* container,QWidget* parent = NULL);
    
    
    
    bool isVisible() const;
    
    DockablePanel* getPanel() const { return _panel; }
    
    boost::shared_ptr<Natron::Project> getInternalProject() const { return _project; }
    
    void save(boost::archive::xml_oarchive& archive) const;
    
    void load(boost::archive::xml_iarchive& archive);
    
    void registerNewColorPicker(boost::shared_ptr<Color_Knob> knob);
    
    void removeColorPicker(boost::shared_ptr<Color_Knob> knob);
    
    bool hasPickers() const { return !_colorPickersEnabled.empty(); }
    
    void setPickersColor(const QColor& color);
    
    /**
     * @brief Retur
     **/
    std::list<boost::shared_ptr<NodeGui> > getVisibleNodes() const;
    
    Gui* getGui() const { return _gui; }
    
public slots:
    
    void createNewFormat();

    void setVisible(bool visible);

    void initializeKnobsGui();

private:
    
    
    Gui* _gui;
    
    boost::shared_ptr<Natron::Project> _project;
    
    DockablePanel* _panel;
    
    bool _created;
    
    std::vector<boost::shared_ptr<Color_Knob> > _colorPickersEnabled;
};


class AddFormatDialog : public QDialog {
    
    Q_OBJECT
    
public:
    
    AddFormatDialog(Natron::Project* project,Gui* gui);
    
    virtual ~AddFormatDialog(){}
    
    Format getFormat() const ;
    
    public slots:
    
    void onCopyFromViewer();
    
private:
    
    Gui* _gui;
    Natron::Project* _project;
    
    QVBoxLayout* _mainLayout;
    
    QWidget* _fromViewerLine;
    QHBoxLayout* _fromViewerLineLayout;
    Button* _copyFromViewerButton;
    ComboBox* _copyFromViewerCombo;
    
    QWidget* _parametersLine;
    QHBoxLayout* _parametersLineLayout;
    QLabel* _widthLabel;
    SpinBox* _widthSpinBox;
    QLabel* _heightLabel;
    SpinBox* _heightSpinBox;
    QLabel* _pixelAspectLabel;
    SpinBox* _pixelAspectSpinBox;
    
    
    QWidget* _formatNameLine;
    QHBoxLayout* _formatNameLayout;
    QLabel* _nameLabel;
    LineEdit* _nameLineEdit;
    
    QWidget* _buttonsLine;
    QHBoxLayout* _buttonsLineLayout;
    Button* _cancelButton;
    Button* _okButton;
};



#endif // PROJECTGUI_H
