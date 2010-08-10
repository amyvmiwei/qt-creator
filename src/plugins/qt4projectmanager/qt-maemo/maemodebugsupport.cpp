/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the Qt Creator.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "maemodebugsupport.h"

#include "maemodeployables.h"
#include "maemodeploystep.h"
#include "maemoglobal.h"
#include "maemorunconfiguration.h"
#include "maemosshrunner.h"

#include <coreplugin/ssh/sftpchannel.h>
#include <debugger/debuggerengine.h>
#include <debugger/debuggerplugin.h>
#include <debugger/debuggerrunner.h>
#include <debugger/gdb/remotegdbserveradapter.h>
#include <debugger/gdb/remoteplaingdbadapter.h>
#include <projectexplorer/toolchain.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

using namespace Core;
using namespace Debugger;
using namespace Debugger::Internal;
using namespace ProjectExplorer;

namespace Qt4ProjectManager {
namespace Internal {

RunControl *MaemoDebugSupport::createDebugRunControl(MaemoRunConfiguration *runConfig)
{
    DebuggerStartParameters params;
    const MaemoDeviceConfig &devConf = runConfig->deviceConfig();
    if (runConfig->useRemoteGdb()) {
        params.startMode = StartRemoteGdb;
        params.executable = runConfig->remoteExecutableFilePath();
        params.debuggerCommand
            = MaemoGlobal::remoteCommandPrefix(runConfig->remoteExecutableFilePath())
                + QLatin1String(" /usr/bin/gdb");
        params.connParams = devConf.server;
        params.localMountDir = runConfig->localDirToMountForRemoteGdb();
        params.remoteMountPoint = MaemoGlobal::remoteProjectSourcesMountPoint();
        const QString execDirAbs
            = QDir::fromNativeSeparators(QFileInfo(runConfig->localExecutableFilePath()).path());
        const QString execDirRel
            = QDir(params.localMountDir).relativeFilePath(execDirAbs);
        params.remoteSourcesDir = QString(params.remoteMountPoint
            + QLatin1Char('/') + execDirRel).toUtf8();
    } else {
        params.startMode = AttachToRemote;
        params.executable = runConfig->localExecutableFilePath();
        params.debuggerCommand = runConfig->gdbCmd();
        params.remoteChannel = devConf.server.host + QLatin1Char(':')
            + gdbServerPort(runConfig, devConf);
        params.remoteArchitecture = QLatin1String("arm");
    }
    params.processArgs = runConfig->arguments();
    params.sysRoot = runConfig->sysRoot();
    params.toolChainType = ToolChain::GCC_MAEMO;
    params.dumperLibrary = runConfig->dumperLib();
    params.remoteDumperLib = uploadDir(devConf).toUtf8() + '/'
        + QFileInfo(runConfig->dumperLib()).fileName().toUtf8();

    DebuggerRunControl * const debuggerRunControl
        = DebuggerPlugin::createDebugger(params, runConfig);
    new MaemoDebugSupport(runConfig, debuggerRunControl);
    return debuggerRunControl;
}

MaemoDebugSupport::MaemoDebugSupport(MaemoRunConfiguration *runConfig,
    DebuggerRunControl *runControl)
    : QObject(runControl), m_runControl(runControl), m_runConfig(runConfig),
      m_deviceConfig(m_runConfig->deviceConfig()),
      m_runner(new MaemoSshRunner(this, m_runConfig, true))
{
    GdbEngine *engine = qobject_cast<GdbEngine *>(m_runControl->engine());
    Q_ASSERT(engine);
    m_gdbAdapter = engine->gdbAdapter();
    Q_ASSERT(m_gdbAdapter);
    connect(m_gdbAdapter, SIGNAL(requestSetup()), this,
        SLOT(handleAdapterSetupRequested()));
    connect(m_runControl, SIGNAL(finished()), this,
        SLOT(handleDebuggingFinished()));
}

MaemoDebugSupport::~MaemoDebugSupport()
{
    stopSsh();
}

void MaemoDebugSupport::handleAdapterSetupRequested()
{
    if (!m_deviceConfig.isValid()) {
        handleAdapterSetupFailed(tr("No device configuration set for run configuration."));
        return;
    }
    m_adapterStarted = false;
    m_stopped = false;
    m_runControl->showMessage(tr("Preparing remote side ..."), AppStuff);
    disconnect(m_runner, 0, this, 0);
    connect(m_runner, SIGNAL(error(QString)), this,
        SLOT(handleSshError(QString)));
    connect(m_runner, SIGNAL(readyForExecution()), this,
        SLOT(startExecution()));
    connect(m_runner, SIGNAL(reportProgress(QString)), this,
        SLOT(handleProgressReport(QString)));
    m_runner->start();
}

void MaemoDebugSupport::handleSshError(const QString &error)
{
    if (!m_stopped && !m_adapterStarted)
        handleAdapterSetupFailed(error);
}

void MaemoDebugSupport::startExecution()
{
    if (m_stopped)
        return;

    const QString &dumperLib = m_runConfig->dumperLib();
    if (!dumperLib.isEmpty()
        && m_runConfig->deployStep()->currentlyNeedsDeployment(m_deviceConfig.server.host,
               MaemoDeployable(dumperLib, uploadDir(m_deviceConfig)))) {
        m_uploader = m_runner->connection()->createSftpChannel();
        connect(m_uploader.data(), SIGNAL(initialized()), this,
                SLOT(handleSftpChannelInitialized()));
        connect(m_uploader.data(), SIGNAL(initializationFailed(QString)), this,
                SLOT(handleSftpChannelInitializationFailed(QString)));
        connect(m_uploader.data(), SIGNAL(finished(Core::SftpJobId, QString)),
                this, SLOT(handleSftpJobFinished(Core::SftpJobId, QString)));
        m_uploader->initialize();
    } else {
        startDebugging();
    }
}

void MaemoDebugSupport::handleSftpChannelInitialized()
{
    if (m_stopped)
        return;

    const QString dumperLib = m_runConfig->dumperLib();
    const QString fileName = QFileInfo(dumperLib).fileName();
    const QString remoteFilePath = uploadDir(m_deviceConfig) + '/' + fileName;
    m_uploadJob = m_uploader->uploadFile(dumperLib, remoteFilePath,
        SftpOverwriteExisting);
    if (m_uploadJob == SftpInvalidJob) {
        handleAdapterSetupFailed(tr("Upload failed: Could not open file '%1'")
            .arg(dumperLib));
    } else {
        m_runControl->showMessage(tr("Started uploading debugging helpers ('%1').")
            .arg(dumperLib), AppStuff);
    }
}

void MaemoDebugSupport::handleSftpChannelInitializationFailed(const QString &error)
{
    if (m_stopped)
        return;

    handleAdapterSetupFailed(error);
}

void MaemoDebugSupport::handleSftpJobFinished(Core::SftpJobId job,
    const QString &error)
{
    if (m_stopped)
        return;

    if (job != m_uploadJob) {
        qWarning("Warning: Unknown debugging helpers upload job %d finished.", job);
        return;
    }

    if (!error.isEmpty()) {
        handleAdapterSetupFailed(tr("Could not upload debugging helpers: %1.")
            .arg(error));
    } else {
        m_runConfig->deployStep()->setDeployed(m_deviceConfig.server.host,
            MaemoDeployable(m_runConfig->dumperLib(), uploadDir(m_deviceConfig)));
        m_runControl->showMessage(tr("Finished uploading debugging helpers."), AppStuff);
        startDebugging();
    }
    m_uploadJob = SftpInvalidJob;
}

void MaemoDebugSupport::startDebugging()
{
    if (useGdb()) {
        handleAdapterSetupDone();
    } else {
        connect(m_runner, SIGNAL(remoteErrorOutput(QByteArray)), this,
            SLOT(handleRemoteErrorOutput(QByteArray)));
        connect(m_runner, SIGNAL(remoteOutput(QByteArray)), this,
            SLOT(handleRemoteOutput(QByteArray)));
        connect(m_runner, SIGNAL(remoteProcessStarted()), this,
            SLOT(handleRemoteProcessStarted()));
        const QString &remoteExe = m_runConfig->remoteExecutableFilePath();
        m_runner->startExecution(QString::fromLocal8Bit("%1 gdbserver :%2 %3 %4")
            .arg(MaemoGlobal::remoteCommandPrefix(remoteExe))
            .arg(gdbServerPort(m_runConfig, m_deviceConfig))
            .arg(remoteExe).arg(m_runConfig->arguments()
            .join(QLatin1String(" "))).toUtf8());
    }
}

void MaemoDebugSupport::handleRemoteProcessStarted()
{
    handleAdapterSetupDone();
}

void MaemoDebugSupport::handleDebuggingFinished()
{
    m_stopped = true;
    stopSsh();
}

void MaemoDebugSupport::handleRemoteOutput(const QByteArray &output)
{
    m_runControl->showMessage(QString::fromUtf8(output), AppOutput);
}

void MaemoDebugSupport::handleRemoteErrorOutput(const QByteArray &output)
{
    m_runControl->showMessage(QString::fromUtf8(output), AppOutput);
}

void MaemoDebugSupport::handleProgressReport(const QString &progressOutput)
{
    m_runControl->showMessage(progressOutput, AppStuff);
}

void MaemoDebugSupport::stopSsh()
{
    //disconnect(m_runner, 0, this, 0);
    if (m_uploader) {
        disconnect(m_uploader.data(), 0, this, 0);
        m_uploader->closeChannel();
    }
    m_runner->stop();
}

void MaemoDebugSupport::handleAdapterSetupFailed(const QString &error)
{
    const QString msg = tr("Initial setup failed: %1").arg(error);
    if (useGdb())
        qobject_cast<RemotePlainGdbAdapter *>(m_gdbAdapter)->handleSetupFailed(msg);
    else
        qobject_cast<RemoteGdbServerAdapter*>(m_gdbAdapter)->handleSetupFailed(msg);
    m_stopped = true;
    stopSsh();
}

void MaemoDebugSupport::handleAdapterSetupDone()
{
    m_adapterStarted = true;
    if (useGdb())
        qobject_cast<RemotePlainGdbAdapter *>(m_gdbAdapter)->handleSetupDone();
    else
        qobject_cast<RemoteGdbServerAdapter*>(m_gdbAdapter)->handleSetupDone();
}

QString MaemoDebugSupport::gdbServerPort(const MaemoRunConfiguration *rc,
    const MaemoDeviceConfig &devConf)
{
    // During configuration we don't know which port to use, so we display
    // something in the config dialog, but we will make sure we use
    // the right port from the information file.
    return devConf.type == MaemoDeviceConfig::Physical
        ? QString::number(devConf.debuggingPort)
        : rc->runtimeGdbServerPort();
}

QString MaemoDebugSupport::uploadDir(const MaemoDeviceConfig &devConf)
{
    return MaemoGlobal::homeDirOnDevice(devConf.server.uname);
}

bool MaemoDebugSupport::useGdb() const
{
    return m_runControl->engine()->startParameters().startMode == StartRemoteGdb;
}

} // namespace Internal
} // namespace Qt4ProjectManager
