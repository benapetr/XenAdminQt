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

#ifndef OVFPACKAGE_H
#define OVFPACKAGE_H

#include <QString>
#include <QStringList>
#include <QDomDocument>
#include <QMap>

/**
 * @brief Virtual hardware configuration parsed from OVF RASD elements.
 *
 * Maps to a single VirtualSystem's VirtualHardwareSection.
 * ResourceType values follow CIM 28 schema:
 *   3  = CPU, 4 = Memory, 10 = Ethernet adapter,
 *  17  = CD/DVD drive, 22 = VHD/VDI disk.
 */
struct OvfVirtualHardware
{
    // CPU
    int vcpuCount = 0;          ///< VCPUs_max / VCPUs_at_startup

    // Memory
    qint64 memoryBytes = 0;     ///< Static/dynamic max memory in bytes

    // Disk sizes by disk file href (used to set VDI virtual_size)
    QMap<QString, qint64> diskSizesByHref;  ///< href → capacity in bytes

    // Network adapter count (number of RASD type-10 items)
    int ethernetCount = 0;
};

/**
 * @brief Minimal OVF/OVA package reader for import wizard source validation.
 *
 * Handles:
 *   - .ovf files (folder package — reads OVF descriptor directly)
 *   - .ova files (uncompressed TAR — extracts OVF descriptor from first entry)
 *   - .ova.gz files (gzip-compressed TAR — not yet supported; reports error)
 *
 * Equivalent to a subset of C# XenOvfApi.Package.
 */
class OvfPackage
{
    public:
        /**
         * @brief Open and parse an OVF/OVA file.
         * @param filePath Absolute path to .ovf, .ova, or .ova.gz file.
         */
        explicit OvfPackage(const QString& filePath);

        ~OvfPackage() = default;

        // Non-copyable
        OvfPackage(const OvfPackage&) = delete;
        OvfPackage& operator=(const OvfPackage&) = delete;

        /** @brief Whether the package was parsed successfully. */
        bool IsValid() const { return this->valid_; }

        /** @brief Error message if !IsValid(). */
        QString ParseError() const { return this->parseError_; }

        /** @brief Package name: base filename without extension of the .ovf descriptor. */
        QString PackageName() const { return this->packageName_; }

        /** @brief Source file path as passed to constructor. */
        QString SourceFile() const { return this->sourceFile_; }

        /**
         * @brief Whether the OVF envelope contains an encryption section.
         *
         * Matches C# OVF.HasEncryption().
         */
        bool HasEncryption() const { return this->hasEncryption_; }

        /**
         * @brief Whether a manifest (.mf) file exists alongside the descriptor.
         *
         * Matches C# Package.HasManifest().
         */
        bool HasManifest() const { return this->hasManifest_; }

        /**
         * @brief Whether a certificate (.cert) file exists alongside the descriptor.
         *
         * Matches C# Package.HasSignature().
         */
        bool HasSignature() const { return this->hasSignature_; }

        /**
         * @brief EULA text strings found in the OVF envelope.
         *
         * May be empty if no EulaSection elements are present.
         */
        QStringList Eulas() const { return this->eulas_; }

        /**
         * @brief Network names from NetworkSection/Network elements.
         *
         * These are the logical network names inside the OVF package. They need
         * to be mapped to real XenServer networks in the wizard.
         */
        QStringList NetworkNames() const { return this->networkNames_; }

        /**
         * @brief Disk file hrefs from References/File elements.
         */
        QStringList DiskFileRefs() const { return this->diskFileRefs_; }

        /**
         * @brief Names of virtual systems in the package (one per VirtualSystem element).
         */
        QStringList VirtualSystemNames() const { return this->virtualSystemNames_; }

        /**
         * @brief Number of virtual systems (VMs) in this package.
         */
        int VirtualSystemCount() const { return this->virtualSystemNames_.size(); }

        /**
         * @brief Number of networks defined in this package.
         */
        int NetworkCount() const { return this->networkNames_.size(); }

        /**
         * @brief Virtual hardware (CPU/memory/disk) per virtual system, keyed by system name/id.
         *
         * Keys match those returned by VirtualSystemNames(). An empty map means RASD parsing
         * was not possible for this package.
         */
        QMap<QString, OvfVirtualHardware> VirtualHardwareBySystem() const
        {
            return this->virtualHardwareBySystem_;
        }

        /**
         * @brief Raw XML text of the OVF descriptor, for advanced inspection.
         */
        QString DescriptorXml() const { return this->descriptorXml_; }

    private:
        void parseFromOvfXml(const QString& xml);
        void parseOvfFile(const QString& ovfFilePath);
        void parseOvaFile(const QString& ovaFilePath);
        QString extractOvfFromTar(const QString& tarFilePath);

        static QString firstAttributeByLocalName(const QDomElement& element, const QString& localName);
        static QDomNodeList elementsByLocalName(QDomDocument doc, const QString& localName);
        static void collectEulas(const QDomElement& root, QStringList& eulas);
        static void collectVirtualSystemNames(const QDomElement& root, QStringList& names);
        static OvfVirtualHardware collectVirtualHardware(const QDomElement& virtualSystemElem,
                                                          const QMap<QString,qint64>& capacitiesByRef);

        QString sourceFile_;
        QString packageName_;
        QString descriptorXml_;
        bool valid_;
        QString parseError_;

        bool hasEncryption_;
        bool hasManifest_;
        bool hasSignature_;

        QStringList eulas_;
        QStringList networkNames_;
        QStringList diskFileRefs_;
        QStringList virtualSystemNames_;
        QMap<QString, OvfVirtualHardware> virtualHardwareBySystem_;
};

#endif // OVFPACKAGE_H
