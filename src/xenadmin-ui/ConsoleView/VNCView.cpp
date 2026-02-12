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

#include "VNCView.h"
#include "VNCTabView.h"
#include "xenlib/xencache.h"
#include "xen/vm.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/sr.h"
#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMainWindow>
#include <QApplication>
#include <QThread>
#include <QScreen>
#include <QWindow>

/**
 * @brief Constructor
 * Reference: XenAdmin/ConsoleView/VNCView.cs lines 64-72
 */
VNCView::VNCView(QSharedPointer<VM> vm,  const QString& elevatedUsername, const QString& elevatedPassword, QWidget* parent)
    : QWidget(parent), m_oldUndockedSize(QSize()), m_oldUndockedLocation(QPoint())
{
    if (!vm)
        return;

    this->m_vm = vm;

    qDebug() << "VNCView: Constructor for VM:" << vm->GetName();

    Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());

    // Create VNCTabView (equivalent to C# new VNCTabView(this, source, ...))
    this->m_vncTabView = new VNCTabView(this, vm, elevatedUsername, elevatedPassword, this);

    // Setup UI
    this->setupUI();

    // Register event listeners
    this->registerEventListeners();

    qDebug() << "VNCView: Constructor complete";
}

/**
 * @brief Destructor
 * Reference: XenAdmin/ConsoleView/VNCView.Designer.cs lines 14-33
 */
VNCView::~VNCView()
{
    qDebug() << "VNCView: Destructor";

    Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());

    this->unregisterEventListeners();

    // Cleanup undocked window
    if (this->m_undockedForm)
    {
        this->m_undockedForm->hide();
        this->m_undockedForm->deleteLater();
        this->m_undockedForm = nullptr;
    }

    // VNCTabView will be deleted by Qt parent-child relationship
}

// ========== Public Methods ==========

bool VNCView::IsDocked() const
{
    // C#: public bool IsDocked => undockedForm == null || !undockedForm.Visible;
    return this->m_undockedForm == nullptr || !this->m_undockedForm->isVisible();
}

void VNCView::Pause()
{
    qDebug() << "VNCView: pause()";

    if (this->m_vncTabView)
        this->m_vncTabView->Pause();
}

void VNCView::Unpause()
{
    qDebug() << "VNCView: unpause()";

    if (this->m_vncTabView)
        this->m_vncTabView->Unpause();
}

void VNCView::DockUnDock()
{
    qDebug() << "VNCView: dockUnDock() - current state:" << (this->IsDocked() ? "docked" : "undocked");

    if (this->IsDocked())
    {
        // ========== UNDOCK ==========
        qDebug() << "VNCView: Undocking console to separate window";

        // Create undocked window if it doesn't exist
        if (!this->m_undockedForm)
        {
            this->m_undockedForm = new QMainWindow();
            this->m_undockedForm->setWindowTitle(undockedWindowTitle());

            // TODO: Set window icon
            // C#: undockedForm.Icon = Program.MainWindow.Icon;

            // Connect close event to re-dock
            connect(this->m_undockedForm, &QMainWindow::destroyed, this, [this]()
            {
                qDebug() << "VNCView: Undocked window destroyed, re-docking";
                if (!this->IsDocked())
                    this->DockUnDock();
            });

            // Handle window state changes (minimize → pause)
            connect(this->m_undockedForm, &QWidget::windowTitleChanged, this, [this]()
            {
                // Window state changed
                Qt::WindowStates state = this->m_undockedForm->windowState();

                if (state & Qt::WindowMinimized)
                {
                    qDebug() << "VNCView: Undocked window minimized, pausing console";
                    this->m_vncTabView->Pause();
                } else
                {
                    qDebug() << "VNCView: Undocked window restored, unpausing console";
                    this->m_vncTabView->Unpause();
                }
            });

            // C#: Set up Resize event
            // Qt doesn't have ResizeEnd, so we'll use resizeEvent + timer
            this->m_undockedFormResized = false;
        }

        // Remove VNCTabView from this widget
        QLayout* currentLayout = layout();
        if (currentLayout)
        {
            currentLayout->removeWidget(this->m_vncTabView);
        } else
        {
            this->m_vncTabView->setParent(nullptr);
        }

        // Add VNCTabView to undocked window
        this->m_undockedForm->setCentralWidget(this->m_vncTabView);

        // Save scaled setting
        this->m_oldScaledSetting = this->m_vncTabView->IsScaled();

        // TODO: Show header bar
        // C#: vncTabView.showHeaderBar(!source.is_control_domain, true);

        // Calculate size to fit console
        QSize growSize = this->m_vncTabView->GrowToFit();
        this->m_undockedForm->resize(growSize);

        // Restore previous geometry if available and on-screen
        if (!this->m_oldUndockedSize.isEmpty() && !this->m_oldUndockedLocation.isNull())
        {
            // TODO: Check if window is on screen
            // C#: HelpersGUI.WindowIsOnScreen(oldUndockedLocation, oldUndockedSize)

            bool isOnScreen = true; // Placeholder

            if (isOnScreen)
            {
                this->m_undockedForm->resize(this->m_oldUndockedSize);
                this->m_undockedForm->move(this->m_oldUndockedLocation);
            }
        }

        // Show undocked window
        this->m_undockedForm->show();

        // TODO: Preserve scale setting when undocked
        // C#: if(Properties.Settings.Default.PreserveScaleWhenUndocked)
        //         vncTabView.IsScaled = oldScaledSetting;

        // Show find/reattach buttons in this widget
        this->m_findConsoleButton->show();
        this->m_reattachConsoleButton->show();
    } else
    {
        // ========== DOCK ==========
        qDebug() << "VNCView: Docking console back to main window";

        // Save undocked window geometry
        this->m_oldUndockedLocation = this->m_undockedForm->pos();
        this->m_oldUndockedSize = this->m_undockedForm->size();

        // TODO: Restore scale setting when docking
        // C#: if (!Properties.Settings.Default.PreserveScaleWhenUndocked)
        //         vncTabView.IsScaled = oldScaledSetting;

        // Hide find/reattach buttons
        this->m_findConsoleButton->hide();
        this->m_reattachConsoleButton->hide();

        // Hide undocked window
        this->m_undockedForm->hide();

        // TODO: Hide header bar
        // C#: vncTabView.showHeaderBar(true, false);

        // Remove VNCTabView from undocked window
        this->m_undockedForm->takeCentralWidget();

        // Add VNCTabView back to this widget
        QLayout* currentLayout = layout();
        if (currentLayout)
        {
            currentLayout->addWidget(this->m_vncTabView);
        } else
        {
            // Create layout if it doesn't exist
            QVBoxLayout* layout = new QVBoxLayout(this);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->addWidget(this->m_vncTabView);
            setLayout(layout);
        }

        // Cleanup undocked window
        this->m_undockedForm->deleteLater();
        this->m_undockedForm = nullptr;
    }

    // Update dock button icon/text
    this->m_vncTabView->UpdateDockButton();

    // Update parent minimum size
    this->m_vncTabView->UpdateParentMinimumSize();

    // Always unpause when docking/undocking (ensure visible console is not paused)
    this->m_vncTabView->Unpause();

    // TODO: Focus VNC
    // C#: vncTabView.focus_vnc();

    // TODO: Reconnect RDP with new dimensions
    // C#: UpdateRDPResolution();

    qDebug() << "VNCView: Dock/undock complete, new state:" << (this->IsDocked() ? "docked" : "undocked");
}

void VNCView::SendCAD()
{
    qDebug() << "VNCView: sendCAD()";

    if (this->m_vncTabView)
        this->m_vncTabView->SendCAD();
}

void VNCView::FocusConsole()
{
    qDebug() << "VNCView: focusConsole()";

    if (this->m_vncTabView)
    {
        // TODO: Implement focus_vnc() in VNCTabView
        // this->_vncTabView->focus_vnc();
        this->m_vncTabView->setFocus();
    }
}

void VNCView::SwitchIfRequired()
{
    qDebug() << "VNCView: switchIfRequired()";

    if (this->m_vncTabView)
    {
        // TODO: Implement SwitchIfRequired() in VNCTabView
        // this->_vncTabView->SwitchIfRequired();
    }
}

QImage VNCView::Snapshot()
{
    qDebug() << "VNCView: snapshot()";

    if (this->m_vncTabView)
        return this->m_vncTabView->Snapshot();

    return QImage();
}

void VNCView::RefreshIsoList()
{
    qDebug() << "VNCView: refreshIsoList()";

    if (this->m_vncTabView)
        this->m_vncTabView->SetupCD();
}

void VNCView::UpdateRDPResolution(bool fullscreen)
{
    qDebug() << "VNCView: updateRDPResolution() - fullscreen:" << fullscreen;

    if (this->m_vncTabView)
    {
        // TODO: Implement UpdateRDPResolution() in VNCTabView
        // this->_vncTabView->UpdateRDPResolution(fullscreen);
    }
}

// ========== Private Slots ==========

void VNCView::onVMPropertyChanged(const QString& propertyName)
{
    qDebug() << "VNCView: onVMPropertyChanged:" << propertyName;

    // Update undocked window title if name changed
    if (propertyName == "name_label" && this->m_undockedForm)
    {
        this->m_undockedForm->setWindowTitle(undockedWindowTitle());
    }
}

void VNCView::onVmDataChanged()
{
    if (this->m_undockedForm)
        this->m_undockedForm->setWindowTitle(undockedWindowTitle());
}

void VNCView::onCacheObjectChanged(XenConnection* connection, const QString& objectType, const QString& objectRef)
{
    Q_UNUSED(objectRef);

    if (!this->m_undockedForm || !this->m_vm)
        return;

    if (!this->m_vm->GetConnection() || this->m_vm->GetConnection() != connection)
        return;

    const XenObjectType type = XenCache::TypeFromString(objectType);
    if (type == XenObjectType::VM
        || type == XenObjectType::Host
        || type == XenObjectType::SR
        || type == XenObjectType::PBD)
        this->m_undockedForm->setWindowTitle(undockedWindowTitle());
}

void VNCView::onFindConsoleButtonClicked()
{
    qDebug() << "VNCView: onFindConsoleButtonClicked()";

    // C#: Lines 215-220
    // Bring undocked window to front

    if (!this->IsDocked() && this->m_undockedForm)
    {
        this->m_undockedForm->raise();
        this->m_undockedForm->activateWindow();

        if (this->m_undockedForm->windowState() & Qt::WindowMinimized)
        {
            this->m_undockedForm->setWindowState(Qt::WindowNoState);
        }
    }
}

void VNCView::onReattachConsoleButtonClicked()
{
    qDebug() << "VNCView: onReattachConsoleButtonClicked()";

    // C#: Lines 222-225
    // Re-dock the console
    DockUnDock();
}

void VNCView::onUndockedWindowStateChanged(Qt::WindowStates oldState, Qt::WindowStates newState)
{
    qDebug() << "VNCView: onUndockedWindowStateChanged()";

    // TODO: Implement window state monitoring
}

void VNCView::onUndockedWindowResizeEnd()
{
    qDebug() << "VNCView: onUndockedWindowResizeEnd()";

    // TODO: Update RDP resolution on resize
    // if (this->_undockedFormResized)
    //     updateRDPResolution();
    // this->_undockedFormResized = false;
}

// ========== Private Methods ==========

void VNCView::registerEventListeners()
{
    qDebug() << "VNCView: registerEventListeners()";

    // Connect VNCTabView signals (C#: VNCTabView calls parentVNCView.DockUnDock())
    if (this->m_vncTabView)
    {
        connect(this->m_vncTabView, &VNCTabView::toggleDockRequested, this, &VNCView::DockUnDock);
        // TODO: Implement fullscreen (VNCTabView.toggleFullscreen creates FullScreenForm)
        connect(this->m_vncTabView, &VNCTabView::toggleFullscreenRequested, this, []() {
            qWarning() << "VNCView: Fullscreen not yet implemented";
        });
    }

    if (this->m_vm)
        connect(this->m_vm.data(), &XenObject::dataChanged, this, &VNCView::onVmDataChanged);

    if (this->m_vm && this->m_vm->GetConnection() && this->m_vm->GetConnection()->GetCache())
    {
        connect(this->m_vm->GetConnection()->GetCache(),
                &XenCache::objectChanged,
                this,
                &VNCView::onCacheObjectChanged);
    }
}

void VNCView::unregisterEventListeners()
{
    qDebug() << "VNCView: unregisterEventListeners()";

    // Disconnect VNCTabView signals
    if (this->m_vncTabView)
    {
        disconnect(this->m_vncTabView, &VNCTabView::toggleDockRequested, this, &VNCView::DockUnDock);
        disconnect(this->m_vncTabView, &VNCTabView::toggleFullscreenRequested, nullptr, nullptr);
    }

    if (this->m_vm)
        disconnect(this->m_vm.data(), &XenObject::dataChanged, this, &VNCView::onVmDataChanged);

    if (this->m_vm && this->m_vm->GetConnection() && this->m_vm->GetConnection()->GetCache())
    {
        disconnect(this->m_vm->GetConnection()->GetCache(),
                   &XenCache::objectChanged,
                   this,
                   &VNCView::onCacheObjectChanged);
    }
}

QString VNCView::undockedWindowTitle() const
{
    // C#: Lines 189-200
    // Return VM name, or "Host: hostname" for control domain, or "SR Driver Domain: srname"

    if (!this->m_vm)
        return tr("Console");

    XenCache* cache = this->m_vm->GetCache();
    if (cache)
    {
        if (this->m_vm->IsControlDomain())
        {
            const QString hostRef = this->m_vm->GetResidentOnRef();
            if (!hostRef.isEmpty() && hostRef != XENOBJECT_NULL)
            {
                QSharedPointer<Host> host = cache->ResolveObject<Host>(XenObjectType::Host, hostRef);
                if (host && host->IsValid())
                {
                    const bool isControlDomainZero =
                        (host->ControlDomainRef() == this->m_vm->OpaqueRef()) || (this->m_vm->Domid() == 0);
                    if (isControlDomainZero)
                        return tr("Host: %1").arg(host->GetName());
                }
            }
        }

        const QList<QSharedPointer<SR>> srs = cache->GetAll<SR>();
        for (const QSharedPointer<SR>& sr : srs)
        {
            if (!sr || !sr->IsValid())
                continue;

            QString driverDomainRef;
            if (sr->HasDriverDomain(&driverDomainRef) && driverDomainRef == this->m_vm->OpaqueRef())
                return tr("SR Driver Domain: %1").arg(sr->GetName());
        }
    }

    const QString vmName = this->m_vm->GetName();
    if (!vmName.isEmpty())
        return vmName;

    return QString("Console: %1").arg(this->m_vm->OpaqueRef());
}

void VNCView::setupUI()
{
    qDebug() << "VNCView: setupUI()";

    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

    // Add VNCTabView (fills most of the space)
    mainLayout->addWidget(this->m_vncTabView, 1);

    // Create button layout for find/reattach buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    buttonLayout->setContentsMargins(8, 4, 8, 8);

    // Create "Find Console" button
    this->m_findConsoleButton = new QPushButton(tr("Find Console"), this);
    this->m_findConsoleButton->setToolTip(tr("Bring the undocked console window to front"));
    this->m_findConsoleButton->hide(); // Hidden when docked
    connect(this->m_findConsoleButton, &QPushButton::clicked, this, &VNCView::onFindConsoleButtonClicked);

    // Create "Reattach Console" button
    this->m_reattachConsoleButton = new QPushButton(tr("Reattach Console"), this);
    this->m_reattachConsoleButton->setToolTip(tr("Dock the console back to the main window"));
    this->m_reattachConsoleButton->hide(); // Hidden when docked
    connect(this->m_reattachConsoleButton, &QPushButton::clicked, this, &VNCView::onReattachConsoleButtonClicked);

    // Add buttons to layout
    buttonLayout->addStretch();
    buttonLayout->addWidget(this->m_findConsoleButton);
    buttonLayout->addWidget(this->m_reattachConsoleButton);

    // Add button layout to main layout
    mainLayout->addLayout(buttonLayout);

    this->setLayout(mainLayout);

    qDebug() << "VNCView: setupUI() complete";
}
