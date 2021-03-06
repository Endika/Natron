//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

#ifndef PROJECTPRIVATE_H
#define PROJECTPRIVATE_H

#include <map>
#include <list>
#include "Global/Macros.h"
CLANG_DIAG_OFF(deprecated)
CLANG_DIAG_OFF(uninitialized)
#include <QDateTime>
#include <QString>
#include <QFuture>
#include <QFutureWatcher>
#include <QMutex>
CLANG_DIAG_ON(deprecated)
CLANG_DIAG_ON(uninitialized)


#include "Engine/Format.h"
#include "Engine/KnobTypes.h"
#include "Engine/KnobFile.h"
#include "Engine/KnobFactory.h"

class QTimer;
class TimeLine;
class NodeSerialization;
class ProjectSerialization;
class File_Knob;
namespace Natron {
class Node;
class OutputEffectInstance;
class Project;

inline QString
generateStringFromFormat(const Format & f)
{
    QString formatStr;

    formatStr.append(f.getName().c_str());
    formatStr.append("  ");
    formatStr.append(QString::number(f.width()));
    formatStr.append(" x ");
    formatStr.append(QString::number(f.height()));
    formatStr.append("  ");
    formatStr.append(QString::number(f.getPixelAspectRatio()));

    return formatStr;
}

struct ProjectPrivate
{
    Natron::Project* _publicInterface;
    mutable QMutex projectLock; //< protects the whole project
    QString projectName; //< name of the project, e.g: "Untitled.EXT"
    QString projectPath; //< path of the project, e.g: /Users/Lala/Projects/
    QString lastAutoSaveFilePath; //< path + name of the last auto-save file
    bool hasProjectBeenSavedByUser; //< has this project ever been saved by the user?
    QDateTime ageSinceLastSave; //< the last time the user saved
    QDateTime lastAutoSave; //< the last time since autosave
    QDateTime projectCreationTime; //< the project creation time
    
    std::list<Format> builtinFormats;
    std::list<Format> additionalFormats; //< added by the user
    mutable QMutex formatMutex; //< protects builtinFormats & additionalFormats
    

    ///Project parameters (settings)
    boost::shared_ptr<Path_Knob> envVars;
    boost::shared_ptr<Choice_Knob> formatKnob; //< built from builtinFormats & additionalFormats
    boost::shared_ptr<Button_Knob> addFormatKnob;
    boost::shared_ptr<Int_Knob> viewsCount;
    boost::shared_ptr<Int_Knob> mainView;
    boost::shared_ptr<Bool_Knob> previewMode; //< auto or manual
    boost::shared_ptr<Choice_Knob> colorSpace8u;
    boost::shared_ptr<Choice_Knob> colorSpace16u;
    boost::shared_ptr<Choice_Knob> colorSpace32f;
    boost::shared_ptr<Double_Knob> frameRate;
    boost::shared_ptr<Int_Knob> frameRange;
    boost::shared_ptr<Bool_Knob> lockFrameRange;
    
    boost::shared_ptr<String_Knob> natronVersion;
    boost::shared_ptr<String_Knob> originalAuthorName,lastAuthorName;
    boost::shared_ptr<String_Knob> projectCreationDate;
    boost::shared_ptr<String_Knob> saveDate;
    
    boost::shared_ptr<TimeLine> timeline; // global timeline
    mutable QMutex nodesLock; //< protects nodeCounters & currentNodes
    bool autoSetProjectFormat;
    std::vector< boost::shared_ptr<Natron::Node> > currentNodes;
    Natron::Project* project;
    mutable QMutex isLoadingProjectMutex;
    bool isLoadingProject; //< true when the project is loading
    bool isLoadingProjectInternal; //< true when loading the internal project (not gui)
    mutable QMutex isSavingProjectMutex;
    bool isSavingProject; //< true when the project is saving
    boost::shared_ptr<QTimer> autoSaveTimer;
    std::list<boost::shared_ptr<QFutureWatcher<void> > > autoSaveFutures;

    
    ProjectPrivate(Natron::Project* project);

    bool restoreFromSerialization(const ProjectSerialization & obj,const QString& name,const QString& path,bool isAutoSave,const QString& realFilePath);

    bool findFormat(int index,Format* format) const;
    
    /**
     * @brief Auto fills the project directory parameter given the project file path
     **/
    void autoSetProjectDirectory(const QString& path);
};
}

#endif // PROJECTPRIVATE_H
