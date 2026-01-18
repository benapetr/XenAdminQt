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

#ifndef VMMEMORYCONTROLS_H
#define VMMEMORYCONTROLS_H

#include <QWidget>
#include <QLabel>
#include <QGridLayout>
#include <QSharedPointer>
#include "xen/vm.h"
#include "vmshinybar.h"

/**
 * @brief Displays VM memory statistics and visualization
 *
 * Shows VM memory settings and current usage with labels for:
 * - Dynamic minimum memory
 * - Dynamic maximum memory
 * - Static maximum memory (if different from dynamic max)
 * - Current memory usage (from metrics)
 *
 * Includes a VMShinyBar for visual representation.
 *
 * Qt port of C# XenAdmin VMMemoryControlsNoEdit control.
 */
class VMMemoryControls : public QWidget
{
    Q_OBJECT

    public:
        explicit VMMemoryControls(QWidget* parent = nullptr);

        /**
         * @brief Set the VMs to display
         * @param vms List of VMs (typically one, but can be multiple for group display)
         */
        void SetVMs(const QList<QSharedPointer<VM>>& vms);

        /**
         * @brief Unregister all event handlers
         */
        void UnregisterHandlers();

    private slots:
        void Refresh();
        void onVMDataChanged();
        void onVMMetricsChanged();

    private:
        QList<QSharedPointer<VM>> vms_;
        
        // UI elements
        VMShinyBar* vmShinyBar_ = nullptr;
        QLabel* labelDynMin_ = nullptr;
        QLabel* labelDynMax_ = nullptr;
        QLabel* labelStatMax_ = nullptr;
        
        QLabel* valueDynMin_ = nullptr;
        QLabel* valueDynMax_ = nullptr;
        QLabel* valueStatMax_ = nullptr;
        
        void SetupUi();
};

#endif // VMMEMORYCONTROLS_H
