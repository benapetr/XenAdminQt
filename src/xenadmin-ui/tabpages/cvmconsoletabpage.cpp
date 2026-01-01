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

#include "cvmconsoletabpage.h"
#include "ui_cvmconsoletabpage.h"
#include "../ConsoleView/ConsolePanel.h"
#include <QVBoxLayout>
#include <QDebug>

/**
 * @brief Constructor
 */
CvmConsoleTabPage::CvmConsoleTabPage(QWidget* parent)
    : BaseTabPage(parent), ui(new Ui::CvmConsoleTabPage), m_consolePanel(nullptr)
{
    qDebug() << "CvmConsoleTabPage: Constructor";
    ui->setupUi(this);
}

/**
 * @brief Destructor
 */
CvmConsoleTabPage::~CvmConsoleTabPage()
{
    qDebug() << "CvmConsoleTabPage: Destructor";

    // ConsolePanel is owned by MainWindow, don't delete it
    // Just remove from layout if present
    if (m_consolePanel && ui->cvmConsolePanelLayout->indexOf(m_consolePanel) >= 0)
    {
        ui->cvmConsolePanelLayout->removeWidget(m_consolePanel);
    }

    delete ui;
}

/**
 * @brief Check if this tab is applicable for given object type
 *
 * Reference: MainWindow.cs BuildTabList() line 1376:
 * if (consoleFeatures.Count == 0 && !multi && !SearchMode && isSRSelected && selectedSr.HasDriverDomain(out _))
 *     newTabs.Add(TabPageCvmConsole);
 */
bool CvmConsoleTabPage::IsApplicableForObjectType(const QString& objectType) const
{
    // CVM Console tab is only shown for storage repositories (SR)
    // MainWindow will check if SR has driver domain before showing this tab
    return objectType == "sr";
}

/**
 * @brief Called when page is shown in tab widget
 *
 * This is just a notification - actual console switching happens
 * in MainWindow::onTabChanged()
 */
void CvmConsoleTabPage::OnPageShown()
{
    qDebug() << "CvmConsoleTabPage: onPageShown()";
    // MainWindow handles console unpause in onTabChanged()
}

/**
 * @brief Called when page is hidden (another tab selected)
 */
void CvmConsoleTabPage::OnPageHidden()
{
    qDebug() << "CvmConsoleTabPage: onPageHidden()";
    // MainWindow handles console pause in onTabChanged()
}

/**
 * @brief Update page content when object data changes
 */
void CvmConsoleTabPage::refreshContent()
{
    // CvmConsolePanel handles its own updates
}

/**
 * @brief Set the CvmConsolePanel widget to display
 *
 * Reference: C# AddTabContents(CvmConsolePanel, TabPageCvmConsole) - line 187
 */
void CvmConsoleTabPage::setConsolePanel(ConsolePanel* panel)
{
    qDebug() << "CvmConsoleTabPage: setConsolePanel() - panel:" << panel;

    // Remove old panel if present
    if (m_consolePanel && ui->cvmConsolePanelLayout->indexOf(m_consolePanel) >= 0)
    {
        ui->cvmConsolePanelLayout->removeWidget(m_consolePanel);
        m_consolePanel->setParent(nullptr);
    }

    m_consolePanel = panel;

    // Add new panel to layout
    if (m_consolePanel)
    {
        ui->cvmConsolePanelLayout->addWidget(m_consolePanel);
        m_consolePanel->setParent(this);
    }
}
