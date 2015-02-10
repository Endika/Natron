//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

#include <iostream>
#include <string>
#include <cassert>
#include <QApplication>
#include <QStringList>
#include <QString>
#include <QDir>

#if defined(Q_OS_MAC)
#include "client/mac/crash_generation/crash_generation_server.h"
#elif defined(Q_OS_LINUX)
#include "client/linux/crash_generation/crash_generation_server.h"
#elif defined(Q_OS_WIN32)
#include "client/windows/crash_generation/crash_generation_server.h"
#endif
#include "CrashDialog.h"


using namespace google_breakpad;

/*Converts a std::string to wide string*/
inline std::wstring
s2ws(const std::string & s)
{
    
    
#ifdef Q_OS_WIN32
    int len;
    int slength = (int)s.length() + 1;
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
    wchar_t* buf = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;
    return r;
#else
    std::wstring dest;
    
    size_t max = s.size() * 4;
    mbtowc (NULL, NULL, max);  /* reset mbtowc */
    
    const char* cstr = s.c_str();
    
    while (max > 0) {
        wchar_t w;
        size_t length = mbtowc(&w,cstr,max);
        if (length < 1) {
            break;
        }
        dest.push_back(w);
        cstr += length;
        max -= length;
    }
    return dest;
#endif
    
}


void dumpRequest_xPlatform(const QString& filePath)
{
    CallbacksManager::instance()->s_emitDoCallBackOnMainThread(filePath);
}

#if defined(Q_OS_MAC)
void OnClientDumpRequest(void *context,
                         const ClientInfo &client_info,
                         const std::string &file_path) {

    dumpRequest_xPlatform(file_path.c_str());
}
#elif defined(Q_OS_LINUX)
void OnClientDumpRequest(void* context,
                         const ClientInfo* client_info,
                         const string* file_path) {

    dumpRequest_xPlatform(file_path->c_str());
}
#elif defined(Q_OS_WIN32)
void OnClientDumpRequest(void* context,
                         const google_breakpad::ClientInfo* client_info,
                         const std::wstring* file_path) {

    QString str = QString::fromWCharArray(file_path.c_str());
    dumpRequest_xPlatform(str);
}
#endif


static void printUsage(const char* programName)
{
    CallbacksManager::instance()->writeDebugMessage(QString(programName) + "  <breakpad_pipe> <fd> <natron_init_com_pipe>");
}

int
main(int argc,
     char *argv[])
{

    QApplication app(argc,argv);

    CallbacksManager manager;

    if (argc < 4) {
        manager.writeDebugMessage("Wrong number of arguments.");
        printUsage(app.applicationName().toStdString().c_str());
        return 1;
    }

    QStringList args = app.arguments();
    assert(args.size() == 4);
    QString qPipeName = args[1];
    
    manager.writeDebugMessage("Crash reporter started with following arguments: " + qPipeName + " " + args[2] + " " + args[3]);
    
    QString dumpPath = QDir::tempPath();
#if defined(Q_OS_MAC)
    CrashGenerationServer breakpad_server(qPipeName.toStdString().c_str(),
                                          0, // filter cb
                                          0, // filter ctx
                                          OnClientDumpRequest, // dump cb
                                          0, // dump ctx
                                          0, // exit cb
                                          0, // exit ctx
                                          true, // auto-generate dumps
                                          dumpPath.toStdString()); // path to dump to
#elif defined(Q_OS_LINUX)
    int listenFd = args[2].toInt();
    std::string stdDumpPath = dumpPath.toStdString();
    CrashGenerationServer breakpad_server(listenFd,
                                          OnClientDumpRequest, // dump cb
                                          0, // dump ctx
                                          0, // exit cb
                                          0, // exit ctx
                                          true, // auto-generate dumps
                                          &stdDumpPath); // path to dump to
#elif defined(Q_OS_WIN32)
    std::string pipeName = qPipeName.toStdString();
    std::wstring wpipeName = Natron::s2ws(pipeName);
    std::string stdDumPath = dumpPath.toStdString();
    std::wstring stdWDumpPath = Natron::s2ws(stdDumPath);
    CrashGenerationServer breakpad_server(wpipeName,
                                          0, // SECURITY ATTRS
                                          0, // on client connected cb
                                          0, // on client connected ctx
                                          OnClientDumpRequest, // dump cb
                                          0, // dump ctx
                                          0, // exit cb
                                          0, // exit ctx
                                          0, // upload request cb
                                          0, //  upload request ctx
                                          true, // auto-generate dumps
                                          &stdWDumpPath); // path to dump to
#endif
    if (!breakpad_server.Start()) {
        manager.writeDebugMessage("Failure to start breakpad server, crash generation will fail.");
        return 1;
    } else {
        manager.writeDebugMessage("Crash generation server started successfully.");
    }
    

    manager.initOuptutPipe(args[3]);
    
    int ret = app.exec();
    manager.writeDebugMessage("Exiting now.");
    return ret;
}
