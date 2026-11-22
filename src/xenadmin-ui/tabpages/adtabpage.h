/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

// C# equivalent: xenadmin/XenAdmin/TabPages/AdPage.cs

#ifndef ADTABPAGE_H
#define ADTABPAGE_H

#include "basetabpage.h"
#include <QSet>
#include <functional>

namespace Ui
{
    class AdTabPage;
}

class Subject;
class QTimer;
class QPoint;

/**
 * "Active Directory Users": joins or leaves an AD domain and manages the users and groups
 * allowed to log in, their roles and their sessions. Shown for pools and live standalone
 * servers. Talks to the server only when a button is used; showing it reads the cache, plus a
 * poll every 5 seconds of who is logged in while the tab is visible and AD is enabled.
 */
class AdTabPage : public BaseTabPage
{
    Q_OBJECT

    public:
        explicit AdTabPage(QWidget* parent = nullptr);
        ~AdTabPage() override;

        QString GetTitle() const override;
        Type GetType() const override { return Type::Ad; }
        QString HelpID() const override { return "TabPageAD"; }
        bool IsApplicableForObjectType(XenObjectType objectType) const override;

        /**
         * Disconnects a connection without asking, after the user logs their own session out
         * (C# DisconnectCommand). MainWindow sets it, which keeps this tab free of MainWindow.
         */
        static std::function<void(XenConnection*)> DisconnectConnection;
        void OnPageShown() override;
        void OnPageHidden() override;

    protected:
        void refreshContent() override;
        void removeObject() override;
        void updateObject() override;

    private slots:
        void checkAdType();
        void repopulateList();
        void enableButtons();
        void onCacheChanged(XenConnection* connection, XenObjectType type, const QString& ref);
        void onCellClicked(int row, int column);
        void onContextMenu(const QPoint& pos);
        void pollLoggedInStatus();
        void onJoinLeave();
        void onAdd();
        void onRemove();
        void onLogout();
        void onChangeRoles();

    private:
        enum class State { Unavailable, Disabled, Configuring, Enabled };

        void setState(State state);
        bool adActionRunning() const;
        QString domain() const;
        bool isPool() const;
        QString connectionName(int length) const;
        //! The row's subject ref, empty for the local root account row
        QString refAt(int row) const;
        QList<QSharedPointer<Subject>> selectedSubjects(bool* rootSelected = nullptr) const;
        void refreshRow(int row);
        bool confirmLogout(bool logSelfOut, bool isRoleChange, const QList<QSharedPointer<Subject>>& subjects);
        void logoutSubjects(const QList<QSharedPointer<Subject>>& subjects);
        void logOutCurrentSubject(const QSharedPointer<Subject>& subject);
        static QString labelOf(const QSharedPointer<Subject>& subject);

        Ui::AdTabPage* ui;
        State m_state = State::Unavailable;
        QTimer* m_refreshTimer;
        QTimer* m_pollTimer;
        bool m_pollInFlight = false;
        bool m_statusKnown = false;       //!< C# AdSubjectRow.statusLost inverted
        QSet<QString> m_loggedInSids;
        QSet<QString> m_expanded;         //!< subject refs, "" for the root row
        bool m_filling = false;
        QString m_storedDomain;           //!< C# keeps these for the next join attempt
        QString m_storedUsername;
};

#endif // ADTABPAGE_H
