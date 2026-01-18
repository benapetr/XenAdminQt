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

#ifndef VMMEMORYROW_H
#define VMMEMORYROW_H

#include <QWidget>
#include <QFrame>
#include <QSharedPointer>
#include "xen/vm.h"
#include "vmmemorycontrols.h"

/**
 * @brief Container widget for VM memory display
 *
 * Combines a VM label/icon with memory controls in a bordered panel.
 * Used in memory management UI to show memory statistics for VMs.
 *
 * Qt port of C# XenAdmin VMMemoryRow control.
 */
class VMMemoryRow : public QWidget
{
    Q_OBJECT

    public:
        explicit VMMemoryRow(const QList<QSharedPointer<VM>>& vms, bool expanded, QWidget* parent = nullptr);
        ~VMMemoryRow();

        /**
         * @brief Get the VMs this row displays
         * @return List of VM objects
         */
        QList<QSharedPointer<VM>> GetVMs() const { return this->vms_; }

        /**
         * @brief Check if row is in expanded mode
         * @return True if expanded
         */
        bool IsExpanded() const { return this->expanded_; }

        /**
         * @brief Unregister all event handlers
         */
        void UnregisterHandlers();

    private:
        QList<QSharedPointer<VM>> vms_;
        bool expanded_;
        QFrame* panelLabel_;
        QFrame* panelControls_;
        VMMemoryControls* vmMemoryControls_;
        
        void SetupUi();
};

#endif // VMMEMORYROW_H
