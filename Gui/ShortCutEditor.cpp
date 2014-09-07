//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

#include "ShortCutEditor.h"

#include <list>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QTextDocument>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QApplication>

#include "Gui/Button.h"
#include "Gui/GuiApplicationManager.h"
#include "Gui/ActionShortcuts.h"

struct GuiBoundAction
{
    QTreeWidgetItem* item;
    BoundAction* action;
};

struct GuiShortCutGroup
{
    std::list<GuiBoundAction> actions;
    QTreeWidgetItem* item;
};

static QString
keybindToString(const Qt::KeyboardModifiers & modifiers,
                Qt::Key key)
{
    return makeKeySequence(modifiers, key).toString(QKeySequence::NativeText);
}

static QString
mouseShortcutToString(const Qt::KeyboardModifiers & modifiers,
                      Qt::MouseButton button)
{
    QString ret = makeKeySequence(modifiers, (Qt::Key)0).toString(QKeySequence::NativeText);

    switch (button) {
    case Qt::LeftButton:
        ret.append( QObject::tr("LeftButton") );
        break;
    case Qt::MiddleButton:
        ret.append( QObject::tr("MiddleButton") );
        break;
    case Qt::RightButton:
        ret.append( QObject::tr("RightButton") );
        break;
    default:
        break;
    }

    return ret;
}

typedef std::list<GuiShortCutGroup> GuiAppShorcuts;

///A small hack to the QTreeWidget class to make 2 fuctions public so we can use them in the ShortcutDelegate class
class HackedTreeWidget
    : public QTreeWidget
{
public:

    HackedTreeWidget(QWidget* parent)
        : QTreeWidget(parent)
    {
    }

    QModelIndex indexFromItem_natron(QTreeWidgetItem * item,
                                     int column = 0) const
    {
        return indexFromItem(item,column);
    }

    QTreeWidgetItem* itemFromIndex_natron(const QModelIndex & index) const
    {
        return itemFromIndex(index);
    }
};

struct ShortCutEditorPrivate
{
    QVBoxLayout* mainLayout;
    HackedTreeWidget* tree;
    QGroupBox* shortcutGroup;
    QHBoxLayout* shortcutGroupLayout;
    QLabel* shortcutLabel;
    KeybindRecorder* shortcutEditor;
    Button* validateButton;
    Button* clearButton;
    Button* resetButton;
    QWidget* buttonsContainer;
    QHBoxLayout* buttonsLayout;
    Button* restoreDefaultsButton;
    Button* applyButton;
    Button* cancelButton;
    Button* okButton;
    GuiAppShorcuts appShortcuts;

    ShortCutEditorPrivate()
        : mainLayout(0)
          , tree(0)
          , shortcutGroup(0)
          , shortcutGroupLayout(0)
          , shortcutLabel(0)
          , validateButton(0)
          , clearButton(0)
          , resetButton(0)
          , buttonsContainer(0)
          , buttonsLayout(0)
          , restoreDefaultsButton(0)
          , applyButton(0)
          , cancelButton(0)
          , okButton(0)
          , appShortcuts()
    {
    }

    BoundAction* getActionForTreeItem(QTreeWidgetItem* item) const
    {
        for (GuiAppShorcuts::const_iterator it = appShortcuts.begin(); it != appShortcuts.end(); ++it) {
            if (it->item == item) {
                return NULL;
            }
            for (std::list<GuiBoundAction>::const_iterator it2 = it->actions.begin(); it2 != it->actions.end(); ++it2) {
                if (it2->item == item) {
                    return it2->action;
                }
            }
        }

        return (BoundAction*)NULL;
    }
    
    GuiAppShorcuts::iterator buildGroupHierarchy(QString grouping);
    
    void makeGuiActionForShortcut(GuiAppShorcuts::iterator guiGroupIterator,BoundAction* action);
};

static QString
makeItemShortCutText(const BoundAction* action,
                     bool useDefault)
{
    const KeyBoundAction* ka = dynamic_cast<const KeyBoundAction*>(action);
    const MouseAction* ma = dynamic_cast<const MouseAction*>(action);
    QString shortcutStr;

    if (ka) {
        if (useDefault) {
            shortcutStr = keybindToString(ka->defaultModifiers, ka->defaultShortcut);
        } else {
            shortcutStr = keybindToString(ka->modifiers, ka->currentShortcut);
        }
    } else if (ma) {
        if (useDefault) {
            shortcutStr = mouseShortcutToString(ma->defaultModifiers, ma->button);
        } else {
            shortcutStr = mouseShortcutToString(ma->modifiers, ma->button);
        }
    } else {
        assert(false);
    }

    return shortcutStr;
}

static void
setItemShortCutText(QTreeWidgetItem* item,
                    const BoundAction* action,
                    bool useDefault)
{
    item->setText( 1, makeItemShortCutText(action,useDefault) );
}

class ShortcutDelegate
    : public QStyledItemDelegate
{
    HackedTreeWidget* tree;

public:

    ShortcutDelegate(HackedTreeWidget* parent)
        : QStyledItemDelegate(parent)
          , tree(parent)
    {
    }

private:

    virtual void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const OVERRIDE FINAL;
};

ShortCutEditor::ShortCutEditor(QWidget* parent)
    : QWidget(parent)
      , _imp( new ShortCutEditorPrivate() )
{
    _imp->mainLayout = new QVBoxLayout(this);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::Window);
    setWindowTitle( tr("Shortcuts editor") );
    _imp->tree = new HackedTreeWidget(this);
    _imp->tree->setColumnCount(2);
    QStringList headers;
    headers << tr("Command") << tr("Shortcut");
    _imp->tree->setHeaderLabels(headers);
    _imp->tree->setSelectionMode(QAbstractItemView::SingleSelection);
    _imp->tree->setAttribute(Qt::WA_MacShowFocusRect,0);
    _imp->tree->setSortingEnabled(false);
    _imp->tree->setToolTip( Qt::convertFromPlainText(
                                tr("In this table is represented each action of the application that can have a possible keybind/mouse shortcut."
                                   " Note that this table also have some special assignments which also involve the mouse. "
                                   "You cannot assign a keybind to a shortcut involving the mouse and vice versa. "
                                   "Note that internally " NATRON_APPLICATION_NAME " does an emulation of a three-button mouse "
                                   "if your computer doesn't have one, that is: \n"
                                   "---> Middle mouse button is emulated by holding down Options (alt) coupled with a left click.\n "
                                   "---> Right mouse button is emulated by holding down Command (cmd) coupled with a left click."),Qt::WhiteSpaceNormal) );
    _imp->tree->setItemDelegate( new ShortcutDelegate(_imp->tree) );
    
    const AppShortcuts & appShortcuts = appPTR->getAllShortcuts();
    
    for (AppShortcuts::const_iterator it = appShortcuts.begin(); it != appShortcuts.end(); ++it) {
      
        GuiAppShorcuts::iterator foundGuiGroup = _imp->buildGroupHierarchy(it->first);
        assert(foundGuiGroup != _imp->appShortcuts.end());
        
        for (GroupShortcuts::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            _imp->makeGuiActionForShortcut(foundGuiGroup, it2->second);
        }
    }
    
    _imp->tree->resizeColumnToContents(0);
    QObject::connect( _imp->tree, SIGNAL( itemSelectionChanged() ), this, SLOT( onSelectionChanged() ) );

    _imp->mainLayout->addWidget(_imp->tree);

    _imp->shortcutGroup = new QGroupBox(tr("Shortcut"),this);
    _imp->mainLayout->addWidget(_imp->shortcutGroup);

    _imp->shortcutGroupLayout = new QHBoxLayout(_imp->shortcutGroup);
    _imp->shortcutGroupLayout->setContentsMargins(0, 0, 0, 0);

    _imp->shortcutLabel = new QLabel(_imp->shortcutGroup);
    _imp->shortcutLabel->setText( tr("Sequence:") );
    _imp->shortcutGroupLayout->addWidget(_imp->shortcutLabel);

    _imp->shortcutEditor = new KeybindRecorder(_imp->shortcutGroup);
    _imp->shortcutEditor->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    _imp->shortcutEditor->setPlaceholderText( tr("Type to set shortcut") );
    _imp->shortcutGroupLayout->addWidget(_imp->shortcutEditor);

    _imp->validateButton = new Button(tr("Validate"),_imp->shortcutGroup);
    _imp->validateButton->setToolTip( tr("Validates the shortcut on the field editor and set the selected shortcut.") );
    _imp->shortcutGroupLayout->addWidget(_imp->validateButton);
    QObject::connect( _imp->validateButton, SIGNAL( clicked(bool) ), this, SLOT( onValidateButtonClicked() ) );

    _imp->clearButton = new Button(tr("Clear"),_imp->shortcutGroup);
    QObject::connect( _imp->clearButton, SIGNAL( clicked(bool) ), this, SLOT( onClearButtonClicked() ) );
    _imp->shortcutGroupLayout->addWidget(_imp->clearButton);

    _imp->resetButton = new Button(tr("Reset"),_imp->shortcutGroup);
    QObject::connect( _imp->resetButton, SIGNAL( clicked(bool) ), this, SLOT( onResetButtonClicked() ) );
    _imp->shortcutGroupLayout->addWidget(_imp->resetButton);

    _imp->buttonsContainer = new QWidget(this);
    _imp->buttonsLayout = new QHBoxLayout(_imp->buttonsContainer);

    _imp->mainLayout->QLayout::addWidget(_imp->buttonsContainer);

    _imp->restoreDefaultsButton = new Button(tr("Restore defaults"),_imp->buttonsContainer);
    QObject::connect( _imp->restoreDefaultsButton, SIGNAL( clicked(bool) ), this, SLOT( onRestoreDefaultsButtonClicked() ) );
    _imp->buttonsLayout->addWidget(_imp->restoreDefaultsButton);

    _imp->applyButton = new Button(tr("Apply"),_imp->buttonsContainer);
    QObject::connect( _imp->applyButton, SIGNAL( clicked(bool) ), this, SLOT( onApplyButtonClicked() ) );
    _imp->buttonsLayout->addWidget(_imp->applyButton);

    _imp->buttonsLayout->addStretch();

    _imp->cancelButton = new Button(tr("Cancel"),_imp->buttonsContainer);
    QObject::connect( _imp->cancelButton, SIGNAL( clicked(bool) ), this, SLOT( onCancelButtonClicked() ) );
    _imp->buttonsLayout->addWidget(_imp->cancelButton);

    _imp->okButton = new Button(tr("Ok"),_imp->buttonsContainer);
    QObject::connect( _imp->okButton, SIGNAL( clicked(bool) ), this, SLOT( onOkButtonClicked() ) );
    _imp->buttonsLayout->addWidget(_imp->okButton);

    _imp->buttonsLayout->addStretch();

    resize(700,400);
}

ShortCutEditor::~ShortCutEditor()
{
}

GuiAppShorcuts::iterator
ShortCutEditorPrivate::buildGroupHierarchy(QString grouping)
{
    ///Do not allow empty grouping, make them under the Global shortcut
    if ( grouping.isEmpty() ) {
        grouping = kShortcutGroupGlobal;
    }
    
    ///Groups are separated by a '/'
    QStringList groupingSplit = grouping.split( QChar('/') );
    assert(groupingSplit.size() > 0);
    const QString & lastGroupName = groupingSplit.back();
    
    ///Find out whether the deepest parent group of the action already exist
    GuiAppShorcuts::iterator foundGuiGroup = appShortcuts.end();
    for (GuiAppShorcuts::iterator it2 = appShortcuts.begin(); it2 != appShortcuts.end(); ++it2) {
        if (it2->item->text(0) == lastGroupName) {
            foundGuiGroup = it2;
            break;
        }
    }
    
    QTreeWidgetItem* groupParent;
    if ( foundGuiGroup != appShortcuts.end() ) {
        groupParent = foundGuiGroup->item;
    } else {
        groupParent = 0;
        for (int i = 0; i < groupingSplit.size(); ++i) {
            QTreeWidgetItem* groupingItem;
            bool existAlready = false;
            if (groupParent) {
                for (int j = 0; j < groupParent->childCount(); ++j) {
                    QTreeWidgetItem* child = groupParent->child(j);
                    if (child->text(0) == groupingSplit[i]) {
                        groupingItem = child;
                        existAlready = true;
                        break;
                    }
                }
                if (!existAlready) {
                    groupingItem = new QTreeWidgetItem(groupParent);
                    groupParent->addChild(groupingItem);
                }
            } else {
                for (int j = 0; j < tree->topLevelItemCount(); ++j) {
                    QTreeWidgetItem* topLvlItem = tree->topLevelItem(j);
                    if (topLvlItem->text(0) == groupingSplit[i]) {
                        groupingItem = topLvlItem;
                        existAlready = true;
                        break;
                    }
                }
                if (!existAlready) {
                    groupingItem = new QTreeWidgetItem(tree);
                    tree->addTopLevelItem(groupingItem);
                }
            }
            if (!existAlready) {
                /* expand only top-level groups */
                groupingItem->setExpanded(i == 0);
                groupingItem->setFlags(Qt::ItemIsEnabled);
                groupingItem->setText(0, groupingSplit[i]);
            }
            groupParent = groupingItem;
        }
        GuiShortCutGroup group;
        group.item = groupParent;
        foundGuiGroup = appShortcuts.insert(appShortcuts.end(), group);
    }
    return foundGuiGroup;
}

void
ShortCutEditorPrivate::makeGuiActionForShortcut(GuiAppShorcuts::iterator guiGroupIterator,BoundAction* action)
{
    GuiBoundAction guiAction;
    guiAction.action = action;
    guiAction.item = new QTreeWidgetItem(guiGroupIterator->item);
    guiAction.item->setText(0, guiAction.action->description);
    const KeyBoundAction* ka = dynamic_cast<const KeyBoundAction*>(action);
    const MouseAction* ma = dynamic_cast<const MouseAction*>(action);
    QString shortcutStr;
    if (ka) {
        shortcutStr = keybindToString(ka->modifiers, ka->currentShortcut);
    } else if (ma) {
        shortcutStr = mouseShortcutToString(ma->modifiers, ma->button);
    } else {
        assert(false);
    }
    if (!action->editable) {
        guiAction.item->setToolTip( 0, QObject::tr("This action is standard and its shortcut cannot be edited.") );
        guiAction.item->setToolTip( 1, QObject::tr("This action is standard and its shortcut cannot be edited.") );
        guiAction.item->setDisabled(true);
    }
    guiAction.item->setExpanded(true);
    guiAction.item->setText(1, shortcutStr);
    guiGroupIterator->actions.push_back(guiAction);
    guiGroupIterator->item->addChild(guiAction.item);

}

void
ShortCutEditor::addShortcut(BoundAction* action)
{
    GuiAppShorcuts::iterator foundGuiGroup = _imp->buildGroupHierarchy(action->grouping);
    assert(foundGuiGroup != _imp->appShortcuts.end());
    
    _imp->makeGuiActionForShortcut(foundGuiGroup, action);

}

void
ShortCutEditor::onSelectionChanged()
{
    QList<QTreeWidgetItem*> items = _imp->tree->selectedItems();
    if (items.size() > 1) {
        //we do not support selection of more than 1 item
        return;
    }

    if ( items.empty() ) {
        _imp->shortcutEditor->setText("");
        _imp->shortcutEditor->setPlaceholderText( tr("Type to set shortcut") );
        _imp->shortcutEditor->setReadOnly(true);

        return;
    }
    QTreeWidgetItem* selection = items.front();
    if ( !selection->isDisabled() ) {
        _imp->shortcutEditor->setReadOnly(false);
        _imp->clearButton->setEnabled(true);
        _imp->resetButton->setEnabled(true);
    } else {
        _imp->shortcutEditor->setReadOnly(true);
        _imp->clearButton->setEnabled(false);
        _imp->resetButton->setEnabled(false);
    }

    BoundAction* action = _imp->getActionForTreeItem(selection);
    assert(action);
    _imp->shortcutEditor->setText( makeItemShortCutText(action, false) );
}

void
ShortCutEditor::onValidateButtonClicked()
{
    QString text = _imp->shortcutEditor->text();

    QList<QTreeWidgetItem*> items = _imp->tree->selectedItems();
    if ( (items.size() > 1) || items.empty() ) {
        return;
    }

    QTreeWidgetItem* selection = items.front();
    QKeySequence seq(text,QKeySequence::NativeText);
    BoundAction* action = _imp->getActionForTreeItem(selection);

    //only keybinds can be edited...
    KeyBoundAction* ka = dynamic_cast<KeyBoundAction*>(action);
    assert(ka);

    Qt::KeyboardModifiers modifiers;
    Qt::Key symbol;
    extractKeySequence(seq, modifiers, symbol);

    for (GuiAppShorcuts::iterator it = _imp->appShortcuts.begin(); it != _imp->appShortcuts.end(); ++it) {
        for (std::list<GuiBoundAction>::iterator it2 = it->actions.begin(); it2 != it->actions.end(); ++it2) {
            if (it2->action != action) {
                KeyBoundAction* keyAction = dynamic_cast<KeyBoundAction*>(it2->action);
                if ( keyAction && (keyAction->modifiers == modifiers) && (keyAction->currentShortcut == symbol) ) {
                    QString err = QString("Cannot bind this shortcut because the following action is already using it: %1")
                                  .arg( it2->item->text(0) );
                    _imp->shortcutEditor->clear();
                    Natron::errorDialog( tr("Shortcut editor").toStdString(), tr( err.toStdString().c_str() ).toStdString() );

                    return;
                }
            }
        }
    }

    selection->setText(1,text);
    action->modifiers = modifiers;
    ka->currentShortcut = symbol;
    appPTR->notifyShortcutChanged(ka);
}

void
ShortCutEditor::onClearButtonClicked()
{
    QList<QTreeWidgetItem*> items = _imp->tree->selectedItems();
    if (items.size() > 1) {
        //we do not support selection of more than 1 item
        return;
    }

    if ( items.empty() ) {
        _imp->shortcutEditor->setText("");
        _imp->shortcutEditor->setPlaceholderText( tr("Type to set shortcut") );

        return;
    }

    QTreeWidgetItem* selection = items.front();
    BoundAction* action = _imp->getActionForTreeItem(selection);
    assert(action);
    action->modifiers = Qt::NoModifier;
    MouseAction* ma = dynamic_cast<MouseAction*>(action);
    KeyBoundAction* ka = dynamic_cast<KeyBoundAction*>(action);
    if (ma) {
        ma->button = Qt::NoButton;
    } else {
        assert(ka);
        ka->currentShortcut = (Qt::Key)0;
    }

    selection->setText(1, "");
    _imp->shortcutEditor->setText("");
    _imp->shortcutEditor->setFocus();
}

void
ShortCutEditor::onResetButtonClicked()
{
    QList<QTreeWidgetItem*> items = _imp->tree->selectedItems();
    if (items.size() > 1) {
        //we do not support selection of more than 1 item
        return;
    }

    if ( items.empty() ) {
        _imp->shortcutEditor->setText("");
        _imp->shortcutEditor->setPlaceholderText( tr("Type to set shortcut") );

        return;
    }

    QTreeWidgetItem* selection = items.front();
    BoundAction* action = _imp->getActionForTreeItem(selection);
    assert(action);
    action->modifiers = action->defaultModifiers;
    KeyBoundAction* ka = dynamic_cast<KeyBoundAction*>(action);
    if (ka) {
        ka->currentShortcut = ka->defaultShortcut;
        appPTR->notifyShortcutChanged(ka);
    }
    setItemShortCutText(selection, action, true);
    _imp->shortcutEditor->setText( makeItemShortCutText(action, true) );
}

void
ShortCutEditor::onRestoreDefaultsButtonClicked()
{
    Natron::StandardButton reply = Natron::questionDialog( tr("Restore defaults").toStdString(), tr("Restoring default shortcuts "
                                                                                                    "will wipe all the current configuration "
                                                                                                    "are you sure you want to do this?").toStdString() );

    if (reply == Natron::Yes) {
        appPTR->restoreDefaultShortcuts();
        for (GuiAppShorcuts::const_iterator it = _imp->appShortcuts.begin(); it != _imp->appShortcuts.end(); ++it) {
            for (std::list<GuiBoundAction>::const_iterator it2 = it->actions.begin(); it2 != it->actions.end(); ++it2) {
                setItemShortCutText(it2->item, it2->action, true);
            }
        }
        _imp->tree->clearSelection();
    }
}

void
ShortCutEditor::onApplyButtonClicked()
{
    appPTR->saveShortcuts();
}

void
ShortCutEditor::onCancelButtonClicked()
{
    close();
}

void
ShortCutEditor::onOkButtonClicked()
{
    appPTR->saveShortcuts();
    close();
}

void
ShortCutEditor::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        close();
    } else {
        QWidget::keyPressEvent(e);
    }
}

void
ShortcutDelegate::paint(QPainter * painter,
                        const QStyleOptionViewItem & option,
                        const QModelIndex & index) const
{
    QTreeWidgetItem* item = tree->itemFromIndex_natron(index);

    if (!item) {
        QStyledItemDelegate::paint(painter, option, index);

        return;
    }

    ///Determine whether the item is top level or not
    bool isTopLevel = false;
    int topLvlCount = tree->topLevelItemCount();
    for (int i = 0; i < topLvlCount; ++i) {
        if (tree->topLevelItem(i) == item) {
            isTopLevel = true;
            break;
        }
    }

    QFont font = painter->font();
    QPen pen;

    if (isTopLevel) {
        font.setBold(true);
        font.setPixelSize(15);
        pen.setColor(Qt::black);
    } else {
        font.setBold(false);
        font.setPixelSize(11);
        if ( item->isDisabled() ) {
            pen.setColor(Qt::black);
        } else {
            pen.setColor( QColor(200,200,200) );
        }
    }
    painter->setFont(font);
    painter->setPen(pen);

    // get the proper subrect from the style
    QStyle *style = QApplication::style();
    QRect geom = style->subElementRect(QStyle::SE_ItemViewItemText, &option);

    ///Draw the item name column
    if (option.state & QStyle::State_Selected) {
        painter->fillRect( geom, option.palette.highlight() );
    }
    QRect r;
    painter->drawText(geom,Qt::TextSingleLine,item->data(index.column(), Qt::DisplayRole).toString(),&r);
} // paint

KeybindRecorder::KeybindRecorder(QWidget* parent)
    : LineEdit(parent)
{
}

KeybindRecorder::~KeybindRecorder()
{
}

void
KeybindRecorder::keyPressEvent(QKeyEvent* e)
{
    if ( !isEnabled() || isReadOnly() ) {
        return;
    }

    QKeySequence *seq;
    if (e->key() == Qt::Key_Control) {
        seq = new QKeySequence(Qt::CTRL);
    } else if (e->key() == Qt::Key_Shift) {
        seq = new QKeySequence(Qt::SHIFT);
    } else if (e->key() == Qt::Key_Meta) {
        seq = new QKeySequence(Qt::META);
    } else if (e->key() == Qt::Key_Alt) {
        seq = new QKeySequence(Qt::ALT);
    } else {
        seq = new QKeySequence( e->key() );
    }

    QString seqStr = seq->toString(QKeySequence::NativeText);
    delete seq;
    QString txt = text()  + seqStr;
    setText(txt);
}

