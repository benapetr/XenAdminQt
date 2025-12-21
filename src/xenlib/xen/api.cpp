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

#include "api.h"
#include "session.h"
#include "jsonrpcclient.h"
#include <QDebug>

class XenRpcAPI::Private
{
    public:
        XenSession* session = nullptr;
        int nextRequestId = 1; // Request ID counter for JSON-RPC
};

XenRpcAPI::XenRpcAPI(XenSession* session, QObject* parent) : QObject(parent), d(new Private)
{
    this->d->session = session;
}

XenRpcAPI::~XenRpcAPI()
{
    delete this->d;
}

QString XenRpcAPI::getSessionId() const
{
    if (this->d->session && this->d->session->isLoggedIn())
    {
        return this->d->session->getSessionId();
    }
    return QString();
}

QVariant XenRpcAPI::getVMRecord(const QString& vmRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getVMRecord", "Not authenticated");
        return QVariant();
    }

    // Make API call to get VM record
    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.get_record", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("getVMRecord", "Failed to communicate with server: " + this->d->session->getLastError());
        return QVariant();
    }

    // Parse the JSON-RPC response
    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("getVMRecord", "Failed to parse server response");
        return QVariant();
    }

    emit this->apiCallCompleted("getVMRecord", response);
    return response;
}

QString XenRpcAPI::cloneVM(const QString& vmRef, const QString& newName)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("cloneVM", "Not authenticated");
        return QString();
    }

    // Make API call to clone VM - returns new VM ref
    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);
    params.append(newName);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.clone", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("cloneVM", "Failed to communicate with server: " + this->d->session->getLastError());
        return QString();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("cloneVM", "Server returned an error");
        return QString();
    }

    emit this->apiCallCompleted("cloneVM", response);
    return response.toString(); // Return the new VM reference
}

bool XenRpcAPI::deleteVM(const QString& vmRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("deleteVM", "Not authenticated");
        return false;
    }

    // Make API call to destroy VM
    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.destroy", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("deleteVM", "Failed to communicate with server: " + this->d->session->getLastError());
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("deleteVM", "Server returned an error");
        return false;
    }

    emit this->apiCallCompleted("deleteVM", true);
    return true;
}

bool XenRpcAPI::exportVM(const QString& vmRef, const QString& fileName, const QString& format)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("exportVM", "Not authenticated");
        return false;
    }

    // Make API call to export VM
    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);
    params.append(fileName);
    params.append(format);

    QString apiMethod = (format == "ovf") ? "VM.export_ovf" : "VM.export";
    QByteArray jsonRpcRequest = this->buildJsonRpcCall(apiMethod, params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("exportVM", "Failed to communicate with server: " + this->d->session->getLastError());
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("exportVM", "Server returned an error");
        return false;
    }

    emit this->apiCallCompleted("exportVM", true);
    return true;
}

QString XenRpcAPI::getVMPowerState(const QString& vmRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getVMPowerState", "Not authenticated");
        return QString();
    }

    // Get VM record to access power state
    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.get_power_state", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("getVMPowerState", "Failed to communicate with server: " + this->d->session->getLastError());
        return QString();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("getVMPowerState", "Server returned an error");
        return QString();
    }

    emit this->apiCallCompleted("getVMPowerState", response);
    return response.toString();
}

bool XenRpcAPI::setVMField(const QString& vmRef, const QString& field, const QVariant& value)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("setVMField", "Not authenticated");
        return false;
    }

    if (vmRef.isEmpty() || field.isEmpty())
    {
        emit this->apiCallFailed("setVMField", "Invalid parameters");
        return false;
    }

    // Build method name based on field (e.g., VM.set_name_label, VM.set_VCPUs_max)
    QString method = QString("VM.set_%1").arg(field);

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);
    params.append(value);

    qDebug() << "XenAPI::setVMField: Calling" << method << "with value:" << value;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall(method, params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        QString error = "Failed to communicate with server: " + this->d->session->getLastError();
        emit this->apiCallFailed("setVMField", error);
        qWarning() << "XenAPI::setVMField:" << error;
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("setVMField", "Server returned an error");
        qWarning() << "XenAPI::setVMField: Server returned an error for field" << field;
        return false;
    }

    emit this->apiCallCompleted("setVMField", response);
    qDebug() << "XenAPI::setVMField: Successfully set" << field << "to" << value;
    return true;
}

bool XenRpcAPI::setVMOtherConfigKey(const QString& vmRef, const QString& key, const QString& value)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("setVMOtherConfigKey", "Not authenticated");
        return false;
    }

    if (vmRef.isEmpty() || key.isEmpty())
    {
        emit this->apiCallFailed("setVMOtherConfigKey", "Invalid parameters");
        return false;
    }

    // Call VM.add_to_other_config to set a key-value pair
    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);
    params.append(key);
    params.append(value);

    qDebug() << "XenAPI::setVMOtherConfigKey: Setting" << key << "=" << value << "for VM" << vmRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.add_to_other_config", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        QString error = "Failed to communicate with server: " + this->d->session->getLastError();
        emit this->apiCallFailed("setVMOtherConfigKey", error);
        qWarning() << "XenAPI::setVMOtherConfigKey:" << error;
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("setVMOtherConfigKey", "Server returned an error");
        qWarning() << "XenAPI::setVMOtherConfigKey: Server returned an error";
        return false;
    }

    emit this->apiCallCompleted("setVMOtherConfigKey", response);
    qDebug() << "XenAPI::setVMOtherConfigKey: Successfully set other_config key";
    return true;
}

// VM CPU and Memory Configuration Methods

bool XenRpcAPI::setVMVCPUsMax(const QString& vmRef, int vcpus)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("setVMVCPUsMax", "Not authenticated");
        return false;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);
    params.append(vcpus);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.set_VCPUs_max", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("setVMVCPUsMax", "Failed to communicate with server");
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("setVMVCPUsMax", "Server returned an error");
        return false;
    }

    emit this->apiCallCompleted("setVMVCPUsMax", response);
    return true;
}

bool XenRpcAPI::setVMVCPUsAtStartup(const QString& vmRef, int vcpus)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("setVMVCPUsAtStartup", "Not authenticated");
        return false;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);
    params.append(vcpus);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.set_VCPUs_at_startup", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("setVMVCPUsAtStartup", "Failed to communicate with server");
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("setVMVCPUsAtStartup", "Server returned an error");
        return false;
    }

    emit this->apiCallCompleted("setVMVCPUsAtStartup", response);
    return true;
}

bool XenRpcAPI::setVMMemoryLimits(const QString& vmRef, qint64 staticMin, qint64 staticMax, qint64 dynamicMin, qint64 dynamicMax)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("setVMMemoryLimits", "Not authenticated");
        return false;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);
    params.append(staticMin);
    params.append(staticMax);
    params.append(dynamicMin);
    params.append(dynamicMax);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.set_memory_limits", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("setVMMemoryLimits", "Failed to communicate with server");
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("setVMMemoryLimits", "Server returned an error");
        return false;
    }

    emit this->apiCallCompleted("setVMMemoryLimits", response);
    return true;
}

// VBD (Virtual Block Device) Operations

QVariant XenRpcAPI::getVBDRecord(const QString& vbdRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getVBDRecord", "Not authenticated");
        return QVariant();
    }

    if (vbdRef.isEmpty())
    {
        emit this->apiCallFailed("getVBDRecord", "Invalid VBD reference");
        return QVariant();
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vbdRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VBD.get_record", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        QString error = "Failed to communicate with server: " + this->d->session->getLastError();
        emit this->apiCallFailed("getVBDRecord", error);
        return QVariant();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("getVBDRecord", "Server returned an error");
        return QVariant();
    }

    emit this->apiCallCompleted("getVBDRecord", response);
    return response;
}

QVariantList XenRpcAPI::getVMVBDs(const QString& vmRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getVMVBDs", "Not authenticated");
        return QVariantList();
    }

    if (vmRef.isEmpty())
    {
        emit this->apiCallFailed("getVMVBDs", "Invalid VM reference");
        return QVariantList();
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.get_VBDs", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("getVMVBDs", "Failed to communicate with server: " + this->d->session->getLastError());
        return QVariantList();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("getVMVBDs", "Failed to parse server response");
        return QVariantList();
    }

    QVariantList vbdRefs = response.toList();
    QVariantList vbds;

    // Get detailed records for each VBD
    for (const QVariant& vbdRefVariant : vbdRefs)
    {
        QString vbdRef = vbdRefVariant.toString();
        if (!vbdRef.isEmpty())
        {
            QVariant vbdRecord = this->getVBDRecord(vbdRef);
            if (!vbdRecord.isNull())
            {
                QVariantMap vbdMap = vbdRecord.toMap();
                vbdMap["ref"] = vbdRef;
                vbds.append(vbdMap);
            }
        }
    }

    emit this->apiCallCompleted("getVMVBDs", vbds);
    return vbds;
}

// VIF (Virtual Network Interface) Operations

QVariantList XenRpcAPI::getVMVIFs(const QString& vmRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getVMVIFs", "Not authenticated");
        return QVariantList();
    }

    if (vmRef.isEmpty())
    {
        emit this->apiCallFailed("getVMVIFs", "Invalid VM reference");
        return QVariantList();
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.get_VIFs", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("getVMVIFs", "Failed to communicate with server: " + this->d->session->getLastError());
        return QVariantList();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("getVMVIFs", "Failed to parse server response");
        return QVariantList();
    }

    QVariantList vifRefs = response.toList();
    QVariantList vifs;

    // Get detailed records for each VIF
    for (const QVariant& vifRefVariant : vifRefs)
    {
        QString vifRef = vifRefVariant.toString();
        if (!vifRef.isEmpty())
        {
            QVariant vifRecord = this->getVIFRecord(vifRef);
            if (!vifRecord.isNull())
            {
                QVariantMap vifMap = vifRecord.toMap();
                vifMap["ref"] = vifRef;
                vifs.append(vifMap);
            }
        }
    }

    emit this->apiCallCompleted("getVMVIFs", vifs);
    return vifs;
}

QVariant XenRpcAPI::getVIFRecord(const QString& vifRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getVIFRecord", "Not authenticated");
        return QVariant();
    }

    if (vifRef.isEmpty())
    {
        emit this->apiCallFailed("getVIFRecord", "Invalid VIF reference");
        return QVariant();
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vifRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VIF.get_record", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        QString error = "Failed to communicate with server: " + this->d->session->getLastError();
        emit this->apiCallFailed("getVIFRecord", error);
        return QVariant();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("getVIFRecord", "Server returned an error");
        return QVariant();
    }

    emit this->apiCallCompleted("getVIFRecord", response);
    return response;
}

QString XenRpcAPI::createVIF(const QString& vmRef, const QString& networkRef, const QString& device, const QString& mac)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("createVIF", "Not authenticated");
        return QString();
    }

    if (vmRef.isEmpty() || networkRef.isEmpty())
    {
        emit this->apiCallFailed("createVIF", "Invalid VM or Network reference");
        return QString();
    }

    // Build VIF record
    QVariantMap vifRecord;
    vifRecord["VM"] = vmRef;
    vifRecord["network"] = networkRef;
    vifRecord["device"] = device;
    vifRecord["MAC"] = mac; // Empty string for auto-generated MAC
    vifRecord["MTU"] = QString::number(1500);
    vifRecord["other_config"] = QVariantMap();
    vifRecord["qos_algorithm_type"] = QString("");
    vifRecord["qos_algorithm_params"] = QVariantMap();
    vifRecord["locking_mode"] = "network_default";
    vifRecord["ipv4_allowed"] = QVariantList();
    vifRecord["ipv6_allowed"] = QVariantList();

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vifRecord);

    qDebug() << "XenAPI::createVIF: Creating VIF for VM" << vmRef << "on network" << networkRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VIF.create", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("createVIF", "Failed to communicate with server: " + this->d->session->getLastError());
        return QString();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("createVIF", "Server returned an error");
        return QString();
    }

    QString vifRef = response.toString();
    emit this->apiCallCompleted("createVIF", vifRef);
    qDebug() << "XenAPI::createVIF: Successfully created VIF:" << vifRef;
    return vifRef;
}

// VM migration operations
QString XenRpcAPI::poolMigrateVM(const QString& vmRef, const QString& hostRef, bool live)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("poolMigrateVM", "Not authenticated");
        return QString();
    }

    if (vmRef.isEmpty() || hostRef.isEmpty())
    {
        emit this->apiCallFailed("poolMigrateVM", "Invalid VM or host reference");
        return QString();
    }

    // Build options map with live migration flag
    QVariantMap options;
    options["live"] = live ? "true" : "false";

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);
    params.append(hostRef);
    params.append(options);

    qDebug() << "XenAPI::poolMigrateVM: Migrating VM" << vmRef << "to host" << hostRef << "(live:" << live << ")";

    // Use async version to return task reference
    QByteArray jsonRpcRequest = this->buildJsonRpcCall("Async.VM.pool_migrate", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("poolMigrateVM", "Failed to communicate with server: " + this->d->session->getLastError());
        return QString();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("poolMigrateVM", "Server returned an error");
        return QString();
    }

    QString taskRef = response.toString();
    emit this->apiCallCompleted("poolMigrateVM", taskRef);
    qDebug() << "XenAPI::poolMigrateVM: Migration task started:" << taskRef;
    return taskRef;
}

bool XenRpcAPI::assertCanMigrateVM(const QString& vmRef, const QString& hostRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("assertCanMigrateVM", "Not authenticated");
        return false;
    }

    if (vmRef.isEmpty() || hostRef.isEmpty())
    {
        emit this->apiCallFailed("assertCanMigrateVM", "Invalid VM or host reference");
        return false;
    }

    // Build options map with live migration flag
    QVariantMap options;
    options["live"] = "true";

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);
    params.append(hostRef);
    params.append(options);

    qDebug() << "XenAPI::assertCanMigrateVM: Checking if VM" << vmRef << "can migrate to host" << hostRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.assert_can_migrate", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("assertCanMigrateVM", "Failed to communicate with server: " + this->d->session->getLastError());
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);

    // VM.assert_can_migrate doesn't return a value on success, it throws an exception on failure
    // If we get here without an error, the migration is possible
    if (!response.isNull())
    {
        emit this->apiCallCompleted("assertCanMigrateVM", true);
        qDebug() << "XenAPI::assertCanMigrateVM: VM can be migrated";
        return true;
    }

    emit this->apiCallFailed("assertCanMigrateVM", "Migration not possible");
    return false;
}

// Snapshot operations
QVariantList XenRpcAPI::getVMSnapshots(const QString& vmRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getVMSnapshots", "Not authenticated");
        return QVariantList();
    }

    if (vmRef.isEmpty())
    {
        emit this->apiCallFailed("getVMSnapshots", "Invalid VM reference");
        return QVariantList();
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.get_snapshots", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        QString error = "Failed to communicate with server: " + this->d->session->getLastError();
        emit this->apiCallFailed("getVMSnapshots", error);
        qWarning() << "XenAPI::getVMSnapshots:" << error;
        return QVariantList();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull() || !response.canConvert<QVariantList>())
    {
        emit this->apiCallFailed("getVMSnapshots", "Invalid response from server");
        return QVariantList();
    }

    QVariantList snapshotRefs = response.toList();
    QVariantList snapshots;

    // Get full snapshot records
    for (const QVariant& snapshotRef : snapshotRefs)
    {
        QVariant snapshotRecord = getSnapshotRecord(snapshotRef.toString());
        if (!snapshotRecord.isNull())
        {
            snapshots.append(snapshotRecord);
        }
    }

    emit this->apiCallCompleted("getVMSnapshots", QVariant::fromValue(snapshots));
    return snapshots;
}

QString XenRpcAPI::createVMSnapshot(const QString& vmRef, const QString& name, const QString& description)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("createVMSnapshot", "Not authenticated");
        return QString();
    }

    if (vmRef.isEmpty() || name.isEmpty())
    {
        emit this->apiCallFailed("createVMSnapshot", "Invalid parameters");
        return QString();
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);
    params.append(name);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.snapshot", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        QString error = "Failed to communicate with server: " + this->d->session->getLastError();
        emit this->apiCallFailed("createVMSnapshot", error);
        qWarning() << "XenAPI::createVMSnapshot:" << error;
        return QString();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("createVMSnapshot", "Server returned an error");
        return QString();
    }

    QString snapshotRef = response.toString();

    // Set description if provided
    if (!description.isEmpty() && !snapshotRef.isEmpty())
    {
        QVariantList descParams;
        descParams.append(this->d->session->getSessionId());
        descParams.append(snapshotRef);
        descParams.append(description);

        QByteArray descRequest = this->buildJsonRpcCall("VM.set_name_description", descParams);
        this->d->session->sendApiRequest(QString::fromUtf8(descRequest));
    }

    emit this->apiCallCompleted("createVMSnapshot", snapshotRef);
    qDebug() << "XenAPI::createVMSnapshot: Created snapshot" << name << "with ref:" << snapshotRef;
    return snapshotRef;
}

QString XenRpcAPI::createVMSnapshotWithQuiesce(const QString& vmRef, const QString& name, const QString& description)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("createVMSnapshotWithQuiesce", "Not authenticated");
        return QString();
    }

    if (vmRef.isEmpty() || name.isEmpty())
    {
        emit this->apiCallFailed("createVMSnapshotWithQuiesce", "Invalid parameters");
        return QString();
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);
    params.append(name);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.snapshot_with_quiesce", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        QString error = "Failed to communicate with server: " + this->d->session->getLastError();
        emit this->apiCallFailed("createVMSnapshotWithQuiesce", error);
        qWarning() << "XenAPI::createVMSnapshotWithQuiesce:" << error;
        return QString();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("createVMSnapshotWithQuiesce", "Server returned an error");
        return QString();
    }

    QString snapshotRef = response.toString();

    // Set description if provided
    if (!description.isEmpty() && !snapshotRef.isEmpty())
    {
        QVariantList descParams;
        descParams.append(this->d->session->getSessionId());
        descParams.append(snapshotRef);
        descParams.append(description);

        QByteArray descRequest = this->buildJsonRpcCall("VM.set_name_description", descParams);
        this->d->session->sendApiRequest(QString::fromUtf8(descRequest));
    }

    emit this->apiCallCompleted("createVMSnapshotWithQuiesce", snapshotRef);
    qDebug() << "XenAPI::createVMSnapshotWithQuiesce: Created quiesced snapshot" << name << "with ref:" << snapshotRef;
    return snapshotRef;
}

QString XenRpcAPI::createVMCheckpoint(const QString& vmRef, const QString& name, const QString& description)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("createVMCheckpoint", "Not authenticated");
        return QString();
    }

    if (vmRef.isEmpty() || name.isEmpty())
    {
        emit this->apiCallFailed("createVMCheckpoint", "Invalid parameters");
        return QString();
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);
    params.append(name);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.checkpoint", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        QString error = "Failed to communicate with server: " + this->d->session->getLastError();
        emit this->apiCallFailed("createVMCheckpoint", error);
        qWarning() << "XenAPI::createVMCheckpoint:" << error;
        return QString();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("createVMCheckpoint", "Server returned an error");
        return QString();
    }

    QString snapshotRef = response.toString();

    // Set description if provided
    if (!description.isEmpty() && !snapshotRef.isEmpty())
    {
        QVariantList descParams;
        descParams.append(this->d->session->getSessionId());
        descParams.append(snapshotRef);
        descParams.append(description);

        QByteArray descRequest = this->buildJsonRpcCall("VM.set_name_description", descParams);
        this->d->session->sendApiRequest(QString::fromUtf8(descRequest));
    }

    emit this->apiCallCompleted("createVMCheckpoint", snapshotRef);
    qDebug() << "XenAPI::createVMCheckpoint: Created checkpoint" << name << "with ref:" << snapshotRef;
    return snapshotRef;
}

bool XenRpcAPI::deleteSnapshot(const QString& snapshotRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("deleteSnapshot", "Not authenticated");
        return false;
    }

    if (snapshotRef.isEmpty())
    {
        emit this->apiCallFailed("deleteSnapshot", "Invalid snapshot reference");
        return false;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(snapshotRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.destroy", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        QString error = "Failed to communicate with server: " + this->d->session->getLastError();
        emit this->apiCallFailed("deleteSnapshot", error);
        qWarning() << "XenAPI::deleteSnapshot:" << error;
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("deleteSnapshot", "Server returned an error");
        return false;
    }

    emit this->apiCallCompleted("deleteSnapshot", response);
    qDebug() << "XenAPI::deleteSnapshot: Deleted snapshot:" << snapshotRef;
    return true;
}

bool XenRpcAPI::revertToSnapshot(const QString& snapshotRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("revertToSnapshot", "Not authenticated");
        return false;
    }

    if (snapshotRef.isEmpty())
    {
        emit this->apiCallFailed("revertToSnapshot", "Invalid snapshot reference");
        return false;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(snapshotRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.revert", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        QString error = "Failed to communicate with server: " + this->d->session->getLastError();
        emit this->apiCallFailed("revertToSnapshot", error);
        qWarning() << "XenAPI::revertToSnapshot:" << error;
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("revertToSnapshot", "Server returned an error");
        return false;
    }

    emit this->apiCallCompleted("revertToSnapshot", response);
    qDebug() << "XenAPI::revertToSnapshot: Reverted to snapshot:" << snapshotRef;
    return true;
}

QVariant XenRpcAPI::getSnapshotRecord(const QString& snapshotRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getSnapshotRecord", "Not authenticated");
        return QVariant();
    }

    if (snapshotRef.isEmpty())
    {
        emit this->apiCallFailed("getSnapshotRecord", "Invalid snapshot reference");
        return QVariant();
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(snapshotRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.get_record", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        QString error = "Failed to communicate with server: " + this->d->session->getLastError();
        emit this->apiCallFailed("getSnapshotRecord", error);
        qWarning() << "XenAPI::getSnapshotRecord:" << error;
        return QVariant();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("getSnapshotRecord", "Server returned an error");
        return QVariant();
    }

    QVariantMap record = response.toMap();
    record["ref"] = snapshotRef;

    emit this->apiCallCompleted("getSnapshotRecord", record);
    return record;
}

QVariantList XenRpcAPI::getHosts()
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        qDebug() << "XenAPI::getHosts - Not authenticated";
        emit this->apiCallFailed("getHosts", "Not authenticated");
        return QVariantList();
    }

    // Make actual Xen API call to get all host references
    QVariantList params;
    params.append(this->d->session->getSessionId());

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("host.get_all", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        qDebug() << "XenAPI::getHosts - Failed to communicate with server";
        emit this->apiCallFailed("getHosts", "Failed to communicate with server: " + this->d->session->getLastError());
        return QVariantList();
    }

    // Parse the API response
    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        qDebug() << "XenAPI::getHosts - Failed to parse response:" << QString::fromUtf8(responseData);
        emit this->apiCallFailed("getHosts", "Failed to parse server response");
        return QVariantList();
    }

    QVariantList hostRefs = response.toList();
    qDebug() << "XenAPI::getHosts - Got" << hostRefs.size() << "host refs";

    QVariantList hosts;

    // Get records for each host
    for (const QVariant& hostRef : hostRefs)
    {
        QVariant hostRecord = this->getHostRecord(hostRef.toString());
        if (!hostRecord.isNull())
        {
            // Add the ref to the record so we can find it later
            QVariantMap hostMap = hostRecord.toMap();
            hostMap["ref"] = hostRef.toString();
            hosts.append(hostMap);
        }
    }

    qDebug() << "XenAPI::getHosts - Returning" << hosts.size() << "hosts";
    emit this->apiCallCompleted("getHosts", hosts);
    return hosts;
}

QVariant XenRpcAPI::getHostRecord(const QString& hostRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getHostRecord", "Not authenticated");
        return QVariant();
    }

    // Make API call to get host record
    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(hostRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("host.get_record", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("getHostRecord", "Failed to communicate with server: " + this->d->session->getLastError());
        return QVariant();
    }

    // Parse the API response
    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("getHostRecord", "Failed to parse server response");
        return QVariant();
    }

    emit this->apiCallCompleted("getHostRecord", response);
    return response;
}

QVariant XenRpcAPI::getHostServerTime(const QString& hostRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getHostServerTime", "Not authenticated");
        return QVariant();
    }

    QVariantList params;
    params << this->d->session->getSessionId() << hostRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("host.get_servertime", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        emit this->apiCallFailed("getHostServerTime", "Empty response");
        return QVariant();
    }

    QVariant result = this->parseJsonRpcResponse(response);

    if (result.isNull())
    {
        emit this->apiCallFailed("getHostServerTime", "Failed to parse response");
        return QVariant();
    }

    emit this->apiCallCompleted("getHostServerTime", result);
    return result;
}

QVariant XenRpcAPI::getHostInfo(const QString& hostRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getHostInfo", "Not authenticated");
        return QVariant();
    }

    // TODO: Implement actual host info retrieval
    QVariantMap hostInfo;
    hostInfo["ref"] = hostRef;
    hostInfo["name"] = "xenserver1.local";
    hostInfo["uuid"] = "550e8400-e29b-41d4-a716-446655440101";
    hostInfo["hostname"] = "xenserver1.local";
    hostInfo["address"] = "192.168.1.100";
    hostInfo["cpu_count"] = 8;
    hostInfo["cpu_info"] = QVariantMap{
        {"vendor", "Intel"},
        {"model", "Intel(R) Xeon(R) CPU E5-2620 v4 @ 2.10GHz"}};
    hostInfo["memory_total"] = static_cast<qlonglong>(34359738368LL); // 32GB in bytes
    hostInfo["memory_free"] = static_cast<qlonglong>(17179869184LL);  // 16GB free
    hostInfo["enabled"] = true;
    hostInfo["software_version"] = QVariantMap{
        {"product_brand", "XenServer"},
        {"product_version", "8.2.0"},
        {"build_number", "25441c"}};

    emit this->apiCallCompleted("getHostInfo", hostInfo);
    return hostInfo;
}

QVariantList XenRpcAPI::getPools()
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getPools", "Not authenticated");
        return QVariantList();
    }

    // Make actual Xen API call to get all pool references
    QVariantList params;
    params.append(this->d->session->getSessionId());

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("pool.get_all", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("getPools", "Failed to communicate with server: " + this->d->session->getLastError());
        return QVariantList();
    }

    // Parse the API response
    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("getPools", "Failed to parse server response");
        return QVariantList();
    }

    QVariantList poolRefs = response.toList();
    QVariantList pools;

    // Get records for each pool
    for (const QVariant& poolRef : poolRefs)
    {
        QVariant poolRecord = this->getPoolRecord(poolRef.toString());
        if (!poolRecord.isNull())
        {
            // Add the ref to the record so we can find it later
            QVariantMap poolMap = poolRecord.toMap();
            poolMap["ref"] = poolRef.toString();
            pools.append(poolMap);
        }
    }

    emit this->apiCallCompleted("getPools", pools);
    return pools;
}

QVariant XenRpcAPI::getPoolRecord(const QString& poolRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getPoolRecord", "Not authenticated");
        return QVariant();
    }

    // Make API call to get pool record
    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(poolRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("pool.get_record", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("getPoolRecord", "Failed to communicate with server: " + this->d->session->getLastError());
        return QVariant();
    }

    // Parse the API response
    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("getPoolRecord", "Failed to parse server response");
        return QVariant();
    }

    emit this->apiCallCompleted("getPoolRecord", response);
    return response;
}

bool XenRpcAPI::setPoolField(const QString& poolRef, const QString& field, const QVariant& value)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
        return false;

    QString methodName = QString("pool.set_%1").arg(field);

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(poolRef);
    params.append(value);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall(methodName, params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));
    if (response.isEmpty())
        return false;

    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

bool XenRpcAPI::setPoolMigrationCompression(const QString& poolRef, bool enabled)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
        return false;

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(poolRef);
    params.append(enabled);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("pool.set_migration_compression", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));
    if (response.isEmpty())
        return false;

    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

bool XenRpcAPI::joinPool(const QString& masterAddress, const QString& username, const QString& password)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("joinPool", "Not authenticated");
        return false;
    }

    if (masterAddress.isEmpty() || username.isEmpty() || password.isEmpty())
    {
        emit this->apiCallFailed("joinPool", "Invalid parameters");
        return false;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(masterAddress);
    params.append(username);
    params.append(password);

    qDebug() << "XenAPI::joinPool: Joining pool with master at" << masterAddress;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("pool.join", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("joinPool", "Failed to communicate with server: " + this->d->session->getLastError());
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("joinPool", "Server returned an error");
        return false;
    }

    emit this->apiCallCompleted("joinPool", response);
    qDebug() << "XenAPI::joinPool: Successfully joined pool";
    return true;
}

bool XenRpcAPI::ejectFromPool(const QString& hostRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("ejectFromPool", "Not authenticated");
        return false;
    }

    if (hostRef.isEmpty())
    {
        emit this->apiCallFailed("ejectFromPool", "Invalid host reference");
        return false;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(hostRef);

    qDebug() << "XenAPI::ejectFromPool: Ejecting host" << hostRef << "from pool";

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("pool.eject", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("ejectFromPool", "Failed to communicate with server: " + this->d->session->getLastError());
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("ejectFromPool", "Server returned an error");
        return false;
    }

    emit this->apiCallCompleted("ejectFromPool", response);
    qDebug() << "XenAPI::ejectFromPool: Successfully ejected host from pool";
    return true;
}

bool XenRpcAPI::setSRField(const QString& srRef, const QString& field, const QVariant& value)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
        return false;

    QString methodName = QString("SR.set_%1").arg(field);

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(srRef);
    params.append(value);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall(methodName, params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));
    if (response.isEmpty())
        return false;

    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

// PBD (Physical Block Device) operations
QVariantList XenRpcAPI::getPBDs()
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getPBDs", "Not authenticated");
        return QVariantList();
    }

    // Make API call to get all PBD references
    QVariantList params;
    params.append(this->d->session->getSessionId());

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("PBD.get_all", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("getPBDs", "Failed to communicate with server: " + this->d->session->getLastError());
        return QVariantList();
    }

    // Parse the response
    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("getPBDs", "Failed to parse server response");
        return QVariantList();
    }

    QVariantList pbdRefs = response.toList();
    QVariantList pbds;

    // Get records for each PBD
    for (const QVariant& pbdRef : pbdRefs)
    {
        QVariant pbdRecord = this->getPBDRecord(pbdRef.toString());
        if (!pbdRecord.isNull())
        {
            // Add the ref to the record
            QVariantMap pbdMap = pbdRecord.toMap();
            pbdMap["ref"] = pbdRef.toString();
            pbds.append(pbdMap);
        }
    }

    emit this->apiCallCompleted("getPBDs", pbds);
    return pbds;
}

QVariant XenRpcAPI::getPBDRecord(const QString& pbdRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getPBDRecord", "Not authenticated");
        return QVariant();
    }

    // Make API call to get PBD record
    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(pbdRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("PBD.get_record", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("getPBDRecord", "Failed to communicate with server: " + this->d->session->getLastError());
        return QVariant();
    }

    // Parse the response
    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("getPBDRecord", "Failed to parse server response");
        return QVariant();
    }

    emit this->apiCallCompleted("getPBDRecord", response);
    return response;
}

bool XenRpcAPI::setNetworkField(const QString& networkRef, const QString& field, const QVariant& value)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
        return false;

    QString methodName = QString("network.set_%1").arg(field);

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(networkRef);
    params.append(value);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall(methodName, params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));
    if (response.isEmpty())
        return false;

    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

bool XenRpcAPI::repairSR(const QString& srRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("repairSR", "Not authenticated");
        return false;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(srRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("SR.scan", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    QVariant result = this->parseJsonRpcResponse(response);

    if (result.isNull())
    {
        emit this->apiCallFailed("repairSR", "Failed to scan/repair SR");
        return false;
    }

    emit this->apiCallCompleted("repairSR", true);
    return true;
}

bool XenRpcAPI::setDefaultSR(const QString& srRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("setDefaultSR", "Not authenticated");
        return false;
    }

    // Get the pool reference first
    QVariantList getPoolParams;
    getPoolParams.append(this->d->session->getSessionId());

    QByteArray getPoolRequest = this->buildJsonRpcCall("pool.get_all", getPoolParams);
    QByteArray poolResponse = this->d->session->sendApiRequest(QString::fromUtf8(getPoolRequest));
    QVariant poolListResult = this->parseJsonRpcResponse(poolResponse);

    if (!poolListResult.canConvert<QVariantList>() || poolListResult.toList().isEmpty())
    {
        emit this->apiCallFailed("setDefaultSR", "No pool found");
        return false;
    }

    QString poolRef = poolListResult.toList().first().toString();

    // Set the default SR on the pool
    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(poolRef);
    params.append(srRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("pool.set_default_SR", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    QVariant result = this->parseJsonRpcResponse(response);

    if (result.isNull())
    {
        emit this->apiCallFailed("setDefaultSR", "Failed to set default SR");
        return false;
    }

    emit this->apiCallCompleted("setDefaultSR", true);
    return true;
}

void XenRpcAPI::handleEventPolling()
{
    // TODO: Implement actual event polling mechanism
    // This would poll the Xen server for events and emit appropriate signals
    emit this->eventReceived("session.pool_patch_upload_cancelled");
}

// JSON-RPC wrapper methods (delegate to JsonRpcClient)
QByteArray XenRpcAPI::buildJsonRpcCall(const QString& method, const QVariantList& params)
{
    int requestId = this->d->nextRequestId++;
    return Xen::JsonRpcClient::buildJsonRpcCall(method, params, requestId);
}

QVariant XenRpcAPI::parseJsonRpcResponse(const QByteArray& response)
{
    return Xen::JsonRpcClient::parseJsonRpcResponse(response);
}

void XenRpcAPI::makeAsyncCall(const QString& method, const QVariantList& params, const QString& callId)
{
    Q_UNUSED(callId);

    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed(method, "Not logged in");
        return;
    }

    // TODO: Implement async API calls
    emit this->apiCallCompleted(method, QVariant());
}

bool XenRpcAPI::rebootHost(const QString& hostRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
        return false;

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(hostRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("host.reboot", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));
    if (response.isEmpty())
        return false;

    // Parse response for success
    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

bool XenRpcAPI::shutdownHost(const QString& hostRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
        return false;

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(hostRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("host.shutdown", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));
    if (response.isEmpty())
        return false;

    // Parse response for success
    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

bool XenRpcAPI::disableHost(const QString& hostRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
        return false;

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(hostRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("host.disable", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));
    if (response.isEmpty())
        return false;

    // Parse response for success
    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

bool XenRpcAPI::enableHost(const QString& hostRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
        return false;

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(hostRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("host.enable", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));
    if (response.isEmpty())
        return false;

    // Parse response for success
    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

bool XenRpcAPI::setHostField(const QString& hostRef, const QString& field, const QVariant& value)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
        return false;

    QString methodName = QString("host.set_%1").arg(field);

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(hostRef);
    params.append(value);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall(methodName, params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));
    if (response.isEmpty())
        return false;

    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

bool XenRpcAPI::setHostOtherConfig(const QString& hostRef, const QString& key, const QString& value)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
        return false;

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(hostRef);
    params.append(key);
    params.append(value);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("host.add_to_other_config", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));
    if (response.isEmpty())
        return false;

    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

// ============================================================================
// Data Source Operations (Performance Monitoring)
// ============================================================================

double XenRpcAPI::queryVMDataSource(const QString& vmRef, const QString& dataSource)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("VM.query_data_source", "Not logged in");
        return 0.0;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << vmRef;
    params << dataSource;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.query_data_source", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        emit this->apiCallFailed("VM.query_data_source", "Empty response");
        return 0.0;
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return result.toDouble();
}

double XenRpcAPI::queryHostDataSource(const QString& hostRef, const QString& dataSource)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("host.query_data_source", "Not logged in");
        return 0.0;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << hostRef;
    params << dataSource;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("host.query_data_source", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        emit this->apiCallFailed("host.query_data_source", "Empty response");
        return 0.0;
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return result.toDouble();
}

QVariant XenRpcAPI::getTaskRecord(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("task.get_record", "Not logged in");
        return QVariant();
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.get_record", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        emit this->apiCallFailed("task.get_record", "Empty response");
        return QVariant();
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return result;
}

QString XenRpcAPI::getTaskStatus(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        return QString();
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.get_status", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        return QString();
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return result.toString();
}

double XenRpcAPI::getTaskProgress(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        return -1.0;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.get_progress", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        return -1.0;
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return result.toDouble();
}

QString XenRpcAPI::getTaskResult(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        return QString();
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.get_result", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        return QString();
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return result.toString();
}

QStringList XenRpcAPI::getTaskErrorInfo(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        return QStringList();
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.get_error_info", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        return QStringList();
    }

    QVariant result = this->parseJsonRpcResponse(response);
    QStringList errorList;

    if (result.canConvert<QVariantList>())
    {
        QVariantList list = result.toList();
        for (const QVariant& item : list)
        {
            errorList.append(item.toString());
        }
    }

    return errorList;
}

QVariantMap XenRpcAPI::getAllTaskRecords()
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("task.get_all_records", "Not logged in");
        return QVariantMap();
    }

    QVariantList params;
    params << this->d->session->getSessionId();

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.get_all_records", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        emit this->apiCallFailed("task.get_all_records", "Empty response");
        return QVariantMap();
    }

    QVariant result = this->parseJsonRpcResponse(response);

    if (result.canConvert<QVariantMap>())
    {
        return result.toMap();
    }

    return QVariantMap();
}

bool XenRpcAPI::cancelTask(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("task.cancel", "Not logged in");
        return false;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.cancel", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        emit this->apiCallFailed("task.cancel", "Empty response");
        return false;
    }

    // Check if response indicates success
    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

bool XenRpcAPI::destroyTask(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("task.destroy", "Not logged in");
        return false;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.destroy", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        emit this->apiCallFailed("task.destroy", "Empty response");
        return false;
    }

    // Check if response indicates success
    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

bool XenRpcAPI::addToTaskOtherConfig(const QString& taskRef, const QString& key, const QString& value)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        return false;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;
    params << key;
    params << value;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.add_to_other_config", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        return false;
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

bool XenRpcAPI::removeFromTaskOtherConfig(const QString& taskRef, const QString& key)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        return false;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;
    params << key;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.remove_from_other_config", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        return false;
    }

    QVariant result = this->parseJsonRpcResponse(response);
    return !result.isNull();
}

QVariantMap XenRpcAPI::getTaskOtherConfig(const QString& taskRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        return QVariantMap();
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << taskRef;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("task.get_other_config", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        return QVariantMap();
    }

    QVariant result = this->parseJsonRpcResponse(response);
    if (result.canConvert<QVariantMap>())
    {
        return result.toMap();
    }

    return QVariantMap();
}

// Event.from - Modern event API (API 1.9+)
// This is the main event method used by XenServer
// Returns: { "events": [...], "token": "...", "valid_ref_counts": {...} }
QVariantMap XenRpcAPI::eventFrom(const QStringList& classes, const QString& token, double timeout)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("event.from", "Not logged in");
        return QVariantMap();
    }

    // Build parameters: session_id, classes, token, timeout
    QVariantList params;
    params << this->d->session->getSessionId();
    params << classes;
    params << token;
    params << timeout;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("event.from", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (response.isEmpty())
    {
        emit this->apiCallFailed("event.from", "Empty response");
        return QVariantMap();
    }

    // Parse response - should be a struct with "events", "token", "valid_ref_counts"
    QVariant result = this->parseJsonRpcResponse(response);

    // Convert to map if it's not already
    if (result.type() == QVariant::Map)
    {
        return result.toMap();
    }

    emit this->apiCallFailed("event.from", "Invalid response format");
    return QVariantMap();
}

// Legacy event registration (pre-1.9 API, rarely used now)
bool XenRpcAPI::eventRegister(const QStringList& classes)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("event.register", "Not logged in");
        return false;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << classes;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("event.register", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    return !response.isEmpty();
}

// Legacy event unregistration (pre-1.9 API, rarely used now)
bool XenRpcAPI::eventUnregister(const QStringList& classes)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("event.unregister", "Not logged in");
        return false;
    }

    QVariantList params;
    params << this->d->session->getSessionId();
    params << classes;

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("event.unregister", params);
    QByteArray response = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    return !response.isEmpty();
}

// Network operations
QString XenRpcAPI::createNetwork(const QString& name, const QString& description, const QVariantMap& otherConfig)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("createNetwork", "Not authenticated");
        return QString();
    }

    // Build network record
    QVariantMap networkRecord;
    networkRecord["name_label"] = name;
    networkRecord["name_description"] = description;
    networkRecord["other_config"] = otherConfig;
    networkRecord["MTU"] = 1500;            // Default MTU
    networkRecord["tags"] = QVariantList(); // Empty tags

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(networkRecord);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("network.create", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("createNetwork", "Failed to communicate with server: " + this->d->session->getLastError());
        return QString();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("createNetwork", "Failed to parse server response");
        return QString();
    }

    // Response should be the network reference
    return response.toString();
}

bool XenRpcAPI::destroyNetwork(const QString& networkRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("destroyNetwork", "Not authenticated");
        return false;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(networkRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("network.destroy", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("destroyNetwork", "Failed to communicate with server: " + this->d->session->getLastError());
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    return !response.isNull();
}

bool XenRpcAPI::setNetworkOtherConfig(const QString& networkRef, const QVariantMap& otherConfig)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("setNetworkOtherConfig", "Not authenticated");
        return false;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(networkRef);
    params.append(otherConfig);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("network.set_other_config", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("setNetworkOtherConfig", "Failed to communicate with server: " + this->d->session->getLastError());
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    return !response.isNull();
}

bool XenRpcAPI::setNetworkMTU(const QString& networkRef, qint64 mtu)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("setNetworkMTU", "Not authenticated");
        return false;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(networkRef);
    params.append(mtu);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("network.set_MTU", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("setNetworkMTU", "Failed to communicate with server: " + this->d->session->getLastError());
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    return !response.isNull();
}

QString XenRpcAPI::createBond(const QString& networkRef, const QStringList& pifRefs, const QString& mac, const QString& mode)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("createBond", "Not authenticated");
        return QString();
    }

    if (pifRefs.size() < 2)
    {
        emit this->apiCallFailed("createBond", "At least 2 PIFs required for bond");
        return QString();
    }

    // Convert PIF refs to QVariantList
    QVariantList pifList;
    for (const QString& pifRef : pifRefs)
    {
        pifList.append(pifRef);
    }

    // Build properties map
    QVariantMap properties;
    if (!mode.isEmpty())
    {
        properties["mode"] = mode;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(networkRef);
    params.append(pifList);
    params.append(mac.isEmpty() ? QString("") : mac);
    params.append(properties);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("Bond.create", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("createBond", "Failed to communicate with server: " + this->d->session->getLastError());
        return QString();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("createBond", "Server returned an error");
        return QString();
    }

    QString bondRef = response.toString();
    emit this->apiCallCompleted("createBond", bondRef);
    return bondRef;
}

bool XenRpcAPI::destroyBond(const QString& bondRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("destroyBond", "Not authenticated");
        return false;
    }

    if (bondRef.isEmpty())
    {
        emit this->apiCallFailed("destroyBond", "Invalid bond reference");
        return false;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(bondRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("Bond.destroy", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("destroyBond", "Failed to communicate with server: " + this->d->session->getLastError());
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("destroyBond", "Server returned an error");
        return false;
    }

    emit this->apiCallCompleted("destroyBond", true);
    return true;
}

bool XenRpcAPI::reconfigurePIF(const QString& pifRef, const QString& mode, const QString& ip,
                               const QString& netmask, const QString& gateway, const QString& dns)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("reconfigurePIF", "Not authenticated");
        return false;
    }

    if (pifRef.isEmpty())
    {
        emit this->apiCallFailed("reconfigurePIF", "Invalid PIF reference");
        return false;
    }

    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(pifRef);
    params.append(mode); // "Static" or "DHCP"
    params.append(ip);
    params.append(netmask);
    params.append(gateway);
    params.append(dns);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("PIF.reconfigure_ip", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("reconfigurePIF", "Failed to communicate with server: " + this->d->session->getLastError());
        return false;
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("reconfigurePIF", "Server returned an error");
        return false;
    }

    emit this->apiCallCompleted("reconfigurePIF", true);
    return true;
}

bool XenRpcAPI::reconfigurePIFDHCP(const QString& pifRef)
{
    return reconfigurePIF(pifRef, "DHCP", "", "", "", "");
}

QVariantList XenRpcAPI::getVMConsoles(const QString& vmRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getVMConsoles", "Not authenticated");
        return QVariantList();
    }

    // Make API call to get VM consoles
    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(vmRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("VM.get_consoles", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("getVMConsoles", "Failed to communicate with server: " + this->d->session->getLastError());
        return QVariantList();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    if (response.isNull())
    {
        emit this->apiCallFailed("getVMConsoles", "Failed to parse server response");
        return QVariantList();
    }

    return response.toList();
}

QVariant XenRpcAPI::getConsoleRecord(const QString& consoleRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getConsoleRecord", "Not authenticated");
        return QVariant();
    }

    // Make API call to get console record
    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(consoleRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("console.get_record", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("getConsoleRecord", "Failed to communicate with server: " + this->d->session->getLastError());
        return QVariant();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    return response;
}

QString XenRpcAPI::getConsoleProtocol(const QString& consoleRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getConsoleProtocol", "Not authenticated");
        return QString();
    }

    // Make API call to get console protocol
    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(consoleRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("console.get_protocol", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("getConsoleProtocol", "Failed to communicate with server: " + this->d->session->getLastError());
        return QString();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    return response.toString();
}

QString XenRpcAPI::getConsoleLocation(const QString& consoleRef)
{
    if (!this->d->session || !this->d->session->isLoggedIn())
    {
        emit this->apiCallFailed("getConsoleLocation", "Not authenticated");
        return QString();
    }

    // Make API call to get console location
    QVariantList params;
    params.append(this->d->session->getSessionId());
    params.append(consoleRef);

    QByteArray jsonRpcRequest = this->buildJsonRpcCall("console.get_location", params);
    QByteArray responseData = this->d->session->sendApiRequest(QString::fromUtf8(jsonRpcRequest));

    if (responseData.isEmpty())
    {
        emit this->apiCallFailed("getConsoleLocation", "Failed to communicate with server: " + this->d->session->getLastError());
        return QString();
    }

    QVariant response = this->parseJsonRpcResponse(responseData);
    return response.toString();
}
