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

import QtQuick 2.1
import QtQuick.Controls 1.1 as Controls
import QtQuick.Controls.Styles 1.1

Controls.ComboBox {
    id: comboBox

    property variant backendValue

    property color textColor: colorLogic.textColor
    property string scope: "Qt"

    property bool useInteger: false

    property bool __isCompleted: false

    property bool manualMapping: false

    signal valueFromBackendChanged

    ColorLogic {
        id: colorLogic
        backendValue: comboBox.backendValue
        onValueFromBackendChanged: {
            invalidate();
        }

        function invalidate() {

            if (manualMapping) {
                valueFromBackendChanged();
            } else if (!comboBox.useInteger) {
                var enumString = comboBox.backendValue.enumeration;

                if (enumString === "")
                    enumString = comboBox.backendValue.value

                var index = comboBox.find(enumString)

                if (index < 0)
                    index = 0

                if (index !== comboBox.currentIndex)
                    comboBox.currentIndex = index

            } else {
                if (comboBox.currentIndex !== backendValue.value)
                    comboBox.currentIndex = backendValue.value
            }
        }
    }

    onCurrentTextChanged: {
        if (!__isCompleted)
            return;

        if (backendValue === undefined)
            return;

        if (manualMapping)
            return;

        if (!comboBox.useInteger) {
            backendValue.setEnumeration(comboBox.scope, comboBox.currentText);
        } else {
            backendValue.value = comboBox.currentIndex;
        }
    }

    Component.onCompleted: {
        colorLogic.invalidate()
        __isCompleted = true;
    }

    style: CustomComboBoxStyle {
        textColor: comboBox.textColor
    }

    ExtendedFunctionButton {
        x: 2
        y: 4
        backendValue: comboBox.backendValue
        visible: comboBox.enabled
    }
}
