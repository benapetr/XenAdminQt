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

#include "consoletabpage.h"
#include "ui_consoletabpage.h"
#include "../ConsoleView/ConsolePanel.h"
#include <QDebug>

/**
 * @brief Constructor
 * Reference: MainWindow.cs InitializeComponent - console tab setup
 */
ConsoleTabPage::ConsoleTabPage(QWidget* parent)
    : BaseTabPage(parent), ui(new Ui::ConsoleTabPage), m_consolePanel(nullptr)
{
    qDebug() << "ConsoleTabPage: Constructor";

    ui->setupUi(this);

    qDebug() << "ConsoleTabPage: Constructor complete";
}

/**
 * @brief Destructor
 */
ConsoleTabPage::~ConsoleTabPage()
{
    qDebug() << "ConsoleTabPage: Destructor";

    // ConsolePanel is owned by MainWindow, don't delete it here

    delete ui;
}

/**
 * @brief Check if tab is applicable for object type
 * Reference: Console tab is shown for VMs and Hosts
 */
bool ConsoleTabPage::IsApplicableForObjectType(const QString& objectType) const
{
    // Console tab is applicable to VMs and Hosts
    return objectType == "vm" || objectType == "host";
}

/**
 * @brief Set the ConsolePanel instance (called by MainWindow)
 * Reference: MainWindow.cs AddTabContents() line 186
 */
void ConsoleTabPage::SetConsolePanel(ConsolePanel* consolePanel)
{
    qDebug() << "ConsoleTabPage: setConsolePanel()";

    if (m_consolePanel == consolePanel)
        return;

    // Remove old panel if any
    if (m_consolePanel)
    {
        ui->consolePanelLayout->removeWidget(m_consolePanel);
    }

    m_consolePanel = consolePanel;

    // Add new panel to layout
    if (m_consolePanel)
    {
        ui->consolePanelLayout->addWidget(m_consolePanel);
    }
}

/**
 * @brief Called when console tab becomes visible
 * Reference: MainWindow.cs TheTabControl_SelectedIndexChanged (lines 1653-1667)
 *
 * C# logic:
 * - Pause CvmConsolePanel (other console)
 * - If VM selected: ConsolePanel.SetCurrentSource(vm)
 * - If Host selected: ConsolePanel.SetCurrentSource(host)
 * - ConsolePanel.UnpauseActiveView(focusIfTabClicked)
 * - ConsolePanel.UpdateRDPResolution()
 */
void ConsoleTabPage::OnPageShown()
{
    qDebug() << "ConsoleTabPage: onPageShown()";

    // Note: MainWindow handles the actual console switching logic
    // This is just a notification that the tab is now visible
    //
    // MainWindow will call:
    // 1. consolePanel->setCurrentSource(vmRef or hostRef)
    // 2. consolePanel->unpauseActiveView(true)
    // 3. consolePanel->updateRDPResolution()
}

/**
 * @brief Called when console tab is hidden
 * Reference: MainWindow.cs TheTabControl_Deselected (lines 1628-1636)
 *
 * C# logic:
 * - Call PageHidden() on the tab page being hidden
 * - Tab page can perform cleanup if needed
 */
void ConsoleTabPage::OnPageHidden()
{
    qDebug() << "ConsoleTabPage: onPageHidden()";

    // Note: MainWindow handles pausing consoles when switching away
    // This is just a notification that the tab is now hidden
    //
    // MainWindow will call:
    // consolePanel->pauseAllDockedViews()
}

/**
 * @brief Refresh content - not used with ConsolePanel
 *
 * ConsolePanel handles all content updates internally.
 * MainWindow calls consolePanel->setCurrentSource() directly.
 */
void ConsoleTabPage::refreshContent()
{
    // Not used - ConsolePanel updates are handled by MainWindow
    // via setCurrentSource() calls in tab selection changed handler
}
