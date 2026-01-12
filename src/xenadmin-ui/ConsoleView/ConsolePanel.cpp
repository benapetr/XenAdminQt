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

#include "ConsolePanel.h"
#include "ui_ConsolePanel.h"
#include "VNCView.h"
#include "xen/network/connection.h"
#include "xen/host.h"
#include "xencache.h"
#include <QDebug>
#include <QVBoxLayout>
#include <QThread>
#include <QApplication>

/**
 * @brief Constructor
 * Reference: ConsolePanel.cs lines 49-54
 */
ConsolePanel::ConsolePanel(QWidget* parent) : QWidget(parent), ui(new Ui::ConsolePanel), m_currentVmRef()
{
    qDebug() << "ConsolePanel: Constructor START - parent:" << parent << "this:" << this;

    Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());

    qDebug() << "ConsolePanel: About to call setupUi()";
    this->ui->setupUi(this);
    qDebug() << "ConsolePanel: setupUi() complete";

    // Initially hide warning/error panels
    this->ui->rbacWarningPanel->setVisible(false);
    this->ui->errorPanel->setVisible(false);

    // Check focus policies of created widgets
    qDebug() << "ConsolePanel: Focus policy of this:" << this->focusPolicy();
    qDebug() << "ConsolePanel: Focus policy of consoleContainer:" << this->ui->consoleContainer->focusPolicy();
    qDebug() << "ConsolePanel: Focus policy of rbacWarningPanel:" << this->ui->rbacWarningPanel->focusPolicy();
    qDebug() << "ConsolePanel: Focus policy of errorPanel:" << this->ui->errorPanel->focusPolicy();

    qDebug() << "ConsolePanel: Constructor complete";
}

/**
 * @brief Destructor
 */
ConsolePanel::~ConsolePanel()
{
    qDebug() << "ConsolePanel: Destructor";

    Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());

    // Cleanup all cached VNCView instances
    for (auto it = this->_vncViews.begin(); it != this->_vncViews.end(); ++it)
    {
        VNCView* view = it.value();
        if (view)
        {
            view->deleteLater();
        }
    }
    this->_vncViews.clear();

    delete this->ui;
}

// ========== Public Methods ==========

void ConsolePanel::PauseAllDockedViews()
{
    qDebug() << "ConsolePanel: pauseAllDockedViews() - pausing" << this->_vncViews.count() << "views";

    // C#: Lines 58-64
    for (auto it = this->_vncViews.begin(); it != this->_vncViews.end(); ++it)
    {
        VNCView* vncView = it.value();
        if (vncView && vncView->IsDocked())
        {
            vncView->Pause();
        }
    }
}

void ConsolePanel::ResetAllViews()
{
    qDebug() << "ConsolePanel: resetAllViews() - clearing cache";

    // C#: Lines 66-69

    // Remove active view from UI
    if (this->_activeVNCView)
    {
        this->ui->consoleLayout->removeWidget(this->_activeVNCView);
        this->_activeVNCView = nullptr;
    }

    // Dispose all cached views
    for (auto it = this->_vncViews.begin(); it != this->_vncViews.end(); ++it)
    {
        VNCView* view = it.value();
        if (view)
        {
            view->deleteLater();
        }
    }

    this->_vncViews.clear();
    this->m_currentVmRef.clear();
}

void ConsolePanel::UnpauseActiveView(bool focus)
{
    qDebug() << "ConsolePanel: unpauseActiveView() - focus:" << focus;

    // C#: Lines 71-91
    // Explicitly pause all docked consoles except the active one

    for (auto it = this->_vncViews.begin(); it != this->_vncViews.end(); ++it)
    {
        VNCView* vncView = it.value();
        if (vncView != this->_activeVNCView && vncView->IsDocked())
        {
            vncView->Pause();
        }
    }

    // Unpause the active view
    if (this->_activeVNCView)
    {
        this->_activeVNCView->Unpause();

        if (focus)
        {
            this->_activeVNCView->FocusConsole();
            this->_activeVNCView->SwitchIfRequired();
        }
    }
}

void ConsolePanel::UpdateRDPResolution(bool fullscreen)
{
    qDebug() << "ConsolePanel: updateRDPResolution() - fullscreen:" << fullscreen;

    // C#: Lines 93-97
    if (this->_activeVNCView)
    {
        this->_activeVNCView->UpdateRDPResolution(fullscreen);
    }
}

void ConsolePanel::SetCurrentSource(QSharedPointer<XenObject> xen_obj)
{
    this->m_currentObject = xen_obj;

    if (!xen_obj || !xen_obj->GetConnection())
    {
        this->_connection = nullptr;
        if (this->_activeVNCView)
        {
            this->ui->consoleLayout->removeWidget(this->_activeVNCView);
            this->_activeVNCView = nullptr;
        }
        this->m_currentVmRef.clear();
        return;
    }

    QString vmRef = xen_obj->OpaqueRef();

    qDebug() << "ConsolePanel: setCurrentSource() - vmRef:" << vmRef;

    this->_connection = xen_obj->GetConnection();

    Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());

    // C#: Lines 99-155

    this->ui->rbacWarningPanel->setVisible(false);

    // Verify connection is still valid before attempting console operations
    if (this->_connection && !this->_connection->IsConnected())
    {
        qWarning() << "ConsolePanel: XenLib connection lost, cannot set console source";
        setErrorMessage(tr("Connection to server lost"));
        this->m_currentVmRef.clear();
        return;
    }

    // Check RBAC permissions
    QStringList allowedRoles;
    if (rbacDenied(vmRef, allowedRoles))
    {
        qDebug() << "ConsolePanel: RBAC denied for VM:" << vmRef;

        if (this->_activeVNCView)
        {
            this->ui->consoleLayout->removeWidget(this->_activeVNCView);
            this->_activeVNCView = nullptr;
        }

        // TODO: Get user's current roles from XenLib
        QStringList userRoles; // Placeholder

        showRbacWarning(userRoles, allowedRoles);
        this->m_currentVmRef = vmRef;
        return;
    }

    // Check if view exists in cache
    if (!this->_vncViews.contains(vmRef))
    {
        qDebug() << "ConsolePanel: Creating new VNCView for VM:" << vmRef;

        QSharedPointer<VM> vm = qSharedPointerCast<VM>(xen_obj);

        if (!vm)
        {
            qWarning() << "ConsolePanel: unable to cast vm object";
            return;
        }

        // Remove oldest view if cache is full
        if (this->_vncViews.count() >= MAX_ACTIVE_VM_CONSOLES)
        {
            evictOldestView();
        }

        // Create new VNCView
        // Note: Using empty elevated credentials (TODO: support elevated credentials)
        VNCView* newView = new VNCView(vm, QString(), QString(), this);
        this->_vncViews[vmRef] = newView;
    }

    // Switch to view if different from active
    VNCView* targetView = this->_vncViews[vmRef];

    if (this->_activeVNCView != targetView)
    {
        qDebug() << "ConsolePanel: Switching active view from"
                 << (this->_activeVNCView ? "existing" : "none")
                 << "to vmRef:" << vmRef;

        // Remove and hide old active view
        if (this->_activeVNCView)
        {
            this->_activeVNCView->Pause();
            this->ui->consoleLayout->removeWidget(this->_activeVNCView);
            this->_activeVNCView->hide();
        }

        // Set new active view
        this->_activeVNCView = targetView;
        this->ui->consoleLayout->addWidget(this->_activeVNCView);
        this->_activeVNCView->show();
    }

    // Refresh ISO list
    this->_activeVNCView->RefreshIsoList();

    clearErrorMessage();
    this->m_currentVmRef = vmRef;
}

void ConsolePanel::SetCurrentSourceHost(QSharedPointer<XenObject> xen_obj)
{
    this->m_currentObject = xen_obj;

    // C#: Lines 157-170

    if (!xen_obj)
    {
        this->_connection = nullptr;
        qDebug() << "ConsolePanel: No host information when connecting to host VNC console";
        this->setErrorMessage(tr("Could not connect to console"));
        return;
    }

    QString hostRef = xen_obj->OpaqueRef();

    qDebug() << "ConsolePanel: setCurrentSourceHost() - hostRef:" << hostRef;

    this->_connection = xen_obj->GetConnection();

    QSharedPointer<XenObject> dom0;

    if (this->_connection)
    {
        qDebug() << "ConsolePanel: No connection available";

        QSharedPointer<Host> host = qSharedPointerCast<Host>(xen_obj);

        if (!host)
        {
            qWarning() << "ConsolePanel: Failed to lookup host from ref: " << hostRef;
        } else
        {
            dom0 = host->GetCache()->ResolveObject<XenObject>("vm", host->ControlDomainRef());
        }
    }

    if (!dom0)
    {
        qDebug() << "ConsolePanel: No dom0 on host when connecting to host VNC console";
        this->setErrorMessage(tr("Could not find console"));
    } else
    {
        this->SetCurrentSource(dom0);
    }
}

QImage ConsolePanel::Snapshot(QSharedPointer<VM> vm, const QString& elevatedUsername, const QString& elevatedPassword)
{
    QString vmRef = vm->OpaqueRef();
    qDebug() << "ConsolePanel: snapshot() - vmRef:" << vmRef << "elevated:" << !elevatedUsername.isEmpty();

    // C#: Lines 197-234
    // Note: C# calls this off-thread, but Qt UI operations must be on main thread
    Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());

    VNCView* view = nullptr;
    bool useElevatedCredentials = false;

    if (!this->_vncViews.contains(vmRef))
    {
        // Use elevated credentials if provided (CA-91132)
        useElevatedCredentials = !elevatedUsername.isEmpty() && !elevatedPassword.isEmpty();

        if (useElevatedCredentials)
        {
            qDebug() << "ConsolePanel: Creating temporary VNCView with elevated credentials";
            view = new VNCView(vm, elevatedUsername, elevatedPassword, this);
        } else
        {
            // Create view normally and add to cache
            this->SetCurrentSource(vm);
            if (this->_vncViews.contains(vmRef))
                view = this->_vncViews[vmRef];
        }
    } else
    {
        view = this->_vncViews[vmRef];
    }

    if (!view)
    {
        qDebug() << "ConsolePanel: Failed to create VNCView for snapshot";
        return QImage();
    }

    // Take Snapshot
    QImage snapshot = view->Snapshot();

    // TODO: Pause view if not currently active
    // view->pause();

    // Cleanup temporary view if using elevated credentials
    if (useElevatedCredentials)
    {
        qDebug() << "ConsolePanel: Disposing temporary VNCView";
        view->deleteLater();
    }

    return snapshot;
}

void ConsolePanel::closeVncForSource(const QString& vmRef)
{
    qDebug() << "ConsolePanel: closeVncForSource() - vmRef:" << vmRef;

    Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());

    // C#: Lines 236-247

    if (!this->_vncViews.contains(vmRef))
        return;

    VNCView* vncView = this->_vncViews[vmRef];

    if (!vncView->IsDocked())
    {
        qDebug() << "ConsolePanel: Not closing undocked view";
        return;
    }

    // Remove from cache and dispose
    this->_vncViews.remove(vmRef);

    if (vncView == this->_activeVNCView)
    {
        this->ui->consoleLayout->removeWidget(vncView);
        this->_activeVNCView = nullptr;
    }

    vncView->deleteLater();
}

void ConsolePanel::SendCAD()
{
    qDebug() << "ConsolePanel: sendCAD()";

    // C#: Lines 259-263
    if (this->_activeVNCView)
    {
        this->_activeVNCView->SendCAD();
    }
}

// ========== Protected Methods ==========

void ConsolePanel::setErrorMessage(const QString& message)
{
    qDebug() << "ConsolePanel: setErrorMessage() -" << message;

    // C#: Lines 249-254
    this->ui->errorLabel->setText(message);
    this->ui->errorPanel->setVisible(true);

    // Clear current source
    this->SetCurrentSource(QSharedPointer<XenObject>());
}

void ConsolePanel::clearErrorMessage()
{
    qDebug() << "ConsolePanel: clearErrorMessage()";

    // C#: Lines 256-259
    this->ui->errorPanel->setVisible(false);
}

// ========== Private Methods ==========

bool ConsolePanel::rbacDenied(const QString& vmRef, QStringList& allowedRoles)
{
    // C#: Lines 172-195

    if (vmRef.isEmpty() || !this->_connection)
    {
        allowedRoles.clear();
        return false;
    }

    // TODO: Implement RBAC checking via XenLib
    // C#: Check session.IsLocalSuperuser, session.Roles, etc.
    // C#: Build role list based on whether VM is control domain
    //     - Control domain: "http/connect_console/host_console"
    //     - Regular VM: "http/connect_console"
    // C#: Check if user's roles intersect with allowed roles

    // For now, allow all access (no RBAC enforcement)
    allowedRoles.clear();
    return false;
}

void ConsolePanel::showRbacWarning(const QStringList& userRoles, const QStringList& allowedRoles)
{
    qDebug() << "ConsolePanel: showRbacWarning() - userRoles:" << userRoles
             << "allowedRoles:" << allowedRoles;

    // C#: Lines 124-133
    // Format warning message based on number of allowed roles

    QString message;
    if (allowedRoles.count() == 1)
    {
        message = tr("You do not have permission to view this console.\n"
                     "Your current role: %1\n"
                     "Required role: %2")
                      .arg(userRoles.join(", "))
                      .arg(allowedRoles.join(", "));
    } else
    {
        message = tr("You do not have permission to view this console.\n"
                     "Your current roles: %1\n"
                     "Required roles: %2")
                      .arg(userRoles.join(", "))
                      .arg(allowedRoles.join(", "));
    }

    this->ui->rbacWarningLabel->setText(message);
    this->ui->rbacWarningPanel->setVisible(true);
}

void ConsolePanel::evictOldestView()
{
    qDebug() << "ConsolePanel: evictOldestView() - cache size:" << this->_vncViews.count();

    // C#: Lines 142-151
    // Remove oldest (first) cached view that is docked

    int toRemoveCount = this->_vncViews.count() - MAX_ACTIVE_VM_CONSOLES + 1;
    if (toRemoveCount <= 0)
        return;

    qDebug() << "ConsolePanel: Removing" << toRemoveCount << "oldest views";

    // Iterate through cache and remove docked views until we've freed enough space
    auto it = this->_vncViews.begin();
    int removed = 0;

    while (it != this->_vncViews.end() && removed < toRemoveCount)
    {
        VNCView* view = it.value();

        if (view->IsDocked())
        {
            qDebug() << "ConsolePanel: Evicting view for VM:" << it.key();

            if (view == this->_activeVNCView)
            {
                this->ui->consoleLayout->removeWidget(view);
                this->_activeVNCView = nullptr;
            }

            view->deleteLater();
            it = this->_vncViews.erase(it);
            removed++;
        } else
        {
            ++it;
        }
    }

    qDebug() << "ConsolePanel: Evicted" << removed << "views, cache size now:" << this->_vncViews.count();
}

// ========== CvmConsolePanel Implementation ==========

CvmConsolePanel::CvmConsolePanel(QWidget* parent)
    : ConsolePanel(parent)
{
    // Just call base class constructor
    // Don't call setupUi() again - already called by ConsolePanel constructor
    qDebug() << "CvmConsolePanel: Constructor (derived class)";
}

void CvmConsolePanel::SetCurrentSourceHost(QSharedPointer<XenObject> xen_obj)
{
    this->m_currentObject = xen_obj;

    // C#: Lines 273-286

    if (!xen_obj)
    {
        this->_connection = nullptr;
        qDebug() << "CvmConsolePanel: No host information when connecting to CVM console";
        setErrorMessage(tr("Could not connect to console"));
        return;
    }

    QString hostRef = xen_obj->OpaqueRef();
    this->_connection = xen_obj->GetConnection();

    // Find CVM (other control domain) for this host
    QSharedPointer<XenObject> cvm_obj = xen_obj->GetCache()->ResolveObject<XenObject>("vm", getOtherControlDomainForHost(hostRef));

    if (!cvm_obj)
    {
        qDebug() << "CvmConsolePanel: Could not find CVM console on host";
        setErrorMessage(tr("Could not find console"));
    } else
    {
        SetCurrentSource(cvm_obj);
    }
}

QString CvmConsolePanel::getOtherControlDomainForHost(const QString& hostRef)
{
    qDebug() << "CvmConsolePanel: getOtherControlDomainForHost() - hostRef:" << hostRef;

    if (!this->_connection)
        return QString();

    // TODO: Implement via XenLib
    // C#: Host.OtherControlDomains().FirstOrDefault() - returns CVM for this host
    // XenAPI: Look for VM where is_control_domain=true and resident_on=hostRef
    //         but exclude dom0 (domid != 0)

    return QString(); // Placeholder
}
