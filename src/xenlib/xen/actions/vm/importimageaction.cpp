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

#include "importimageaction.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_VTPM.h"
#include "../../session.h"
#include "../../network/connection.h"
#include <QFileInfo>
#include <QDebug>

// ─── Constructor ─────────────────────────────────────────────────────────────

ImportImageAction::ImportImageAction(XenConnection* connection,
                                     const QString& vmName,
                                     int vcpuCount,
                                     qint64 memoryMb,
                                     const QString& srRef,
                                     const QString& networkRef,
                                     const QString& hostRef,
                                     const QString& filePath,
                                     qint64 diskCapacityBytes,
                                     BootMode bootMode,
                                     bool assignVtpm,
                                     bool startAutomatically,
                                     QObject* parent)
    : ImportApplianceAction(connection,
                            filePath,       // m_ovfFilePath (used for VDI description strings)
                            {},             // vmMappings (unused — run() does its own logic)
                            false,          // verifyManifest
                            false,          // verifySignature
                            false,          // runFixups
                            {},             // fixupIsoSrRef
                            false,          // startAutomatically (handled locally)
                            parent)
    , m_vmName(vmName)
    , m_vcpuCount(qMax(1, vcpuCount))
    , m_memoryMb(qMax(static_cast<qint64>(64), memoryMb))
    , m_srRef(srRef)
    , m_networkRef(networkRef)
    , m_hostRef(hostRef)
    , m_diskCapacityBytes(diskCapacityBytes)
    , m_bootMode(bootMode)
    , m_assignVtpm(assignVtpm)
    , m_startAutomatically(startAutomatically)
{
    this->setDescriptionSafe(QString("Importing disk image '%1'...").arg(vmName));
}

// ─── run ─────────────────────────────────────────────────────────────────────

void ImportImageAction::run()
{
    XenAPI::Session* session = this->GetSession();
    if (!session)
    {
        this->setError("No active session");
        this->setState(Failed);
        return;
    }

    try
    {
        // ── Step 1: Create VM record ──────────────────────────────────────
        this->setDescriptionSafe(QString("Creating VM '%1'...").arg(this->m_vmName));
        this->setPercentCompleteSafe(5);
        this->checkCancelled();

        const qint64 memBytes  = this->m_memoryMb * 1024 * 1024;
        const qint64 memMinBytes = qMax(static_cast<qint64>(64 * 1024 * 1024), memBytes / 4);

        QVariantMap vmRecord;
        vmRecord["name_label"]             = this->m_vmName;
        vmRecord["name_description"]       = QString("Imported from %1")
                                               .arg(QFileInfo(this->m_ovfFilePath).fileName());
        vmRecord["memory_static_max"]      = memBytes;
        vmRecord["memory_static_min"]      = memMinBytes;
        vmRecord["memory_dynamic_max"]     = memBytes;
        vmRecord["memory_dynamic_min"]     = memMinBytes;
        vmRecord["VCPUs_max"]              = this->m_vcpuCount;
        vmRecord["VCPUs_at_startup"]       = this->m_vcpuCount;
        vmRecord["actions_after_shutdown"] = QString("destroy");
        vmRecord["actions_after_reboot"]   = QString("restart");
        vmRecord["actions_after_crash"]    = QString("restart");

        // ── Boot firmware ─────────────────────────────────────────────────
        // C# equivalent: ImportImageAction sets HVM_boot_policy + HVM_boot_params
        // based on the BootModesControl selection.
        QVariantMap bootParams;
        QVariantMap platform;
        platform["nx"]     = "true";
        platform["acpi"]   = "1";
        platform["apic"]   = "true";
        platform["pae"]    = "true";
        platform["stdvga"] = "false";

        if (this->m_bootMode == BootMode_Bios)
        {
            vmRecord["HVM_boot_policy"] = QString("BIOS order");
            bootParams["order"] = "dc";
        } else
        {
            // UEFI and UEFI Secure Boot both use empty HVM_boot_policy + firmware=uefi
            vmRecord["HVM_boot_policy"] = QString("");
            bootParams["firmware"] = "uefi";
            bootParams["order"]    = "cdn";  // CD, disk, net
            if (this->m_bootMode == BootMode_UefiSecure)
                platform["secureboot"] = "true";
        }

        vmRecord["HVM_boot_params"] = bootParams;
        vmRecord["platform"]        = platform;
        vmRecord["other_config"] = QVariantMap();
        if (!this->m_hostRef.isEmpty())
            vmRecord["affinity"] = this->m_hostRef;

        QString vmRef;
        try
        {
            vmRef = this->createVm(vmRecord, {});
        }
        catch (const std::exception& e)
        {
            this->setError(QString("Failed to create VM '%1': %2").arg(this->m_vmName, e.what()));
            this->setState(Failed);
            return;
        }

        this->m_imageVmRef = vmRef;
        qDebug() << "ImportImageAction: created VM" << vmRef;

        // ── Step 1b: Attach vTPM (optional) ──────────────────────────────
        if (this->m_assignVtpm)
        {
            this->checkCancelled();
            try
            {
                const QString vtpmRef = XenAPI::VTPM::create(session, vmRef, /*isUnique=*/false);
                qDebug() << "ImportImageAction: vTPM created:" << vtpmRef;
            }
            catch (const std::exception& e)
            {
                qWarning() << "ImportImageAction: vTPM attach failed (may not be supported):" << e.what();
            }
        }

        this->setPercentCompleteSafe(15);

        // ── Step 2: Upload disk ───────────────────────────────────────────
        this->setDescriptionSafe(QString("Uploading disk image..."));
        this->checkCancelled();

        // Use provided capacity; fall back to file size for VMDK or undetected formats
        qint64 virtualSizeBytes = this->m_diskCapacityBytes;
        if (virtualSizeBytes <= 0)
            virtualSizeBytes = QFileInfo(this->m_ovfFilePath).size();

        const QString diskLabel = this->m_vmName + " disk";
        QString vdiRef;
        try
        {
            vdiRef = this->uploadDisk(this->m_srRef, diskLabel, this->m_ovfFilePath,
                                      virtualSizeBytes, 15, 85);
        }
        catch (const std::exception& e)
        {
            this->setError(QString("Failed to upload disk image: %1").arg(e.what()));
            this->cleanupVm(vmRef);
            this->setState(Failed);
            return;
        }

        this->setPercentCompleteSafe(85);

        // ── Step 3: Attach disk as boot VBD ──────────────────────────────
        this->checkCancelled();
        try
        {
            this->attachDisk(vmRef, vdiRef, /*bootable=*/true, "RW", "Disk");
        }
        catch (const std::exception& e)
        {
            qWarning() << "ImportImageAction: VBD attach failed:" << e.what();
            // Non-fatal — VM was created and disk uploaded
        }

        // ── Step 4: Create VIF ────────────────────────────────────────────
        if (!this->m_networkRef.isEmpty())
        {
            this->checkCancelled();
            try
            {
                this->createVif(vmRef, this->m_networkRef, 0, {});
            }
            catch (const std::exception& e)
            {
                qWarning() << "ImportImageAction: VIF create failed:" << e.what();
            }
        }

        this->setPercentCompleteSafe(90);

        // ── Step 5: Auto-start ────────────────────────────────────────────
        if (this->m_startAutomatically)
        {
            this->setDescriptionSafe(QString("Starting VM '%1'...").arg(this->m_vmName));
            this->checkCancelled();
            try
            {
                const QString taskRef = XenAPI::VM::async_start(session, vmRef, false, false);
                this->pollToCompletion(taskRef, 90, 100, false);
            }
            catch (const std::exception& e)
            {
                qWarning() << "ImportImageAction: VM start failed:" << e.what();
            }
        }

        this->setPercentCompleteSafe(100);
        this->setDescriptionSafe(QString("Import of '%1' complete.").arg(this->m_vmName));
        this->setState(Completed);
    }
    catch (const std::exception& e)
    {
        this->setError(QString("Unexpected error: %1").arg(e.what()));
        if (!this->m_imageVmRef.isEmpty())
            try { this->cleanupVm(this->m_imageVmRef); } catch (...) {}
        this->setState(Failed);
    }
    catch (...)
    {
        // CancelledException from checkCancelled() is defined in an anonymous
        // namespace inside importapplianceaction.cpp and cannot be caught by name here.
        // Treat any other thrown object as a cancellation.
        if (!this->m_imageVmRef.isEmpty())
            try { this->cleanupVm(this->m_imageVmRef); } catch (...) {}
        this->setDescriptionSafe("Import cancelled.");
        this->setState(Cancelled);
    }
}
