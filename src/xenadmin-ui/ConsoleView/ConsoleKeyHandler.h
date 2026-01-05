/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CONSOLEKEYHANDLER_H
#define CONSOLEKEYHANDLER_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <functional>

/**
 * @brief Predefined console keyboard shortcuts
 *
 * These shortcuts are used for common console operations like
 * fullscreen, CAD injection, etc.
 */
enum class ConsoleShortcutKey
{
    CTRL_ALT,    // Ctrl+Alt combination
    CTRL_ALT_F,  // Ctrl+Alt+F for fullscreen
    F12,         // F12 key
    CTRL_ENTER,  // Ctrl+Enter
    ALT_SHIFT_U, // Alt+Shift+U
    F11,         // F11 for fullscreen
    RIGHT_CTRL,  // Right Ctrl key
    LEFT_ALT,    // Left Alt key
    CTRL_ALT_INS // Ctrl+Alt+Insert for CAD
};

/**
 * @brief Handler for console keyboard shortcuts
 *
 * This class manages keyboard shortcuts for the console, matching
 * the C# ConsoleKeyHandler behavior. It supports both Qt::Key codes
 * and scan codes for advanced keyboard handling.
 */
class ConsoleKeyHandler : public QObject
{
    Q_OBJECT

    public:
        // Scan code constants (match C# values)
        static constexpr int CTRL_SCAN = 29;
        static constexpr int ALT_SCAN = 56;
        static constexpr int CTRL2_SCAN = 157;
        static constexpr int ALT2_SCAN = 184;
        static constexpr int GR_SCAN = 541;
        static constexpr int DEL_SCAN = 211;
        static constexpr int INS_SCAN = 210;
        static constexpr int L_SHIFT_SCAN = 0x2A;
        static constexpr int R_SHIFT_SCAN = 0x36;
        static constexpr int F11_SCAN = 87;
        static constexpr int F12_SCAN = 88;
        static constexpr int F_SCAN = 33;
        static constexpr int U_SCAN = 22;
        static constexpr int ENTER_SCAN = 28;

        explicit ConsoleKeyHandler(QObject* parent = nullptr);

        /**
         * @brief Register a handler for a predefined shortcut key
         * @param shortcutKey The shortcut key combination
         * @param handler Function to call when shortcut is triggered
         */
        void addKeyHandler(ConsoleShortcutKey shortcutKey, std::function<void()> handler);

        /**
         * @brief Register a handler for a custom key list (Qt::Key codes)
         * @param keyList List of Qt::Key codes that must be pressed together (order doesn't matter)
         * @param handler Function to call when combination is triggered
         */
        void addKeyHandler(const QList<Qt::Key>& keyList, std::function<void()> handler);

        /**
         * @brief Register a handler for a custom scan code list
         * @param scanList List of scan codes that must be pressed together (order doesn't matter)
         * @param handler Function to call when combination is triggered
         */
        void addKeyHandler(const QList<int>& scanList, std::function<void()> handler);

        /**
         * @brief Remove handler for a predefined shortcut key
         */
        void removeKeyHandler(ConsoleShortcutKey shortcutKey);

        /**
         * @brief Remove handler for a custom key list
         */
        void removeKeyHandler(const QList<Qt::Key>& keyList);

        /**
         * @brief Remove handler for a custom scan code list
         */
        void removeKeyHandler(const QList<int>& scanList);

        /**
         * @brief Clear all registered key handlers
         * Equivalent to C#: clearing all event subscriptions in Dispose()
         */
        void clearHandlers();

        /**
         * @brief Handle key press/release events (Qt::Key codes)
         * @param key The Qt::Key code
         * @param pressed true for press, false for release
         * @return true if handled by a shortcut, false otherwise
         */
        bool handleKeyEvent(Qt::Key key, bool pressed);

        /**
         * @brief Handle key press/release events (scan codes)
         * @param scanCode The keyboard scan code
         * @param pressed true for press, false for release
         * @return true if handled by a shortcut, false otherwise
         */
        bool handleScanEvent(int scanCode, bool pressed);

        /**
         * @brief Get list of modifier keys
         */
        const QList<Qt::Key>& modifierKeys() const
        {
            return m_modifierKeys;
        }

        /**
         * @brief Get list of modifier scan codes
         */
        const QList<int>& modifierScans() const
        {
            return m_modifierScans;
        }

    private:
        // Currently pressed keys (Qt::Key codes)
        QSet<Qt::Key> m_depressedKeys;

        // Currently pressed scan codes
        QSet<int> m_depressedScans;

        // Registered key handlers (Qt::Key combinations) - use sorted QList for map keys
        QMap<QList<Qt::Key>, std::function<void()>> m_extraKeys;

        // Registered scan code handlers - use sorted QList for map keys
        QMap<QList<int>, std::function<void()>> m_extraScans;

        // Modifier keys list
        QList<Qt::Key> m_modifierKeys;

        // Modifier scan codes list
        QList<int> m_modifierScans;

        // Flag to track if a modifier key was pressed alone
        bool m_modifierKeyPressedAlone;

        /**
         * @brief Generic handler for key/scan events
         * @tparam T Either Qt::Key or int (for scan codes)
         */
        template <typename T>
        bool handleExtras(bool pressed, QSet<T>& depressed,
                          const QMap<QList<T>, std::function<void()>>& methods,
                          T key, const QList<T>& modifierKeys,
                          bool& modifierKeyPressedAlone);
};

#endif // CONSOLEKEYHANDLER_H
