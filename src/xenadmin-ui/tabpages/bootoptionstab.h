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

#ifndef BOOTOPTIONSTAB_H
#define BOOTOPTIONSTAB_H

#include "basetabpage.h"
#include <QCheckBox>
#include <QListWidget>
#include <QPushButton>
#include <QComboBox>
#include <QTextEdit>
#include <QLabel>

/**
 * Boot Options tab page for VM properties dialog.
 * Allows configuration of:
 * - Boot order (HVM VMs: CD/DVD, Hard Disk, Network)
 * - Auto power on setting
 * - OS boot parameters (PV VMs)
 * - Boot device selection (PV VMs)
 */
class BootOptionsTab : public BaseTabPage
{
    Q_OBJECT

public:
    explicit BootOptionsTab(QWidget* parent = nullptr);

    QString tabTitle() const override
    {
        return "Boot Options";
    }
    bool isApplicableForObjectType(const QString& objectType) const override
    {
        return objectType == "vm";
    }
    void setXenObject(const QString& objectType, const QString& objectRef, const QVariantMap& objectData) override;

    /**
     * Check if any boot settings have been modified.
     */
    bool hasChanges() const;

    /**
     * Apply boot settings changes to the VM via XenAPI.
     */
    void applyChanges();

private slots:
    void onBootOrderSelectionChanged();
    void onMoveUpClicked();
    void onMoveDownClicked();
    void onAutoBootChanged(int state);
    void onPVBootDeviceChanged(int index);
    void onOsParamsChanged();

private:
    void createUI();
    void updateBootOrderList();
    void moveBootOrderItem(bool moveUp);
    QString getBootOrderString() const;
    bool isHVM() const;
    bool isPVBootableDVD() const;

    QString m_vmRef;
    QVariantMap m_vmData;

    // HVM boot order widgets
    QWidget* m_hvmWidget;
    QListWidget* m_bootOrderList;
    QPushButton* m_moveUpButton;
    QPushButton* m_moveDownButton;
    QLabel* m_hvmInfoLabel;

    // PV boot device widgets
    QWidget* m_pvWidget;
    QComboBox* m_pvBootDeviceCombo;
    QTextEdit* m_osParamsEdit;
    QLabel* m_pvInfoLabel;

    // Auto boot
    QCheckBox* m_autoBootCheckBox;
    QLabel* m_autoBootInfoLabel;

    // Original values for change detection
    QString m_originalBootOrder;
    bool m_originalAutoBoot;
    QString m_originalOsParams;
    bool m_originalPVBootFromCD;
};

#endif // BOOTOPTIONSTAB_H
