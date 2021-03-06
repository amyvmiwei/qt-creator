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

#ifndef ACTIONMANAGER_H
#define ACTIONMANAGER_H

#include "coreplugin/core_global.h"
#include "coreplugin/id.h"
#include <coreplugin/actionmanager/command.h>

#include <QObject>
#include <QList>

QT_BEGIN_NAMESPACE
class QAction;
class QSettings;
class QString;
QT_END_NAMESPACE

namespace Core {

class ActionContainer;

namespace Internal {
class CorePlugin;
class MainWindow;
} // Internal

class CORE_EXPORT ActionManager : public QObject
{
    Q_OBJECT
public:
    static ActionManager *instance();

    static ActionContainer *createMenu(Id id);
    static ActionContainer *createMenuBar(Id id);

    static Command *registerAction(QAction *action, Id id, const Context &context, bool scriptable = false);

    static Command *command(Id id);
    static ActionContainer *actionContainer(Id id);

    static QList<Command *> commands();

    static void unregisterAction(QAction *action, Id id);

    static void setPresentationModeEnabled(bool enabled);
    static bool isPresentationModeEnabled();

signals:
    void commandListChanged();
    void commandAdded(Core::Id id);

private:
    ActionManager(QObject *parent = 0);
    ~ActionManager();
    static void initialize(QObject *parent);
    static void saveSettings(QSettings *settings);
    static void setContext(const Context &context);

    friend class Core::Internal::CorePlugin; // initialization
    friend class Core::Internal::MainWindow; // saving settings and setting context
};

} // namespace Core

#endif // ACTIONMANAGER_H
