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

#ifndef CPUMEMORYEDITPAGE_H
#define CPUMEMORYEDITPAGE_H

#include "ieditpage.h"
#include <QSharedPointer>
#include <QVariantMap>
#include <functional>

class AsyncOperation;
class VM;
class QComboBox;

namespace Ui
{
    class CpuMemoryEditPage;
}

/**
 * @brief CPU and Memory configuration page for VMs
 *
 * Matches C# XenAdmin.SettingsPanels.CpuMemoryEditPage
 * Allows configuring:
 * - Maximum vCPUs (VCPUs_max)
 * - Initial vCPUs (VCPUs_at_startup)
 * - CPU topology (cores per socket)
 */
class CpuMemoryEditPage : public IEditPage
{
    Q_OBJECT

public:
    explicit CpuMemoryEditPage(QWidget* parent = nullptr);
    ~CpuMemoryEditPage() override;

    // IEditPage interface
    QString text() const override;
    QString subText() const override;
    QIcon image() const override;

    void setXenObjects(const QString& objectRef,
                       const QString& objectType,
                       const QVariantMap& objectDataBefore,
                       const QVariantMap& objectDataCopy) override;

    AsyncOperation* saveSettings() override;
    bool isValidToSave() const override;
    void showLocalValidationMessages() override;
    void hideLocalValidationMessages() override;
    void cleanup() override;
    bool hasChanged() const override;
    QVariantMap getModifiedObjectData() const override;

private slots:
    void onVcpusSelectionChanged();
    void onVcpusAtStartupSelectionChanged();
    void onTopologySelectionChanged();
    void onPriorityChanged(int value);

private:
    void repopulate();
    void initializeVCpuControls();
    void populateVCpuComboBox(QComboBox* comboBox,
                              qint64 min,
                              qint64 max,
                              qint64 currentValue,
                              const std::function<bool(qint64)>& isValid) const;
    void populateVcpus(qint64 maxVcpus, qint64 currentVcpus);
    void populateVcpusAtStartup(qint64 maxVcpus, qint64 currentValue);
    void populateTopology(qint64 vcpusAtStartup,
                          qint64 vcpusMax,
                          qint64 coresPerSocket,
                          qint64 maxCoresPerSocket);
    void updateTopologyOptions(qint64 noOfVcpus);
    bool isValidVcpu(qint64 noOfVcpus) const;
    void validateVcpuSettings();
    void validateTopologySettings();
    void refreshCurrentVcpus();
    void showCpuWarnings(const QStringList& warnings);
    void showTopologyWarnings(const QStringList& warnings);
    void updateSubText();
    qint64 selectedVcpusMax() const;
    qint64 selectedVcpusAtStartup() const;
    qint64 selectedCoresPerSocket() const;
    QString productBrand() const;

    Ui::CpuMemoryEditPage* ui;
    QString m_vmRef;
    QVariantMap m_objectDataBefore;
    QVariantMap m_objectDataCopy;
    QSharedPointer<VM> m_vm;

    // Original values
    bool m_validToSave;
    qint64 m_origVcpus;
    qint64 m_origVCPUsMax;
    qint64 m_origVCPUsAtStartup;
    qint64 m_origCoresPerSocket;
    qint64 m_prevVcpusMax;
    double m_origVcpuWeight;
    double m_currentVcpuWeight;
    bool m_isVcpuHotplugSupported;
    int m_minVcpus;

    qint64 m_topologyOrigVcpus;
    qint64 m_topologyOrigCoresPerSocket;
    qint64 m_maxCoresPerSocket;
};

#endif // CPUMEMORYEDITPAGE_H
