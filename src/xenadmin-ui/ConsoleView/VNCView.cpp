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
#include "xen/vm.h"
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
    : QWidget(parent), _oldUndockedSize(QSize()), _oldUndockedLocation(QPoint())
{
    if (!vm)
        return;

    this->m_vm = vm;

    qDebug() << "VNCView: Constructor for VM:" << vm->GetName();

    Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());

    // Create VNCTabView (equivalent to C# new VNCTabView(this, source, ...))
    this->_vncTabView = new VNCTabView(this, vm, elevatedUsername, elevatedPassword, this);

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
    if (this->_undockedForm)
    {
        this->_undockedForm->hide();
        this->_undockedForm->deleteLater();
        this->_undockedForm = nullptr;
    }

    // VNCTabView will be deleted by Qt parent-child relationship
}

// ========== Public Methods ==========

bool VNCView::IsDocked() const
{
    // C#: public bool IsDocked => undockedForm == null || !undockedForm.Visible;
    return this->_undockedForm == nullptr || !this->_undockedForm->isVisible();
}

void VNCView::Pause()
{
    qDebug() << "VNCView: pause()";

    if (this->_vncTabView)
        this->_vncTabView->Pause();
}

void VNCView::Unpause()
{
    qDebug() << "VNCView: unpause()";

    if (this->_vncTabView)
        this->_vncTabView->Unpause();
}

void VNCView::DockUnDock()
{
    qDebug() << "VNCView: dockUnDock() - current state:" << (this->IsDocked() ? "docked" : "undocked");

    if (this->IsDocked())
    {
        // ========== UNDOCK ==========
        qDebug() << "VNCView: Undocking console to separate window";

        // Create undocked window if it doesn't exist
        if (!this->_undockedForm)
        {
            this->_undockedForm = new QMainWindow();
            this->_undockedForm->setWindowTitle(undockedWindowTitle());

            // TODO: Set window icon
            // C#: undockedForm.Icon = Program.MainWindow.Icon;

            // Connect close event to re-dock
            connect(this->_undockedForm, &QMainWindow::destroyed, this, [this]()
            {
                qDebug() << "VNCView: Undocked window destroyed, re-docking";
                if (!this->IsDocked())
                    DockUnDock();
            });

            // Handle window state changes (minimize → pause)
            connect(this->_undockedForm, &QWidget::windowTitleChanged, this, [this]()
            {
                // Window state changed
                Qt::WindowStates state = this->_undockedForm->windowState();

                if (state & Qt::WindowMinimized)
                {
                    qDebug() << "VNCView: Undocked window minimized, pausing console";
                    this->_vncTabView->Pause();
                } else
                {
                    qDebug() << "VNCView: Undocked window restored, unpausing console";
                    this->_vncTabView->Unpause();
                }
            });

            // C#: Set up Resize event
            // Qt doesn't have ResizeEnd, so we'll use resizeEvent + timer
            this->_undockedFormResized = false;
        }

        // Remove VNCTabView from this widget
        QLayout* currentLayout = layout();
        if (currentLayout)
        {
            currentLayout->removeWidget(this->_vncTabView);
        } else
        {
            this->_vncTabView->setParent(nullptr);
        }

        // Add VNCTabView to undocked window
        this->_undockedForm->setCentralWidget(this->_vncTabView);

        // Save scaled setting
        this->_oldScaledSetting = this->_vncTabView->IsScaled();

        // TODO: Show header bar
        // C#: vncTabView.showHeaderBar(!source.is_control_domain, true);

        // Calculate size to fit console
        QSize growSize = this->_vncTabView->GrowToFit();
        this->_undockedForm->resize(growSize);

        // Restore previous geometry if available and on-screen
        if (!this->_oldUndockedSize.isEmpty() && !this->_oldUndockedLocation.isNull())
        {
            // TODO: Check if window is on screen
            // C#: HelpersGUI.WindowIsOnScreen(oldUndockedLocation, oldUndockedSize)

            bool isOnScreen = true; // Placeholder

            if (isOnScreen)
            {
                this->_undockedForm->resize(this->_oldUndockedSize);
                this->_undockedForm->move(this->_oldUndockedLocation);
            }
        }

        // Show undocked window
        this->_undockedForm->show();

        // TODO: Preserve scale setting when undocked
        // C#: if(Properties.Settings.Default.PreserveScaleWhenUndocked)
        //         vncTabView.IsScaled = oldScaledSetting;

        // Show find/reattach buttons in this widget
        this->_findConsoleButton->show();
        this->_reattachConsoleButton->show();
    } else
    {
        // ========== DOCK ==========
        qDebug() << "VNCView: Docking console back to main window";

        // Save undocked window geometry
        this->_oldUndockedLocation = this->_undockedForm->pos();
        this->_oldUndockedSize = this->_undockedForm->size();

        // TODO: Restore scale setting when docking
        // C#: if (!Properties.Settings.Default.PreserveScaleWhenUndocked)
        //         vncTabView.IsScaled = oldScaledSetting;

        // Hide find/reattach buttons
        this->_findConsoleButton->hide();
        this->_reattachConsoleButton->hide();

        // Hide undocked window
        this->_undockedForm->hide();

        // TODO: Hide header bar
        // C#: vncTabView.showHeaderBar(true, false);

        // Remove VNCTabView from undocked window
        this->_undockedForm->takeCentralWidget();

        // Add VNCTabView back to this widget
        QLayout* currentLayout = layout();
        if (currentLayout)
        {
            currentLayout->addWidget(this->_vncTabView);
        } else
        {
            // Create layout if it doesn't exist
            QVBoxLayout* layout = new QVBoxLayout(this);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->addWidget(this->_vncTabView);
            setLayout(layout);
        }

        // Cleanup undocked window
        this->_undockedForm->deleteLater();
        this->_undockedForm = nullptr;
    }

    // Update dock button icon/text
    this->_vncTabView->UpdateDockButton();

    // Update parent minimum size
    this->_vncTabView->UpdateParentMinimumSize();

    // Always unpause when docking/undocking (ensure visible console is not paused)
    this->_vncTabView->Unpause();

    // TODO: Focus VNC
    // C#: vncTabView.focus_vnc();

    // TODO: Reconnect RDP with new dimensions
    // C#: UpdateRDPResolution();

    qDebug() << "VNCView: Dock/undock complete, new state:" << (this->IsDocked() ? "docked" : "undocked");
}

void VNCView::SendCAD()
{
    qDebug() << "VNCView: sendCAD()";

    if (this->_vncTabView)
        this->_vncTabView->SendCAD();
}

void VNCView::FocusConsole()
{
    qDebug() << "VNCView: focusConsole()";

    if (this->_vncTabView)
    {
        // TODO: Implement focus_vnc() in VNCTabView
        // this->_vncTabView->focus_vnc();
        this->_vncTabView->setFocus();
    }
}

void VNCView::SwitchIfRequired()
{
    qDebug() << "VNCView: switchIfRequired()";

    if (this->_vncTabView)
    {
        // TODO: Implement SwitchIfRequired() in VNCTabView
        // this->_vncTabView->SwitchIfRequired();
    }
}

QImage VNCView::Snapshot()
{
    qDebug() << "VNCView: snapshot()";

    if (this->_vncTabView)
    {
        // TODO: Implement Snapshot() in VNCTabView
        // return this->_vncTabView->Snapshot();
        return QImage();
    }

    return QImage();
}

void VNCView::RefreshIsoList()
{
    qDebug() << "VNCView: refreshIsoList()";

    if (this->_vncTabView)
        this->_vncTabView->SetupCD();
}

void VNCView::UpdateRDPResolution(bool fullscreen)
{
    qDebug() << "VNCView: updateRDPResolution() - fullscreen:" << fullscreen;

    if (this->_vncTabView)
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
    if (propertyName == "name_label" && this->_undockedForm)
    {
        this->_undockedForm->setWindowTitle(undockedWindowTitle());
    }
}

void VNCView::onFindConsoleButtonClicked()
{
    qDebug() << "VNCView: onFindConsoleButtonClicked()";

    // C#: Lines 215-220
    // Bring undocked window to front

    if (!this->IsDocked() && this->_undockedForm)
    {
        this->_undockedForm->raise();
        this->_undockedForm->activateWindow();

        if (this->_undockedForm->windowState() & Qt::WindowMinimized)
        {
            this->_undockedForm->setWindowState(Qt::WindowNoState);
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
    if (this->_vncTabView)
    {
        connect(this->_vncTabView, &VNCTabView::toggleDockRequested, this, &VNCView::DockUnDock);
        // TODO: Implement fullscreen (VNCTabView.toggleFullscreen creates FullScreenForm)
        connect(this->_vncTabView, &VNCTabView::toggleFullscreenRequested, this, []() {
            qWarning() << "VNCView: Fullscreen not yet implemented";
        });
    }

    // TODO: Connect to XenLib VM property change signals
    // C#: source.PropertyChanged += Server_PropertyChanged;
}

void VNCView::unregisterEventListeners()
{
    qDebug() << "VNCView: unregisterEventListeners()";

    // Disconnect VNCTabView signals
    if (this->_vncTabView)
    {
        disconnect(this->_vncTabView, &VNCTabView::toggleDockRequested, this, &VNCView::DockUnDock);
        disconnect(this->_vncTabView, &VNCTabView::toggleFullscreenRequested, nullptr, nullptr);
    }

    // TODO: Disconnect from XenLib VM property change signals
    // C#: source.PropertyChanged -= Server_PropertyChanged;
}

QString VNCView::undockedWindowTitle() const
{
    // C#: Lines 189-200
    // Return VM name, or "Host: hostname" for control domain, or "SR Driver Domain: srname"

    // TODO: Check if VM is control domain or SR driver domain
    // C#: if (vm.IsControlDomainZero(out Host host))
    //         return string.Format(Messages.CONSOLE_HOST, host.Name());
    // C#: if (vm.IsSrDriverDomain(out SR sr))
    //         return string.Format(Messages.CONSOLE_SR_DRIVER_DOMAIN, sr.Name());

    // For now, just return VM ref (will be replaced with VM name from XenLib)
    if (!this->m_vm)
        return "NULL";
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
    mainLayout->addWidget(this->_vncTabView, 1);

    // Create button layout for find/reattach buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    buttonLayout->setContentsMargins(8, 4, 8, 8);

    // Create "Find Console" button
    this->_findConsoleButton = new QPushButton(tr("Find Console"), this);
    this->_findConsoleButton->setToolTip(tr("Bring the undocked console window to front"));
    this->_findConsoleButton->hide(); // Hidden when docked
    connect(this->_findConsoleButton, &QPushButton::clicked, this, &VNCView::onFindConsoleButtonClicked);

    // Create "Reattach Console" button
    this->_reattachConsoleButton = new QPushButton(tr("Reattach Console"), this);
    this->_reattachConsoleButton->setToolTip(tr("Dock the console back to the main window"));
    this->_reattachConsoleButton->hide(); // Hidden when docked
    connect(this->_reattachConsoleButton, &QPushButton::clicked, this, &VNCView::onReattachConsoleButtonClicked);

    // Add buttons to layout
    buttonLayout->addStretch();
    buttonLayout->addWidget(this->_findConsoleButton);
    buttonLayout->addWidget(this->_reattachConsoleButton);

    // Add button layout to main layout
    mainLayout->addLayout(buttonLayout);

    this->setLayout(mainLayout);

    qDebug() << "VNCView: setupUI() complete";
}
