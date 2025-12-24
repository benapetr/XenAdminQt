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

#ifndef HOST_H
#define HOST_H

#include "xenobject.h"

class Pool;
class SR;
class VM;

/**
 * @brief Host - A physical host
 *
 * Qt equivalent of C# XenAPI.Host class. Represents a physical XenServer host.
 *
 * Key properties (from C# Host class):
 * - name_label, name_description, hostname, address
 * - enabled (maintenance mode status)
 * - resident_VMs (VMs running on this host)
 * - PIFs (physical network interfaces)
 * - PBDs (physical block devices / storage connections)
 * - software_version, capabilities
 * - memory_overhead, cpu_info
 */
class XENLIB_EXPORT Host : public XenObject
{
    Q_OBJECT
    Q_PROPERTY(QString hostname READ hostname NOTIFY dataChanged)
    Q_PROPERTY(QString address READ address NOTIFY dataChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY dataChanged)

    public:
        explicit Host(XenConnection* connection,
                      const QString& opaqueRef,
                      QObject* parent = nullptr);
        ~Host() override = default;

        /**
         * @brief Get hostname
         * @return Hostname string
         */
        QString hostname() const;

        /**
         * @brief Get IP address
         * @return IP address string
         */
        QString address() const;

        /**
         * @brief Check if host is enabled (not in maintenance mode)
         * @return true if enabled
         */
        bool enabled() const;

        /**
         * @brief Get list of VMs resident on this host
         * @return List of VM opaque references
         */
        QStringList residentVMRefs() const;

        /**
         * @brief Get software version info
         * @return Map of version keys/values
         */
        QVariantMap softwareVersion() const;

        /**
         * @brief Get host capabilities
         * @return List of capability strings
         */
        QStringList capabilities() const;

        /**
         * @brief Get CPU info
         * @return Map of CPU information
         */
        QVariantMap cpuInfo() const;

        /**
         * @brief Get number of CPU sockets
         * @return Socket count or 0 if unknown
         */
        int cpuSockets() const;

        /**
         * @brief Get total CPU count
         * @return CPU count or 0 if unknown
         */
        int cpuCount() const;

        /**
         * @brief Get cores per socket
         * @return Cores per socket or 0 if unknown
         */
        int coresPerSocket() const;

        /**
         * @brief Get physical CPU count from host_CPUs list
         * @return Count of host CPUs
         */
        int hostCpuCount() const;

        /**
         * @brief Get other_config dictionary
         * @return Map of additional configuration
         */
        QVariantMap otherConfig() const;

        /**
         * @brief Get tags
         * @return List of tag strings
         */
        QStringList tags() const;

        /**
         * @brief Get suspend image SR reference
         * @return SR opaque reference
         */
        QString suspendImageSRRef() const;

        /**
         * @brief Get crash dump SR reference
         * @return SR opaque reference
         */
        QString crashDumpSRRef() const;

        /**
         * @brief Get list of PBD (storage connection) references
         * @return List of PBD opaque references
         */
        QStringList pbdRefs() const;

        /**
         * @brief Get list of PIF (network interface) references
         * @return List of PIF opaque references
         */
        QStringList pifRefs() const;

        /**
         * @brief Check if host is pool master
         * @return true if this host is the pool master
         */
        bool isMaster() const;

        /**
         * @brief Get pool reference this host belongs to
         *
         * Note: In XenAPI, hosts don't directly store pool ref,
         * we need to query the pool that references this host as master/member
         *
         * @return Pool opaque reference
         */
        QString poolRef() const;

        qint64 memoryOverhead() const;
        qint64 apiVersionMajor() const;
        qint64 apiVersionMinor() const;
        QString apiVersionVendor() const;
        QVariantMap apiVersionVendorImplementation() const;
        QVariantMap cpuConfiguration() const;
        QString schedPolicy() const;
        QStringList hostCPURefs() const;
        QStringList allowedOperations() const;
        QVariantMap currentOperations() const;
        QStringList supportedBootloaders() const;
        QVariantMap logging() const;
        QString metricsRef() const;
        QStringList haStatefiles() const;
        QStringList haNetworkPeers() const;
        QVariantMap biosStrings() const;
        QVariantMap chipsetInfo() const;
        QString externalAuthType() const;
        QString externalAuthServiceName() const;
        QVariantMap externalAuthConfiguration() const;
        QString powerOnMode() const;
        QVariantMap powerOnConfig() const;
        QString localCacheSRRef() const;
        QStringList pciRefs() const;
        QStringList pgpuRefs() const;
        QStringList pusbRefs() const;
        QStringList patchRefs() const;
        QStringList updateRefs() const;
        QStringList updatesRequiringRebootRefs() const;
        QStringList featureRefs() const;
        QStringList pendingGuidances() const;
        bool sslLegacy() const;
        bool tlsVerificationEnabled() const;
        bool httpsOnly() const;
        QVariantMap guestVCPUsParams() const;
        QString display() const;
        QList<qint64> virtualHardwarePlatformVersions() const;
        QString controlDomainRef() const;
        QString iscsiIqn() const;
        bool multipathing() const;
        QString uefiCertificates() const;
        QStringList certificateRefs() const;
        QStringList editions() const;
        QStringList crashdumpRefs() const;
        QDateTime lastSoftwareUpdate() const;
        QString latestSyncedUpdatesApplied() const;
        QVariantMap licenseParams() const;
        QString edition() const;
        QVariantMap licenseServer() const;

    protected:
        QString objectType() const override;
};

#endif // HOST_H
