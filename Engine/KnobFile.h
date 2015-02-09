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

#ifndef NATRON_ENGINE_KNOBFILE_H_
#define NATRON_ENGINE_KNOBFILE_H_

// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>

#include <vector>
#include <map>

#include "Global/Macros.h"
CLANG_DIAG_OFF(deprecated)
#include <QtCore/QObject>
CLANG_DIAG_ON(deprecated)
#include <QtCore/QMutex>
#include <QtCore/QString>

#include "Engine/KnobTypes.h"

#include "Global/Macros.h"

namespace SequenceParsing {
class SequenceFromFiles;
}
/******************************FILE_KNOB**************************************/

class File_Knob
    : public QObject, public AnimatingString_KnobHelper
{
    Q_OBJECT

public:

    static KnobHelper * BuildKnob(KnobHolder* holder,
                                  const std::string &description,
                                  int dimension,
                                  bool declaredByPlugin = true)
    {
        return new File_Knob(holder, description, dimension,declaredByPlugin);
    }

    File_Knob(KnobHolder* holder,
              const std::string &description,
              int dimension,
              bool declaredByPlugin);

    virtual ~File_Knob();

    static const std::string & typeNameStatic();

    void setAsInputImage()
    {
        _isInputImage = true;
    }

    bool isInputImageFile() const
    {
        return _isInputImage;
    }

    /**
     * @brief firstFrame
     * @return Returns the index of the first frame in the sequence held by this Reader.
     */
    int firstFrame() const;

    /**
     * @brief lastFrame
     * @return Returns the index of the last frame in the sequence held by this Reader.
     */
    int lastFrame() const;

    void open_file()
    {
        Q_EMIT openFile();
    }

    /**
     * @brief getRandomFrameName
     * @param f The index of the frame.
     */
    std::string getFileName(int time) const;

Q_SIGNALS:

    void openFile();

private:


    virtual bool canAnimate() const OVERRIDE FINAL;
    virtual const std::string & typeName() const OVERRIDE FINAL;

    int frameCount() const;

    static const std::string _typeNameStr;
    int _isInputImage;
};

/******************************OUTPUT_FILE_KNOB**************************************/

class OutputFile_Knob
    :  public QObject, public Knob<std::string>
{
    Q_OBJECT

public:

    static KnobHelper * BuildKnob(KnobHolder* holder,
                                  const std::string &description,
                                  int dimension,
                                  bool declaredByPlugin = true)
    {
        return new OutputFile_Knob(holder, description, dimension,declaredByPlugin);
    }

    OutputFile_Knob(KnobHolder* holder,
                    const std::string &description,
                    int dimension,
                    bool declaredByPlugin);
    static const std::string & typeNameStatic();

    void setAsOutputImageFile()
    {
        _isOutputImage = true;
    }

    bool isOutputImageFile() const
    {
        return _isOutputImage;
    }

    void open_file()
    {
        Q_EMIT openFile(_sequenceDialog && _isOutputImage);
    }

    void turnOffSequences()
    {
        _sequenceDialog = false;
    }

    bool isSequencesDialogEnabled() const
    {
        return _sequenceDialog;
    }

    QString generateFileNameAtTime(SequenceTime time) const;

Q_SIGNALS:

    void openFile(bool);

private:

    virtual bool canAnimate() const OVERRIDE FINAL;
    virtual const std::string & typeName() const OVERRIDE FINAL;

private:
    static const std::string _typeNameStr;
    bool _isOutputImage;
    bool _sequenceDialog;
};


/******************************PATH_KNOB**************************************/


/**
 * @brief A Path knob could also be called Environment_variable_Knob. 
 * The string is encoded the following way:
 * [VariableName1]:[Value1];[VariableName2]:[Value2] etc...
 * Split all the ';' characters to get all different variables
 * then for each variable split the ':' to get the name and the value of the variable.
 **/
class Path_Knob
    : public Knob<std::string>
{

public:

    static KnobHelper * BuildKnob(KnobHolder* holder,
                                  const std::string &description,
                                  int dimension,
                                  bool declaredByPlugin = true)
    {
        return new Path_Knob(holder, description, dimension,declaredByPlugin);
    }

    Path_Knob(KnobHolder* holder,
              const std::string &description,
              int dimension,
              bool declaredByPlugin);
    static const std::string & typeNameStatic();


    void setMultiPath(bool b);

    bool isMultiPath() const;

    void getPaths(std::list<std::string>* paths) const;
    
    ///Doesn't work if isMultiPath() == false
    void getVariables(std::list<std::pair<std::string,std::string> >* paths) const;
    
    void setPaths(const std::list<std::pair<std::string,std::string> >& paths);
    
    void prependPath(const std::string& path) ;
    void appendPath(const std::string& path) ;

private:
    
    static std::string generateUniquePathID(const std::list<std::pair<std::string,std::string> >& paths);

    virtual bool canAnimate() const OVERRIDE FINAL;
    virtual const std::string & typeName() const OVERRIDE FINAL;

private:
    static const std::string _typeNameStr;
    bool _isMultiPath;
};

#endif // NATRON_ENGINE_KNOBFILE_H_
