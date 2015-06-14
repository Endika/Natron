#include "DopeSheetEditorUndoRedo.h"

#include <QDebug> //REMOVEME

#include "Engine/Knob.h"
#include "Engine/Node.h"
#include "Engine/NodeGroup.h"
#include "Engine/NoOp.h"
#include <Engine/ViewerInstance.h>

#include "Global/GlobalDefines.h"

#include "Gui/DockablePanel.h"
#include "Gui/DopeSheet.h"
#include "Gui/KnobGui.h"
#include "Gui/NodeGui.h"


typedef std::map<boost::weak_ptr<KnobI>, KnobGui *> KnobsAndGuis;


////////////////////////// Helpers //////////////////////////

namespace {

static bool dsSelectedKeyLessFunctor(const DSKeyPtr &left,
                                     const DSKeyPtr &right)
{
    return left->key.getTime() < right->key.getTime();
}

void renderOnce(Natron::EffectInstance *effectInstance)
{
    assert(effectInstance);

    std::set<ViewerInstance *> toRender;

    std::list<ViewerInstance *> connectedViewers;
    effectInstance->getNode()->hasViewersConnected(&connectedViewers);

    toRender.insert(connectedViewers.begin(), connectedViewers.end());

    for (std::set<ViewerInstance *>::const_iterator viIt = toRender.begin(); viIt != toRender.end(); ++viIt) {
        ViewerInstance *viewer = (*viIt);

        viewer->renderCurrentFrame(true);
    }
}

void renderOnce(std::set<KnobHolder *> *holders)
{
    std::set<ViewerInstance *> toRender;

    for (std::set<KnobHolder *>::iterator khIt = holders->begin(); khIt != holders->end(); ++khIt) {
        Natron::EffectInstance *effectInstance = dynamic_cast<Natron::EffectInstance *>(*khIt);

        effectInstance->endChanges(true);

        std::list<ViewerInstance *> connectedViewers;

        effectInstance->getNode()->hasViewersConnected(&connectedViewers);

        toRender.insert(connectedViewers.begin(), connectedViewers.end());
    }

    for (std::set<ViewerInstance *>::const_iterator viIt = toRender.begin(); viIt != toRender.end(); ++viIt) {
        ViewerInstance *viewer = (*viIt);

        viewer->renderCurrentFrame(true);
    }
}

} // anon namespace


////////////////////////// DSMoveKeysCommand //////////////////////////

DSMoveKeysCommand::DSMoveKeysCommand(const DSKeyPtrList &keys,
                                     double dt,
                                     DopeSheet *model,
                                     QUndoCommand *parent) :
    QUndoCommand(parent),
    _keys(keys),
    _dt(dt),
    _model(model)
{
    setText(QObject::tr("Move selected keys"));

    _keys.sort(dsSelectedKeyLessFunctor);
}

void DSMoveKeysCommand::undo()
{
    moveSelectedKeyframes(-_dt);
}

void DSMoveKeysCommand::redo()
{
    moveSelectedKeyframes(_dt);
}

void DSMoveKeysCommand::moveSelectedKeyframes(double dt)
{
    std::set<KnobHolder *> knobHolders;

    for (DSKeyPtrList::iterator it = _keys.begin(); it != _keys.end(); ++it) {
        DSKeyPtr selectedKey = (*it);

        KnobHolder *holder = selectedKey->context->getKnobGui()->getKnob()->getHolder();

        knobHolders.insert(holder);
    }

    for (std::set<KnobHolder *>::iterator khIt = knobHolders.begin(); khIt != knobHolders.end(); ++khIt) {
        Natron::EffectInstance *effectInstance = dynamic_cast<Natron::EffectInstance *>(*khIt);

        effectInstance->beginChanges();
    }

    for (DSKeyPtrList::iterator it = _keys.begin(); it != _keys.end(); ++it) {
        DSKeyPtr selectedKey = (*it);

        boost::shared_ptr<KnobI> knob = selectedKey->context->getKnobGui()->getKnob();

        knob->moveValueAtTime(selectedKey->key.getTime(),
                              selectedKey->context->getDimension(),
                              dt, 0, &selectedKey->key);
    }

    renderOnce(&knobHolders);

    _model->getSelectionModel()->emit_keyframeSelectionChanged();
}

int DSMoveKeysCommand::id() const
{
    return kDopeSheetEditorMoveKeysCommandCompressionID;
}

bool DSMoveKeysCommand::mergeWith(const QUndoCommand *other)
{
    const DSMoveKeysCommand *cmd = dynamic_cast<const DSMoveKeysCommand *>(other);

    if (cmd->id() != id()) {
        return false;
    }

    if (cmd->_keys.size() != _keys.size()) {
        return false;
    }

    DSKeyPtrList::const_iterator itOther = cmd->_keys.begin();

    for (DSKeyPtrList::const_iterator it = _keys.begin(); it != _keys.end(); ++it, ++itOther) {
        if (*itOther != *it) {
            return false;
        }
    }

    _dt += cmd->_dt;

    return true;
}


////////////////////////// DSLeftTrimReaderCommand //////////////////////////

DSLeftTrimReaderCommand::DSLeftTrimReaderCommand(DSNode *dsNodeReader,
                                                 double oldTime,
                                                 double newTime,
                                                 DopeSheet *model,
                                                 QUndoCommand *parent) :
    QUndoCommand(parent),
    _dsNodeReader(dsNodeReader),
    _oldTime(oldTime),
    _newTime(newTime),
    _model(model)
{
    setText(QObject::tr("Trim left"));
}

void DSLeftTrimReaderCommand::undo()
{
    trimLeft(_oldTime);
}

void DSLeftTrimReaderCommand::redo()
{
    trimLeft(_newTime);
}

void DSLeftTrimReaderCommand::trimLeft(double firstFrame)
{
    Knob<int> *firstFrameKnob = dynamic_cast<Knob<int> *>(_dsNodeReader->getInternalNode()->getKnobByName("firstFrame").get());
    assert(firstFrameKnob);

    KnobHolder *holder = firstFrameKnob->getHolder();
    Natron::EffectInstance *effectInstance = dynamic_cast<Natron::EffectInstance *>(holder);

    effectInstance->beginChanges();
    KnobHelper::ValueChangedReturnCodeEnum r = firstFrameKnob->setValue(firstFrame, 0, Natron::eValueChangedReasonNatronGuiEdited, 0);
    effectInstance->endChanges(true);

    renderOnce(effectInstance);

    Q_UNUSED(r);
}

int DSLeftTrimReaderCommand::id() const
{
    return kDopeSheetEditorLeftTrimCommandCompressionID;
}

bool DSLeftTrimReaderCommand::mergeWith(const QUndoCommand *other)
{
    const DSLeftTrimReaderCommand *cmd = dynamic_cast<const DSLeftTrimReaderCommand *>(other);

    if (cmd->id() != id()) {
        return false;
    }

    if (cmd->_dsNodeReader != _dsNodeReader) {
        return false;
    }

    _newTime = cmd->_oldTime;

    return true;
}


////////////////////////// DSRightTrimReaderCommand //////////////////////////

DSRightTrimReaderCommand::DSRightTrimReaderCommand(DSNode *dsNodeReader,
                                                   double oldTime, double newTime,
                                                   DopeSheet *model,
                                                   QUndoCommand *parent) :
    QUndoCommand(parent),
    _dsNodeReader(dsNodeReader),
    _oldTime(oldTime),
    _newTime(newTime),
    _model(model)
{
    setText(QObject::tr("Trim right"));
}

void DSRightTrimReaderCommand::undo()
{
    trimRight(_oldTime);
}

void DSRightTrimReaderCommand::redo()
{
    trimRight(_newTime);
}

void DSRightTrimReaderCommand::trimRight(double lastFrame)
{
    Knob<int> *lastFrameKnob = dynamic_cast<Knob<int> *>(_dsNodeReader->getInternalNode()->getKnobByName("lastFrame").get());
    assert(lastFrameKnob);

    KnobHolder *holder = lastFrameKnob->getHolder();
    Natron::EffectInstance *effectInstance = dynamic_cast<Natron::EffectInstance *>(holder);

    effectInstance->beginChanges();
    KnobHelper::ValueChangedReturnCodeEnum r = lastFrameKnob->setValue(lastFrame, 0, Natron::eValueChangedReasonNatronGuiEdited, 0);
    effectInstance->endChanges(true);

    renderOnce(effectInstance);

    Q_UNUSED(r);
}

int DSRightTrimReaderCommand::id() const
{
    return kDopeSheetEditorRightTrimCommandCompressionID;
}

bool DSRightTrimReaderCommand::mergeWith(const QUndoCommand *other)
{
    const DSRightTrimReaderCommand *cmd = dynamic_cast<const DSRightTrimReaderCommand *>(other);

    if (cmd->id() != id()) {
        return false;
    }

    if (cmd->_dsNodeReader != _dsNodeReader) {
        return false;
    }

    _newTime = cmd->_oldTime;

    return true;
}


////////////////////////// DSSlipReaderCommand //////////////////////////

DSSlipReaderCommand::DSSlipReaderCommand(DSNode *dsNodeReader,
                                         double dt,
                                         DopeSheet *model,
                                         QUndoCommand *parent) :
    QUndoCommand(parent),
    _dsNodeReader(dsNodeReader),
    _dt(dt),
    _model(model)
{
    setText(QObject::tr("Slip reader"));
}

void DSSlipReaderCommand::undo()
{
    slipReader(-_dt);
}

void DSSlipReaderCommand::redo()
{
    slipReader(_dt);
}

int DSSlipReaderCommand::id() const
{
    return kDopeSheetEditorSlipReaderCommandCompressionID;
}

bool DSSlipReaderCommand::mergeWith(const QUndoCommand *other)
{
    const DSSlipReaderCommand *cmd = dynamic_cast<const DSSlipReaderCommand *>(other);

    if (cmd->id() != id()) {
        return false;
    }

    if (cmd->_dsNodeReader != _dsNodeReader) {
        return false;
    }

    _dt += cmd->_dt;

    return true;
}

void DSSlipReaderCommand::slipReader(double dt)
{
    NodePtr node = _dsNodeReader->getInternalNode();

    Knob<int> *firstFrameKnob = dynamic_cast<Knob<int> *>(node->getKnobByName("firstFrame").get());
    assert(firstFrameKnob);
    Knob<int> *lastFrameKnob = dynamic_cast<Knob<int> *>(node->getKnobByName("lastFrame").get());
    assert(lastFrameKnob);
    Knob<int> *startingTimeKnob = dynamic_cast<Knob<int> *>(node->getKnobByName("timeOffset").get());
    assert(startingTimeKnob);

    KnobHolder *holder = lastFrameKnob->getHolder();
    Natron::EffectInstance *effectInstance = dynamic_cast<Natron::EffectInstance *>(holder);

    effectInstance->beginChanges();
    {
        KnobHelper::ValueChangedReturnCodeEnum r;
        int oldStartingTime = startingTimeKnob->getValue();

        r = firstFrameKnob->setValue(firstFrameKnob->getValue() + dt, 0, Natron::eValueChangedReasonNatronGuiEdited, 0);
        r = lastFrameKnob->setValue(lastFrameKnob->getValue() + dt, 0, Natron::eValueChangedReasonNatronGuiEdited, 0);

        r = startingTimeKnob->setValue(oldStartingTime, 0, Natron::eValueChangedReasonNatronGuiEdited, 0);

        Q_UNUSED(r);

    }
    effectInstance->endChanges(true);

    renderOnce(effectInstance);
}


////////////////////////// DSMoveReaderCommand //////////////////////////

DSMoveReaderCommand::DSMoveReaderCommand(DSNode *dsNodeReader,
                                         double oldTime,
                                         double newTime,
                                         DopeSheet *model,
                                         QUndoCommand *parent) :
    QUndoCommand(parent),
    _dsNodeReader(dsNodeReader),
    _oldTime(oldTime),
    _newTime(newTime),
    _model(model)
{
    setText(QObject::tr("Move reader"));
}

void DSMoveReaderCommand::undo()
{
    moveReader(_oldTime);
}

void DSMoveReaderCommand::redo()
{
    moveReader(_newTime);
}

void DSMoveReaderCommand::moveReader(double time)
{
    Knob<int> *timeOffsetKnob = dynamic_cast<Knob<int> *>(_dsNodeReader->getInternalNode()->getKnobByName("timeOffset").get());
    assert(timeOffsetKnob);

    KnobHolder *holder = timeOffsetKnob->getHolder();
    Natron::EffectInstance *effectInstance = dynamic_cast<Natron::EffectInstance *>(holder);

    effectInstance->beginChanges();
    KnobHelper::ValueChangedReturnCodeEnum r = timeOffsetKnob->setValue(time, 0, Natron::eValueChangedReasonNatronGuiEdited, 0);
    effectInstance->endChanges(true);

    Q_UNUSED(r);

    renderOnce(effectInstance);
}

int DSMoveReaderCommand::id() const
{
    return kDopeSheetEditorMoveClipCommandCompressionID;
}

bool DSMoveReaderCommand::mergeWith(const QUndoCommand *other)
{
    const DSMoveReaderCommand *cmd = dynamic_cast<const DSMoveReaderCommand *>(other);

    if (cmd->id() != id()) {
        return false;
    }

    if (cmd->_dsNodeReader != _dsNodeReader) {
        return false;
    }

    _newTime = cmd->_oldTime;

    return true;
}


////////////////////////// DSRemoveKeysCommand //////////////////////////

DSRemoveKeysCommand::DSRemoveKeysCommand(const std::vector<DSSelectedKey> &keys,
                                         DopeSheet *model,
                                         QUndoCommand *parent) :
    QUndoCommand(parent),
    _keys(keys),
    _model(model)
{
    setText(QObject::tr("Delete selected keyframes"));
}

void DSRemoveKeysCommand::undo()
{
    addOrRemoveKeyframe(true);
}

void DSRemoveKeysCommand::redo()
{
    addOrRemoveKeyframe(false);
}

void DSRemoveKeysCommand::addOrRemoveKeyframe(bool add)
{
    for (std::vector<DSSelectedKey>::iterator it = _keys.begin(); it != _keys.end(); ++it) {
        DSSelectedKey selected = (*it);
        KnobGui *knobGui = selected.context->getKnobGui();

        if (add) {
            knobGui->setKeyframe(selected.key.getTime(), selected.key, selected.context->getDimension());
        }
        else {
            knobGui->removeKeyFrame(selected.key.getTime(), selected.context->getDimension());
            selected.context->getTreeItem()->setSelected(false);
        }
    }

    _model->getSelectionModel()->emit_keyframeSelectionChanged();
}


////////////////////////// DSMoveGroupCommand //////////////////////////

DSMoveGroupCommand::DSMoveGroupCommand(DSNode *dsNodeGroup, double dt, DopeSheet *model, QUndoCommand *parent) :
    QUndoCommand(parent),
    _dsNodeGroup(dsNodeGroup),
    _dt(dt),
    _model(model)
{
    setText(QObject::tr("Move Group Keyframes"));
}

void DSMoveGroupCommand::undo()
{
    moveGroupKeyframes(-_dt);
}

void DSMoveGroupCommand::redo()
{
    moveGroupKeyframes(_dt);
}

int DSMoveGroupCommand::id() const
{
    return kDopeSheetEditorMoveGroupCommandCompressionID;
}

bool DSMoveGroupCommand::mergeWith(const QUndoCommand *other)
{
    const DSMoveGroupCommand *cmd = dynamic_cast<const DSMoveGroupCommand *>(other);

    if (cmd->id() != id()) {
        return false;
    }

    if (cmd->_dsNodeGroup != _dsNodeGroup) {
        return false;
    }

    _dt += cmd->_dt;

    return true;
}

void DSMoveGroupCommand::moveGroupKeyframes(double dt)
{
    NodeGroup *group = dynamic_cast<NodeGroup *>(_dsNodeGroup->getInternalNode()->getLiveInstance());
    NodeList nodes = group->getNodes();

    std::set<KnobHolder *> knobHolders;

    for (NodeList::iterator it = nodes.begin(); it != nodes.end(); ++it) {
        boost::shared_ptr<NodeGui> nodeGui = boost::dynamic_pointer_cast<NodeGui>((*it)->getNodeGui());

        if (dynamic_cast<GroupInput *>(nodeGui->getNode()->getLiveInstance()) ||
                dynamic_cast<GroupOutput *>(nodeGui->getNode()->getLiveInstance())) {
            continue;
        }

        if (!nodeGui->getSettingPanel() || !nodeGui->getSettingPanel()->isVisible()) {
            continue;
        }

        const KnobsAndGuis &knobs = nodeGui->getKnobs();

        for (KnobsAndGuis::const_iterator knobIt = knobs.begin(); knobIt != knobs.end(); ++knobIt) {
            KnobGui *knobGui = (*knobIt).second;
            boost::shared_ptr<KnobI> knob = knobGui->getKnob();

            if (!knob->hasAnimation()) {
                continue;
            }

            KnobHolder *holder = knob->getHolder();
            knobHolders.insert(holder);

            Natron::EffectInstance *effectInstance = dynamic_cast<Natron::EffectInstance *>(holder);
            effectInstance->beginChanges();

            for (int dim = 0; dim < knobGui->getKnob()->getDimension(); ++dim) {
                if (!knob->isAnimated(dim)) {
                    continue;
                }

                KeyFrameSet keyframes = knobGui->getCurve(dim)->getKeyFrames_mt_safe();

                for (KeyFrameSet::iterator kfIt = keyframes.begin(); kfIt != keyframes.end(); ++kfIt) {
                    KeyFrame kf = (*kfIt);

                    KeyFrame fake;

                    knob->moveValueAtTime(kf.getTime(), dim, dt, 0, &fake);
                }
            }
        }
    }

    renderOnce(&knobHolders);

    _model->getSelectionModel()->clearKeyframeSelection();
    _model->emit_modelChanged();
}


////////////////////////// DSChangeNodeLabel //////////////////////////

DSChangeNodeLabelCommand::DSChangeNodeLabelCommand(DSNode *dsNode,
                                                   const QString &oldLabel,
                                                   const QString &newLabel,
                                                   QUndoCommand *parent) :
    QUndoCommand(parent),
    _dsNode(dsNode),
    _oldLabel(oldLabel),
    _newLabel(newLabel)
{
    setText(QObject::tr("Change node label"));
}

void DSChangeNodeLabelCommand::undo()
{
    changeNodeLabel(_oldLabel);
}

void DSChangeNodeLabelCommand::redo()
{
    changeNodeLabel(_newLabel);
}

void DSChangeNodeLabelCommand::changeNodeLabel(const QString &label)
{
    _dsNode->getInternalNode()->setLabel(label.toStdString());
    _dsNode->getTreeItem()->setText(0, label);
}


////////////////////////// DSSetSelectedKeysInterpolationCommand //////////////////////////

DSSetSelectedKeysInterpolationCommand::DSSetSelectedKeysInterpolationCommand(const std::list<DSKeyInterpolationChange> &changes,
                                                                             DopeSheet *model,
                                                                             QUndoCommand *parent) :
    QUndoCommand(parent),
    _changes(changes),
    _model(model)
{
    setText(QObject::tr("Set selected keys interpolation"));
}

void DSSetSelectedKeysInterpolationCommand::undo()
{
    setInterpolation(true);
}

void DSSetSelectedKeysInterpolationCommand::redo()
{
    setInterpolation(false);
}

void DSSetSelectedKeysInterpolationCommand::setInterpolation(bool undo)
{
    for (std::list<DSKeyInterpolationChange>::iterator it = _changes.begin(); it != _changes.end(); ++it) {
        Natron::KeyframeTypeEnum interp = undo ? it->_oldInterpType : it->_newInterpType;

        it->_key->context->getKnobGui()->getKnob()->setInterpolationAtTime(it->_key->context->getDimension(),
                                                                          it->_key->key.getTime(),
                                                                          interp,
                                                                          &it->_key->key);
    }

    _model->emit_modelChanged();
}


////////////////////////// DSAddKeysCommand //////////////////////////

DSPasteKeysCommand::DSPasteKeysCommand(const std::vector<DSSelectedKey> &keys,
                                       DopeSheet *model,
                                       QUndoCommand *parent) :
    QUndoCommand(parent),
    _keys(keys),
    _model(model)
{
    setText(QObject::tr("Paste keyframes"));
}

void DSPasteKeysCommand::undo()
{
    addOrRemoveKeyframe(false);
}

void DSPasteKeysCommand::redo()
{
    addOrRemoveKeyframe(true);
}

void DSPasteKeysCommand::addOrRemoveKeyframe(bool add)
{
    for (std::vector<DSSelectedKey>::const_iterator it = _keys.begin(); it != _keys.end(); ++it) {
        DSSelectedKey key = (*it);

        boost::shared_ptr<KnobI> knob = key.context->getInternalKnob();
        knob->beginChanges();

        SequenceTime currentTime = _model->getCurrentFrame();

        double keyTime = key.key.getTime();

        if (add) {
            Knob<double>* isDouble = dynamic_cast<Knob<double>*>(knob.get());
            Knob<bool>* isBool = dynamic_cast<Knob<bool>*>(knob.get());
            Knob<int>* isInt = dynamic_cast<Knob<int>*>(knob.get());
            Knob<std::string>* isString = dynamic_cast<Knob<std::string>*>(knob.get());

            if (isDouble) {
                isDouble->setValueAtTime(currentTime, isDouble->getValueAtTime(keyTime), key.context->getDimension());
            } else if (isBool) {
                isBool->setValueAtTime(currentTime, isBool->getValueAtTime(keyTime), key.context->getDimension());
            } else if (isInt) {
                isInt->setValueAtTime(currentTime, isInt->getValueAtTime(keyTime), key.context->getDimension());
            } else if (isString) {
                isString->setValueAtTime(currentTime, isString->getValueAtTime(keyTime), key.context->getDimension());
            }
        }
        else {
            knob->deleteValueAtTime(currentTime, key.context->getDimension());
        }

        knob->endChanges();
    }

    _model->emit_modelChanged();
}
