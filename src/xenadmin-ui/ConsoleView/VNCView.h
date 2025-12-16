/* Copyright (c) Petr Bena
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
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file VNCView.h
 * @brief Docking manager for VNCTabView - handles undocked windows, pause/unpause, geometry
 *
 * Reference: xenadmin/XenAdmin/ConsoleView/VNCView.cs
 * This class wraps VNCTabView and manages:
 * - Docking/undocking to separate window
 * - Window geometry persistence
 * - Pause/unpause on visibility changes
 * - "Find Console" and "Reattach" buttons when undocked
 */

#ifndef VNCVIEW_H
#define VNCVIEW_H

#include <QWidget>
#include <QString>
#include <QSize>
#include <QPoint>
#include <QImage>

// Forward declarations
class VNCTabView;
class XenLib;
class QMainWindow;
class QPushButton;

/**
 * @class VNCView
 * @brief Docking manager wrapper for VNCTabView
 *
 * Reference: XenAdmin/ConsoleView/VNCView.cs (257 lines)
 *
 * Key responsibilities:
 * - Owns VNCTabView instance
 * - Manages docked (embedded) vs undocked (separate window) states
 * - Shows "Find Console" and "Reattach" buttons when undocked
 * - Persists window geometry between dock/undock cycles
 * - Pauses console when undocked window is minimized
 * - Updates window title when VM name changes
 *
 * Layout when docked:
 * - VNCTabView fills entire VNCView widget
 * - findConsoleButton and reattachConsoleButton are hidden
 *
 * Layout when undocked:
 * - VNCTabView moves to separate QMainWindow
 * - findConsoleButton and reattachConsoleButton become visible in VNCView
 * - Separate window has close button that re-docks
 */
class VNCView : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * Reference: VNCView.cs lines 64-72
     *
     * @param vmRef VM OpaqueRef
     * @param elevatedUsername Optional elevated credentials username
     * @param elevatedPassword Optional elevated credentials password
     * @param xenLib XenLib instance for XenAPI access
     * @param parent Qt parent widget
     */
    explicit VNCView(const QString& vmRef,
                     const QString& elevatedUsername,
                     const QString& elevatedPassword,
                     XenLib* xenLib,
                     QWidget* parent = nullptr);

    /**
     * @brief Destructor - cleanup undocked window, unregister listeners
     * Reference: VNCView.Designer.cs lines 14-33
     */
    ~VNCView();

    /**
     * @brief Check if console is docked (embedded in main window)
     * Reference: VNCView.cs line 50
     * @return true if docked, false if undocked to separate window
     */
    bool isDocked() const;

    /**
     * @brief Pause console updates (when hidden or minimized)
     * Reference: VNCView.cs lines 52-55
     */
    void pause();

    /**
     * @brief Resume console updates (when visible)
     * Reference: VNCView.cs lines 57-60
     */
    void unpause();

    /**
     * @brief Toggle between docked and undocked states
     * Reference: VNCView.cs lines 87-187
     *
     * When docking:
     * - Hides undocked window
     * - Moves VNCTabView back to this widget
     * - Saves window geometry for next undock
     * - Hides find/reattach buttons
     *
     * When undocking:
     * - Creates separate QMainWindow
     * - Moves VNCTabView to window
     * - Restores saved geometry if available
     * - Shows find/reattach buttons
     * - Sets up minimize/close handlers
     */
    void dockUnDock();

    /**
     * @brief Send Ctrl+Alt+Del to console
     * Reference: VNCView.cs lines 225-228
     */
    void sendCAD();

    /**
     * @brief Focus the console widget
     * Reference: VNCView.cs lines 230-233
     */
    void focusConsole();

    /**
     * @brief Switch console protocol if required (auto-RDP)
     * Reference: VNCView.cs lines 235-238
     */
    void switchIfRequired();

    /**
     * @brief Capture screenshot of console
     * Reference: VNCView.cs lines 240-243
     * @return QImage with current console display
     */
    QImage snapshot();

    /**
     * @brief Refresh CD/DVD ISO list
     * Reference: VNCView.cs lines 245-248
     */
    void refreshIsoList();

    /**
     * @brief Update RDP resolution (for RDP protocol)
     * Reference: VNCView.cs lines 250-253
     * @param fullscreen Whether to use fullscreen resolution
     */
    void updateRDPResolution(bool fullscreen = false);

    /**
     * @brief Access to VNCTabView (for ConsolePanel integration)
     */
    VNCTabView* vncTabView()
    {
        return _vncTabView;
    }

private slots:
    /**
     * @brief Handle VM property changes (name_label for window title)
     * Reference: VNCView.cs lines 207-213
     */
    void onVMPropertyChanged(const QString& propertyName);

    /**
     * @brief "Find Console" button clicked - bring undocked window to front
     * Reference: VNCView.cs lines 215-220
     */
    void onFindConsoleButtonClicked();

    /**
     * @brief "Reattach Console" button clicked - re-dock window
     * Reference: VNCView.cs lines 222-225
     */
    void onReattachConsoleButtonClicked();

    /**
     * @brief Handle undocked window state changes (minimize → pause)
     * Internal slot for QMainWindow state monitoring
     */
    void onUndockedWindowStateChanged(Qt::WindowStates oldState, Qt::WindowStates newState);

    /**
     * @brief Handle undocked window resize end
     * Internal slot for RDP resolution update
     */
    void onUndockedWindowResizeEnd();

private:
    /**
     * @brief Register event listeners (VM property changes)
     * Called in constructor
     */
    void registerEventListeners();

    /**
     * @brief Unregister all event listeners
     * Reference: VNCView.cs lines 74-78
     */
    void unregisterEventListeners();

    /**
     * @brief Generate window title for undocked window
     * Reference: VNCView.cs lines 189-200
     * @return Window title (VM name, or "Host: hostname", or "SR Driver Domain: srname")
     */
    QString undockedWindowTitle() const;

    /**
     * @brief Setup UI layout (buttons, VNCTabView)
     * Qt equivalent to InitializeComponent()
     */
    void setupUI();

private:
    QString _vmRef;  ///< VM OpaqueRef
    XenLib* _xenLib; ///< XenLib instance

    VNCTabView* _vncTabView;    ///< Wrapped console UI
    QMainWindow* _undockedForm; ///< Separate window when undocked

    QPushButton* _findConsoleButton;     ///< "Find Console" button (shown when undocked)
    QPushButton* _reattachConsoleButton; ///< "Reattach Console" button (shown when undocked)

    // Geometry persistence (C#: oldUndockedSize, oldUndockedLocation, oldScaledSetting)
    QSize _oldUndockedSize;      ///< Last undocked window size
    QPoint _oldUndockedLocation; ///< Last undocked window position
    bool _oldScaledSetting;      ///< Scale checkbox state before undocking

    // Resize tracking (C#: undockedFormResized)
    bool _undockedFormResized; ///< Helper to detect actual resize (not just move)
};

#endif // VNCVIEW_H
