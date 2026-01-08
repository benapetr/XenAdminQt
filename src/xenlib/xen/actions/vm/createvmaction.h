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

#ifndef CREATEVMACTION_H
#define CREATEVMACTION_H

#include "../../asyncoperation.h"
#include <QString>
#include <QList>

class XenConnection;
namespace XenAPI { class Session; }

class CreateVMAction : public AsyncOperation
{
    Q_OBJECT

    public:
        enum class InstallMethod
        {
            None,
            CD,
            Network
        };

        enum class BootMode
        {
            Auto,
            Bios,
            Uefi,
            SecureUefi
        };

        // TODO consolidate these types, they are defined again similarly in UI code, like new VM wizard

        struct DiskConfig
        {
            QString vdiRef;
            QString srRef;
            qint64 sizeBytes = 0;
            QString device;
            bool bootable = false;
            QString nameLabel;
            QString nameDescription;
            QString mode = "RW";
            QString vdiType = "user";
            bool sharable = false;
            bool readOnly = false;
        };

        struct VifConfig
        {
            QString networkRef;
            QString device;
            QString mac;
        };

        CreateVMAction(XenConnection* connection,
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
                       QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        void addDisks(XenAPI::Session* session, const QString& vmRef);
        QString getDiskVbd(XenAPI::Session* session, const DiskConfig& disk, const QVariantList& vbds);
        bool diskOk(XenAPI::Session* session, const DiskConfig& disk, const QString& vbdRef);
        QString moveDisk(XenAPI::Session* session, const QString& vmRef, const DiskConfig& disk, const QString& vbdRef, double progress, double step);
        QString createDisk(XenAPI::Session* session, const QString& vmRef, const DiskConfig& disk, double progress, double step);
        void addVmHint(XenAPI::Session* session, const QString& vmRef, const QString& vdiRef);
        QString createVdi(XenAPI::Session* session, const DiskConfig& disk, double progress1, double progress2);
        void createVbd(XenAPI::Session* session, const QString& vmRef, const DiskConfig& disk, const QString& vdiRef, double progress1, double progress2, bool bootable);
        bool isDeviceAtPositionZero(const DiskConfig& disk) const;
        bool srIsHbaLunPerVdi(XenAPI::Session* session, const QString& srRef);

        QString m_templateRef;
        QString m_nameLabel;
        QString m_nameDescription;
        InstallMethod m_installMethod;
        QString m_pvArgs;
        QString m_cdVdiRef;
        QString m_installUrl;
        BootMode m_bootMode;
        QString m_homeServerRef;
        qint64 m_vcpusMax;
        qint64 m_vcpusAtStartup;
        qint64 m_memoryDynamicMinMb;
        qint64 m_memoryDynamicMaxMb;
        qint64 m_memoryStaticMaxMb;
        qint64 m_coresPerSocket;
        QList<DiskConfig> m_disks;
        QList<VifConfig> m_vifs;
        bool m_startAfter;
        bool m_assignVtpm;
};

#endif // CREATEVMACTION_H
