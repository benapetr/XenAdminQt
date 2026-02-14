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
#include <QSharedPointer>
#include <QSet>

class XenCache;
class Pool;
class VM;
class XenConnection;
class WizardNavigationPane;
class QShowEvent;
namespace Ui
{
    class HAWizard;
}

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
        explicit HAWizard(QSharedPointer<Pool> pool, QWidget* parent = nullptr);
        ~HAWizard() override;

        // Page IDs
        enum PageIds
        {
            Page_Intro = 0,
            Page_RbacWarning = 1,
            Page_ChooseSR = 2,
            Page_AssignPriorities = 3,
            Page_Finish = 4
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
        void showEvent(QShowEvent* event) override;
        void initializePage(int id) override;
        bool validateCurrentPage() override;
        void accept() override;

    private slots:
        void onCurrentIdChanged(int id);
        void onHeartbeatSRSelectionChanged();
        void onNtolChanged(int value);
        void onVmSelectionChanged();
        void onSelectedPriorityChanged(int index);
        void onSelectedOrderChanged(int value);
        void onSelectedDelayChanged(int value);
        void scanForHeartbeatSRs();
        void updateFinishPage();

    private:
        XenCache* cache() const;
        QWizardPage* createIntroPage();
        QWizardPage* createRbacWarningPage();
        QWizardPage* createChooseSRPage();
        QWizardPage* createAssignPrioritiesPage();
        QWizardPage* createFinishPage();

        void populateVMTable();
        void updateNtolCalculation();
        QVariantMap buildNtolConfig() const;
        void updateAgilityForRows();
        bool performHeartbeatSRScan();
        int wizardStepIndexForPage(int pageId) const;
        void applyNtolCalculationResult(int requestId, bool ok, qint64 ntolMax, const QVariantMap& ntolConfig, const QString& poolRef, XenConnection* connection);
        void applyAgilityResults(int requestId, const QMap<QString, bool>& agileMap, const QMap<QString, QString>& reasonMap);
        void setNtolUpdateInProgress(bool inProgress);
        bool isRestartPriority(const QString& priority) const;
        bool isVmProtectable(const QSharedPointer<VM>& vm) const;
        QString normalizePriority(const QString& priority) const;
        QString priorityDisplayText(const QString& priority) const;
        void setVmRowValues(int row, const QString& vmRef);
        void refreshSelectionEditors();
        QString priorityToString(HaRestartPriority priority) const;
        HaRestartPriority stringToPriority(const QString& str) const;
        int countVMsByPriority(HaRestartPriority priority) const;

        QSharedPointer<Pool> m_pool;
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
        QComboBox* m_selectedPriorityCombo = nullptr;
        QSpinBox* m_selectedOrderSpin = nullptr;
        QSpinBox* m_selectedDelaySpin = nullptr;

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
        qint64 m_ntol = 0;
        qint64 m_maxNtol = 0;
        QMap<QString, QVariantMap> m_vmStartupOptions;
        QMap<QString, HaRestartPriority> m_vmPriorities;
        QMap<QString, bool> m_vmAgilityKnown;
        QMap<QString, bool> m_vmIsAgile;
        QSet<QString> m_pendingPriorityInitialization;
        bool m_protectVmsByDefault = true;
        int m_ntolRequestId = 0;
        bool m_ntolUpdateInProgress = false;
        bool m_ntolInitializedFromServer = false;
        bool m_updatingSelectionEditors = false;
        int m_agilityRequestId = 0;
        bool m_rbacRequired = false;
        bool m_rbacBlockingFailure = false;
        QLabel* m_rbacWarningLabel = nullptr;
        WizardNavigationPane* m_navigationPane = nullptr;
        bool m_brokenSrWarningShown = false;

        // SR list from scan
        QList<QPair<QString, QString>> m_heartbeatSRs; // ref, name pairs
        Ui::HAWizard* ui = nullptr;
};

#endif // HAWIZARD_H
