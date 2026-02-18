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

#ifndef BALLOONINGDIALOG_H
#define BALLOONINGDIALOG_H

#include <QDialog>
#include <QSharedPointer>
#include "controls/memoryspinner.h"

class XenConnection;
class ChangeMemorySettingsAction;
class VM;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class BallooningDialog;
}
QT_END_NAMESPACE

/**
 * @brief Dialog for editing VM memory (ballooning) settings
 *
 * Allows users to configure VM memory allocation, choosing between:
 * - Fixed allocation: Single static memory value
 * - Dynamic allocation: Memory range (min/max) with DMC support
 *
 * Matches C# XenAdmin BallooningDialog.cs functionality.
 * Uses ChangeMemorySettingsAction for applying changes.
 */
class BallooningDialog : public QDialog
{
    Q_OBJECT

    public:
        /**
         * @brief Construct ballooning dialog
         * @param vmRef VM opaque reference
         * @param connection XenConnection instance for API access
         * @param parent Parent widget
         */
        explicit BallooningDialog(const QSharedPointer<VM> &vm, QWidget* parent = nullptr);
        ~BallooningDialog();

    private slots:
        void onFixedRadioToggled(bool checked);
        void onDynamicRadioToggled(bool checked);
        void onFixedValueChanged(double value);
        void onDynMinValueChanged(double value);
        void onDynMaxValueChanged(double value);
        void onShinyBarSliderDragged();
        void onAdvancedToggled(bool checked);
        void onStaticMinValueChanged(double value);
        void onUnitsChanged(int index);
        void onAccepted();

    private:
        Ui::BallooningDialog* ui;
        XenConnection* m_connection;
        QSharedPointer<VM> m_vm;
        // workaround for Qt bug - isVisible returns false even when it's true
        bool checkboxDeferVisible = false;

        bool m_hasBallooning = false; // Whether VM supports DMC (Dynamic Memory Control)
        bool m_isTemplate = false;
        double m_maxDynMin;
        qint64 m_memorySpinnerMax;

        qint64 m_originalStaticMin;
        qint64 m_originalStaticMax;
        qint64 m_originalDynamicMin;
        qint64 m_originalDynamicMax;
        MemorySpinner::Unit m_memoryUnit = MemorySpinner::Unit::MB;

        void populateControls();
        void updateDMCAvailability();
        void updateSpinnerRanges();
        void updateShinyBar();
        void setIncrements();
        void applyUnitToSpinners();
        void setSpinnerEnabled(bool fixed, bool dynamic);
        qint64 currentDynamicMinBytes() const;
        qint64 currentDynamicMaxBytes() const;
        qint64 currentStaticMaxBytes() const;
        qint64 currentStaticMinBytes() const;
        double getMemoryRatio() const;
        double calcMaxDynMin() const;
        bool vmUsesBallooning() const;
        qint64 getMemorySpinnerMax() const;
        qint64 calcIncrementBytes(qint64 staticMaxBytes) const;

        QString unitName() const;

        bool validateMemorySettings() const;
        bool applyMemoryChanges();
};

#endif // BALLOONINGDIALOG_H
