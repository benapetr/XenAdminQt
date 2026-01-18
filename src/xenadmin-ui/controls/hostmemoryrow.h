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

#ifndef HOSTMEMORYROW_H
#define HOSTMEMORYROW_H

#include <QWidget>
#include <QSharedPointer>

class Host;
class VM;
class HostMetrics;
class VMMetrics;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class HostMemoryRow;
}
QT_END_NAMESPACE

/**
 * @brief Display row showing host memory information
 *
 * C# equivalent: HostMemoryRow.cs / HostMemoryControls.cs combined
 * 
 * Displays:
 * - Host name
 * - Total memory
 * - Used memory
 * - Available memory (including balloon-able memory)
 * - Total dynamic maximum (sum of all VM dynamic max)
 * - Overcommit percentage
 * - Control domain (dom0) memory
 * - Visual memory bar showing VM allocations
 */
class HostMemoryRow : public QWidget
{
    Q_OBJECT

    public:
        explicit HostMemoryRow(QWidget* parent = nullptr);
        explicit HostMemoryRow(const QSharedPointer<Host>& host, QWidget* parent = nullptr);
        ~HostMemoryRow();

        /**
         * @brief Set the host to display
         * @param host Host object
         * 
         * C# equivalent: HostMemoryControls.host property setter
         * Subscribes to host and VM property changes
         */
        void SetHost(const QSharedPointer<Host>& host);

        /**
         * @brief Unregister all event handlers
         * 
         * C# equivalent: HostMemoryControls.UnregisterHandlers()
         * Must be called before destroying widget to prevent dangling connections
         */
        void UnregisterHandlers();

    protected:
        /**
         * @brief Update display with current values
         * 
         * C# equivalent: HostMemoryControls.OnPaint()
         * Calculates and displays all memory values
         */
        void Refresh();

    private slots:
        void onHostDataChanged();
        void onVMDataChanged();
        void onMetricsDataChanged();

    private:
        Ui::HostMemoryRow* ui;
        QSharedPointer<Host> host_;
        
        QString formatMemorySize(qint64 bytes) const;
};

#endif // HOSTMEMORYROW_H
