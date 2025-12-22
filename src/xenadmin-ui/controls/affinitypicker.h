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

#ifndef AFFINITYPICKER_H
#define AFFINITYPICKER_H

#include <QMap>
#include <QSharedPointer>
#include <QWidget>
#include <QVariantMap>

namespace Ui
{
    class AffinityPicker;
}

class XenConnection;
class Host;

/**
 * @brief Affinity picker control for selecting a VM home server
 *
 * Qt port of C# XenAdmin.Controls.AffinityPicker.
 * Used by VM Properties and New VM wizard to select an affinity host.
 */
class AffinityPicker : public QWidget
{
    Q_OBJECT

    public:
        explicit AffinityPicker(QWidget* parent = nullptr);
        ~AffinityPicker() override;

        void setAffinity(XenConnection* connection,
                         const QString& affinityRef,
                         const QString& srHostRef);

        QString selectedAffinityRef() const;
        bool validState() const;

        void setAutoSelectAffinity(bool enabled);
        bool autoSelectAffinity() const;

    signals:
        void selectedAffinityChanged();

    protected:
        void showEvent(QShowEvent* event) override;

    private slots:
        void onStaticRadioToggled(bool checked);
        void onSelectionChanged();

    private:
        void loadServers();
        void updateControl();
        void selectRadioButtons();
        bool selectAffinityServer();
        bool selectServer(const QString& hostRef);
        bool selectSomething();
        bool hasFullyConnectedSharedStorage() const;
        bool isHostLive(const QSharedPointer<Host>& host) const;

        Ui::AffinityPicker* ui;
        XenConnection* m_connection;
        QString m_affinityRef;
        QString m_srHostRef;
        QMap<QString, QSharedPointer<Host>> m_hosts;
        bool m_autoSelectAffinity;
        bool m_selectedOnVisibleChanged;
};

#endif // AFFINITYPICKER_H
