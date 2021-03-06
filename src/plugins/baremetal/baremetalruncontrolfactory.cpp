/****************************************************************************
**
** Copyright (C) 2014 Tim Sander <tim@krieglstein.org>
** Copyright (C) 2014 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://www.qt.io/licensing.  For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "baremetalruncontrolfactory.h"
#include "baremetalgdbcommandsdeploystep.h"
#include "baremetalrunconfiguration.h"
#include "baremetaldevice.h"
#include "baremetaldebugsupport.h"

#include "gdbserverprovider.h"
#include "gdbserverprovidermanager.h"

#include <debugger/debuggerruncontrol.h>
#include <debugger/debuggerstartparameters.h>
#include <debugger/debuggerkitinformation.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/toolchain.h>
#include <projectexplorer/project.h>
#include <projectexplorer/buildconfiguration.h>
#include <analyzerbase/analyzerstartparameters.h>
#include <analyzerbase/analyzermanager.h>
#include <analyzerbase/analyzerruncontrol.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/target.h>
#include <utils/portlist.h>
#include <utils/qtcassert.h>

#include <QApplication>

using namespace Analyzer;
using namespace Debugger;
using namespace ProjectExplorer;

namespace BareMetal {
namespace Internal {

BareMetalRunControlFactory::BareMetalRunControlFactory(QObject *parent) :
    ProjectExplorer::IRunControlFactory(parent)
{
}

BareMetalRunControlFactory::~BareMetalRunControlFactory()
{
}

bool BareMetalRunControlFactory::canRun(RunConfiguration *runConfiguration, RunMode mode) const
{
    if (mode != NormalRunMode && mode != DebugRunMode && mode != DebugRunModeWithBreakOnMain)
        return false;

    const QByteArray idStr = runConfiguration->id().name();
    return runConfiguration->isEnabled() && idStr.startsWith(BareMetalRunConfiguration::IdPrefix);
}

RunControl *BareMetalRunControlFactory::create(
        RunConfiguration *runConfiguration, RunMode mode, QString *errorMessage)
{
    QTC_ASSERT(canRun(runConfiguration, mode), return 0);

    const auto rc = qobject_cast<BareMetalRunConfiguration *>(runConfiguration);
    QTC_ASSERT(rc, return 0);

    const QString bin = rc->localExecutableFilePath();
    if (bin.isEmpty()) {
        *errorMessage = tr("Cannot debug: Local executable is not set.");
        return 0;
    } else if (!QFile::exists(bin)) {
        *errorMessage = tr("Cannot debug: Could not find executable for \"%1\".")
                .arg(bin);
        return 0;
    }

    const Target *target = rc->target();
    QTC_ASSERT(target, return 0);

    const Kit *kit = target->kit();
    QTC_ASSERT(kit, return 0);

    auto dev = qSharedPointerCast<const BareMetalDevice>(DeviceKitInformation::device(kit));
    if (!dev) {
        *errorMessage = tr("Cannot debug: Kit has no device.");
        return 0;
    }

    const GdbServerProvider *p = GdbServerProviderManager::instance()->findProvider(
                dev->gdbServerProviderId());
    if (!p) {
        *errorMessage = tr("Cannot debug: Device has no GDB server provider configuration.");
        return 0;
    }

    DebuggerStartParameters sp;

    if (const ToolChain *tc = ToolChainKitInformation::toolChain(kit))
        sp.toolChainAbi = tc->targetAbi();

    if (const Project *project = target->project()) {
        sp.projectSourceDirectory = project->projectDirectory().toString();
        sp.projectSourceFiles = project->files(Project::ExcludeGeneratedFiles);

        if (const BuildConfiguration *bc = target->activeBuildConfiguration()) {
            sp.projectBuildDirectory = bc->buildDirectory().toString();
            if (const BuildStepList *bsl = bc->stepList(BareMetalGdbCommandsDeployStep::stepId())) {
                foreach (const BuildStep *bs, bsl->steps()) {
                    const auto ds = qobject_cast<const BareMetalGdbCommandsDeployStep *>(bs);
                    if (ds) {
                        if (!sp.commandsAfterConnect.endsWith("\n"))
                            sp.commandsAfterConnect.append("\n");
                        sp.commandsAfterConnect.append(ds->gdbCommands().toLatin1());
                    }
                }
            }
        }
    }

    sp.executable = bin;
    sp.sysRoot = SysRootKitInformation::sysRoot(kit).toString();
    sp.debuggerCommand = DebuggerKitInformation::debuggerCommand(kit).toString();
    sp.languages |= CppLanguage;
    sp.processArgs = rc->arguments();
    sp.startMode = AttachToRemoteServer;
    sp.displayName = rc->displayName();
    sp.commandsAfterConnect = p->initCommands().toLatin1();
    sp.commandsForReset = p->resetCommands().toLatin1();
    sp.remoteChannel = p->channel();
    sp.useContinueInsteadOfRun = true;

    if (p->startupMode() == GdbServerProvider::StartupOnNetwork)
        sp.remoteSetupNeeded = true;

    DebuggerRunControl *runControl =
            DebuggerRunControlFactory::doCreate(sp, rc, errorMessage);
    if (runControl && sp.remoteSetupNeeded) {
        const auto debugSupport = new BareMetalDebugSupport(dev, runControl);
        Q_UNUSED(debugSupport);
    }

    return runControl;
}

} // namespace Internal
} // namespace BareMetal
