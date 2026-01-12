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

#include "vmsnapshotcreateaction.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_Blob.h"
#include "../../vm.h"
#include <QtCore/QDebug>
#include <QtCore/QBuffer>

const QString VMSnapshotCreateAction::VNC_SNAPSHOT_NAME = QStringLiteral("XenCenter.VNCSnapshot");

VMSnapshotCreateAction::VMSnapshotCreateAction(QSharedPointer<VM> vm,
                                               const QString& newName,
                                               const QString& newDescription,
                                               SnapshotType type,
                                               const QImage& screenshot,
                                               QObject* parent)
    : AsyncOperation(vm ? vm->GetConnection() : nullptr,
                     QString("Snapshot '%1'").arg(newName),
                     "Creating snapshot...",
                     parent),
      m_vm(vm), m_newName(newName), m_newDescription(newDescription), m_type(type), m_screenshot(screenshot)
{
    if (!m_vm || !m_vm->IsValid())
        qWarning() << "VMSnapshotCreateAction: Invalid VM object";
}

void VMSnapshotCreateAction::run()
{
    if (!m_vm || !m_vm->IsValid())
    {
        setError("Invalid VM object");
        return;
    }

    qDebug() << "VMSnapshotCreateAction::run() starting - VM:" << m_vm->OpaqueRef() << "Name:" << m_newName << "Type:" << m_type;

    try
    {
        SetDescription("Creating snapshot...");
        SetPercentComplete(0);

        if (!GetSession())
        {
            setError("No session available");
            qWarning() << "VMSnapshotCreateAction: No session available";
            return;
        }

        QString taskRef;

        // Execute the appropriate snapshot operation based on type
        switch (m_type)
        {
        case QUIESCED_DISK:
            qDebug() << "Creating quiesced disk snapshot:" << m_newName;
            taskRef = XenAPI::VM::async_snapshot_with_quiesce(GetSession(), m_vm->OpaqueRef(), m_newName);
            break;

        case DISK_AND_MEMORY:
            qDebug() << "Creating disk and memory checkpoint:" << m_newName;
            taskRef = XenAPI::VM::async_checkpoint(GetSession(), m_vm->OpaqueRef(), m_newName);
            break;

        case DISK:
        default:
            qDebug() << "Creating disk snapshot:" << m_newName;
            taskRef = XenAPI::VM::async_snapshot(GetSession(), m_vm->OpaqueRef(), m_newName);
            break;
        }

        qDebug() << "VMSnapshotCreateAction: Got task ref:" << taskRef;

        if (taskRef.isEmpty())
        {
            setError("Failed to get task reference from API call");
            qWarning() << "VMSnapshotCreateAction: Empty task ref returned";
            return;
        }

        // Poll task to completion (0-90%)
        qDebug() << "VMSnapshotCreateAction: Starting task polling...";
        pollToCompletion(taskRef, 0, 90);

        // Get the snapshot reference from task result
        m_snapshotRef = GetResult();

        if (m_snapshotRef.isEmpty())
        {
            setError("Failed to get snapshot reference from task result");
            qWarning() << "VMSnapshotCreateAction: Empty snapshot ref from task result";
            return;
        }

        qDebug() << "Snapshot created:" << m_snapshotRef;

        SetPercentComplete(90);

        // Set the description on the snapshot
        if (!m_newDescription.isEmpty())
        {
            SetDescription("Setting snapshot description...");
            XenAPI::VM::set_name_description(GetSession(), m_snapshotRef, m_newDescription);
        }

        SetPercentComplete(95);

        // C#: SaveImageInBlob(Result, screenshot); (line 130)
        // Save screenshot as JPEG blob if available
        if (!m_screenshot.isNull())
        {
            SetDescription("Saving console screenshot...");
            saveImageInBlob(m_snapshotRef, m_screenshot);
        }

        SetPercentComplete(100);
        SetDescription(QString("Snapshot '%1' created successfully").arg(m_newName));
        qDebug() << "VMSnapshotCreateAction::run() completed successfully";

    } catch (const std::exception& e)
    {
        QString error = QString("Failed to create snapshot: %1").arg(e.what());
        qCritical() << "VMSnapshotCreateAction exception:" << error;
        setError(error);
    }
}

void VMSnapshotCreateAction::saveImageInBlob(const QString& snapshotRef, const QImage& image)
{
    // C#: VMSnapshotCreateAction.cs lines 132-148
    // Saves console screenshot as JPEG blob attached to snapshot VM

    if (image.isNull())
    {
        qDebug() << "VMSnapshotCreateAction: No screenshot to save";
        return;
    }

    try
    {
        // Convert QImage to JPEG in memory
        QByteArray jpegData;
        QBuffer buffer(&jpegData);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "JPEG", 85))
        { // 85% quality
            qWarning() << "VMSnapshotCreateAction: Failed to convert image to JPEG";
            return;
        }

        qDebug() << "VMSnapshotCreateAction: Converted screenshot to JPEG, size:" << jpegData.size();

        // C#:
        // XenRef<Blob> blobRef = VM.create_new_blob(Session, newVmRef, VNC_SNAPSHOT, "image/jpeg", false);
        // Blob blob = Connection.WaitForCache(blobRef);
        // blob.Save(saveStream, Session);
        QString blobRef = XenAPI::VM::create_new_blob(GetSession(), snapshotRef, VNC_SNAPSHOT_NAME, "image/jpeg", false);
        if (blobRef.isEmpty())
        {
            qWarning() << "VMSnapshotCreateAction: Failed to create blob for screenshot";
            return;
        }

        XenAPI::Blob::save(GetSession(), blobRef, jpegData);

    } catch (const std::exception& e)
    {
        // Don't fail the entire snapshot if screenshot save fails
        qWarning() << "VMSnapshotCreateAction: Failed to save screenshot blob:" << e.what();
    }
}
