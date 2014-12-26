//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. 
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 */



#ifndef FILESYSTEMMODEL_H
#define FILESYSTEMMODEL_H
#ifndef Q_MOC_RUN
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#endif
#include <QThread>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QDir>

#include "Global/GlobalDefines.h"

namespace SequenceParsing
{
class SequenceFromFiles;
}

class QFileInfo;

struct FileSystemItemPrivate;
class FileSystemItem
{
    
public:

    
    FileSystemItem(bool isDir,
                   const QString& filename,
                   const boost::shared_ptr<SequenceParsing::SequenceFromFiles>& sequence,
                   const QDateTime& dateModified,
                   quint64 size,
                   FileSystemItem* parent = 0);
    
    ~FileSystemItem();
    
    boost::shared_ptr<FileSystemItem> childAt(int position) const;
    
    int childCount() const;
    
    int indexInParent() const;
    
    FileSystemItem* parent() const;
    
    /**
     * @brief If the item is a file this function will return its absolute file-path.
     * If it is a directory this function will return the directory absolute path.
     * This is faster than fileName()
     **/
    const QString& absoluteFilePath() const;
    
    bool isDir() const;
    
    boost::shared_ptr<SequenceParsing::SequenceFromFiles> getSequence() const;
    
    /**
     * @brief Returns the fileName without path.
     * If this is a directory this function returns an empty string.
     **/
    const QString& fileName() const;
    
    const QString& fileExtension() const;
    
    const QDateTime& getLastModified() const;
    
    quint64 getSize() const;
    
    /**
     * @brief Add a new child, MT-safe
     **/
    void addChild(const boost::shared_ptr<FileSystemItem>& child);
    
    /**
     * @brief Remove all children, MT-safe
     **/
    void clearChildren();
    
    /**
     * @brief Tries to find in this item and its children an item with a matching path.
     * @param path The path of the directory/file that has been split by QDir::separator()
     **/
    boost::shared_ptr<FileSystemItem> matchPath(const QStringList& path,
                                                int startIndex = 0) const;
    
    
private:
    
    boost::scoped_ptr<FileSystemItemPrivate> _imp;
    
};

class FileSystemModel;
struct FileGathererThreadPrivate;
class FileGathererThread : public QThread
{
    Q_OBJECT
    
public:
    
    FileGathererThread(FileSystemModel* model);
    
    virtual ~FileGathererThread();
    
    void abortGathering();
    
    void quitGatherer();
    
    void fetchDirectory(const boost::shared_ptr<FileSystemItem>& item);
    
    bool isWorking() const;
signals:
    
    void directoryLoaded(QString);
    

private:
    
    virtual void run() OVERRIDE FINAL;
    
    void gatheringKernel(const boost::shared_ptr<FileSystemItem>& item);
    
    boost::scoped_ptr<FileGathererThreadPrivate> _imp;
    
};


class SortableViewI {
    
public:
    
    SortableViewI() {}
    
    virtual ~SortableViewI() {}
    
    /**
     * @brief Returns the order for the sort indicator. If no section has a sort indicator the return value of this function is undefined.
     **/
    virtual Qt::SortOrder sortIndicatorOrder() const = 0;
    
    /**
     * @brief Returns the logical index of the section that has a sort indicator. By default this is section 0.
     **/
    virtual int	sortIndicatorSection() const = 0;
    
    /**
     * @brief Called when the section containing the sort indicator or the order indicated is changed. 
     * The section's logical index is specified by logicalIndex and the sort order is specified by order.
     **/
    virtual void onSortIndicatorChanged(int logicalIndex,Qt::SortOrder order) = 0;
};

struct FileSystemModelPrivate;
class FileSystemModel : public QAbstractItemModel
{
    
    Q_OBJECT
    
public:
    
    enum Roles {
        FilePathRole = Qt::UserRole + 1,
        FileNameRole = Qt::UserRole + 2,
    };
    
    enum Sections {
        Name = 0,
        Size,
        Type,
        DateModified,
        EndSections //do not use
    };
    
    FileSystemModel(SortableViewI* view);
    
    ///////////////////////////////////////Overriden from QAbstractItemModel///////////////////////////////////////
    
    virtual ~FileSystemModel();

	static bool isDriveName(const QString& name);
    
    virtual QVariant headerData(int section, Qt::Orientation orientation,int role) const OVERRIDE FINAL WARN_UNUSED_RETURN;
    
    virtual Qt::ItemFlags flags(const QModelIndex &index) const OVERRIDE FINAL WARN_UNUSED_RETURN;
    
    virtual int columnCount(const QModelIndex & parent) const OVERRIDE FINAL WARN_UNUSED_RETURN;
    
    virtual int rowCount(const QModelIndex & parent) const OVERRIDE FINAL WARN_UNUSED_RETURN;
    
    virtual QVariant data(const QModelIndex &index, int role) const OVERRIDE FINAL WARN_UNUSED_RETURN;
    
    virtual QModelIndex index(int row, int column, const QModelIndex &parent) const OVERRIDE FINAL WARN_UNUSED_RETURN;
    
    virtual QModelIndex parent(const QModelIndex &index) const OVERRIDE FINAL WARN_UNUSED_RETURN;
    
    virtual QStringList mimeTypes() const OVERRIDE FINAL WARN_UNUSED_RETURN;
    
    virtual QMimeData* mimeData(const QModelIndexList & indexes) const OVERRIDE FINAL WARN_UNUSED_RETURN;
    
    /////////////////////////////////////// End overrides ////////////////////////////////////////////////////
    
    QModelIndex index(const QString& path, int column = 0) const WARN_UNUSED_RETURN;
    
    boost::shared_ptr<FileSystemItem> getFileSystemItem(const QString& path) const WARN_UNUSED_RETURN;
    FileSystemItem* getFileSystemItem(const QModelIndex& index) const WARN_UNUSED_RETURN;
    
    QModelIndex index(FileSystemItem* item, int column = 0) const WARN_UNUSED_RETURN;
    
    bool isDir(const QModelIndex &index) WARN_UNUSED_RETURN;
    
    // absoluteFilePath method of QFileInfo class cause the file system
    // query hence causes slower performance.
    QString absolutePath(const QModelIndex &index) const WARN_UNUSED_RETURN;
    
    QString rootPath() const WARN_UNUSED_RETURN;
        
    /**
     * @brief Set the root path of the filesystem to the given path. This will force it to load the directory
     * and its content will then be accessible via index(...) and iterating through rowCount().
     * You may only use these methods once the directoryLoaded signal is sent, indicating that the worker thread
     * has gathered all info.
     **/
    void setRootPath(const QString& path);
    
    QVariant myComputer(int role = Qt::DisplayRole) const WARN_UNUSED_RETURN;
    
    void setFilter(const QDir::Filters& filters);
    
    const QDir::Filters filter() const WARN_UNUSED_RETURN;
    
    /**
     * @brief Set regexp filters (in the wildcard unix form).
     * @param filters A suite of regexp separated by a space
     **/
    void setRegexpFilters(const QString& filters);
    
    /**
     * @brief Generates an encoded regexp filter that can be passed to setRegexpFilters from a list of file extensions
     * @extensions A list of file extensions in the form "jpg", "png", "exr" etc...
     **/
    static QString generateRegexpFilterFromFileExtensions(const QStringList& extensions) WARN_UNUSED_RETURN;
    
    /*
     * @brief Check if the path is accepted by the filter installed by the user
     */
    bool isAcceptedByRegexps(const QString & path) const WARN_UNUSED_RETURN;
    
    /**
     * @brief Set the file-system in sequence mode: it will try to recnognize file sequences instead of only separate files/directory.
     * By default this is disabled.
     **/
    void setSequenceModeEnabled(bool sequenceMode);
    
    /**
     * @brief Returns true if sequence mode is enabled
     **/
    bool isSequenceModeEnabled() const;
    
    /*
     * @brief Make a new directory
     */
    QModelIndex mkdir(const QModelIndex& parent,const QString& name) WARN_UNUSED_RETURN;

    Qt::SortOrder sortIndicatorOrder() const WARN_UNUSED_RETURN;
    
    int	sortIndicatorSection() const WARN_UNUSED_RETURN;
    
    void onSortIndicatorChanged(int logicalIndex,Qt::SortOrder order);
    
public slots:
    
    void onDirectoryLoadedByGatherer(const QString& directory);
    
    void onWatchedDirectoryChanged(const QString& directory);
    
    void onWatchedFileChanged(const QString& file);
    
signals:
    
    void rootPathChanged(QString);
    
    void directoryLoaded(QString);
    
private:
    
    void cleanAndRefreshItem(const boost::shared_ptr<FileSystemItem>& item);
    
    void resetCompletly();
    
    boost::scoped_ptr<FileSystemModelPrivate> _imp;
};

#endif // FILESYSTEMMODEL_H
