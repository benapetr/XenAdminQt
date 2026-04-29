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
#include <functional>

/**
 * @brief Xen-specific VM recommendations parsed from OVF VirtualSystemOtherConfigurationData.
 *
 * Corresponds to the @c <restrictions> XML blob stored in VM.recommendations on XenServer.
 * In an OVF package this blob is serialised as the value of the configuration data entry
 * named @c "recommendations".
 *
 * C# equivalent: VM.GetRecommendations() / GetRestrictionValue<T>()
 */
struct OvfRecommendations
{
    /// Whether SR-IOV networks may be assigned to this VM (restriction field "allow-network-sriov").
    /// -1 = not specified in OVF; 0 = prohibited; 1 = allowed.
    int allowNetworkSriov = -1;

    /// Maximum number of vCPUs (restriction field "vcpus-max"), or -1 if unspecified.
    int maxVcpus = -1;

    /// Minimum number of vCPUs (restriction field "vcpus-min"), or -1 if unspecified.
    int minVcpus = -1;

    /// Maximum memory in bytes (restriction field "memory-static-max"), or -1 if unspecified.
    qint64 maxMemoryBytes = -1;

    /// Whether the VM supports UEFI boot (restriction field "supports-uefi" == "yes").
    bool supportsUefi = false;

    /// Whether the VM supports Secure Boot (restriction field "supports-secure-boot" == "yes").
    bool supportsSecureBoot = false;
};

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
         * @brief Xen-specific VM recommendations per virtual system, keyed by system name/id.
         *
         * Keys match those returned by VirtualSystemNames(). An empty map means the OVF
         * does not contain Citrix/XenServer recommendation entries.
         *
         * C# equivalent: OVF.FindVirtualHardwareSectionByAffinity() + GetRestrictionValue()
         */
        QMap<QString, OvfRecommendations> RecommendationsBySystem() const
        {
            return this->m_recommendationsBySystem;
        }

        /**
         * @brief Whether any virtual system in this package allows SR-IOV network assignment.
         *
         * Returns @c true when all virtual systems either allow SR-IOV or do not specify the
         * restriction.  Returns @c false only when at least one virtual system explicitly
         * prohibits it (@c allow-network-sriov == 0).
         *
         * C# equivalent: ImportSelectNetworkPage.AllowSriovNetwork()
         */
        bool AllowsNetworkSriov() const;

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

        /**
         * @brief Validate the OVF package and collect non-fatal warnings.
         *
         * Performs the same checks as the C# XenOvfApi.OVF.Validate():
         *   - OVF version present and known
         *   - Referenced disk/disk-image files exist relative to the package directory
         *   - Disk files are referenced from at least one RASD (linkage check)
         *   - Known file extensions for referenced files
         *
         * The method always returns true as long as the package envelope could be
         * loaded (i.e. IsValid() is true). Warnings describe non-fatal issues that
         * the user can acknowledge and ignore.  Returns false only when the envelope
         * cannot be loaded at all (same condition as !IsValid()).
         *
         * C# equivalent: OVF.Validate(Package, out List<string> warnings)
         *
         * @param warningsOut  Receives a list of human-readable warning strings.
         * @return true if the package is structurally importable, false if not.
         */
        bool Validate(QStringList& warningsOut) const;

        /**
         * @brief Verify the integrity of OVF package files against a manifest.
         *
         * Reads @c baseName.mf from @p workingDir and computes SHA-1 or SHA-256
         * hashes of every file listed in the manifest. An error is returned if any
         * file is missing or its hash does not match the recorded digest.
         *
         * Manifest line format (C# / XenServer convention):
         * @code
         *   SHA1(filename.ovf)= abcdef01234567890...
         *   SHA256(another-disk.vhd)= 0123456789abcdef...
         * @endcode
         *
         * @param workingDir   Directory that contains both the manifest file and
         *                     all referenced package files.
         * @param baseName     Base name (no extension) used to locate the .mf file,
         *                     e.g. @c "mypackage" → @c workingDir/mypackage.mf.
         * @param errorOut     Receives a human-readable error message on failure.
         * @param cancelCheck  Optional; verification aborts if this returns @c true.
         * @return @c true when all digests match, @c false on any failure.
         *
         * C# equivalent: Package.VerifyManifest()
         */
        static bool VerifyManifest(const QString& workingDir,
                                   const QString& baseName,
                                   QString& errorOut,
                                   std::function<bool()> cancelCheck = nullptr);

        /**
         * @brief Extract all files from a TAR (.ova) archive to a directory.
         *
         * Each entry in the archive is written as a flat file under @p destDir.
         * Subdirectory path components in entry names are stripped — all files
         * land directly in @p destDir.
         *
         * @param ovaPath    Absolute path to the .ova / .tar archive.
         * @param destDir    Existing directory to extract files into.
         * @param errorOut   Receives an error message on failure.
         * @param cancelCheck  Optional callable; extraction stops if it returns true.
         * @return true on success, false on error or cancellation.
         *
         * C# equivalent: Package.ExtractToWorkingDir() / ArchiveIterator.ExtractAllContents()
         */
        static bool ExtractAllToDir(const QString& ovaPath,
                                    const QString& destDir,
                                    QString& errorOut,
                                    std::function<bool()> cancelCheck = nullptr);

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
        static OvfRecommendations parseRecommendations(const QDomElement& virtualSystemElem);

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
        QMap<QString, OvfRecommendations> m_recommendationsBySystem;
};

#endif // OVFPACKAGE_H
