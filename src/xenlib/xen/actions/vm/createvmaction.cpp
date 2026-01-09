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

#include "createvmaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_VBD.h"
#include "../../xenapi/xenapi_VDI.h"
#include "../../xenapi/xenapi_VIF.h"
#include "../../xenapi/xenapi_SR.h"
#include "../vm/vmstartaction.h"
#include "../../jsonrpcclient.h"
#include "../../vm.h"
#include <QDebug>
#include <stdexcept>

namespace
{
    QString makeHiddenName(const QString& name)
    {
        return QString("__gui__%1").arg(name);
    }

    bool isHvmVm(const QVariantMap& vmRecord)
    {
        return vmRecord.contains("HVM_boot_policy") &&
               !vmRecord.value("HVM_boot_policy").toString().isEmpty();
    }

    QVariantMap getBootParamsForNetworkFirst(const QVariantMap& current)
    {
        QVariantMap params = current;
        QString order = params.value("order").toString().toLower();
        int idx = order.indexOf("n");
        if (idx == -1)
            order = "n" + order;
        else if (idx > 0)
            order.remove(idx, 1).insert(0, "n");

        if (order.isEmpty())
            order = "ncd";

        params["order"] = order;
        return params;
    }
}

CreateVMAction::CreateVMAction(XenConnection* connection,
                               const QString& templateRef,
                               const QString& nameLabel,
                               const QString& nameDescription,
                               InstallMethod installMethod,
                               const QString& pvArgs,
                               const QString& cdVdiRef,
                               const QString& installUrl,
                               BootMode bootMode,
                               const QString& homeServerRef,
                               qint64 vcpusMax,
                               qint64 vcpusAtStartup,
                               qint64 memoryDynamicMinMb,
                               qint64 memoryDynamicMaxMb,
                               qint64 memoryStaticMaxMb,
                               qint64 coresPerSocket,
                               const QList<DiskConfig>& disks,
                               const QList<VifConfig>& vifs,
                               bool startAfter,
                               bool assignVtpm,
                               QObject* parent)
    : AsyncOperation(connection,
                     QString("Creating VM '%1'").arg(nameLabel),
                     QString("Creating VM from template"),
                     parent),
      m_templateRef(templateRef),
      m_nameLabel(nameLabel),
      m_nameDescription(nameDescription),
      m_installMethod(installMethod),
      m_pvArgs(pvArgs),
      m_cdVdiRef(cdVdiRef),
      m_installUrl(installUrl),
      m_bootMode(bootMode),
      m_homeServerRef(homeServerRef),
      m_vcpusMax(vcpusMax),
      m_vcpusAtStartup(vcpusAtStartup),
      m_memoryDynamicMinMb(memoryDynamicMinMb),
      m_memoryDynamicMaxMb(memoryDynamicMaxMb),
      m_memoryStaticMaxMb(memoryStaticMaxMb),
      m_coresPerSocket(coresPerSocket),
      m_disks(disks),
      m_vifs(vifs),
      m_startAfter(startAfter),
      m_assignVtpm(assignVtpm)
{
}

void CreateVMAction::run()
{
    XenAPI::Session* session = this->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        this->setError("Not connected to XenServer");
        return;
    }

    try
    {
        this->SetDescription(QString("Cloning template"));
        QString hiddenName = makeHiddenName(this->m_nameLabel);
        QString cloneTask = XenAPI::VM::async_clone(session, this->m_templateRef, hiddenName);
        this->pollToCompletion(cloneTask, 0, 10);

        QString newVmRef = this->GetResult();
        if (newVmRef.isEmpty())
            throw std::runtime_error("Clone returned empty VM ref");

        QSharedPointer<VM> cachedVm = this->GetConnection()->WaitForCacheObject<VM>(
            "vm",
            newVmRef,
            60000,
            [this]() { return this->IsCancelled(); });
        if (!cachedVm)
            throw std::runtime_error("VM did not appear in cache after clone");

        QVariantMap templateRecord = XenAPI::VM::get_record(session, this->m_templateRef);
        QVariantMap vmRecord = XenAPI::VM::get_record(session, newVmRef);
        bool isHvm = isHvmVm(vmRecord);

        this->SetDescription(QString("Provisioning VM"));
        QVariantMap otherConfig = vmRecord.value("other_config").toMap();
        if (otherConfig.contains("disks"))
        {
            otherConfig.remove("disks");
            XenAPI::VM::set_other_config(session, newVmRef, otherConfig);
        }
        QString provisionTask = XenAPI::VM::async_provision(session, newVmRef);
        this->pollToCompletion(provisionTask, 10, 60);

        this->SetDescription(QString("Saving VM properties"));
        XenAPI::VM::set_name_label(session, newVmRef, this->m_nameLabel);
        XenAPI::VM::set_name_description(session, newVmRef, this->m_nameDescription);

        if (this->m_vcpusMax > 0)
        {
            XenAPI::VM::set_VCPUs_max(session, newVmRef, this->m_vcpusMax);
        }
        if (this->m_vcpusAtStartup > 0)
        {
            XenAPI::VM::set_VCPUs_at_startup(session, newVmRef, this->m_vcpusAtStartup);
        }

        if (this->m_coresPerSocket > 0)
        {
            QVariantMap platform = vmRecord.value("platform").toMap();
            platform["cores-per-socket"] = QString::number(this->m_coresPerSocket);
            XenAPI::VM::set_platform(session, newVmRef, platform);
        }

        if (this->m_memoryStaticMaxMb > 0 && this->m_memoryDynamicMinMb > 0 && this->m_memoryDynamicMaxMb > 0)
        {
            qint64 staticMax = this->m_memoryStaticMaxMb * 1024 * 1024;
            qint64 dynMin = this->m_memoryDynamicMinMb * 1024 * 1024;
            qint64 dynMax = this->m_memoryDynamicMaxMb * 1024 * 1024;
            qint64 staticMin = templateRecord.value("memory_static_min").toLongLong();
            if (staticMin <= 0)
                staticMin = dynMin > 0 ? dynMin : staticMax;
            XenAPI::VM::set_memory_limits(session, newVmRef, staticMin, staticMax, dynMin, dynMax);
        }

        if (!this->m_homeServerRef.isEmpty())
        {
            XenAPI::VM::set_affinity(session, newVmRef, this->m_homeServerRef);
        }

        if (isHvm && (this->m_disks.isEmpty() || this->m_installMethod == InstallMethod::Network))
        {
            QVariantMap bootParams = vmRecord.value("HVM_boot_params").toMap();
            XenAPI::VM::set_HVM_boot_params(session, newVmRef, getBootParamsForNetworkFirst(bootParams));
        }

        if (!isHvm && !this->m_pvArgs.isEmpty())
        {
            XenAPI::VM::set_PV_args(session, newVmRef, this->m_pvArgs);
        } else if (isHvm && this->m_bootMode != BootMode::Auto)
        {
            QVariantMap bootParams = vmRecord.value("HVM_boot_params").toMap();
            bootParams["firmware"] = (this->m_bootMode == BootMode::Bios) ? "bios" : "uefi";
            XenAPI::VM::set_HVM_boot_params(session, newVmRef, bootParams);

            QVariantMap platform = vmRecord.value("platform").toMap();
            platform["secureboot"] = (this->m_bootMode == BootMode::SecureUefi) ? "true" : "false";
            XenAPI::VM::set_platform(session, newVmRef, platform);
        }

        this->SetDescription(QString("Configuring CD/DVD drive"));
        vmRecord = XenAPI::VM::get_record(session, newVmRef);
        QVariantList vbdRefs = vmRecord.value("VBDs").toList();
        QString cdVbdRef;
        for (const QVariant& vbdRefVar : vbdRefs)
        {
            QString vbdRef = vbdRefVar.toString();
            QVariantMap vbdRecord = XenAPI::VBD::get_record(session, vbdRef);
            if (vbdRecord.value("type").toString() != "CD")
                continue;

            QString userdevice = vbdRecord.value("userdevice").toString();
            if (!userdevice.isEmpty() && QStringLiteral("0123").contains(userdevice))
            {
                cdVbdRef = vbdRef;
                break;
            }
        }

        if (cdVbdRef.isEmpty())
        {
            QVariantList allowedDevices = XenAPI::VM::get_allowed_VBD_devices(session, newVmRef).toList();
            QString device = allowedDevices.contains("3") ? "3" : (allowedDevices.isEmpty() ? "0" : allowedDevices.first().toString());

            QVariantMap vbdRecord;
            vbdRecord["VM"] = newVmRef;
            vbdRecord["VDI"] = "OpaqueRef:NULL";
            vbdRecord["bootable"] = (this->m_installMethod == InstallMethod::CD);
            vbdRecord["device"] = "";
            vbdRecord["userdevice"] = device;
            vbdRecord["empty"] = true;
            vbdRecord["type"] = "CD";
            vbdRecord["mode"] = "RO";
            vbdRecord["unpluggable"] = true;
            vbdRecord["other_config"] = QVariantMap();
            vbdRecord["qos_algorithm_type"] = "";
            vbdRecord["qos_algorithm_params"] = QVariantMap();

            cdVbdRef = XenAPI::VBD::create(session, vbdRecord);
        }

        if (!cdVbdRef.isEmpty())
        {
            QVariantMap cdRecord = XenAPI::VBD::get_record(session, cdVbdRef);
            bool cdEmpty = cdRecord.value("empty", true).toBool();
            if (!cdEmpty)
            {
                QString taskRef = XenAPI::VBD::async_eject(session, cdVbdRef);
                this->pollToCompletion(taskRef, 60, 65, true);
            }

            if (this->m_installMethod == InstallMethod::CD && !this->m_cdVdiRef.isEmpty())
            {
                QString taskRef = XenAPI::VBD::async_insert(session, cdVbdRef, this->m_cdVdiRef);
                this->pollToCompletion(taskRef, 65, 70, true);
            }
        }

        this->addDisks(session, newVmRef);

        this->SetDescription(QString("Configuring networks"));
        vmRecord = XenAPI::VM::get_record(session, newVmRef);
        QVariantList vifRefs = vmRecord.value("VIFs").toList();
        for (const QVariant& vifRefVar : vifRefs)
        {
            QString vifRef = vifRefVar.toString();
            if (!vifRef.isEmpty())
                XenAPI::VIF::destroy(session, vifRef);
        }

        for (const VifConfig& vif : this->m_vifs)
        {
            QVariantMap vifRecord;
            vifRecord["VM"] = newVmRef;
            vifRecord["network"] = vif.networkRef;
            vifRecord["device"] = vif.device;
            vifRecord["MAC"] = vif.mac;
            vifRecord["MTU"] = QString::number(1500);
            vifRecord["other_config"] = QVariantMap();
            vifRecord["qos_algorithm_type"] = "";
            vifRecord["qos_algorithm_params"] = QVariantMap();
            vifRecord["locking_mode"] = "network_default";
            vifRecord["ipv4_allowed"] = QVariantList();
            vifRecord["ipv6_allowed"] = QVariantList();

            XenAPI::VIF::create(session, vifRecord);
        }

        if (this->m_startAfter)
        {
            VM* vmInstance = new VM(this->GetConnection(), newVmRef);
            VMStartAction* startAction = new VMStartAction(QSharedPointer<VM>(vmInstance), nullptr, nullptr, this);
            startAction->RunAsync();
        }

        if (this->HasError() || this->IsCancelled())
            return;

        this->SetDescription(QString("VM created successfully"));
    }
    catch (const std::exception& e)
    {
        this->setError(QString("Failed to create VM: %1").arg(e.what()));
    }
}

void CreateVMAction::addDisks(XenAPI::Session* session, const QString& vmRef)
{
    if (this->m_disks.isEmpty())
        return;

    this->SetDescription(QString("Creating disks"));

    QVariantMap vmRecord = XenAPI::VM::get_record(session, vmRef);
    QVariantList vbdRefs = vmRecord.value("VBDs").toList();

    bool firstDisk = true;
    QString suspendSr;

    double progress = 70;
    double step = 20.0 / this->m_disks.size();

    for (const DiskConfig& disk : this->m_disks)
    {
        QString vbdRef = getDiskVbd(session, disk, vbdRefs);
        QString vdiRef;
        if (!vbdRef.isEmpty())
        {
            QVariantMap vbdRecord = XenAPI::VBD::get_record(session, vbdRef);
            vdiRef = vbdRecord.value("VDI").toString();
        }

        if (!diskOk(session, disk, vbdRef))
        {
            if (!vbdRef.isEmpty())
                vdiRef = moveDisk(session, vmRef, disk, vbdRef, progress, step);
            else
                vdiRef = createDisk(session, vmRef, disk, progress, step);
        }

        if (vdiRef.isEmpty())
        {
            progress += step;
            continue;
        }

        QString diskName = disk.nameLabel.isEmpty()
            ? QString("%1 Disk %2").arg(this->m_nameLabel, disk.device)
            : disk.nameLabel;
        QString diskDescription = disk.nameDescription;

        QVariantMap vdiRecord = XenAPI::VDI::get_record(session, vdiRef);
        if (vdiRecord.value("name_description").toString() != diskDescription)
            XenAPI::VDI::set_name_description(session, vdiRef, diskDescription);
        if (vdiRecord.value("name_label").toString() != diskName)
            XenAPI::VDI::set_name_label(session, vdiRef, diskName);

        if (firstDisk)
        {
            QString srRef = vdiRecord.value("SR").toString();
            if (!srRef.isEmpty() && !srIsHbaLunPerVdi(session, srRef))
                suspendSr = srRef;

            firstDisk = false;
        }

        progress += step;
    }

    XenAPI::VM::set_suspend_SR(session, vmRef, suspendSr);
}

QString CreateVMAction::getDiskVbd(XenAPI::Session* session, const DiskConfig& disk, const QVariantList& vbds)
{
    if (disk.device.isEmpty())
        return QString();

    for (const QVariant& vbdRefVar : vbds)
    {
        QString vbdRef = vbdRefVar.toString();
        if (vbdRef.isEmpty())
            continue;

        QVariantMap vbdRecord = XenAPI::VBD::get_record(session, vbdRef);
        if (vbdRecord.value("userdevice").toString() == disk.device)
            return vbdRef;
    }

    return QString();
}

bool CreateVMAction::diskOk(XenAPI::Session* session, const DiskConfig& disk, const QString& vbdRef)
{
    if (vbdRef.isEmpty())
        return false;

    QVariantMap vbdRecord = XenAPI::VBD::get_record(session, vbdRef);
    QString vdiRef = vbdRecord.value("VDI").toString();
    if (vdiRef.isEmpty())
        return false;

    QVariantMap vdiRecord = XenAPI::VDI::get_record(session, vdiRef);
    if (vdiRecord.isEmpty())
        return false;

    if (disk.srRef.isEmpty())
        return true;

    return vdiRecord.value("SR").toString() == disk.srRef;
}

QString CreateVMAction::moveDisk(XenAPI::Session* session, const QString& vmRef, const DiskConfig& disk, const QString& vbdRef, double progress, double step)
{
    QVariantMap vbdRecord = XenAPI::VBD::get_record(session, vbdRef);
    QString oldVdiRef = vbdRecord.value("VDI").toString();
    if (oldVdiRef.isEmpty())
        throw std::runtime_error("VBD has no VDI attached");

    QString targetSr = disk.srRef;
    if (targetSr.isEmpty())
    {
        QVariantMap vdiRecord = XenAPI::VDI::get_record(session, oldVdiRef);
        targetSr = vdiRecord.value("SR").toString();
    }

    QString copyTask = XenAPI::VDI::async_copy(session, oldVdiRef, targetSr);
    if (copyTask.isEmpty())
        throw std::runtime_error("VDI.copy returned empty task ref");

    this->pollToCompletion(copyTask, progress, progress + 0.25 * step);
    QString newVdiRef = this->GetResult();

    if (newVdiRef.isEmpty())
        throw std::runtime_error("VDI.copy returned empty VDI ref");

    addVmHint(session, vmRef, newVdiRef);

    QString destroyVbdTask = XenAPI::VBD::async_destroy(session, vbdRef);
    this->pollToCompletion(destroyVbdTask, progress + 0.25 * step, progress + 0.5 * step);

    QString destroyVdiTask = XenAPI::VDI::async_destroy(session, oldVdiRef);
    this->pollToCompletion(destroyVdiTask, progress + 0.5 * step, progress + 0.75 * step);

    createVbd(session, vmRef, disk, newVdiRef, progress + 0.75 * step, progress + step, isDeviceAtPositionZero(disk));
    return newVdiRef;
}

QString CreateVMAction::createDisk(XenAPI::Session* session, const QString& vmRef, const DiskConfig& disk, double progress, double step)
{
    QString vdiRef = disk.vdiRef;
    bool bootable = false;

    if (vdiRef.isEmpty())
    {
        vdiRef = createVdi(session, disk, progress, progress + 0.75 * step);
        bootable = isDeviceAtPositionZero(disk) && this->m_installMethod != InstallMethod::CD;
    }

    addVmHint(session, vmRef, vdiRef);
    createVbd(session, vmRef, disk, vdiRef, progress + 0.75 * step, progress + step, bootable);
    return vdiRef;
}

void CreateVMAction::addVmHint(XenAPI::Session* session, const QString& vmRef, const QString& vdiRef)
{
    if (vdiRef.isEmpty())
        return;

    QVariantMap smConfig = XenAPI::VDI::get_sm_config(session, vdiRef);
    smConfig["vmhint"] = vmRef;
    XenAPI::VDI::set_sm_config(session, vdiRef, smConfig);
}

QString CreateVMAction::createVdi(XenAPI::Session* session, const DiskConfig& disk, double progress1, double progress2)
{
    if (disk.srRef.isEmpty())
        throw std::runtime_error("Disk SR is not set");

    QString diskName = disk.nameLabel.isEmpty()
        ? QString("%1 Disk %2").arg(this->m_nameLabel, disk.device)
        : disk.nameLabel;
    QString diskDescription = disk.nameDescription;

    QVariantMap vdiRecord;
    vdiRecord["name_label"] = diskName;
    vdiRecord["name_description"] = diskDescription;
    vdiRecord["read_only"] = disk.readOnly;
    vdiRecord["sharable"] = disk.sharable;
    vdiRecord["SR"] = disk.srRef;
    vdiRecord["type"] = disk.vdiType.isEmpty() ? "user" : disk.vdiType;
    vdiRecord["virtual_size"] = disk.sizeBytes;
    vdiRecord["sm_config"] = QVariantMap();
    vdiRecord["other_config"] = QVariantMap();

    qDebug() << "[CreateVMAction] VDI.create record:" << vdiRecord;
    QString createTask = XenAPI::VDI::async_create(session, vdiRecord);
    if (createTask.isEmpty())
    {
        qDebug() << "[CreateVMAction] VDI.async_create failed:" << Xen::JsonRpcClient::lastError();
        throw std::runtime_error("VDI.create returned empty task ref");
    }

    this->pollToCompletion(createTask, progress1, progress2);
    QString vdiRef = this->GetResult();
    if (vdiRef.isEmpty())
        throw std::runtime_error("VDI.create returned empty VDI ref");
    return vdiRef;
}

void CreateVMAction::createVbd(XenAPI::Session* session, const QString& vmRef, const DiskConfig& disk, const QString& vdiRef, double progress1, double progress2, bool bootable)
{
    QVariantList devices = XenAPI::VM::get_allowed_VBD_devices(session, vmRef).toList();
    if (devices.isEmpty())
        throw std::runtime_error("No available VBD devices");

    QString userdevice = disk.device;
    if (userdevice.isEmpty() || !devices.contains(userdevice))
        userdevice = devices.first().toString();

    QVariantMap vbdRecord;
    vbdRecord["VM"] = vmRef;
    vbdRecord["VDI"] = vdiRef;
    vbdRecord["bootable"] = bootable;
    vbdRecord["empty"] = false;
    vbdRecord["unpluggable"] = true;
    vbdRecord["mode"] = "RW";
    vbdRecord["type"] = "Disk";
    vbdRecord["userdevice"] = userdevice;
    vbdRecord["device"] = "";
    vbdRecord["other_config"] = QVariantMap();
    vbdRecord["qos_algorithm_type"] = "";
    vbdRecord["qos_algorithm_params"] = QVariantMap();

    qDebug() << "[CreateVMAction] VBD.create record:" << vbdRecord;
    QString createTask = XenAPI::VBD::async_create(session, vbdRecord);
    if (createTask.isEmpty())
    {
        qDebug() << "[CreateVMAction] VBD.async_create failed:" << Xen::JsonRpcClient::lastError();
        throw std::runtime_error("VBD.create returned empty task ref");
    }

    this->pollToCompletion(createTask, progress1, progress2);
}

bool CreateVMAction::isDeviceAtPositionZero(const DiskConfig& disk) const
{
    return disk.device == "0";
}

bool CreateVMAction::srIsHbaLunPerVdi(XenAPI::Session* session, const QString& srRef)
{
    if (srRef.isEmpty())
        return false;

    QVariantMap srRecord = XenAPI::SR::get_record(session, srRef);
    return srRecord.value("type").toString() == "rawhba";
}
