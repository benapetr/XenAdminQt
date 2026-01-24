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

#ifndef VIFDIALOG_H
#define VIFDIALOG_H

#include <QDialog>
#include <QVariantMap>
#include <QSharedPointer>
#include <QString>

class XenConnection;
class VM;
class VIF;
class Network;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class VIFDialog;
}
QT_END_NAMESPACE

/**
 * @brief VIFDialog - Dialog for creating/editing virtual network interfaces
 *
 * Qt port of C# XenAdmin.Dialogs.VIFDialog
 *
 * Features:
 * - Network selection dropdown (filtered by pool)
 * - MAC address configuration (autogenerate or manual)
 * - QoS bandwidth limit settings
 * - Input validation
 * - Duplicate MAC address detection
 *
 * C# path: XenAdmin/Dialogs/VIFDialog.cs
 */
class VIFDialog : public QDialog
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor for creating a new VIF
         * @param vm VM object
         * @param deviceId Device ID for the new VIF
         * @param parent Parent widget
         */
        explicit VIFDialog(QSharedPointer<VM> vm, int deviceId, QWidget* parent = nullptr);

        /**
         * @brief Constructor for creating a new VIF without VM context (wizard usage)
         * @param connection XenConnection instance
         * @param deviceId Device ID for the new VIF
         * @param parent Parent widget
         */
        explicit VIFDialog(XenConnection* connection, int deviceId, QWidget* parent = nullptr);

        /**
         * @brief Constructor for editing an existing VIF
         * @param vif VIF object to edit
         * @param parent Parent widget
         */
        explicit VIFDialog(QSharedPointer<VIF> vif, QWidget* parent = nullptr);

        /**
         * @brief Constructor for editing a pending VIF settings map (wizard usage)
         * @param connection XenConnection instance
         * @param existingVif VIF settings map to prefill
         * @param deviceId Device ID for the VIF
         * @param parent Parent widget
         */
        explicit VIFDialog(XenConnection* connection, const QVariantMap& existingVif, int deviceId, QWidget* parent = nullptr);

        ~VIFDialog();

        /**
         * @brief Get the configured VIF settings
         * @return VIF record as QVariantMap
         *
         * For new VIFs: contains network, MAC, device, qos_algorithm_type, qos_algorithm_params
         * For existing VIFs: contains all VIF fields with updated values
         */
        QVariantMap getVifSettings() const;

        /**
         * @brief Check if any changes were made (for edit mode)
         * @return true if settings changed from original
         */
        bool hasChanges() const;

    protected:
        void showEvent(QShowEvent* event) override;

    private slots:
        void onNetworkChanged();
        void onMACRadioChanged();
        void onMACTextChanged();
        void onQoSCheckboxChanged();
        void onQoSValueChanged();
        void validateInput();

    private:
        void loadNetworks();
        void loadVifDetails();
        bool isValidNetwork(QString* error = nullptr) const;
        bool isValidMAC(QString* error = nullptr) const;
        bool isValidQoS(QString* error = nullptr) const;
        QString getSelectedNetworkRef() const;
        QString getSelectedMAC() const;
        bool isDuplicateMAC(const QString& mac) const;

        Ui::VIFDialog* ui;
        XenConnection* m_connection;
        QSharedPointer<VM> m_vm;   // VM (for new VIFs)
        QSharedPointer<VIF> m_vif; // VIF (for editing)
        QString m_vmRef;           // VM reference (for wizard VIFs)
        int m_deviceId;            // Device ID for new VIF
        QVariantMap m_existingVif; // Original VIF data (for editing)
        bool m_isEditMode;         // true if editing existing VIF
};

#endif // VIFDIALOG_H
