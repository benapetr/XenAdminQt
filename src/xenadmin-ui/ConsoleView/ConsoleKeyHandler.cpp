/* Copyright (c) Cloud Software Group, Inc.
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 * *   Redistributions of source code must retain the above
 *     copyright notice, this list of conditions and the
 *     following disclaimer.
 * *   Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the
 *     following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ConsoleKeyHandler.h"
#include <QDebug>
#include <algorithm> // for std::sort

ConsoleKeyHandler::ConsoleKeyHandler(QObject* parent)
    : QObject(parent), m_modifierKeyPressedAlone(false)
{
    // Initialize modifier keys list (matches C# ConsoleKeyHandler)
    this->m_modifierKeys << Qt::Key_Control << Qt::Key_Shift << Qt::Key_Alt << Qt::Key_Meta;

    // Initialize modifier scan codes list
    this->m_modifierScans << CTRL_SCAN << CTRL2_SCAN << L_SHIFT_SCAN << R_SHIFT_SCAN << ALT_SCAN << ALT2_SCAN << GR_SCAN;
}

void ConsoleKeyHandler::addKeyHandler(ConsoleShortcutKey shortcutKey, std::function<void()> handler)
{
    switch (shortcutKey)
    {
    case ConsoleShortcutKey::CTRL_ALT:
        this->addKeyHandler(QList<Qt::Key>{Qt::Key_Control, Qt::Key_Alt}, handler);
        this->addKeyHandler(QList<int>{CTRL_SCAN, ALT_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL2_SCAN, ALT2_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL2_SCAN, ALT2_SCAN, GR_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL2_SCAN, ALT_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL_SCAN, ALT2_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL_SCAN, ALT2_SCAN, GR_SCAN}, handler);
        break;

    case ConsoleShortcutKey::CTRL_ALT_F:
        this->addKeyHandler(QList<Qt::Key>{Qt::Key_Control, Qt::Key_Alt, Qt::Key_F}, handler);
        this->addKeyHandler(QList<int>{CTRL_SCAN, ALT_SCAN, F_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL2_SCAN, ALT2_SCAN, F_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL2_SCAN, ALT2_SCAN, GR_SCAN, F_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL2_SCAN, ALT_SCAN, F_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL_SCAN, ALT2_SCAN, F_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL_SCAN, ALT2_SCAN, GR_SCAN, F_SCAN}, handler);
        break;

    case ConsoleShortcutKey::F12:
        this->addKeyHandler(QList<Qt::Key>{Qt::Key_F12}, handler);
        this->addKeyHandler(QList<int>{F12_SCAN}, handler);
        break;

    case ConsoleShortcutKey::CTRL_ENTER:
        this->addKeyHandler(QList<Qt::Key>{Qt::Key_Control, Qt::Key_Return}, handler);
        this->addKeyHandler(QList<Qt::Key>{Qt::Key_Control, Qt::Key_Enter}, handler);
        this->addKeyHandler(QList<int>{CTRL_SCAN, ENTER_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL2_SCAN, ENTER_SCAN}, handler);
        break;

    case ConsoleShortcutKey::ALT_SHIFT_U:
        this->addKeyHandler(QList<Qt::Key>{Qt::Key_Alt, Qt::Key_Shift, Qt::Key_U}, handler);
        this->addKeyHandler(QList<int>{ALT_SCAN, L_SHIFT_SCAN, U_SCAN}, handler);
        this->addKeyHandler(QList<int>{ALT2_SCAN, L_SHIFT_SCAN, U_SCAN}, handler);
        this->addKeyHandler(QList<int>{ALT_SCAN, R_SHIFT_SCAN, U_SCAN}, handler);
        this->addKeyHandler(QList<int>{ALT2_SCAN, R_SHIFT_SCAN, U_SCAN}, handler);
        this->addKeyHandler(QList<int>{ALT2_SCAN, R_SHIFT_SCAN, GR_SCAN, U_SCAN}, handler);
        this->addKeyHandler(QList<int>{ALT2_SCAN, L_SHIFT_SCAN, GR_SCAN, U_SCAN}, handler);
        break;

    case ConsoleShortcutKey::F11:
        this->addKeyHandler(QList<Qt::Key>{Qt::Key_F11}, handler);
        this->addKeyHandler(QList<int>{F11_SCAN}, handler);
        break;

    case ConsoleShortcutKey::RIGHT_CTRL:
        // Qt doesn't distinguish left/right control in Qt::Key, use scan codes
        this->addKeyHandler(QList<int>{CTRL2_SCAN}, handler);
        break;

    case ConsoleShortcutKey::LEFT_ALT:
        // Qt doesn't distinguish left/right alt in Qt::Key, use scan codes
        this->addKeyHandler(QList<int>{ALT_SCAN}, handler);
        break;

    case ConsoleShortcutKey::CTRL_ALT_INS:
        this->addKeyHandler(QList<Qt::Key>{Qt::Key_Control, Qt::Key_Alt, Qt::Key_Insert}, handler);
        this->addKeyHandler(QList<int>{CTRL_SCAN, ALT_SCAN, INS_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL2_SCAN, ALT2_SCAN, INS_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL2_SCAN, ALT2_SCAN, GR_SCAN, INS_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL_SCAN, ALT2_SCAN, INS_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL2_SCAN, ALT_SCAN, INS_SCAN}, handler);
        this->addKeyHandler(QList<int>{CTRL_SCAN, ALT2_SCAN, GR_SCAN, INS_SCAN}, handler);
        break;
    }
}

void ConsoleKeyHandler::addKeyHandler(const QList<Qt::Key>& keyList, std::function<void()> handler)
{
    // Sort the list for consistent comparison
    QList<Qt::Key> sortedList = keyList;
    std::sort(sortedList.begin(), sortedList.end());
    this->m_extraKeys[sortedList] = handler;
}

void ConsoleKeyHandler::addKeyHandler(const QList<int>& scanList, std::function<void()> handler)
{
    // Sort the list for consistent comparison
    QList<int> sortedList = scanList;
    std::sort(sortedList.begin(), sortedList.end());
    this->m_extraScans[sortedList] = handler;
}

void ConsoleKeyHandler::removeKeyHandler(ConsoleShortcutKey shortcutKey)
{
    switch (shortcutKey)
    {
    case ConsoleShortcutKey::CTRL_ALT:
        this->removeKeyHandler(QList<Qt::Key>{Qt::Key_Control, Qt::Key_Alt});
        this->removeKeyHandler(QList<int>{CTRL_SCAN, ALT_SCAN});
        this->removeKeyHandler(QList<int>{CTRL2_SCAN, ALT2_SCAN});
        this->removeKeyHandler(QList<int>{CTRL2_SCAN, ALT2_SCAN, GR_SCAN});
        this->removeKeyHandler(QList<int>{CTRL2_SCAN, ALT_SCAN});
        this->removeKeyHandler(QList<int>{CTRL_SCAN, ALT2_SCAN});
        this->removeKeyHandler(QList<int>{CTRL_SCAN, ALT2_SCAN, GR_SCAN});
        break;

    case ConsoleShortcutKey::CTRL_ALT_F:
        this->removeKeyHandler(QList<Qt::Key>{Qt::Key_Control, Qt::Key_Alt, Qt::Key_F});
        this->removeKeyHandler(QList<int>{CTRL_SCAN, ALT_SCAN, F_SCAN});
        this->removeKeyHandler(QList<int>{CTRL2_SCAN, ALT2_SCAN, F_SCAN});
        this->removeKeyHandler(QList<int>{CTRL2_SCAN, ALT2_SCAN, GR_SCAN, F_SCAN});
        this->removeKeyHandler(QList<int>{CTRL2_SCAN, ALT_SCAN, F_SCAN});
        this->removeKeyHandler(QList<int>{CTRL_SCAN, ALT2_SCAN, F_SCAN});
        this->removeKeyHandler(QList<int>{CTRL_SCAN, ALT2_SCAN, GR_SCAN, F_SCAN});
        break;

    case ConsoleShortcutKey::F12:
        this->removeKeyHandler(QList<Qt::Key>{Qt::Key_F12});
        this->removeKeyHandler(QList<int>{F12_SCAN});
        break;

    case ConsoleShortcutKey::CTRL_ENTER:
        this->removeKeyHandler(QList<Qt::Key>{Qt::Key_Control, Qt::Key_Return});
        this->removeKeyHandler(QList<Qt::Key>{Qt::Key_Control, Qt::Key_Enter});
        this->removeKeyHandler(QList<int>{CTRL_SCAN, ENTER_SCAN});
        this->removeKeyHandler(QList<int>{CTRL2_SCAN, ENTER_SCAN});
        break;

    case ConsoleShortcutKey::ALT_SHIFT_U:
        this->removeKeyHandler(QList<Qt::Key>{Qt::Key_Alt, Qt::Key_Shift, Qt::Key_U});
        this->removeKeyHandler(QList<int>{ALT_SCAN, L_SHIFT_SCAN, U_SCAN});
        this->removeKeyHandler(QList<int>{ALT2_SCAN, L_SHIFT_SCAN, U_SCAN});
        this->removeKeyHandler(QList<int>{ALT_SCAN, R_SHIFT_SCAN, U_SCAN});
        this->removeKeyHandler(QList<int>{ALT2_SCAN, R_SHIFT_SCAN, U_SCAN});
        this->removeKeyHandler(QList<int>{ALT2_SCAN, R_SHIFT_SCAN, GR_SCAN, U_SCAN});
        this->removeKeyHandler(QList<int>{ALT2_SCAN, L_SHIFT_SCAN, GR_SCAN, U_SCAN});
        break;

    case ConsoleShortcutKey::F11:
        this->removeKeyHandler(QList<Qt::Key>{Qt::Key_F11});
        this->removeKeyHandler(QList<int>{F11_SCAN});
        break;

    case ConsoleShortcutKey::RIGHT_CTRL:
        this->removeKeyHandler(QList<int>{CTRL2_SCAN});
        break;

    case ConsoleShortcutKey::LEFT_ALT:
        this->removeKeyHandler(QList<int>{ALT_SCAN});
        break;

    case ConsoleShortcutKey::CTRL_ALT_INS:
        this->removeKeyHandler(QList<Qt::Key>{Qt::Key_Control, Qt::Key_Alt, Qt::Key_Insert});
        this->removeKeyHandler(QList<int>{CTRL_SCAN, ALT_SCAN, INS_SCAN});
        this->removeKeyHandler(QList<int>{CTRL2_SCAN, ALT2_SCAN, INS_SCAN});
        this->removeKeyHandler(QList<int>{CTRL2_SCAN, ALT2_SCAN, GR_SCAN, INS_SCAN});
        this->removeKeyHandler(QList<int>{CTRL_SCAN, ALT2_SCAN, INS_SCAN});
        this->removeKeyHandler(QList<int>{CTRL2_SCAN, ALT_SCAN, INS_SCAN});
        this->removeKeyHandler(QList<int>{CTRL_SCAN, ALT2_SCAN, GR_SCAN, INS_SCAN});
        break;
    }
}

void ConsoleKeyHandler::removeKeyHandler(const QList<Qt::Key>& keyList)
{
    // Sort the list for consistent comparison
    QList<Qt::Key> sortedList = keyList;
    std::sort(sortedList.begin(), sortedList.end());
    this->m_extraKeys.remove(sortedList);
}

void ConsoleKeyHandler::removeKeyHandler(const QList<int>& scanList)
{
    // Sort the list for consistent comparison
    QList<int> sortedList = scanList;
    std::sort(sortedList.begin(), sortedList.end());
    this->m_extraScans.remove(sortedList);
}

void ConsoleKeyHandler::clearHandlers()
{
    this->m_extraKeys.clear();
    this->m_extraScans.clear();
    this->m_depressedKeys.clear();
    this->m_depressedScans.clear();
    this->m_modifierKeyPressedAlone = false;
}

bool ConsoleKeyHandler::handleKeyEvent(Qt::Key key, bool pressed)
{
    return this->handleExtras(pressed, this->m_depressedKeys, this->m_extraKeys, key, this->m_modifierKeys, this->m_modifierKeyPressedAlone);
}

bool ConsoleKeyHandler::handleScanEvent(int scanCode, bool pressed)
{
    return this->handleExtras(pressed, this->m_depressedScans, this->m_extraScans, scanCode, this->m_modifierScans, this->m_modifierKeyPressedAlone);
}

template <typename T>
bool ConsoleKeyHandler::handleExtras(bool pressed, QSet<T>& depressed, const QMap<QList<T>, std::function<void()>>& methods, T key,
                                     const QList<T>& modifierKeys, bool& modifierKeyPressedAlone)
{
    if (pressed)
    {
        depressed.insert(key);
        if (modifierKeyPressedAlone)
            modifierKeyPressedAlone = false;
    } else
    {
        // Handle modifier key released alone
        if (modifierKeyPressedAlone)
        {
            // Convert QSet to sorted QList for comparison
            QList<T> sortedDepressed = depressed.values();
            std::sort(sortedDepressed.begin(), sortedDepressed.end());

            if (methods.contains(sortedDepressed) && depressed.size() == 1)
            {
                methods[sortedDepressed]();
                depressed.clear();
                return true;
            }
        }
        depressed.remove(key);
    }

    // Check if current key combination matches any handler
    if (pressed)
    {
        // Convert QSet to sorted QList for comparison
        QList<T> sortedDepressed = depressed.values();
        std::sort(sortedDepressed.begin(), sortedDepressed.end());

        if (methods.contains(sortedDepressed))
        {
            // Single modifier keys are processed when released
            if (depressed.size() == 1 && modifierKeys.contains(key))
            {
                modifierKeyPressedAlone = true;
            } else
            {
                methods[sortedDepressed]();
                return true;
            }
        }
    }

    return false;
}

// Explicit template instantiations
template bool ConsoleKeyHandler::handleExtras<Qt::Key>(bool, QSet<Qt::Key>&, const QMap<QList<Qt::Key>, std::function<void()>>&, Qt::Key,
                                                       const QList<Qt::Key>&, bool&);
template bool ConsoleKeyHandler::handleExtras<int>(bool, QSet<int>&, const QMap<QList<int>, std::function<void()>>&, int, const QList<int>&,
                                                   bool&);
