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
#include "../vm/vmstartaction.h"
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
                               qint64 vcpusAtStartup,
                               qint64 memoryStaticMaxMb,
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
      m_vcpusAtStartup(vcpusAtStartup),
      m_memoryStaticMaxMb(memoryStaticMaxMb),
      m_disks(disks),
      m_vifs(vifs),
      m_startAfter(startAfter),
      m_assignVtpm(assignVtpm)
{
}

void CreateVMAction::run()
{
    XenAPI::Session* sess = GetSession();
    if (!sess || !sess->IsLoggedIn())
    {
        setError("Not connected to XenServer");
        return;
    }

    try
    {
        SetDescription(QString("Cloning template"));
        QString hiddenName = makeHiddenName(m_nameLabel);
        QString cloneTask = XenAPI::VM::async_clone(sess, m_templateRef, hiddenName);
        pollToCompletion(cloneTask, 0, 10);

        QString newVmRef = GetResult();
        if (newVmRef.isEmpty())
            throw std::runtime_error("Clone returned empty VM ref");

        SetResult(newVmRef);

        QVariantMap templateRecord = XenAPI::VM::get_record(sess, m_templateRef);
        QVariantMap vmRecord = XenAPI::VM::get_record(sess, newVmRef);
        bool isHvm = isHvmVm(vmRecord);

        SetDescription(QString("Saving VM properties"));
        XenAPI::VM::set_name_label(sess, newVmRef, m_nameLabel);
        XenAPI::VM::set_name_description(sess, newVmRef, m_nameDescription);

        if (m_vcpusAtStartup > 0)
        {
            XenAPI::VM::set_VCPUs_max(sess, newVmRef, m_vcpusAtStartup);
            XenAPI::VM::set_VCPUs_at_startup(sess, newVmRef, m_vcpusAtStartup);
        }

        if (m_memoryStaticMaxMb > 0)
        {
            qint64 memoryBytes = m_memoryStaticMaxMb * 1024 * 1024;
            qint64 staticMin = templateRecord.value("memory_static_min").toLongLong();
            if (staticMin <= 0)
                staticMin = memoryBytes;
            XenAPI::VM::set_memory_limits(sess, newVmRef, staticMin, memoryBytes, memoryBytes, memoryBytes);
        }

        if (!m_homeServerRef.isEmpty())
        {
            XenAPI::VM::set_affinity(sess, newVmRef, m_homeServerRef);
        }

        if (isHvm && (m_disks.isEmpty() || m_installMethod == InstallMethod::Network))
        {
            QVariantMap bootParams = vmRecord.value("HVM_boot_params").toMap();
            XenAPI::VM::set_HVM_boot_params(sess, newVmRef, getBootParamsForNetworkFirst(bootParams));
        }

        if (!isHvm && !m_pvArgs.isEmpty())
        {
            XenAPI::VM::set_PV_args(sess, newVmRef, m_pvArgs);
        }
        else if (isHvm && m_bootMode != BootMode::Auto)
        {
            QVariantMap bootParams = vmRecord.value("HVM_boot_params").toMap();
            bootParams["firmware"] = (m_bootMode == BootMode::Bios) ? "bios" : "uefi";
            XenAPI::VM::set_HVM_boot_params(sess, newVmRef, bootParams);

            QVariantMap platform = vmRecord.value("platform").toMap();
            platform["secureboot"] = (m_bootMode == BootMode::SecureUefi) ? "true" : "false";
            XenAPI::VM::set_platform(sess, newVmRef, platform);
        }

        SetDescription(QString("Configuring CD/DVD drive"));
        vmRecord = XenAPI::VM::get_record(sess, newVmRef);
        QVariantList vbdRefs = vmRecord.value("VBDs").toList();
        QString cdVbdRef;
        for (const QVariant& vbdRefVar : vbdRefs)
        {
            QString vbdRef = vbdRefVar.toString();
            QVariantMap vbdRecord = XenAPI::VBD::get_record(sess, vbdRef);
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
            QVariantList allowedDevices = XenAPI::VM::get_allowed_VBD_devices(sess, newVmRef).toList();
            QString device = allowedDevices.contains("3") ? "3" : (allowedDevices.isEmpty() ? "0" : allowedDevices.first().toString());

            QVariantMap vbdRecord;
            vbdRecord["VM"] = newVmRef;
            vbdRecord["bootable"] = (m_installMethod == InstallMethod::CD);
            vbdRecord["device"] = "";
            vbdRecord["userdevice"] = device;
            vbdRecord["empty"] = true;
            vbdRecord["type"] = "CD";
            vbdRecord["mode"] = "RO";
            vbdRecord["unpluggable"] = true;
            vbdRecord["other_config"] = QVariantMap();
            vbdRecord["qos_algorithm_type"] = "";
            vbdRecord["qos_algorithm_params"] = QVariantMap();

            cdVbdRef = XenAPI::VBD::create(sess, vbdRecord);
        }

        if (!cdVbdRef.isEmpty())
        {
            QVariantMap cdRecord = XenAPI::VBD::get_record(sess, cdVbdRef);
            bool cdEmpty = cdRecord.value("empty", true).toBool();
            if (!cdEmpty)
            {
                QString taskRef = XenAPI::VBD::async_eject(sess, cdVbdRef);
                pollToCompletion(taskRef, 60, 65, true);
            }

            if (m_installMethod == InstallMethod::CD && !m_cdVdiRef.isEmpty())
            {
                QString taskRef = XenAPI::VBD::async_insert(sess, cdVbdRef, m_cdVdiRef);
                pollToCompletion(taskRef, 65, 70, true);
            }
        }

        SetDescription(QString("Configuring disks"));
        for (const DiskConfig& disk : m_disks)
        {
            QString vdiRef = disk.vdiRef;
            if (vdiRef.isEmpty())
            {
                QVariantMap vdiRecord;
                vdiRecord["name_label"] = QString("%1 Disk %2").arg(m_nameLabel, disk.device);
                vdiRecord["name_description"] = "";
                vdiRecord["SR"] = disk.srRef;
                vdiRecord["virtual_size"] = QString::number(disk.sizeBytes);
                vdiRecord["type"] = "user";
                vdiRecord["sharable"] = false;
                vdiRecord["read_only"] = false;
                vdiRecord["other_config"] = QVariantMap();

                vdiRef = XenAPI::VDI::create(sess, vdiRecord);
            }

            QVariantMap vbdRecord;
            vbdRecord["VM"] = newVmRef;
            vbdRecord["VDI"] = vdiRef;
            vbdRecord["userdevice"] = disk.device;
            vbdRecord["bootable"] = disk.bootable;
            vbdRecord["mode"] = "RW";
            vbdRecord["type"] = "Disk";
            vbdRecord["unpluggable"] = true;
            vbdRecord["empty"] = false;
            vbdRecord["other_config"] = QVariantMap();
            vbdRecord["qos_algorithm_type"] = "";
            vbdRecord["qos_algorithm_params"] = QVariantMap();

            XenAPI::VBD::create(sess, vbdRecord);
        }

        SetDescription(QString("Configuring networks"));
        vmRecord = XenAPI::VM::get_record(sess, newVmRef);
        QVariantList vifRefs = vmRecord.value("VIFs").toList();
        for (const QVariant& vifRefVar : vifRefs)
        {
            QString vifRef = vifRefVar.toString();
            if (!vifRef.isEmpty())
                XenAPI::VIF::destroy(sess, vifRef);
        }

        for (const VifConfig& vif : m_vifs)
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

            XenAPI::VIF::create(sess, vifRecord);
        }

        if (m_startAfter)
        {
            VM* vmInstance = new VM(GetConnection(), newVmRef);
            VMStartAction* startAction = new VMStartAction(QSharedPointer<VM>(vmInstance), nullptr, nullptr, this);
            startAction->RunAsync();
        }

        SetDescription(QString("VM created successfully"));
    }
    catch (const std::exception& e)
    {
        setError(QString("Failed to create VM: %1").arg(e.what()));
    }
}
