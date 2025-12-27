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

#ifndef NETWORKPROPERTIESDIALOG_H
#define NETWORKPROPERTIESDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

QT_FORWARD_DECLARE_CLASS(XenLib)

class NetworkPropertiesDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit NetworkPropertiesDialog(XenLib* xenLib, const QString& networkUuid, QWidget* parent = nullptr);

    private slots:
        void onOkClicked();
        void onCancelClicked();
        void onNetworksReceived(const QVariantList& networks);

    private:
        void setupGeneralTab();
        void setupAdvancedTab();
        void requestNetworkData();
        void populateNetworkData();
        void saveNetworkData();

        XenLib* m_xenLib;
        QString m_networkUuid;
        QString m_networkRef;
        QVariantMap m_networkRecord; // Stores the received network record

        // Tabs
        QTabWidget* m_tabWidget;

        // General tab widgets
        QLineEdit* m_nameEdit;
        QTextEdit* m_descriptionEdit;
        QLabel* m_uuidLabel;
        QLabel* m_bridgeLabel;
        QLabel* m_mtuLabel;
        QLabel* m_managedLabel;

        // Advanced tab widgets
        QLineEdit* m_tagsEdit;
        QListWidget* m_otherConfigList;

        // Buttons
        QPushButton* m_okButton;
        QPushButton* m_cancelButton;
};

#endif // NETWORKPROPERTIESDIALOG_H
