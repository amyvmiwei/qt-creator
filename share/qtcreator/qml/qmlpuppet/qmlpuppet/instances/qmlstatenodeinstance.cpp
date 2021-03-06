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

#include "qmlstatenodeinstance.h"

#include <private/qdeclarativestategroup_p.h>

#include "qmlpropertychangesnodeinstance.h"
#include <private/qdeclarativestateoperations_p.h>

namespace QmlDesigner {
namespace Internal {

/**
  \class QmlStateNodeInstance

  QmlStateNodeInstance manages a QDeclarativeState object.
  */

QmlStateNodeInstance::QmlStateNodeInstance(QDeclarativeState *object) :
        ObjectNodeInstance(object)
{
}

QmlStateNodeInstance::Pointer
        QmlStateNodeInstance::create(QObject *object)
{
    QDeclarativeState *stateObject = qobject_cast<QDeclarativeState*>(object);

    Q_ASSERT(stateObject);

    Pointer instance(new QmlStateNodeInstance(stateObject));

    instance->populateResetHashes();

    return instance;
}

void QmlStateNodeInstance::activateState()
{
    if (stateGroup()
            && !isStateActive()
            && nodeInstanceServer()->hasInstanceForObject(object())) {
        nodeInstanceServer()->setStateInstance(nodeInstanceServer()->instanceForObject(object()));
        stateGroup()->setState(property("name").toString());
    }
}

void QmlStateNodeInstance::deactivateState()
{
    if (stateGroup()) {
        if (isStateActive()) {
            nodeInstanceServer()->clearStateInstance();
            stateGroup()->setState(QString());
        }
    }
}

QDeclarativeState *QmlStateNodeInstance::stateObject() const
{
    Q_ASSERT(object());
    Q_ASSERT(qobject_cast<QDeclarativeState*>(object()));
    return static_cast<QDeclarativeState*>(object());
}

QDeclarativeStateGroup *QmlStateNodeInstance::stateGroup() const
{
    return stateObject()->stateGroup();
}

bool QmlStateNodeInstance::isStateActive() const
{
    return stateObject() && stateGroup() && stateGroup()->state() == property("name");
}

void QmlStateNodeInstance::setPropertyVariant(const PropertyName &name, const QVariant &value)
{
    bool hasParent = parent();
    bool isStateOfTheRootModelNode = parentInstance() && parentInstance()->isRootNodeInstance();
    if (name == "when" && (!hasParent || isStateOfTheRootModelNode))
        return;

    ObjectNodeInstance::setPropertyVariant(name, value);
}

void QmlStateNodeInstance::setPropertyBinding(const PropertyName &name, const QString &expression)
{
    bool hasParent = parent();
    bool isStateOfTheRootModelNode = parentInstance() && parentInstance()->isRootNodeInstance();
    if (name == "when" && (!hasParent || isStateOfTheRootModelNode))
        return;

    ObjectNodeInstance::setPropertyBinding(name, expression);
}

bool QmlStateNodeInstance::updateStateVariant(const ObjectNodeInstance::Pointer &target, const PropertyName &propertyName, const QVariant &value)
{
    return stateObject()->changeValueInRevertList(target->object(), propertyName, value);
}

bool QmlStateNodeInstance::updateStateBinding(const ObjectNodeInstance::Pointer &target, const PropertyName &propertyName, const QString &expression)
{
    return stateObject()->changeValueInRevertList(target->object(), propertyName, expression);
}

bool QmlStateNodeInstance::resetStateProperty(const ObjectNodeInstance::Pointer &target, const PropertyName &propertyName, const QVariant & /* resetValue */)
{
    return stateObject()->removeEntryFromRevertList(target->object(), propertyName);
}

} // namespace Internal
} // namespace QmlDesigner
