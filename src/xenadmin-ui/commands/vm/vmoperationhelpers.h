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

#ifndef VMOPERATIONHELPERS_H
#define VMOPERATIONHELPERS_H

#include <QString>
#include <QObject>

class XenConnection;
class XenLib;
class Failure;  // Forward declaration

/*!
 * \brief Helper methods for VM operation commands
 *
 * Provides static utility methods for VM operations, particularly for
 * diagnosing why VM start/resume operations fail.
 *
 * Matches xenadmin/XenAdmin/Commands/VMOperationCommand.cs pattern.
 */
class VMOperationHelpers : public QObject
{
    Q_OBJECT

public:
    /*!
     * \brief Show diagnosis form for VM start failures
     *
     * Walks through all hosts in the pool, calls VM.assert_can_boot_here on each,
     * and displays the results in a CommandErrorDialog.
     *
     * Matches C# VMOperationCommand.StartDiagnosisForm(VM vm, bool isStart)
     *
     * \param xenLib XenLib instance for cache access
     * \param connection XenServer connection
     * \param vmRef VM opaque reference
     * \param vmName VM name (for display)
     * \param isStart true if this is a start operation, false for resume
     * \param parent Parent widget for dialogs
     */
    static void startDiagnosisForm(XenLib* xenLib,
                                   XenConnection* connection,
                                   const QString& vmRef,
                                   const QString& vmName,
                                   bool isStart,
                                   QWidget* parent = nullptr);

    /*!
     * \brief Show diagnosis form after catching a Failure
     *
     * Inspects the Failure error code and shows appropriate dialog:
     * - NO_HOSTS_AVAILABLE: Shows per-host diagnosis (calls other overload)
     * - HA_OPERATION_WOULD_BREAK_FAILOVER_PLAN: Offers to reduce ntol and retry
     *
     * Matches C# VMOperationCommand.StartDiagnosisForm(VMStartAbstractAction, Failure)
     *
     * \param xenLib XenLib instance for cache access
     * \param connection XenServer connection
     * \param vmRef VM opaque reference
     * \param vmName VM name (for display)
     * \param isStart true if this is a start operation, false for resume
     * \param failure The Failure exception that was caught
     * \param parent Parent widget for dialogs
     */
    static void startDiagnosisForm(XenLib* xenLib,
                                   XenConnection* connection,
                                   const QString& vmRef,
                                   const QString& vmName,
                                   bool isStart,
                                   const Failure& failure,
                                   QWidget* parent = nullptr);

private:
    VMOperationHelpers() = delete;  // Static-only class
};

#endif // VMOPERATIONHELPERS_H
