/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "genericprojectplugin.h"

#include "genericbuildconfiguration.h"
#include "genericprojectmanager.h"
#include "genericprojectwizard.h"
#include "genericprojectconstants.h"
#include "genericprojectfileseditor.h"
#include "genericmakestep.h"
#include "genericproject.h"

#include <coreplugin/icore.h>
#include <coreplugin/mimedatabase.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/actioncontainer.h>

#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projecttree.h>
#include <projectexplorer/selectablefilesmodel.h>

#include <QtPlugin>
#include <QDebug>

using namespace Core;
using namespace ProjectExplorer;

namespace GenericProjectManager {
namespace Internal {

GenericProjectPlugin::GenericProjectPlugin()
    : m_contextMenuProject(0)
{ }

bool GenericProjectPlugin::initialize(const QStringList &, QString *errorMessage)
{
    const QLatin1String mimetypesXml(":genericproject/GenericProjectManager.mimetypes.xml");

    if (!MimeDatabase::addMimeTypes(mimetypesXml, errorMessage))
        return false;

    addAutoReleasedObject(new Manager);
    addAutoReleasedObject(new ProjectFilesFactory);
    addAutoReleasedObject(new GenericMakeStepFactory);
    addAutoReleasedObject(new GenericProjectWizard);
    addAutoReleasedObject(new GenericBuildConfigurationFactory);

    ActionContainer *mproject =
            ActionManager::actionContainer(ProjectExplorer::Constants::M_PROJECTCONTEXT);

    auto editFilesAction = new QAction(tr("Edit Files..."), this);
    Command *command = ActionManager::registerAction(editFilesAction,
        "GenericProjectManager.EditFiles", Context(Constants::PROJECTCONTEXT));
    command->setAttribute(Command::CA_Hide);
    mproject->addAction(command, ProjectExplorer::Constants::G_PROJECT_FILES);

    connect(editFilesAction, &QAction::triggered,
            this, &GenericProjectPlugin::editFiles);

    connect(ProjectTree::instance(), &ProjectTree::aboutToShowContextMenu,
            [this] (Project *project, Node *) { m_contextMenuProject = project; });

    return true;
}

void GenericProjectPlugin::editFiles()
{
    GenericProject *genericProject = static_cast<GenericProject *>(m_contextMenuProject);
    SelectableFilesDialogEditFiles sfd(genericProject->projectFilePath().toFileInfo().path(), genericProject->files(),
                              ICore::mainWindow());
    if (sfd.exec() == QDialog::Accepted)
        genericProject->setFiles(sfd.selectedFiles());
}

} // namespace Internal
} // namespace GenericProjectManager
