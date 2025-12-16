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

#ifndef CVMCONSOLETABPAGE_H
#define CVMCONSOLETABPAGE_H

#include "basetabpage.h"

// Forward declarations
class ConsolePanel;

namespace Ui
{
    class CvmConsoleTabPage;
}

/**
 * @class CvmConsoleTabPage
 * @brief Tab page for CVM (Citrix VM / driver domain) console display
 *
 * Reference: XenAdmin/MainWindow.cs TabPageCvmConsole
 *
 * This tab page displays the console for storage driver domains (CVMs)
 * associated with storage repositories. It's shown when an SR has a
 * driver domain available.
 *
 * Architecture:
 * - CvmConsoleTabPage is a container for CvmConsolePanel widget
 * - CvmConsolePanel manages the VNC connection to driver domain
 * - MainWindow creates CvmConsolePanel and injects it via setConsolePanel()
 */
class CvmConsoleTabPage : public BaseTabPage
{
    Q_OBJECT

public:
    explicit CvmConsoleTabPage(QWidget* parent = nullptr);
    ~CvmConsoleTabPage() override;

    // ========== BaseTabPage Interface ==========

    /**
     * @brief Get user-visible tab label
     * Reference: C# TabPageCvmConsole text property
     */
    QString tabTitle() const override
    {
        return tr("CVM Console");
    }

    /**
     * @brief Check if this tab is applicable for given object type
     *
     * CVM Console tab is shown for storage repositories that have
     * a driver domain (HasDriverDomain == true).
     *
     * Reference: MainWindow.cs BuildTabList() line 1376
     */
    bool isApplicableForObjectType(const QString& objectType) const override;

    /**
     * @brief Called when page is shown in tab widget
     * Reference: C# TheTabControl_SelectedIndexChanged
     */
    void onPageShown() override;

    /**
     * @brief Called when page is hidden (another tab selected)
     */
    void onPageHidden() override;

    /**
     * @brief Update page content when object data changes
     */
    void refreshContent() override;

    // ========== Console Panel Management ==========

    /**
     * @brief Set the CvmConsolePanel widget to display
     *
     * This is called by MainWindow to inject the shared CvmConsolePanel instance.
     * The panel is owned by MainWindow, not by this tab page.
     *
     * @param panel CvmConsolePanel instance (owned by MainWindow)
     */
    void setConsolePanel(ConsolePanel* panel);

    /**
     * @brief Get current console panel
     * @return CvmConsolePanel instance or nullptr
     */
    ConsolePanel* consolePanel() const
    {
        return m_consolePanel;
    }

private:
    Ui::CvmConsoleTabPage* ui;
    ConsolePanel* m_consolePanel; ///< CvmConsolePanel instance (not owned)
};

#endif // CVMCONSOLETABPAGE_H
