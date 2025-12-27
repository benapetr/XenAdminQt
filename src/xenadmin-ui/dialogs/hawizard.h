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

#ifndef HAWIZARD_H
#define HAWIZARD_H

#include <QWizard>
#include <QWizardPage>
#include <QLabel>
#include <QTextEdit>
#include <QTableWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QProgressBar>
#include <QMap>
#include <QVariantMap>
#include <QStringList>

class XenConnection;
class XenCache;

/**
 * @brief The HAWizard class provides a wizard for enabling High Availability on a pool.
 *
 * Matches the C# XenAdmin/Wizards/HAWizard.cs implementation.
 *
 * The wizard has the following pages:
 * 1. Intro - Explains what HA does
 * 2. ChooseSR - Select heartbeat SR for HA statefile
 * 3. AssignPriorities - Configure VM restart priorities and ntol
 * 4. Finish - Summary of configuration before enabling
 */
class HAWizard : public QWizard
{
    Q_OBJECT

    public:
        explicit HAWizard(XenConnection* connection, const QString& poolRef, QWidget* parent = nullptr);

        // Page IDs
        enum PageIds
        {
            Page_Intro = 0,
            Page_ChooseSR = 1,
            Page_AssignPriorities = 2,
            Page_Finish = 3
        };

        // VM restart priority enumeration (matches C# VM.HaRestartPriority)
        enum HaRestartPriority
        {
            AlwaysRestartHighPriority,
            AlwaysRestart, // Also known as "Restart"
            BestEffort,
            DoNotRestart
        };

        // Get selected heartbeat SR ref
        QString selectedHeartbeatSRRef() const
        {
            return m_selectedHeartbeatSR;
        }

        // Get configured NTOL (number of host failures to tolerate)
        qint64 ntol() const
        {
            return m_ntol;
        }

        // Get VM startup options map: VM ref -> {ha_restart_priority, order, start_delay}
        QMap<QString, QVariantMap> vmStartupOptions() const
        {
            return m_vmStartupOptions;
        }

    protected:
        void initializePage(int id) override;
        bool validateCurrentPage() override;
        void accept() override;

    private slots:
        void onHeartbeatSRSelectionChanged();
        void onPriorityChanged(int row, int column);
        void onNtolChanged(int value);
        void scanForHeartbeatSRs();
        void updateFinishPage();

    private:
        XenCache* cache() const;
        QWizardPage* createIntroPage();
        QWizardPage* createChooseSRPage();
        QWizardPage* createAssignPrioritiesPage();
        QWizardPage* createFinishPage();

        void populateVMTable();
        void updateNtolCalculation();
        QString priorityToString(HaRestartPriority priority) const;
        HaRestartPriority stringToPriority(const QString& str) const;
        int countVMsByPriority(HaRestartPriority priority) const;

        XenConnection* m_connection;
        QString m_poolRef;
        QString m_poolName;

        // ChooseSR page widgets
        QTableWidget* m_srTable;
        QLabel* m_noSRsLabel;
        QProgressBar* m_scanProgress;
        QPushButton* m_rescanButton;

        // AssignPriorities page widgets
        QTableWidget* m_vmTable;
        QSpinBox* m_ntolSpinBox;
        QLabel* m_ntolStatusLabel;
        QLabel* m_maxNtolLabel;

        // Finish page widgets
        QLabel* m_finishSRLabel;
        QLabel* m_finishNtolLabel;
        QLabel* m_finishRestartLabel;
        QLabel* m_finishBestEffortLabel;
        QLabel* m_finishDoNotRestartLabel;
        QLabel* m_finishWarningLabel;
        QLabel* m_finishWarningIcon;

        // State
        QString m_selectedHeartbeatSR;
        QString m_selectedHeartbeatSRName;
        qint64 m_ntol;
        qint64 m_maxNtol;
        QMap<QString, QVariantMap> m_vmStartupOptions;
        QMap<QString, HaRestartPriority> m_vmPriorities;

        // SR list from scan
        QList<QPair<QString, QString>> m_heartbeatSRs; // ref, name pairs
};

#endif // HAWIZARD_H
