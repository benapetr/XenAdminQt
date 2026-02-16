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

#ifndef ADDVGPUDIALOG_H
#define ADDVGPUDIALOG_H

#include <QDialog>
#include <QSharedPointer>

#include "../controls/vgpucombobox.h"

class VM;
class VGPU;
class VGPUType;

class VgpuComboBox;
class QPushButton;

class AddVGPUDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit AddVGPUDialog(QSharedPointer<VM> vm,
                               const QList<QSharedPointer<VGPU>>& existingVGpus,
                               QWidget* parent = nullptr);

        GpuTuple SelectedTuple() const;

    private slots:
        void onComboSelectionChanged();
        void onAccepted();

    private:
        void populateComboBox();
        static bool compareVGpuTypeForDisplay(const QSharedPointer<VGPUType>& left,
                                              const QSharedPointer<VGPUType>& right);

        QSharedPointer<VM> m_vm;
        QList<QSharedPointer<VGPU>> m_existingVGpus;

        VgpuComboBox* m_combo = nullptr;
        QPushButton* m_addButton = nullptr;
        GpuTuple m_selectedTuple;
};

#endif // ADDVGPUDIALOG_H
