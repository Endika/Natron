//  Natron
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

#include <csignal>
#include <cstdio>  // perror
#include <cstdlib> // exit

#if defined(Q_OS_UNIX)
#include <sys/signal.h>
#endif

#include <QCoreApplication>

#include "Engine/AppManager.h"

static void setShutDownSignal(int signalId);
static void handleShutDownSignal(int signalId);

int
main(int argc,
     char *argv[])
{
    bool isBackground;
    QString projectName,mainProcessServerName;
    QStringList writers;
    std::list<std::pair<int,int> > frameRanges;
    AppManager::parseCmdLineArgs(argc,argv,&isBackground,projectName,writers,frameRanges,mainProcessServerName);

    setShutDownSignal(SIGINT);   // shut down on ctrl-c
    setShutDownSignal(SIGTERM);   // shut down on killall
#if defined(Q_OS_UNIX)
    projectName = AppManager::qt_tildeExpansion(projectName);
#endif

    ///auto-background without a project name is not valid.
    if ( projectName.isEmpty() ) {
        AppManager::printUsage(argv[0]);

        return 1;
    }
    AppManager manager;

    if ( !manager.load(argc,argv,projectName,writers,frameRanges,mainProcessServerName) ) {
        AppManager::printUsage(argv[0]);

        return 1;
    } else {
        return 0;
    }

    return 0;
} //main

static void
setShutDownSignal(int signalId)
{
#if defined(Q_OS_UNIX)
    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = handleShutDownSignal;
    if (sigaction(signalId, &sa, NULL) == -1) {
        std::perror("setting up termination signal");
        std::exit(1);
    }
#else
    std::signal(signalId, handleShutDownSignal);
#endif
}

static void
handleShutDownSignal( int /*signalId*/ )
{
    QCoreApplication::exit(0);
}

