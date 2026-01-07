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

#ifndef CONSOLEPANEL_H
#define CONSOLEPANEL_H

#include "xen/vm.h"
#include <QWidget>
#include <QMap>
#include <QString>
#include <QImage>

// Forward declarations
class VNCView;
class XenConnection;
class XenObject;
class VM;

namespace Ui
{
    class ConsolePanel;
}

/**
 * @class ConsolePanel
 * @brief VM console cache manager - maintains pool of up to 10 VNCView instances
 *
 * Reference: XenAdmin/Controls/ConsolePanel.cs (313 lines)
 *
 * This class manages a cache of VNCView instances for multiple VMs, allowing
 * efficient switching between VM consoles without recreating connections.
 *
 * Key features:
 * - Caches up to MAX_ACTIVE_VM_CONSOLES (10) VNCView instances
 * - Automatically removes oldest cached consoles when limit exceeded
 * - Pauses/unpauses consoles based on visibility
 * - RBAC permission checking for console access
 * - Host console support via control domain (dom0) lookup
 * - Error message display for connection failures
 *
 * Architecture:
 * - ConsoleTabPage uses ConsolePanel to display VM consoles
 * - ConsolePanel manages VNCView instances (cache)
 * - VNCView wraps VNCTabView (docking manager)
 * - VNCTabView contains XSVNCScreen (connection layer)
 * - XSVNCScreen uses VNCGraphicsClient (RFB protocol)
 */
class ConsolePanel : public QWidget
{
    Q_OBJECT

    public:
        /**
         * @brief Maximum number of active VM console instances to cache
         * Reference: ConsolePanel.cs line 43
         */
        static constexpr int MAX_ACTIVE_VM_CONSOLES = 10;

        /**
         * @brief Constructor
         * Reference: ConsolePanel.cs lines 49-54
         */
        explicit ConsolePanel(QWidget* parent = nullptr);

        /**
         * @brief Destructor - cleanup cached VNCView instances
         */
        ~ConsolePanel();

        /**
         * @brief Pause all docked VNC views (called when tab hidden)
         * Reference: ConsolePanel.cs lines 58-64
         *
         * Pauses rendering for all docked consoles to save CPU/network resources.
         * Undocked consoles are not paused (user may still be viewing them).
         */
        void PauseAllDockedViews();

        /**
         * @brief Reset all cached views (clear cache)
         * Reference: ConsolePanel.cs lines 66-69
         *
         * Removes all cached VNCView instances. Called on disconnect.
         * TODO: this isn't used yet
         */
        void ResetAllViews();

        /**
         * @brief Unpause the active view and optionally focus it
         * Reference: ConsolePanel.cs lines 71-91
         *
         * Explicitly pauses all docked consoles except the active one,
         * then unpauses the active console.
         *
         * @param focus If true, also focus the console and switch protocol if needed
         */
        void UnpauseActiveView(bool focus = false);

        /**
         * @brief Update RDP resolution for active view
         * Reference: ConsolePanel.cs lines 93-97
         *
         * @param fullscreen True if switching to fullscreen mode
         */
        void UpdateRDPResolution(bool fullscreen = false);

        /**
         * @brief Set current VM source for console display
         * Reference: ConsolePanel.cs lines 99-155
         *
         * Main method for switching VM console. Handles:
         * - RBAC permission checking
         * - Cache lookup/creation
         * - LRU eviction when cache full
         * - Active view switching
         *
         * @param vmRef XenObject to display console for
         */
        void SetCurrentSource(QSharedPointer<XenObject> xen_obj);

        /**
         * @brief Set current host source for console display
         * Reference: ConsolePanel.cs lines 157-170
         *
         * Shows host console by finding dom0 (control domain) and displaying its console.
         *
         * @param hostRef Host OpaqueRef to display console for
         */
        virtual void SetCurrentSourceHost(QSharedPointer<XenObject> xen_obj);

        /**
         * @brief Take Snapshot of VM console (for preview images)
         * Reference: ConsolePanel.cs lines 197-234
         *
         * Creates a temporary VNCView if needed (with elevated credentials),
         * captures screenshot, then disposes temporary view.
         *
         * @param vmRef VM OpaqueRef to Snapshot
         * @param elevatedUsername Optional elevated credentials username
         * @param elevatedPassword Optional elevated credentials password
         * @return Console screenshot image (null if failed)
         */
        QImage Snapshot(QSharedPointer<VM> vm, const QString& elevatedUsername = QString(), const QString& elevatedPassword = QString());

        /**
         * @brief Close VNC connection for specified VM
         * Reference: ConsolePanel.cs lines 236-247
         *
         * Removes VNCView from cache and disposes it (if docked).
         *
         * @param vmRef VM OpaqueRef to close console for
         */
        void closeVncForSource(const QString& vmRef);

        /**
         * @brief Send Ctrl+Alt+Delete to active console
         * Reference: ConsolePanel.cs lines 259-263
         */
        void SendCAD();

        /**
         * @brief Get current active VM reference
         */
        QString CurrentVM() const
        {
            return m_currentVmRef;
        }

        void SetConnection(XenConnection *connection)
        {
            this->_connection = connection;
        }

    protected:
        /**
         * @brief Display error message panel
         * Reference: ConsolePanel.cs lines 249-254
         */
        void setErrorMessage(const QString& message);

        /**
         * @brief Hide error message panel
         * Reference: ConsolePanel.cs lines 256-259
         */
        void clearErrorMessage();

        // ========== Member Variables ==========

        Ui::ConsolePanel* ui;

        XenConnection *_connection = nullptr;;

        //! Active VNCView currently displayed
        VNCView* _activeVNCView = nullptr;

        //! Cache of VNCView instances keyed by VM OpaqueRef
        //! Reference: C# Dictionary<VM, VNCView> vncViews
        QMap<QString, VNCView*> _vncViews;

        //! Current VM OpaqueRef
        QString m_currentVmRef;

        QSharedPointer<XenObject> m_currentObject;

    private:
        // ========== Private Methods ==========

        /**
         * @brief Check if user has RBAC permission to access VM console
         * Reference: ConsolePanel.cs lines 172-195
         *
         * @param vmRef VM OpaqueRef to check
         * @param allowedRoles Output list of roles that have permission
         * @return True if RBAC denies access, false if allowed
         */
        bool rbacDenied(const QString& vmRef, QStringList& allowedRoles);

        /**
         * @brief Show RBAC warning panel with role information
         *
         * @param userRoles Current user's roles
         * @param allowedRoles Roles that have console permission
         */
        void showRbacWarning(const QStringList& userRoles, const QStringList& allowedRoles);

        /**
         * @brief Remove oldest cached console view to make room for new one
         *
         * Removes first cached view (oldest) that is docked.
         * Does not remove undocked views (user may still be viewing them).
         */
        void evictOldestView();
};

/**
 * @class CvmConsolePanel
 * @brief Specialized ConsolePanel for Citrix VM (CVM) consoles
 *
 * Reference: XenAdmin/Controls/ConsolePanel.cs lines 265-289
 *
 * Instead of showing dom0, shows "other control domain" (CVM).
 * Used in XCP-ng for clustered pool master VMs.
 */
class CvmConsolePanel : public ConsolePanel
{
    Q_OBJECT

    public:
        explicit CvmConsolePanel(QWidget* parent = nullptr);

        /**
         * @brief Set current host source for CVM console display
         * Reference: CvmConsolePanel.SetCurrentSource() lines 273-286
         *
         * Shows CVM console by finding "other control domain" instead of dom0.
         */
        void SetCurrentSourceHost(QSharedPointer<XenObject> xen_obj) override;

    private:
        /**
         * @brief Get CVM (other control domain) for host
         *
         * @param hostRef Host OpaqueRef
         * @return VM OpaqueRef of CVM, or empty string if not found
         */
        QString getOtherControlDomainForHost(const QString& hostRef);
};

#endif // CONSOLEPANEL_H
