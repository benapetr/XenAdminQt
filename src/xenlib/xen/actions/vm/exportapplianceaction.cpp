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

#include "exportapplianceaction.h"

#include "../../network/httpclient.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Task.h"
#include "../../vm.h"
#include "../../vbd.h"
#include "../../vdi.h"
#include "../../vif.h"
#include "../../network.h"
#include "../../../xencache.h"
#include "../../../ovf/ovfwriter.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

// ── Constructor ───────────────────────────────────────────────────────────────

ExportApplianceAction::ExportApplianceAction(const QList<QSharedPointer<VM>>& vms,
                                             const QString& applianceDir,
                                             const QString& applianceName,
                                             const QStringList& eulas,
                                             bool createManifest,
                                             bool createOva,
                                             bool compressFiles,
                                             bool shouldVerify,
                                             QObject* parent)
    : AsyncOperation(tr("Exporting OVF Appliance"), tr("Preparing export…"), parent)
    , m_vms(vms)
    , m_applianceDir(applianceDir)
    , m_applianceName(applianceName)
    , m_eulaFilePaths(eulas)
    , m_createManifest(createManifest)
    , m_createOva(createOva)
    , m_compressFiles(compressFiles)
    , m_shouldVerify(shouldVerify)
{
    if (vms.isEmpty())
        throw std::invalid_argument("No VMs supplied to ExportApplianceAction");

    // Use the first VM's connection
    this->m_connection = vms.first()->GetConnection();

    const QString label = (vms.size() == 1)
        ? tr("Export %1 as OVF appliance").arg(vms.first()->GetName())
        : tr("Export %1 VMs as OVF appliance").arg(vms.size());
    this->SetTitle(label);

    // RBAC dependencies matching C# ApplianceAction.StaticRBACDependencies
    this->AddApiMethodToRoleCheck(QStringLiteral("task.create"));
    this->AddApiMethodToRoleCheck(QStringLiteral("VM.add_to_other_config"));
    this->AddApiMethodToRoleCheck(QStringLiteral("VDI.create"));
    this->AddApiMethodToRoleCheck(QStringLiteral("VBD.create"));
    this->AddApiMethodToRoleCheck(QStringLiteral("VIF.create"));
    this->AddApiMethodToRoleCheck(QStringLiteral("Host.call_plugin"));
}

// ── run() ─────────────────────────────────────────────────────────────────────

void ExportApplianceAction::run()
{
    this->SetSafeToExit(false);

    if (!this->GetConnection() || !this->GetSession())
    {
        this->setError(tr("Not connected to XenServer"));
        this->setState(OperationState::Failed);
        return;
    }

    // Create output directory: applianceDir/applianceName/
    const QString appFolder = m_applianceDir + QDir::separator() + m_applianceName;
    {
        QDir d;
        if (!d.mkpath(appFolder))
        {
            this->setError(tr("Cannot create output directory: %1").arg(appFolder));
            this->setState(OperationState::Failed);
            return;
        }
    }

    // Export each VM, 0–80% of total progress
    OvfEnvelopeData envelope;
    envelope.name = m_applianceName;

    const int vmTotal = m_vms.size();
    for (int i = 0; i < vmTotal; ++i)
    {
        if (this->IsCancelled())
            break;

        const QSharedPointer<VM>& vm = m_vms[i];
        this->SetDescription(tr("Exporting %1…").arg(vm->GetName()));

        OvfVirtualSystemEntry sys;
        if (!this->buildVirtualSystem(vm, appFolder, i, vmTotal, sys))
        {
            // Error already set inside buildVirtualSystem
            this->setState(OperationState::Failed);
            return;
        }
        envelope.systems << sys;
        this->SetPercentComplete((i + 1) * 80 / vmTotal);
    }

    if (this->IsCancelled())
    {
        this->setState(OperationState::Cancelled);
        return;
    }

    // Read EULA file content
    for (const QString& eulaPath : m_eulaFilePaths)
    {
        QFile f(eulaPath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            envelope.eulaTexts << QString::fromUtf8(f.readAll());
    }

    // Write OVF descriptor
    const QString ovfFileName = m_applianceName + QStringLiteral(".ovf");
    const QString ovfFilePath = appFolder + QDir::separator() + ovfFileName;
    this->SetDescription(tr("Writing OVF descriptor…"));

    OvfWriter writer;
    if (!writer.saveToFile(envelope, ovfFilePath))
    {
        this->setError(tr("Failed to write OVF file: %1").arg(ovfFilePath));
        this->setState(OperationState::Failed);
        return;
    }
    this->SetPercentComplete(85);

    if (this->IsCancelled())
    {
        this->setState(OperationState::Cancelled);
        return;
    }

    // Manifest
    if (m_createManifest || m_createOva)
    {
        this->SetDescription(tr("Creating manifest…"));
        if (!writer.createManifest(envelope, appFolder, ovfFileName))
        {
            this->setError(tr("Failed to create manifest for package %1").arg(m_applianceName));
            this->setState(OperationState::Failed);
            return;
        }
        this->SetPercentComplete(90);
    }

    // OVA
    if (m_createOva)
    {
        const QString ovaPath = m_applianceDir + QDir::separator()
                                + m_applianceName + QStringLiteral(".ova");
        this->SetDescription(tr("Creating OVA archive %1.ova…").arg(m_applianceName));
        if (!writer.createOva(envelope, appFolder, ovfFileName, ovaPath))
        {
            this->setError(tr("Failed to create OVA archive: %1").arg(ovaPath));
            this->setState(OperationState::Failed);
            return;
        }
        this->SetPercentComplete(98);
    }

    this->SetPercentComplete(100);
    this->SetDescription(tr("Export complete"));
    this->setState(OperationState::Completed);
}

// ── buildVirtualSystem() ──────────────────────────────────────────────────────

bool ExportApplianceAction::buildVirtualSystem(const QSharedPointer<VM>& vm,
                                               const QString& appFolder,
                                               int vmIndex,
                                               int vmTotal,
                                               OvfVirtualSystemEntry& sys)
{
    sys.id          = vm->GetUUID();
    sys.name        = vm->GetName();
    sys.description = vm->GetDescription();

    // VirtualSystemType: "xen-3.0-<arch>"
    const QString arch = vm->Domarch().isEmpty() ? QStringLiteral("unknown") : vm->Domarch();
    const QString pv   = vm->IsHVM() ? QStringLiteral("hvm") : QStringLiteral("xen");
    sys.vmType = QStringLiteral("%1-3.0-%2").arg(pv, arch);

    sys.vcpuAtStartup = vm->VCPUsAtStartup();
    sys.vcpuMax       = vm->VCPUsMax();
    sys.memoryMB      = vm->GetMemoryDynamicMax() / (1024 * 1024);

    // Startup ordering
    const QVariantMap rawData = vm->GetData();
    sys.startOrder    = rawData.value(QStringLiteral("order"),          0).toInt();
    sys.startDelay    = rawData.value(QStringLiteral("start_delay"),    0).toInt();
    sys.shutdownDelay = rawData.value(QStringLiteral("shutdown_delay"), 0).toInt();

    // Networks (VIFs)
    const QList<QSharedPointer<VIF>> vifs = vm->GetVIFs();
    for (const QSharedPointer<VIF>& vif : vifs)
    {
        if (!vif || !vif->IsValid())
            continue;

        OvfNetworkEntry net;
        net.mac = vif->GetMAC();

        const QSharedPointer<Network> network = vif->GetNetwork();
        if (network && network->IsValid())
        {
            net.name        = network->GetName();
            net.description = network->GetDescription();
        } else
        {
            net.name = QStringLiteral("Network %1").arg(vif->GetDevice());
        }
        sys.networks << net;
    }

    // Disks (VBDs) — download each non-CD VDI
    const QList<QSharedPointer<VBD>> vbds = vm->GetVBDs();
    const int vbdTotal = vbds.size();
    int diskIndex = 0;
    int fileIndex = 1; // For file IDs: "file1", "file2", …

    for (int vi = 0; vi < vbdTotal; ++vi)
    {
        const QSharedPointer<VBD>& vbd = vbds[vi];
        if (!vbd || !vbd->IsValid())
            continue;

        if (this->IsCancelled())
            return false;

        OvfDiskEntry disk;
        disk.addressOnParent = vbd->GetUserdevice();
        disk.bootable        = vbd->IsBootable();
        disk.isCdrom         = (vbd->GetType().compare(
                                    QStringLiteral("CD"), Qt::CaseInsensitive) == 0);

        if (disk.isCdrom)
        {
            disk.name = QStringLiteral("CD/DVD Drive");
            disk.description = QStringLiteral("CD-ROM drive");
            // No file to download; still add an entry so the CDROM slot is in the RASD
            disk.diskId  = QStringLiteral("cdrom%1").arg(diskIndex++);
            disk.fileId  = disk.diskId; // Not used for CDROM
            disk.href    = QString();
            sys.disks << disk;
            continue;
        }

        const QSharedPointer<VDI> vdi = vbd->GetVDI();
        if (!vdi || !vdi->IsValid())
            continue;

        const QString vdiUuid   = vdi->GetUUID();
        const qint64 physBytes  = vdi->PhysicalUtilisation();
        const qint64 virtBytes  = vdi->VirtualSize();
        const QString diskName  = vdi->GetName().isEmpty()
                                  ? QStringLiteral("Disk %1").arg(diskIndex)
                                  : vdi->GetName();

        disk.fileId  = QStringLiteral("file%1").arg(fileIndex);
        disk.diskId  = QStringLiteral("disk%1").arg(fileIndex);
        disk.href    = vdiUuid + QStringLiteral(".vhd");
        disk.physicalBytes = physBytes;
        disk.virtualBytes  = virtBytes;
        disk.name        = diskName;
        disk.description = vdi->GetDescription();
        ++fileIndex;
        ++diskIndex;

        const QString vhdPath = appFolder + QDir::separator() + disk.href;

        // Download VHD
        if (!this->downloadVhd(vdiUuid, virtBytes, vhdPath, diskName,
                               vmTotal, vmIndex, vi, vbdTotal))
        {
            return false;
        }

        sys.disks << disk;
    }

    // Xen-specific config (HVM/PV boot params, platform, etc.)
    // Matches C# Export.cs "#region ADD XEN SPECIFICS"
    const QVariantMap hvmBootParams = vm->HVMBootParams();
    if (!hvmBootParams.isEmpty())
    {
        QStringList parts;
        for (auto it = hvmBootParams.cbegin(); it != hvmBootParams.cend(); ++it)
            parts << QStringLiteral("%1=%2").arg(it.key(), it.value().toString());
        sys.xenConfig << qMakePair(QStringLiteral("HVM_boot_params"), parts.join(QChar(';')));
    }

    const QString hvmBootPolicy = vm->HVMBootPolicy();
    if (!hvmBootPolicy.isEmpty())
        sys.xenConfig << qMakePair(QStringLiteral("HVM_boot_policy"), hvmBootPolicy);

    const QVariantMap platform = vm->GetPlatform();
    if (!platform.isEmpty())
    {
        QStringList parts;
        for (auto it = platform.cbegin(); it != platform.cend(); ++it)
            parts << QStringLiteral("%1=%2").arg(it.key(), it.value().toString());
        sys.xenConfig << qMakePair(QStringLiteral("platform"), parts.join(QChar(';')));
    }

    const QString pvBootloader = vm->PVBootloader();
    if (!pvBootloader.isEmpty())
        sys.xenConfig << qMakePair(QStringLiteral("PV_bootloader"), pvBootloader);

    const QString pvKernel = vm->PVKernel();
    if (!pvKernel.isEmpty())
        sys.xenConfig << qMakePair(QStringLiteral("PV_kernel"), pvKernel);

    const QString pvRamdisk = vm->PVRamdisk();
    if (!pvRamdisk.isEmpty())
        sys.xenConfig << qMakePair(QStringLiteral("PV_ramdisk"), pvRamdisk);

    const QString pvArgs = vm->PVArgs();
    if (!pvArgs.isEmpty())
        sys.xenConfig << qMakePair(QStringLiteral("PV_args"), pvArgs);

    const QString pvBootloaderArgs = vm->PVBootloaderArgs();
    if (!pvBootloaderArgs.isEmpty())
        sys.xenConfig << qMakePair(QStringLiteral("PV_bootloader_args"), pvBootloaderArgs);

    return true;
}

// ── downloadVhd() ─────────────────────────────────────────────────────────────

bool ExportApplianceAction::downloadVhd(const QString& vdiUuid,
                                        qint64 vdiSize,
                                        const QString& vhdPath,
                                        const QString& diskLabel,
                                        int vmTotal, int vmIndex,
                                        int vbdIndex, int vbdTotal)
{
    const QString hostname = this->GetConnection()->GetHostname();

    // Create a task for progress tracking
    QString taskRef;
    try
    {
        taskRef = XenAPI::Task::Create(this->GetSession(),
                                       QStringLiteral("export_raw_vdi"),
                                       QStringLiteral("Exporting VDI %1").arg(vdiUuid));
    }
    catch (const std::exception& e)
    {
        this->setError(tr("Failed to create export task: %1").arg(QString::fromStdString(e.what())));
        return false;
    }

    QMap<QString, QString> params;
    params[QStringLiteral("task_id")]    = taskRef;
    params[QStringLiteral("session_id")] = this->GetSession()->GetSessionID();
    params[QStringLiteral("vdi")]        = vdiUuid;
    params[QStringLiteral("format")]     = QStringLiteral("vhd");

    HttpClient http;
    const bool ok = http.getFile(
        hostname,
        QStringLiteral("/export_raw_vdi"),
        params,
        vhdPath,
        [&](qint64 bytes)
        {
            // Scale progress: VM range is 0–80%, this VBD sub-ranges within one VM's slice
            const double vmSlice = 80.0 / vmTotal;
            const double vbdFraction = (vbdTotal > 0 && vdiSize > 0)
                ? (static_cast<double>(vbdIndex) + static_cast<double>(bytes) / vdiSize)
                  / vbdTotal
                : 0.0;
            const int pct = static_cast<int>(vmIndex * vmSlice + vbdFraction * vmSlice);
            this->SetPercentComplete(qBound(0, pct, 80));

            const double mb = bytes / (1024.0 * 1024.0);
            this->SetDescription(tr("Exporting disk %1 (%2 MB)…").arg(diskLabel).arg(mb, 0, 'f', 1));
        },
        [this]() -> bool { return this->IsCancelled(); }
    );

    if (!ok)
    {
        QFile::remove(vhdPath);
        if (this->IsCancelled())
            return false;
        this->setError(tr("Failed to download VHD for disk %1: %2")
                       .arg(diskLabel, http.lastError()));
        return false;
    }

    return true;
}
