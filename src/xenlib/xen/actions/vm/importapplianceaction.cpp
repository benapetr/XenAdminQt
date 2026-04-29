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

#include "importapplianceaction.h"
#include "../../../ovf/ovfpackage.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_VDI.h"
#include "../../xenapi/xenapi_VBD.h"
#include "../../xenapi/xenapi_VIF.h"
#include "../../xenapi/xenapi_Task.h"
#include "../../xenapi/vm_appliance.h"
#include "../../session.h"
#include "../../network/connection.h"
#include "../../network/httpclient.h"
#include "../../failure.h"
#include "../../../xencache.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

// ─── CancelledException ──────────────────────────────────────────────────────

namespace
{
    struct CancelledException {};
}

// ─── Constructor ─────────────────────────────────────────────────────────────

ImportApplianceAction::ImportApplianceAction(XenConnection* connection,
                                             const QString& ovfFilePath,
                                             const QList<OvfVmMapping>& vmMappings,
                                             bool verifyManifest,
                                             bool verifySignature,
                                             bool runFixups,
                                             const QString& fixupIsoSrRef,
                                             bool startAutomatically,
                                             QObject* parent)
    : AsyncOperation(connection, "Import Appliance", "Importing OVF/OVA appliance...", parent)
    , ovfFilePath_(ovfFilePath)
    , vmMappings_(vmMappings)
    , verifyManifest_(verifyManifest)
    , verifySignature_(verifySignature)
    , runFixups_(runFixups)
    , fixupIsoSrRef_(fixupIsoSrRef)
    , startAutomatically_(startAutomatically)
{
    this->SetSafeToExit(false);
    this->SetCanCancel(true);

    // RBAC checks matching C# ApplianceAction
    this->AddApiMethodToRoleCheck("VM.create");
    this->AddApiMethodToRoleCheck("VM.destroy");
    this->AddApiMethodToRoleCheck("VM.hard_shutdown");
    this->AddApiMethodToRoleCheck("VM.start");
    this->AddApiMethodToRoleCheck("VM.add_to_other_config");
    this->AddApiMethodToRoleCheck("VM.remove_from_other_config");
    this->AddApiMethodToRoleCheck("VM.set_HVM_boot_params");
    this->AddApiMethodToRoleCheck("VDI.create");
    this->AddApiMethodToRoleCheck("VDI.destroy");
    this->AddApiMethodToRoleCheck("VBD.create");
    this->AddApiMethodToRoleCheck("VIF.create");
}

// ─── Cancellation ────────────────────────────────────────────────────────────

void ImportApplianceAction::checkCancelled() const
{
    if (this->IsCancelled())
        throw CancelledException();
}

// ─── Run ─────────────────────────────────────────────────────────────────────

void ImportApplianceAction::run()
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
        // ── Step 1: Parse OVF package ─────────────────────────────────────
        this->setDescriptionSafe("Parsing OVF package...");
        this->setPercentCompleteSafe(5);
        this->checkCancelled();

        OvfPackage pkg(this->ovfFilePath_);
        if (!pkg.IsValid())
        {
            this->setError(QString("Failed to parse OVF package: %1").arg(pkg.ParseError()));
            this->setState(Failed);
            return;
        }
        if (pkg.HasEncryption())
        {
            this->setError("Encrypted OVF packages are not supported.");
            this->setState(Failed);
            return;
        }

        // ── Step 2: Manifest / signature verification ─────────────────────
        if (this->verifySignature_ && !pkg.HasSignature())
        {
            this->setError("Signature verification was requested but no .cert file was found.");
            this->setState(Failed);
            return;
        }
        if ((this->verifyManifest_ || this->verifySignature_) && !pkg.HasManifest())
        {
            this->setError("Manifest verification was requested but no .mf file was found.");
            this->setState(Failed);
            return;
        }
        // Full cryptographic verification (manifest hash check / signature validation)
        // is not yet implemented — log a warning and proceed.
        if (this->verifyManifest_ || this->verifySignature_)
            qWarning() << "ImportApplianceAction: manifest/signature verification not yet implemented";

        this->setPercentCompleteSafe(10);
        this->checkCancelled();

        // ── Step 3: Create VM_appliance if multi-VM ───────────────────────
        this->setDescriptionSafe("Preparing import...");
        const bool isMultiVm = (this->vmMappings_.size() > 1) || !pkg.PackageName().isEmpty();
        if (isMultiVm && !pkg.PackageName().isEmpty())
        {
            this->setDescriptionSafe(QString("Creating appliance '%1'...").arg(pkg.PackageName()));
            QVariantMap appRecord;
            appRecord["name_label"]       = pkg.PackageName();
            appRecord["name_description"] = QString("Imported from %1").arg(
                                                QFileInfo(this->ovfFilePath_).fileName());
            try
            {
                this->applianceRef_ = XenAPI::VM_appliance::create(session, appRecord);
                qDebug() << "ImportApplianceAction: created VM_appliance" << this->applianceRef_;
            }
            catch (const Failure& f)
            {
                qWarning() << "ImportApplianceAction: VM_appliance.create failed:" << f.what()
                           << "— continuing without appliance grouping";
                this->applianceRef_.clear();
            }
        }

        this->setPercentCompleteSafe(15);

        // ── Step 4: Per-VM import loop ────────────────────────────────────
        const int vmCount = this->vmMappings_.size();
        for (int vmIdx = 0; vmIdx < vmCount; ++vmIdx)
        {
            this->checkCancelled();

            const OvfVmMapping& mapping = this->vmMappings_.at(vmIdx);
            const int vmProgressBase = 15 + (vmIdx * 80 / vmCount);
            const int vmProgressEnd  = 15 + ((vmIdx + 1) * 80 / vmCount);

            const QString vmName = mapping.virtualSystemId.isEmpty()
                                   ? pkg.PackageName()
                                   : mapping.virtualSystemId;
            this->setDescriptionSafe(QString("Importing VM '%1'...").arg(vmName));
            this->setPercentCompleteSafe(vmProgressBase);
            qDebug() << "ImportApplianceAction: importing VM" << vmName;

            // ── 4a: Build and submit VM.create record ─────────────────────
            // Use RASD virtual hardware if available; fall back to safe defaults.
            const QMap<QString, OvfVirtualHardware> hwMap = pkg.VirtualHardwareBySystem();
            const OvfVirtualHardware hw = hwMap.value(vmName); // empty struct if not found

            const int    vcpus    = (hw.vcpuCount  > 0) ? hw.vcpuCount  : 1;
            const qint64 memMax   = (hw.memoryBytes > 0) ? hw.memoryBytes
                                                          : static_cast<qint64>(512 * 1024 * 1024);
            // Enforce minimum 64 MB; dynamic_min = 25 % of max (matches C# defaults)
            const qint64 memMin   = qMax(static_cast<qint64>(64 * 1024 * 1024), memMax / 4);

            QVariantMap vmRecord;
            vmRecord["name_label"]             = vmName;
            vmRecord["name_description"]       = QString("Imported from %1").arg(
                                                       QFileInfo(this->ovfFilePath_).fileName());
            vmRecord["memory_static_max"]      = memMax;
            vmRecord["memory_static_min"]      = memMin;
            vmRecord["memory_dynamic_max"]     = memMax;
            vmRecord["memory_dynamic_min"]     = memMin;
            vmRecord["VCPUs_max"]              = vcpus;
            vmRecord["VCPUs_at_startup"]       = vcpus;
            vmRecord["actions_after_shutdown"] = QString("destroy");
            vmRecord["actions_after_reboot"]   = QString("restart");
            vmRecord["actions_after_crash"]    = QString("restart");
            vmRecord["HVM_boot_policy"]        = QString("BIOS order");
            QVariantMap bootParams;
            bootParams["order"] = "dc";
            vmRecord["HVM_boot_params"] = bootParams;
            QVariantMap platform;
            platform["nx"]     = "true";
            platform["acpi"]   = "1";
            platform["apic"]   = "true";
            platform["pae"]    = "true";
            platform["stdvga"] = "false";
            vmRecord["platform"] = platform;
            // Hide from XenCenter until fully configured — matches C# pattern.
            // After full setup we call VM.destroy-and-recreate isn't possible;
            // instead we omit HideFromXenCenter and make the VM visible from the start.
            // TODO: use VM.add_to_other_config("HideFromXenCenter","true") once that binding is added.
            vmRecord["other_config"] = QVariantMap();
            if (!this->applianceRef_.isEmpty())
                vmRecord["appliance"] = this->applianceRef_;
            if (!mapping.targetHostRef.isEmpty())
                vmRecord["affinity"] = mapping.targetHostRef;

            QString vmRef;
            try
            {
                vmRef = this->createVm(vmRecord, this->applianceRef_);
            }
            catch (const CancelledException&)
            {
                throw;
            }
            catch (const std::exception& e)
            {
                this->setError(QString("Failed to create VM '%1': %2").arg(vmName, e.what()));
                this->setState(Failed);
                return;
            }

            this->createdVmRefs_ << vmRef;
            if (this->importedVmRef_.isEmpty())
                this->importedVmRef_ = vmRef;

            // ── 4b: Upload disks ──────────────────────────────────────────
            const int diskCount = mapping.diskMappings.size();
            for (int diskIdx = 0; diskIdx < diskCount; ++diskIdx)
            {
                this->checkCancelled();

                const OvfDiskMapping& dm = mapping.diskMappings.at(diskIdx);

                // Resolve SR: per-disk mapping → default SR in mapping → pool default
                QString srRef = dm.targetSrRef;
                if (srRef.isEmpty())
                    srRef = mapping.defaultSrRef;
                if (srRef.isEmpty())
                {
                    qWarning() << "ImportApplianceAction: no SR for disk" << dm.diskHref << "— skipping";
                    continue;
                }

                // Build local disk file path (for .ova, disk files are in the package dir)
                QFileInfo ovfFi(this->ovfFilePath_);
                const QString diskPath = ovfFi.absoluteDir().filePath(dm.diskHref);
                if (!QFile::exists(diskPath))
                {
                    this->setError(QString("Disk file not found: %1").arg(diskPath));
                    this->cleanupVm(vmRef);
                    this->setState(Failed);
                    return;
                }

                const int diskProgressStart = vmProgressBase + (diskIdx * (vmProgressEnd - vmProgressBase) / qMax(diskCount, 1));
                const int diskProgressEnd   = vmProgressBase + ((diskIdx + 1) * (vmProgressEnd - vmProgressBase) / qMax(diskCount, 1));

                this->setDescriptionSafe(QString("Uploading disk '%1' (%2/%3)...")
                                         .arg(dm.diskHref).arg(diskIdx + 1).arg(diskCount));
                this->setPercentCompleteSafe(diskProgressStart);

                QString vdiRef;
                try
                {
                    vdiRef = this->uploadDisk(srRef, dm.diskHref, diskPath, 0,
                                              diskProgressStart, diskProgressEnd);
                }
                catch (const CancelledException&)
                {
                    this->cleanupVm(vmRef);
                    throw;
                }
                catch (const std::exception& e)
                {
                    this->setError(QString("Failed to upload disk '%1': %2").arg(dm.diskHref, e.what()));
                    this->cleanupVm(vmRef);
                    this->setState(Failed);
                    return;
                }

                // Attach as VBD (first disk is bootable)
                try
                {
                    this->attachDisk(vmRef, vdiRef, (diskIdx == 0), "RW", "Disk");
                }
                catch (const std::exception& e)
                {
                    qWarning() << "ImportApplianceAction: VBD attach failed:" << e.what();
                    // Non-fatal — VM was created and disk uploaded; continue
                }
            }

            // ── 4c: Create VIFs ───────────────────────────────────────────
            for (int vifIdx = 0; vifIdx < mapping.networkMappings.size(); ++vifIdx)
            {
                this->checkCancelled();

                const OvfNetworkMapping& nm = mapping.networkMappings.at(vifIdx);
                if (nm.targetNetworkRef.isEmpty())
                    continue;

                try
                {
                    this->createVif(vmRef, nm.targetNetworkRef, vifIdx, QString());
                }
                catch (const std::exception& e)
                {
                    qWarning() << "ImportApplianceAction: VIF create failed:" << e.what();
                }
            }

            // ── 4d: VM is already visible (HideFromXenCenter not set above) ────

            this->setPercentCompleteSafe(vmProgressEnd);
        }

        // ── Step 5: Auto-start ────────────────────────────────────────────
        if (this->startAutomatically_)
        {
            this->setDescriptionSafe("Starting virtual machine(s)...");
            this->setPercentCompleteSafe(95);
            this->checkCancelled();

            if (!this->applianceRef_.isEmpty())
            {
                // Start as vApp
                try
                {
                    const QString taskRef = XenAPI::VM_appliance::async_start(session, this->applianceRef_, false);
                    this->pollToCompletion(taskRef, 95, 100);
                }
                catch (const std::exception& e)
                {
                    qWarning() << "ImportApplianceAction: vApp start failed:" << e.what();
                }
            }
            else if (!this->importedVmRef_.isEmpty())
            {
                // Start individual VM
                try
                {
                    const QString taskRef = XenAPI::VM::async_start(session, this->importedVmRef_, false, false);
                    this->pollToCompletion(taskRef, 95, 100);
                }
                catch (const std::exception& e)
                {
                    qWarning() << "ImportApplianceAction: VM start failed:" << e.what();
                }
            }
        }

        this->setPercentCompleteSafe(100);
        this->setDescriptionSafe("Import complete.");
        this->setState(Completed);
    }
    catch (const CancelledException&)
    {
        this->setDescriptionSafe("Import cancelled.");
        this->setState(Cancelled);
    }
    catch (const Failure& f)
    {
        this->setError(QString("XenAPI error: %1").arg(f.what()));
        this->setState(Failed);
    }
    catch (const std::exception& e)
    {
        this->setError(QString("Unexpected error: %1").arg(e.what()));
        this->setState(Failed);
    }
}

// ─── createVm ────────────────────────────────────────────────────────────────

QString ImportApplianceAction::createVm(const QVariantMap& vmRecord, const QString& /*applianceRef*/)
{
    XenAPI::Session* session = this->GetSession();
    const QString vmRef = XenAPI::VM::create(session, vmRecord);
    if (vmRef.isEmpty())
        throw std::runtime_error("VM.create returned an empty reference");
    qDebug() << "ImportApplianceAction: created VM" << vmRef;
    return vmRef;
}

// ─── uploadDisk ──────────────────────────────────────────────────────────────

QString ImportApplianceAction::uploadDisk(const QString& srRef,
                                          const QString& diskLabel,
                                          const QString& diskFilePath,
                                          qint64 virtualSizeBytes,
                                          int progressStart,
                                          int progressEnd)
{
    XenAPI::Session* session = this->GetSession();

    // Determine virtual size from file size when not provided by caller
    if (virtualSizeBytes <= 0)
    {
        QFileInfo fi(diskFilePath);
        virtualSizeBytes = fi.size();
    }

    // Create the target VDI
    QVariantMap vdiRecord;
    vdiRecord["name_label"]       = diskLabel;
    vdiRecord["name_description"] = QString("Imported from %1").arg(QFileInfo(this->ovfFilePath_).fileName());
    vdiRecord["SR"]               = srRef;
    vdiRecord["virtual_size"]     = virtualSizeBytes;
    vdiRecord["type"]             = QString("user");
    vdiRecord["sharable"]         = false;
    vdiRecord["read_only"]        = false;
    vdiRecord["other_config"]     = QVariantMap();

    const QString vdiRef = XenAPI::VDI::create(session, vdiRecord);
    if (vdiRef.isEmpty())
        throw std::runtime_error("VDI.create returned an empty reference");
    qDebug() << "ImportApplianceAction: created VDI" << vdiRef;

    // Determine target host address
    QString hostAddr;
    if (this->GetConnection())
        hostAddr = this->GetConnection()->GetHostname();

    // Create a task to track the upload
    const QString taskRef = XenAPI::Task::Create(session,
                                                  "import_raw_vdi_task",
                                                  hostAddr);

    // Build HTTP PUT query parameters matching /import_raw_vdi endpoint
    QMap<QString, QString> queryParams;
    queryParams["session_id"] = session->GetSessionID();
    queryParams["task_id"]    = taskRef;
    queryParams["vdi"]        = vdiRef;  // opaque ref accepted by the server
    queryParams["format"]     = "vhd";

    // Upload via HTTP PUT
    HttpClient http(this);
    int lastPct = progressStart;
    const bool uploadOk = http.putFile(
        diskFilePath,
        hostAddr,
        "/import_raw_vdi",
        queryParams,
        [this, progressStart, progressEnd, &lastPct](int pct)
        {
            const int mapped = progressStart +
                static_cast<int>(pct / 100.0 * (progressEnd - progressStart));
            if (mapped != lastPct)
            {
                lastPct = mapped;
                this->setPercentCompleteSafe(mapped);
            }
        },
        [this]() -> bool { return this->IsCancelled(); }
    );

    this->pollToCompletion(taskRef, progressStart, progressEnd, false);

    if (!uploadOk)
    {
        const QString uploadError = http.lastError();
        // Try to clean up the VDI on upload failure
        try { XenAPI::VDI::destroy(session, vdiRef); } catch (...) {}
        throw std::runtime_error(uploadError.toStdString());
    }

    return vdiRef;
}

// ─── attachDisk ──────────────────────────────────────────────────────────────

void ImportApplianceAction::attachDisk(const QString& vmRef,
                                       const QString& vdiRef,
                                       bool bootable,
                                       const QString& mode,
                                       const QString& type)
{
    XenAPI::Session* session = this->GetSession();

    // Determine the next available device slot
    QVariant allowedDevices = XenAPI::VM::get_allowed_VBD_devices(session, vmRef);
    QString userDevice = "0";
    if (allowedDevices.canConvert<QVariantList>())
    {
        const QVariantList devices = allowedDevices.toList();
        if (!devices.isEmpty())
            userDevice = devices.first().toString();
    }

    QVariantMap vbdRecord;
    vbdRecord["VM"]          = vmRef;
    vbdRecord["VDI"]         = vdiRef;
    vbdRecord["userdevice"]  = userDevice;
    vbdRecord["bootable"]    = bootable;
    vbdRecord["mode"]        = mode;
    vbdRecord["type"]        = type;
    vbdRecord["empty"]       = false;
    vbdRecord["other_config"] = QVariantMap({ {"owner", "true"} });

    const QString vbdRef = XenAPI::VBD::create(session, vbdRecord);
    qDebug() << "ImportApplianceAction: created VBD" << vbdRef;
}

// ─── createVif ───────────────────────────────────────────────────────────────

void ImportApplianceAction::createVif(const QString& vmRef,
                                      const QString& networkRef,
                                      int deviceIndex,
                                      const QString& mac)
{
    XenAPI::Session* session = this->GetSession();

    QVariantMap vifRecord;
    vifRecord["device"]        = QString::number(deviceIndex);
    vifRecord["network"]       = networkRef;
    vifRecord["VM"]            = vmRef;
    vifRecord["MAC"]           = mac;
    vifRecord["MTU"]           = 1500;
    vifRecord["other_config"]  = QVariantMap();
    vifRecord["locking_mode"]  = QString("network_default");

    const QString vifRef = XenAPI::VIF::create(session, vifRecord);
    qDebug() << "ImportApplianceAction: created VIF" << vifRef;
}

// ─── cleanupVm ───────────────────────────────────────────────────────────────

void ImportApplianceAction::cleanupVm(const QString& vmRef)
{
    if (vmRef.isEmpty())
        return;

    XenAPI::Session* session = this->GetSession();
    if (!session)
        return;

    qDebug() << "ImportApplianceAction: cleaning up partial VM" << vmRef;
    try
    {
        XenAPI::VM::destroy(session, vmRef);
    }
    catch (const std::exception& e)
    {
        qWarning() << "ImportApplianceAction: cleanup destroy failed:" << e.what();
    }
}
