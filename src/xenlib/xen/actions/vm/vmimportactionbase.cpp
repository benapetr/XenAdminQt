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

#include "vmimportactionbase.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_VDI.h"
#include "../../xenapi/xenapi_VBD.h"
#include "../../xenapi/xenapi_VIF.h"
#include "../../xenapi/xenapi_Task.h"
#include "../../xenapi/xenapi_SR.h"
#include "../../session.h"
#include "../../network/connection.h"
#include "../../network/httpclient.h"
#include <QFileInfo>
#include <QDebug>

// ─── Constructor ─────────────────────────────────────────────────────────────

VmImportActionBase::VmImportActionBase(XenConnection* connection,
                                       const QString& title,
                                       const QString& description,
                                       QObject* parent)
    : AsyncOperation(connection, title, description, parent)
{
}

// ─── checkCancelled ──────────────────────────────────────────────────────────

void VmImportActionBase::checkCancelled() const
{
    if (this->IsCancelled())
        throw CancelledException();
}

// ─── createVm ────────────────────────────────────────────────────────────────

QString VmImportActionBase::createVm(const QVariantMap& vmRecord, const QString& /*applianceRef*/)
{
    XenAPI::Session* session = this->GetSession();
    const QString vmRef = XenAPI::VM::create(session, vmRecord);
    if (vmRef.isEmpty())
        throw std::runtime_error("VM.create returned an empty reference");
    qDebug() << "VmImportActionBase: created VM" << vmRef;
    return vmRef;
}

// ─── uploadDisk ──────────────────────────────────────────────────────────────

QString VmImportActionBase::uploadDisk(const QString& srRef,
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
    vdiRecord["name_description"] = QString("Imported from %1")
                                        .arg(QFileInfo(this->m_sourcePath).fileName());
    vdiRecord["SR"]               = srRef;
    vdiRecord["virtual_size"]     = virtualSizeBytes;
    vdiRecord["type"]             = QString("user");
    vdiRecord["sharable"]         = false;
    vdiRecord["read_only"]        = false;
    vdiRecord["other_config"]     = QVariantMap();

    const QString vdiRef = XenAPI::VDI::create(session, vdiRecord);
    if (vdiRef.isEmpty())
        throw std::runtime_error("VDI.create returned an empty reference");
    qDebug() << "VmImportActionBase: created VDI" << vdiRef;

    // Determine target host address
    QString hostAddr;
    if (this->GetConnection())
        hostAddr = this->GetConnection()->GetHostname();

    // Create a task to track the upload
    const QString taskRef = XenAPI::Task::Create(session,
                                                  "import_raw_vdi_task",
                                                  hostAddr);
    this->SetRelatedTaskRef(taskRef);

    // Build HTTP PUT query parameters matching /import_raw_vdi endpoint
    QMap<QString, QString> queryParams;
    queryParams["session_id"] = session->GetSessionID();
    queryParams["task_id"]    = taskRef;
    queryParams["vdi"]        = vdiRef;
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
    this->SetRelatedTaskRef(QString()); // clear after upload completes

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

void VmImportActionBase::attachDisk(const QString& vmRef,
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
    vbdRecord["VM"]           = vmRef;
    vbdRecord["VDI"]          = vdiRef;
    vbdRecord["userdevice"]   = userDevice;
    vbdRecord["bootable"]     = bootable;
    vbdRecord["mode"]         = mode;
    vbdRecord["type"]         = type;
    vbdRecord["empty"]        = false;
    vbdRecord["other_config"] = QVariantMap({ {"owner", "true"} });

    const QString vbdRef = XenAPI::VBD::create(session, vbdRecord);
    qDebug() << "VmImportActionBase: created VBD" << vbdRef;
}

// ─── createVif ───────────────────────────────────────────────────────────────

void VmImportActionBase::createVif(const QString& vmRef,
                                   const QString& networkRef,
                                   int deviceIndex,
                                   const QString& mac)
{
    XenAPI::Session* session = this->GetSession();

    QVariantMap vifRecord;
    vifRecord["device"]       = QString::number(deviceIndex);
    vifRecord["network"]      = networkRef;
    vifRecord["VM"]           = vmRef;
    vifRecord["MAC"]          = mac;
    vifRecord["MTU"]          = 1500;
    vifRecord["other_config"] = QVariantMap();
    vifRecord["locking_mode"] = QString("network_default");

    const QString vifRef = XenAPI::VIF::create(session, vifRecord);
    qDebug() << "VmImportActionBase: created VIF" << vifRef;
}

// ─── cleanupVm ───────────────────────────────────────────────────────────────

void VmImportActionBase::cleanupVm(const QString& vmRef)
{
    if (vmRef.isEmpty())
        return;

    XenAPI::Session* session = this->GetSession();
    if (!session)
        return;

    qDebug() << "VmImportActionBase: cleaning up partial VM" << vmRef;
    try
    {
        XenAPI::VM::destroy(session, vmRef);
    }
    catch (const std::exception& e)
    {
        qWarning() << "VmImportActionBase: cleanup destroy failed:" << e.what();
    }
}

// ─── applyFixups ─────────────────────────────────────────────────────────────

bool VmImportActionBase::applyFixups(const QString& vmRef, const QString& fixupIsoSrRef)
{
    if (fixupIsoSrRef.isEmpty())
    {
        qWarning() << "VmImportActionBase::applyFixups: no fixup ISO SR specified";
        return false;
    }

    XenAPI::Session* session = this->GetSession();

    // ── Step 1: Find the fixup ISO VDI on the selected SR ─────────────────
    QVariantList vdiRefs;
    try
    {
        QVariantMap srRecord = XenAPI::SR::get_record(session, fixupIsoSrRef);
        QVariant vdiField = srRecord.value("VDIs");
        if (vdiField.canConvert<QVariantList>())
            vdiRefs = vdiField.toList();
    }
    catch (const std::exception& e)
    {
        qWarning() << "VmImportActionBase::applyFixups: failed to read SR record:" << e.what();
        return false;
    }

    QString fixupVdiRef;
    for (const QVariant& v : vdiRefs)
    {
        const QString ref = v.toString();
        if (ref.isEmpty())
            continue;
        try
        {
            const QString name = XenAPI::VDI::get_name_label(session, ref);
            if (name.contains("linuxfixup", Qt::CaseInsensitive) ||
                name.contains("xenserver-linuxfixup", Qt::CaseInsensitive))
            {
                fixupVdiRef = ref;
                break;
            }
        }
        catch (const std::exception&)
        {
            // VDI may have been deleted between list and get — skip
        }
    }

    if (fixupVdiRef.isEmpty())
    {
        qWarning() << "VmImportActionBase::applyFixups: fixup ISO not found on SR"
                   << fixupIsoSrRef
                   << "— ensure 'xenserver-linuxfixup-disk.iso' is uploaded to that SR";
        return false;
    }

    // ── Step 2: Attach it as a read-only CDROM ────────────────────────────
    QVariant allowedDevices = XenAPI::VM::get_allowed_VBD_devices(session, vmRef);
    QString cdDevice = "3";
    if (allowedDevices.canConvert<QVariantList>())
    {
        const QVariantList devices = allowedDevices.toList();
        if (!devices.isEmpty())
            cdDevice = devices.first().toString();
    }

    try
    {
        QVariantMap vbdRecord;
        vbdRecord["VM"]           = vmRef;
        vbdRecord["VDI"]          = fixupVdiRef;
        vbdRecord["userdevice"]   = cdDevice;
        vbdRecord["bootable"]     = false;
        vbdRecord["mode"]         = QString("RO");
        vbdRecord["type"]         = QString("CD");
        vbdRecord["empty"]        = false;
        vbdRecord["other_config"] = QVariantMap();

        XenAPI::VBD::create(session, vbdRecord);
        qDebug() << "VmImportActionBase::applyFixups: attached fixup ISO VDI" << fixupVdiRef;
    }
    catch (const std::exception& e)
    {
        qWarning() << "VmImportActionBase::applyFixups: VBD create failed:" << e.what();
        return false;
    }

    // ── Step 3: Prepend "d" to the HVM boot order ────────────────────────
    try
    {
        QVariantMap bootParams = XenAPI::VM::get_HVM_boot_params(session, vmRef);
        const QString oldOrder = bootParams.value("order", "dc").toString();
        const QString newOrder = oldOrder.startsWith('d') ? oldOrder : ("d" + oldOrder);
        bootParams["order"] = newOrder;
        XenAPI::VM::set_HVM_boot_params(session, vmRef, bootParams);
        qDebug() << "VmImportActionBase::applyFixups: boot order set to" << newOrder;
    }
    catch (const std::exception& e)
    {
        qWarning() << "VmImportActionBase::applyFixups: failed to update boot order:" << e.what();
    }

    return true;
}
