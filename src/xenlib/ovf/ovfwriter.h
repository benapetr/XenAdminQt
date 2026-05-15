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

#ifndef OVFWRITER_H
#define OVFWRITER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QPair>

class QXmlStreamWriter;
class QFile;

/**
 * @brief Data for one VHD disk file referenced in an OVF package.
 *
 * Each disk corresponds to one VDI exported as a VHD file.
 * Mirrors the information C# ExportApplianceAction passes to OVF.AddDisk().
 */
struct OvfDiskEntry
{
    QString fileId;           ///< References/@File/@ovf:id (e.g. "file1")
    QString diskId;           ///< DiskSection/@Disk/@ovf:diskId (e.g. "disk1")
    QString href;             ///< Relative file name (e.g. "vdi-uuid.vhd")
    qint64 physicalBytes = 0; ///< vdi.physical_utilisation — populatedSize
    qint64 virtualBytes  = 0; ///< vdi.virtual_size — capacity
    bool   bootable      = false;
    QString addressOnParent;  ///< vbd.userdevice ("0","1",…)
    QString name;
    QString description;
    bool   isCdrom       = false;
};

/**
 * @brief Data for one virtual NIC referenced in an OVF package.
 */
struct OvfNetworkEntry
{
    QString name;             ///< Network name shown in OVF NetworkSection
    QString description;
    QString mac;              ///< Optional MAC address stored in RASD
};

/**
 * @brief Data model for one VirtualSystem (one exported VM) inside an OVF envelope.
 *
 * Mirrors C# Export.cs EnvelopeType construction per-VM.
 */
struct OvfVirtualSystemEntry
{
    QString id;               ///< ovf:id for VirtualSystem element (VM UUID)
    QString name;             ///< vm.name_label
    QString description;      ///< vm.name_description
    QString vmType;           ///< VirtualSystemType (e.g. "xen-3.0-x86_64")

    // CPU
    int vcpuAtStartup = 1;
    int vcpuMax       = 1;

    // Memory in MB
    qint64 memoryMB   = 256;

    // OS
    int     osId      = 1;   ///< CIM OS type (1 = Other)
    QString osName;
    QString osVersion;

    // Devices
    QList<OvfDiskEntry>    disks;
    QList<OvfNetworkEntry> networks;

    // Xen-specific key/value settings (HVM_boot_params, platform, PV_*, etc.)
    // Each pair is (key, value) written as <xen:Data> entries.
    QList<QPair<QString, QString>> xenConfig;

    // Startup ordering (matches C# OVF.AddStartupSection)
    int startOrder    = 0;
    int startDelay    = 0;
    int shutdownDelay = 0;
};

/**
 * @brief Top-level data model for an OVF appliance (one or more VMs).
 *
 * Passed to OvfWriter::generateXml() / saveToFile() / createManifest() / createOva().
 */
struct OvfEnvelopeData
{
    QString                       name;     ///< Appliance name (package file base name)
    QList<OvfVirtualSystemEntry>  systems;
    QStringList                   eulaTexts; ///< Pre-read EULA text content (not file paths)
};

/**
 * @brief OVF XML writer and package helper.
 *
 * Generates standard OVF 1.1 XML compatible with XenServer/XCP-ng import.
 * Includes helpers for manifest generation and OVA (POSIX TAR) packaging.
 *
 * Mirrors the role of C# XenOvfApi.OVF static class for the export path.
 */
class OvfWriter
{
    public:
        OvfWriter() = default;

        // ── XML generation ──────────────────────────────────────────────────

        /**
         * @brief Generate OVF XML for the given envelope.
         * @return UTF-8 XML string
         */
        QString generateXml(const OvfEnvelopeData& envelope) const;

        /**
         * @brief Write OVF XML to a file.
         * @param envelope    Data model to serialise
         * @param ovfFilePath Absolute path to output .ovf file
         * @return true on success
         */
        bool saveToFile(const OvfEnvelopeData& envelope, const QString& ovfFilePath) const;

        // ── Manifest ────────────────────────────────────────────────────────

        /**
         * @brief Compute SHA256 manifest for all files in a package folder.
         *
         * Lists the .ovf file and all VHD files referenced in the envelope,
         * writes <packageName>.mf next to the .ovf file.
         * Matches C# ExportApplianceAction.Manifest().
         *
         * @param envelope     Data model (used to enumerate file hrefs)
         * @param packageDir   Directory containing the package files
         * @param ovfFileName  Base name of the .ovf file (e.g. "myvm.ovf")
         * @return true on success
         */
        bool createManifest(const OvfEnvelopeData& envelope,
                            const QString& packageDir,
                            const QString& ovfFileName) const;

        // ── OVA packaging ───────────────────────────────────────────────────

        /**
         * @brief Pack an OVF package folder into a single OVA TAR archive.
         *
         * The OVA order follows the OVF spec: .ovf first, then disk files in
         * References order, then the .mf manifest (if present), then .cert.
         *
         * Matches C# OVF.ConvertOVFtoOVA().
         *
         * @param envelope    Data model (used to enumerate files)
         * @param packageDir  Directory containing the unpacked package
         * @param ovfFileName Base name of the .ovf file inside packageDir
         * @param ovaPath     Destination .ova file path
         * @return true on success
         */
        bool createOva(const OvfEnvelopeData& envelope,
                       const QString& packageDir,
                       const QString& ovfFileName,
                       const QString& ovaPath) const;

    private:
        void writeReferences(QXmlStreamWriter& xml, const OvfEnvelopeData& env) const;
        void writeDiskSection(QXmlStreamWriter& xml, const OvfEnvelopeData& env) const;
        void writeNetworkSection(QXmlStreamWriter& xml, const OvfEnvelopeData& env) const;
        void writeEulaSections(QXmlStreamWriter& xml, const OvfEnvelopeData& env) const;
        void writeVirtualSystem(QXmlStreamWriter& xml, const OvfVirtualSystemEntry& sys) const;

        /// Write one POSIX TAR entry to the output file.
        bool writeTarEntry(QFile& tarFile,
                           const QString& memberPath,
                           const QString& memberName) const;
};

#endif // OVFWRITER_H
