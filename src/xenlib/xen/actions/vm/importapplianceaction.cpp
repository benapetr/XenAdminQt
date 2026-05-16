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
#include "../../xenapi/xenapi_Task.h"
#include "../../xenapi/vm_appliance.h"
#include "../../session.h"
#include "../../network/connection.h"
#include "../../failure.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QDebug>

// ─── Local helpers ───────────────────────────────────────────────────────────

// Parse a semicolon-separated "key=value;key2=value2" string into a QVariantMap.
// Matches the inverse of the export serialisation in ExportApplianceAction::buildVirtualSystem().
static QVariantMap parseSemicolonKvMap(const QString& s)
{
    QVariantMap result;
    const QStringList parts = s.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& part : parts)
    {
        const int eq = part.indexOf(QLatin1Char('='));
        if (eq > 0)
            result.insert(part.left(eq).trimmed(), part.mid(eq + 1).trimmed());
    }
    return result;
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
    : VmImportActionBase(connection, "Import Appliance", "Importing OVF/OVA appliance...", parent)
    , m_vmMappings(vmMappings)
    , m_verifyManifest(verifyManifest)
    , m_verifySignature(verifySignature)
    , m_runFixups(runFixups)
    , m_fixupIsoSrRef(fixupIsoSrRef)
    , m_startAutomatically(startAutomatically)
{
    this->SetSafeToExit(false);
    this->SetCanCancel(true);
    this->m_sourcePath = ovfFilePath;

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

// ─── onCancel ────────────────────────────────────────────────────────────────

void ImportApplianceAction::onCancel()
{
    // Cancel any in-flight XenAPI task (upload task registered via SetRelatedTaskRef)
    this->cancelRelatedTask();
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
        // ── Step 0: OVA extraction (if the source is a .ova TAR archive) ────
        // The action receives the original file path. For .ova files, all disk
        // files are inside a TAR archive that must be extracted to a temp
        // directory before we can upload them.  The temp dir is auto-deleted
        // when it goes out of scope at the end of run().
        // C# equivalent: ImportApplianceAction.RunCore() → m_package.ExtractToWorkingDir()
        QTemporaryDir workingDir;
        QString effectiveOvfPath = this->m_sourcePath;

        if (this->m_sourcePath.toLower().endsWith(".ova"))
        {
            if (!workingDir.isValid())
            {
                this->setError("Failed to create temporary directory for OVA extraction.");
                this->setState(Failed);
                return;
            }

            this->setDescriptionSafe("Extracting OVA archive...");
            this->setPercentCompleteSafe(2);
            this->checkCancelled();

            QString extractErr;
            const bool ok = OvfPackage::ExtractAllToDir(
                this->m_sourcePath,
                workingDir.path(),
                extractErr,
                [this]() { return this->IsCancelled(); });

            if (!ok)
            {
                if (this->IsCancelled())
                    throw VmImportActionBase::CancelledException();
                this->setError(QString("OVA extraction failed: %1").arg(extractErr));
                this->setState(Failed);
                return;
            }

            // Find the .ovf descriptor among the extracted files
            QDir dir(workingDir.path());
            const QStringList ovfFiles = dir.entryList(QStringList() << "*.ovf", QDir::Files);
            if (ovfFiles.isEmpty())
            {
                this->setError("No .ovf descriptor found in extracted OVA archive.");
                this->setState(Failed);
                return;
            }
            effectiveOvfPath = dir.filePath(ovfFiles.first());
            qDebug() << "ImportApplianceAction: OVA extracted to" << workingDir.path()
                     << "OVF:" << effectiveOvfPath;
        }

        // ── Step 1: Parse OVF package ─────────────────────────────────────
        this->setDescriptionSafe("Parsing OVF package...");
        this->setPercentCompleteSafe(5);
        this->checkCancelled();

        OvfPackage pkg(effectiveOvfPath);
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
        if (this->m_verifySignature && !pkg.HasSignature())
        {
            this->setError("Signature verification was requested but no .cert file was found.");
            this->setState(Failed);
            return;
        }
        if ((this->m_verifyManifest || this->m_verifySignature) && !pkg.HasManifest())
        {
            this->setError("Manifest verification was requested but no .mf file was found.");
            this->setState(Failed);
            return;
        }
        if (this->m_verifyManifest)
        {
            this->setDescriptionSafe("Verifying manifest...");
            this->checkCancelled();

            const QString workingDir = QFileInfo(effectiveOvfPath).absolutePath();
            const QString baseName   = QFileInfo(effectiveOvfPath).completeBaseName();
            QString verifyErr;
            const bool ok = OvfPackage::VerifyManifest(
                workingDir, baseName, verifyErr,
                [this]() { return this->IsCancelled(); });

            if (this->IsCancelled())
                throw VmImportActionBase::CancelledException();

            if (!ok)
            {
                this->setError(QString("Manifest verification failed: %1").arg(verifyErr));
                this->setState(Failed);
                return;
            }
            qDebug() << "ImportApplianceAction: manifest verified OK";
        }
        // Signature verification (certificate-based) is not yet implemented.
        if (this->m_verifySignature)
            qWarning() << "ImportApplianceAction: certificate signature verification not yet implemented";

        this->setPercentCompleteSafe(10);
        this->checkCancelled();

        // ── Step 3: Create VM_appliance if multi-VM ───────────────────────
        this->setDescriptionSafe("Preparing import...");
        const bool isMultiVm = (this->m_vmMappings.size() > 1) || !pkg.PackageName().isEmpty();
        if (isMultiVm && !pkg.PackageName().isEmpty())
        {
            this->setDescriptionSafe(QString("Creating appliance '%1'...").arg(pkg.PackageName()));
            QVariantMap appRecord;
            appRecord["name_label"]       = pkg.PackageName();
            appRecord["name_description"] = QString("Imported from %1").arg(
                                                QFileInfo(this->m_sourcePath).fileName());
            try
            {
                this->m_applianceRef = XenAPI::VM_appliance::create(session, appRecord);
                qDebug() << "ImportApplianceAction: created VM_appliance" << this->m_applianceRef;
            }
            catch (const Failure& f)
            {
                qWarning() << "ImportApplianceAction: VM_appliance.create failed:" << f.what()
                           << "— continuing without appliance grouping";
                this->m_applianceRef.clear();
            }
        }

        this->setPercentCompleteSafe(15);

        // ── Step 4: Per-VM import loop ────────────────────────────────────
        const int vmCount = this->m_vmMappings.size();
        for (int vmIdx = 0; vmIdx < vmCount; ++vmIdx)
        {
            this->checkCancelled();

            const OvfVmMapping& mapping = this->m_vmMappings.at(vmIdx);
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
                                                       QFileInfo(this->m_sourcePath).fileName());
            vmRecord["memory_static_max"]      = memMax;
            vmRecord["memory_static_min"]      = memMin;
            vmRecord["memory_dynamic_max"]     = memMax;
            vmRecord["memory_dynamic_min"]     = memMin;
            vmRecord["VCPUs_max"]              = vcpus;
            vmRecord["VCPUs_at_startup"]       = vcpus;
            vmRecord["actions_after_shutdown"] = QString("destroy");
            vmRecord["actions_after_reboot"]   = QString("restart");
            vmRecord["actions_after_crash"]    = QString("restart");

            // ── Boot / platform from OVF xen config (or safe defaults) ───
            // Prefer values exported by OvfWriter (xen:Data elements) or C# XenAdmin
            // (VirtualSystemOtherConfigurationData). Fall back to HVM BIOS defaults only
            // when no xen config is present (e.g. third-party OVF tools).
            const QMap<QString, QString> xenCfg = pkg.XenConfigBySystem().value(vmName);

            const bool hasPvConfig = xenCfg.contains("PV_bootloader")
                                  || xenCfg.contains("PV_kernel");

            if (xenCfg.contains("HVM_boot_policy"))
            {
                vmRecord["HVM_boot_policy"] = xenCfg.value("HVM_boot_policy");
            }
            else if (!hasPvConfig)
            {
                // No PV configuration present → assume HVM with BIOS order
                vmRecord["HVM_boot_policy"] = QString("BIOS order");
            }
            // else: PV VM — leave HVM_boot_policy absent

            if (xenCfg.contains("HVM_boot_params"))
            {
                vmRecord["HVM_boot_params"] = parseSemicolonKvMap(xenCfg.value("HVM_boot_params"));
            }
            else if (!hasPvConfig)
            {
                QVariantMap bootParams;
                bootParams["order"] = "dc";
                vmRecord["HVM_boot_params"] = bootParams;
            }

            if (xenCfg.contains("platform"))
            {
                vmRecord["platform"] = parseSemicolonKvMap(xenCfg.value("platform"));
            }
            else
            {
                QVariantMap platform;
                platform["nx"]     = "true";
                platform["acpi"]   = "1";
                platform["apic"]   = "true";
                platform["pae"]    = "true";
                platform["stdvga"] = "false";
                vmRecord["platform"] = platform;
            }

            if (xenCfg.contains("PV_bootloader"))
                vmRecord["PV_bootloader"] = xenCfg.value("PV_bootloader");
            if (xenCfg.contains("PV_kernel"))
                vmRecord["PV_kernel"] = xenCfg.value("PV_kernel");
            if (xenCfg.contains("PV_ramdisk"))
                vmRecord["PV_ramdisk"] = xenCfg.value("PV_ramdisk");
            if (xenCfg.contains("PV_args"))
                vmRecord["PV_args"] = xenCfg.value("PV_args");
            if (xenCfg.contains("PV_bootloader_args"))
                vmRecord["PV_bootloader_args"] = xenCfg.value("PV_bootloader_args");
            // Hide from XenCenter until fully configured — matches C# pattern.
            // After full setup we call VM.destroy-and-recreate isn't possible;
            // instead we omit HideFromXenCenter and make the VM visible from the start.
            // TODO: use VM.add_to_other_config("HideFromXenCenter","true") once that binding is added.
            vmRecord["other_config"] = QVariantMap();
            if (!this->m_applianceRef.isEmpty())
                vmRecord["appliance"] = this->m_applianceRef;
            if (!mapping.targetHostRef.isEmpty())
                vmRecord["affinity"] = mapping.targetHostRef;

            QString vmRef;
            try
            {
                this->setDescriptionSafe(QString("Creating VM record for '%1'...").arg(vmName));
                vmRef = this->createVm(vmRecord, this->m_applianceRef);
            }
            catch (const VmImportActionBase::CancelledException&)
            {
                throw;
            }
            catch (const std::exception& e)
            {
                this->setError(QString("Failed to create VM '%1': %2").arg(vmName, e.what()));
                this->setState(Failed);
                return;
            }

            this->m_createdVmRefs << vmRef;
            if (this->m_importedVmRef.isEmpty())
                this->m_importedVmRef = vmRef;

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

                // Build local disk file path.
                // For .ovf (folder-based), disks are alongside the .ovf.
                // For .ova, effectiveOvfPath points to the extracted .ovf in a temp dir
                // and all disk files were extracted to that same dir.
                QFileInfo ovfFi(effectiveOvfPath);
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
                catch (const VmImportActionBase::CancelledException&)
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
                    this->setDescriptionSafe(QString("Attaching disk '%1' (%2/%3)...")
                                             .arg(dm.diskHref).arg(diskIdx + 1).arg(diskCount));
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
                    this->setDescriptionSafe(QString("Creating network interface %1/%2 for '%3'...")
                                             .arg(vifIdx + 1)
                                             .arg(mapping.networkMappings.size())
                                             .arg(vmName));
                    this->createVif(vmRef, nm.targetNetworkRef, vifIdx, nm.mac);
                }
                catch (const std::exception& e)
                {
                    qWarning() << "ImportApplianceAction: VIF create failed:" << e.what();
                }
            }

            // ── 4d: OS fixups (optional) ──────────────────────────────────
            // C# equivalent: OVF.SetRunOnceBootCDROMOSFixup / SetTargetISOSRInRASD
            // Attach fixup ISO from the selected SR as a CDROM and boot from it first.
            if (this->m_runFixups)
            {
                this->checkCancelled();
                this->setDescriptionSafe(QString("Applying OS fixups for '%1'...").arg(vmName));
                if (!this->applyFixups(vmRef, this->m_fixupIsoSrRef))
                    qWarning() << "ImportApplianceAction: fixups requested but could not be applied for VM" << vmRef;
            }

            // ── 4e: VM is already visible (HideFromXenCenter not set above) ────

            this->setPercentCompleteSafe(vmProgressEnd);
        }

        this->setDescriptionSafe("Finalizing import...");

        // ── Step 5: Auto-start ────────────────────────────────────────────
        if (this->m_startAutomatically)
        {
            this->setDescriptionSafe("Starting virtual machine(s)...");
            this->setPercentCompleteSafe(95);
            this->checkCancelled();

            if (!this->m_applianceRef.isEmpty())
            {
                // Start as vApp
                try
                {
                    const QString taskRef = XenAPI::VM_appliance::async_start(session, this->m_applianceRef, false);
                    this->pollToCompletion(taskRef, 95, 100);
                }
                catch (const std::exception& e)
                {
                    qWarning() << "ImportApplianceAction: vApp start failed:" << e.what();
                }
            }
            else if (!this->m_importedVmRef.isEmpty())
            {
                // Start individual VM
                try
                {
                    const QString taskRef = XenAPI::VM::async_start(session, this->m_importedVmRef, false, false);
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
    catch (const VmImportActionBase::CancelledException&)
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

// ─── QList<QVariantMap> parseVirtualSystems / buildVmRecord
// (implementations moved to importapplianceaction_vs.cpp if needed, or inlined below)

