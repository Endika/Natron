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

#include "ScriptEditor.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QUndoStack>
#include <QUndoCommand>
#include <QFile>
#include <QTextStream>
#include <QThread>
#include <QApplication>
#include <QTimer>
#include <QMutex>
#include <QKeyEvent>

#include "Gui/GuiApplicationManager.h"
#include "Gui/Gui.h"
#include "Gui/Button.h"
#include "Gui/GuiMacros.h"
#include "Gui/SequenceFileDialog.h"
#include "Gui/ScriptTextEdit.h"

struct ScriptEditorPrivate
{
    
    Gui* gui;
    
    QVBoxLayout* mainLayout;
    
    QWidget* buttonsContainer;
    QHBoxLayout* buttonsContainerLayout;
    
    Button* undoB;
    Button* redoB;
    Button* clearHistoB;
    Button* sourceScriptB;
    Button* loadScriptB;
    Button* saveScriptB;
    Button* execScriptB;
    Button* showHideOutputB;
    Button* clearOutputB;
    
    ScriptTextEdit* outputEdit;
    ScriptTextEdit* inputEdit;
    
    QUndoStack history;
    
    QTimer autoSaveTimer;
    QMutex autoSavedScriptMutex;
    QString autoSavedScript;
    
    ScriptEditorPrivate(Gui* gui)
    : gui(gui)
    , mainLayout(0)
    , buttonsContainer(0)
    , buttonsContainerLayout(0)
    , undoB(0)
    , redoB(0)
    , clearHistoB(0)
    , sourceScriptB(0)
    , loadScriptB(0)
    , saveScriptB(0)
    , execScriptB(0)
    , showHideOutputB(0)
    , clearOutputB(0)
    , outputEdit(0)
    , inputEdit(0)
    , autoSaveTimer()
    , autoSavedScriptMutex()
    , autoSavedScript()
    {
        
    }
};

ScriptEditor::ScriptEditor(Gui* gui)
: QWidget(gui)
, _imp(new ScriptEditorPrivate(gui))
{
    _imp->mainLayout = new QVBoxLayout(this);
    _imp->buttonsContainer = new QWidget(this);
    _imp->buttonsContainerLayout = new QHBoxLayout(_imp->buttonsContainer);
    _imp->buttonsContainerLayout->setContentsMargins(0, 0, 0, 0);
    _imp->buttonsContainerLayout->setSpacing(2);
    
    QPixmap undoPix,redoPix,clearHistoPix,sourceScriptPix,loadScriptPix,saveScriptPix,execScriptPix,outputVisiblePix,outputHiddenPix,clearOutpoutPix;
    appPTR->getIcon(Natron::NATRON_PIXMAP_SCRIPT_PREVIOUS_SCRIPT, &undoPix);
    appPTR->getIcon(Natron::NATRON_PIXMAP_SCRIPT_NEXT_SCRIPT, &redoPix);
    appPTR->getIcon(Natron::NATRON_PIXMAP_CLOSE_PANEL, &clearHistoPix);
    appPTR->getIcon(Natron::NATRON_PIXMAP_CLEAR_ALL_ANIMATION, &clearHistoPix);
    appPTR->getIcon(Natron::NATRON_PIXMAP_SCRIPT_CLEAR_OUTPUT,&clearOutpoutPix);
    appPTR->getIcon(Natron::NATRON_PIXMAP_SCRIPT_EXEC_SCRIPT,&execScriptPix);
    appPTR->getIcon(Natron::NATRON_PIXMAP_SCRIPT_OUTPUT_PANE_ACTIVATED,&outputVisiblePix);
    appPTR->getIcon(Natron::NATRON_PIXMAP_SCRIPT_OUTPUT_PANE_DEACTIVATED,&outputHiddenPix);
    appPTR->getIcon(Natron::NATRON_PIXMAP_SCRIPT_LOAD_EXEC_SCRIPT,&sourceScriptPix);
    appPTR->getIcon(Natron::NATRON_PIXMAP_SCRIPT_LOAD_SCRIPT,&loadScriptPix);
    appPTR->getIcon(Natron::NATRON_PIXMAP_SCRIPT_SAVE_SCRIPT,&saveScriptPix);
    
    _imp->undoB = new Button(QIcon(undoPix),"",_imp->buttonsContainer);
    QKeySequence undoSeq(Qt::CTRL + Qt::Key_BracketLeft);
    _imp->undoB->setFixedSize(NATRON_MEDIUM_BUTTON_SIZE, NATRON_MEDIUM_BUTTON_SIZE);
    _imp->undoB->setFocusPolicy(Qt::NoFocus);
    _imp->undoB->setToolTip(tr("Previous script") +
                               tr("<br>Keyboard shortcut: ") + undoSeq.toString(QKeySequence::NativeText) + "</br>");
    _imp->undoB->setEnabled(false);
    QObject::connect(_imp->undoB, SIGNAL(clicked(bool)), this, SLOT(onUndoClicked()));
    
    _imp->redoB = new Button(QIcon(redoPix),"",_imp->buttonsContainer);
    QKeySequence redoSeq(Qt::CTRL + Qt::Key_BracketRight);
    _imp->redoB->setFocusPolicy(Qt::NoFocus);
    _imp->redoB->setFixedSize(NATRON_MEDIUM_BUTTON_SIZE, NATRON_MEDIUM_BUTTON_SIZE);
    _imp->redoB->setToolTip(tr("Next script") +
                            tr("<br>Keyboard shortcut: ") + redoSeq.toString(QKeySequence::NativeText) + "</br>");
    _imp->redoB->setEnabled(false);
    QObject::connect(_imp->redoB, SIGNAL(clicked(bool)), this, SLOT(onRedoClicked()));
    
    _imp->clearHistoB = new Button(QIcon(clearHistoPix),"",_imp->buttonsContainer);
    _imp->clearHistoB->setToolTip(tr("Clear history"));
    _imp->clearHistoB->setFixedSize(NATRON_MEDIUM_BUTTON_SIZE, NATRON_MEDIUM_BUTTON_SIZE);
    _imp->clearHistoB->setFocusPolicy(Qt::NoFocus);
    QObject::connect(_imp->clearHistoB, SIGNAL(clicked(bool)), this, SLOT(onClearHistoryClicked()));
    
    _imp->sourceScriptB = new Button(QIcon(sourceScriptPix),"",_imp->buttonsContainer);
    _imp->sourceScriptB->setToolTip(tr("Open and execute a script"));
    _imp->sourceScriptB->setFocusPolicy(Qt::NoFocus);
    _imp->sourceScriptB->setFixedSize(NATRON_MEDIUM_BUTTON_SIZE, NATRON_MEDIUM_BUTTON_SIZE);
    QObject::connect(_imp->sourceScriptB, SIGNAL(clicked(bool)), this, SLOT(onSourceScriptClicked()));
    
    _imp->loadScriptB = new Button(QIcon(loadScriptPix),"",_imp->buttonsContainer);
    _imp->loadScriptB->setToolTip(tr("Open a script without executing it"));
    _imp->loadScriptB->setFocusPolicy(Qt::NoFocus);
    _imp->loadScriptB->setFixedSize(NATRON_MEDIUM_BUTTON_SIZE, NATRON_MEDIUM_BUTTON_SIZE);
    QObject::connect(_imp->loadScriptB, SIGNAL(clicked(bool)), this, SLOT(onLoadScriptClicked()));
    
    _imp->saveScriptB = new Button(QIcon(saveScriptPix),"",_imp->buttonsContainer);
    _imp->saveScriptB->setToolTip(tr("Save the current script"));
    _imp->saveScriptB->setFocusPolicy(Qt::NoFocus);
    _imp->saveScriptB->setFixedSize(NATRON_MEDIUM_BUTTON_SIZE, NATRON_MEDIUM_BUTTON_SIZE);
    QObject::connect(_imp->saveScriptB, SIGNAL(clicked(bool)), this, SLOT(onSaveScriptClicked()));
    
    _imp->execScriptB = new Button(QIcon(execScriptPix),"",_imp->buttonsContainer);
    QKeySequence execSeq(Qt::CTRL + Qt::Key_Return);
    _imp->execScriptB->setFocusPolicy(Qt::NoFocus);
    _imp->execScriptB->setFixedSize(NATRON_MEDIUM_BUTTON_SIZE, NATRON_MEDIUM_BUTTON_SIZE);
    _imp->execScriptB->setToolTip(tr("Execute the current script")
                                  + tr("<br>Keyboard shortcut: ") + execSeq.toString(QKeySequence::NativeText) + "</br>");

    QObject::connect(_imp->execScriptB, SIGNAL(clicked(bool)), this, SLOT(onExecScriptClicked()));
    
    QIcon icShowHide;
    icShowHide.addPixmap(outputVisiblePix,QIcon::Normal,QIcon::On);
    icShowHide.addPixmap(outputHiddenPix,QIcon::Normal,QIcon::Off);
    _imp->showHideOutputB = new Button(icShowHide,"",_imp->buttonsContainer);
    _imp->showHideOutputB->setToolTip(tr("Show/Hide the output area"));
    _imp->showHideOutputB->setFocusPolicy(Qt::NoFocus);
    _imp->showHideOutputB->setCheckable(true);
    _imp->showHideOutputB->setChecked(true);
    _imp->showHideOutputB->setDown(true);
    _imp->showHideOutputB->setFixedSize(NATRON_MEDIUM_BUTTON_SIZE, NATRON_MEDIUM_BUTTON_SIZE);
    QObject::connect(_imp->showHideOutputB, SIGNAL(clicked(bool)), this, SLOT(onShowHideOutputClicked(bool)));
    
    _imp->clearOutputB = new Button(QIcon(clearOutpoutPix),"",_imp->buttonsContainer);
    QKeySequence clearSeq(Qt::CTRL + Qt::Key_Backspace);
    _imp->clearOutputB->setFocusPolicy(Qt::NoFocus);
    _imp->clearOutputB->setFixedSize(NATRON_MEDIUM_BUTTON_SIZE, NATRON_MEDIUM_BUTTON_SIZE);
    _imp->clearOutputB->setToolTip(tr("Clear the output area")
                                   + tr("<br>Keyboard shortcut: ") + clearSeq.toString(QKeySequence::NativeText) + "</br>");
    QObject::connect(_imp->clearOutputB, SIGNAL(clicked(bool)), this, SLOT(onClearOutputClicked()));
    
    _imp->buttonsContainerLayout->addWidget(_imp->undoB);
    _imp->buttonsContainerLayout->addWidget(_imp->redoB);
    _imp->buttonsContainerLayout->addWidget(_imp->clearHistoB);
    _imp->buttonsContainerLayout->addSpacing(10);
    
    _imp->buttonsContainerLayout->addWidget(_imp->sourceScriptB);
    _imp->buttonsContainerLayout->addWidget(_imp->loadScriptB);
    _imp->buttonsContainerLayout->addWidget(_imp->saveScriptB);
    _imp->buttonsContainerLayout->addWidget(_imp->execScriptB);
    _imp->buttonsContainerLayout->addSpacing(10);
    
    _imp->buttonsContainerLayout->addWidget(_imp->showHideOutputB);
    _imp->buttonsContainerLayout->addWidget(_imp->clearOutputB);
    _imp->buttonsContainerLayout->addStretch();
    
    _imp->outputEdit = new ScriptTextEdit(this);
    _imp->outputEdit->setOutput(true);
    _imp->outputEdit->setFocusPolicy(Qt::ClickFocus);
    _imp->outputEdit->setReadOnly(true);
    
    _imp->inputEdit = new ScriptTextEdit(this);
    QObject::connect(_imp->inputEdit, SIGNAL(textChanged()), this, SLOT(onInputScriptTextChanged()));
    QFontMetrics fm = _imp->inputEdit->fontMetrics();
    _imp->inputEdit->setTabStopWidth(fm.width(' ') * 4);
    _imp->outputEdit->setTabStopWidth(fm.width(' ') * 4);
    
    _imp->mainLayout->addWidget(_imp->buttonsContainer);
    _imp->mainLayout->addWidget(_imp->outputEdit);
    _imp->mainLayout->addWidget(_imp->inputEdit);
    
    QObject::connect(&_imp->history, SIGNAL(canUndoChanged(bool)), this, SLOT(onHistoryCanUndoChanged(bool)));
    QObject::connect(&_imp->history, SIGNAL(canRedoChanged(bool)), this, SLOT(onHistoryCanRedoChanged(bool)));
    
    _imp->autoSaveTimer.setSingleShot(true);
}

ScriptEditor::~ScriptEditor()
{
    
}

void
ScriptEditor::onHistoryCanUndoChanged(bool canUndo)
{
    _imp->undoB->setEnabled(canUndo);
}

void
ScriptEditor::onHistoryCanRedoChanged(bool canRedo)
{
    _imp->redoB->setEnabled(canRedo);
}

void
ScriptEditor::onShowHideOutputClicked(bool clicked)
{
    _imp->showHideOutputB->setDown(clicked);
    _imp->outputEdit->setVisible(clicked);
    
}

void
ScriptEditor::onClearOutputClicked()
{
    _imp->outputEdit->clear();
}

void
ScriptEditor::onClearHistoryClicked()
{
    _imp->inputEdit->clear();
    _imp->history.clear();
}

void
ScriptEditor::onUndoClicked()
{
    _imp->history.undo();
}

void
ScriptEditor::onRedoClicked()
{
    _imp->history.redo();
}

void
ScriptEditor::onSourceScriptClicked()
{
    std::vector<std::string> filters;
    filters.push_back("py");
    SequenceFileDialog dialog(this,filters,false,SequenceFileDialog::eFileDialogModeOpen,_imp->gui->getLastLoadProjectDirectory().toStdString(),
                              _imp->gui,false);
    
    if (dialog.exec()) {
        
        QDir currentDir = dialog.currentDirectory();
        _imp->gui->updateLastOpenedProjectPath(currentDir.absolutePath());

        QString fileName(dialog.selectedFiles().c_str());
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            QTextStream ts(&file);
            QString content = ts.readAll();
            _imp->inputEdit->setPlainText(content);
            onExecScriptClicked();
        } else {
            Natron::errorDialog(tr("Operation failed").toStdString(), tr("Failure to open the file").toStdString());
        }
        
    }
}

void
ScriptEditor::onLoadScriptClicked()
{
    std::vector<std::string> filters;
    filters.push_back("py");
    SequenceFileDialog dialog(this,filters,false,SequenceFileDialog::eFileDialogModeOpen,_imp->gui->getLastLoadProjectDirectory().toStdString(),
                              _imp->gui,false);
    
    if (dialog.exec()) {
        
        QDir currentDir = dialog.currentDirectory();
        _imp->gui->updateLastOpenedProjectPath(currentDir.absolutePath());
        QString fileName(dialog.selectedFiles().c_str());
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            QTextStream ts(&file);
            QString content = ts.readAll();
            _imp->inputEdit->setPlainText(content);
        } else {
            Natron::errorDialog(tr("Operation failed").toStdString(), tr("Failure to open the file").toStdString());
        }
        
    }
}

void
ScriptEditor::onSaveScriptClicked()
{
    std::vector<std::string> filters;
    filters.push_back("py");
    SequenceFileDialog dialog(this,filters,false,SequenceFileDialog::eFileDialogModeSave,_imp->gui->getLastSaveProjectDirectory().toStdString(),
                              _imp->gui,false);
    
    if (dialog.exec()) {
        
        QDir currentDir = dialog.currentDirectory();
        _imp->gui->updateLastSavedProjectPath(currentDir.absolutePath());

        QString fileName(dialog.selectedFiles().c_str());
        QFile file(fileName);
        if (file.open(QIODevice::ReadWrite)) {
            QTextStream ts(&file);
            ts << _imp->inputEdit->toPlainText();
        } else {
            Natron::errorDialog(tr("Operation failed").toStdString(), tr("Failure to save the file").toStdString());
        }
        
    }
}


class InputScriptCommand: public QUndoCommand
{
    
    ScriptEditor* _editor;
    QString _script;
    bool _firstRedoCalled;
    
public:
    
    InputScriptCommand(ScriptEditor* editor,  const QString& script);
    
    void redo();
    
    void undo();
};


InputScriptCommand::InputScriptCommand(ScriptEditor* editor, const QString& script)
: QUndoCommand()
, _editor(editor)
, _script(script)
, _firstRedoCalled(false)
{
    
}

void
InputScriptCommand::redo()
{
    if (_firstRedoCalled) {
        _editor->setInputScript(_script);
    }
    _firstRedoCalled = true;
    setText(QObject::tr("Exec script"));
}

void
InputScriptCommand::undo()
{
    _editor->setInputScript(_script);
   setText(QObject::tr("Exec script"));
}

void
ScriptEditor::onExecScriptClicked()
{
    QString script = _imp->inputEdit->toPlainText();
    std::string error,output;
    
    if (!Natron::interpretPythonScript(script.toStdString(), &error, &output)) {
        _imp->outputEdit->append(Qt::convertFromPlainText(error.c_str(),Qt::WhiteSpaceNormal));
    } else {
        QString toAppend(script);
        if (!output.empty()) {
            toAppend.append("\n#Result:\n");
            toAppend.append(output.c_str());
        }
        _imp->outputEdit->append(toAppend);
        _imp->history.push(new InputScriptCommand(this,script));
        _imp->inputEdit->clear();
    }
    
}

void
ScriptEditor::setInputScript(const QString& script)
{
    _imp->inputEdit->setPlainText(script);
}

QString
ScriptEditor::getInputScript() const
{
    assert(QThread::currentThread() == qApp->thread());
    
    return _imp->inputEdit->toPlainText();
}

void
ScriptEditor::keyPressEvent(QKeyEvent* e)
{
    Qt::Key key = (Qt::Key)e->key();
    if (key == Qt::Key_BracketLeft && modCASIsControl(e)) {
        onUndoClicked();
    } else if (key == Qt::Key_BracketRight && modifierHasControl(e)) {
        onRedoClicked();
    } else if ((key == Qt::Key_Return || key == Qt::Key_Enter) && modifierHasControl(e)) {
        onExecScriptClicked();
    } else if (key == Qt::Key_Backspace && modifierHasControl(e)) {
        onClearOutputClicked();
    } else {
        QWidget::keyPressEvent(e);
    }
    
}

void
ScriptEditor::onInputScriptTextChanged()
{
    if (!_imp->autoSaveTimer.isActive()) {
        _imp->autoSaveTimer.singleShot(5000, this, SLOT(onAutoSaveTimerTimedOut()));
    }
}

void
ScriptEditor::onAutoSaveTimerTimedOut()
{
    QMutexLocker k(&_imp->autoSavedScriptMutex);
    _imp->autoSavedScript = _imp->inputEdit->toPlainText();
}

QString
ScriptEditor::getAutoSavedScript() const
{
    QMutexLocker k(&_imp->autoSavedScriptMutex);
    return _imp->autoSavedScript;
}

void
ScriptEditor::appendToScriptEditor(const QString& str)
{
    _imp->outputEdit->append(str + "\n");
}
