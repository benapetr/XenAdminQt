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

#include "adtabpage.h"
#include "ui_adtabpage.h"
#include "../globals.h"
#include "../dialogs/actionprogressdialog.h"
#include "../dialogs/adpasswordprompt.h"
#include "../dialogs/resolvingsubjectsdialog.h"
#include "../dialogs/roleselectiondialog.h"
#include "../dialogs/warningdialogs/warningdialog.h"
#include "xenlib/operations/multipleaction.h"
#include "xenlib/operations/operationmanager.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/role.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/subject.h"
#include "xenlib/xen/actions/ad/adactions.h"
#include "xenlib/xen/actions/delegatedasyncoperation.h"
#include "xenlib/xen/xenapi/xenapi_Session.h"
#include "xenlib/utils/misc.h"
#include "xenlib/xencache.h"
#include <QHeaderView>
#include <QMenu>
#include <QPointer>
#include <QThreadPool>
#include <QTimer>
#include <algorithm>
#include <memory>

namespace
{
    enum Column { ColExpand, ColType, ColSubject, ColRoles, ColLoggedIn };
    const int ROLE_REF = Qt::UserRole;        // subject ref; empty for the local root account row
    const int ROLE_SORT = Qt::UserRole + 1;

    QString elide(const QString& text, int length)
    {
        return text.length() > length ? text.left(length - 1) + QChar(0x2026) : text;
    }

    //! The subject's roles, highest rank first (C# roles.Sort(); roles.Reverse())
    QList<QSharedPointer<Role>> rolesOf(const QSharedPointer<Subject>& subject)
    {
        QList<QSharedPointer<Role>> roles;
        XenCache* cache = subject->GetConnection()->GetCache();
        for (const QString& ref : subject->RoleRefs())
        {
            if (QSharedPointer<Role> role = cache->ResolveObject<Role>(XenObjectType::Role, ref))
                roles.append(role);
        }
        std::stable_sort(roles.begin(), roles.end(), [](const QSharedPointer<Role>& a, const QSharedPointer<Role>& b)
        {
            return a->Rank() > b->Rank();
        });
        return roles;
    }

    // C# GridViewSubjectList_SortCompare: the root row stays on top whatever the order; the
    // other rows sort by ROLE_SORT (numbers) or naturally by text
    class SubjectItem : public QTableWidgetItem
    {
        public:
            using QTableWidgetItem::QTableWidgetItem;
            bool operator<(const QTableWidgetItem& other) const override
            {
                const bool ascending = this->tableWidget()
                    && this->tableWidget()->horizontalHeader()->sortIndicatorOrder() == Qt::AscendingOrder;
                const bool thisRoot = this->data(ROLE_REF).toString().isEmpty();
                const bool otherRoot = other.data(ROLE_REF).toString().isEmpty();
                if (thisRoot != otherRoot)
                    return thisRoot ? ascending : !ascending;

                const QVariant a = this->data(ROLE_SORT);
                const QVariant b = other.data(ROLE_SORT);
                if (a.userType() == QMetaType::Int)
                    return a.toInt() < b.toInt();
                return Misc::NaturalCompare(a.toString(), b.toString()) < 0;
            }
    };
}

std::function<void(XenConnection*)> AdTabPage::DisconnectConnection;

AdTabPage::AdTabPage(QWidget* parent) : BaseTabPage(parent), ui(new Ui::AdTabPage)
{
    this->ui->setupUi(this);
    QHeaderView* header = this->ui->tableSubjects->horizontalHeader();
    header->setSectionResizeMode(ColExpand, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ColType, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ColSubject, QHeaderView::Interactive);
    header->setSortIndicator(ColSubject, Qt::AscendingOrder);

    const QString rootTip = tr("Cannot modify the root account");
    this->ui->buttonRemove->setToolTip(rootTip);
    this->ui->buttonLogout->setToolTip(rootTip);
    this->ui->buttonChangeRoles->setToolTip(rootTip);

    connect(this->ui->buttonJoinLeave, &QPushButton::clicked, this, &AdTabPage::onJoinLeave);
    connect(this->ui->buttonAdd, &QPushButton::clicked, this, &AdTabPage::onAdd);
    connect(this->ui->buttonRemove, &QPushButton::clicked, this, &AdTabPage::onRemove);
    connect(this->ui->buttonLogout, &QPushButton::clicked, this, &AdTabPage::onLogout);
    connect(this->ui->buttonChangeRoles, &QPushButton::clicked, this, &AdTabPage::onChangeRoles);
    connect(this->ui->tableSubjects, &QTableWidget::cellClicked, this, &AdTabPage::onCellClicked);
    connect(this->ui->tableSubjects, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { this->onCellClicked(row, ColExpand); });
    connect(this->ui->tableSubjects, &QTableWidget::itemSelectionChanged, this, [this]()
    {
        if (!this->m_filling)
            this->enableButtons();
    });
    connect(this->ui->tableSubjects, &QTableWidget::customContextMenuRequested, this, &AdTabPage::onContextMenu);

    this->m_refreshTimer = new QTimer(this);
    this->m_refreshTimer->setSingleShot(true);
    this->m_refreshTimer->setInterval(200);
    connect(this->m_refreshTimer, &QTimer::timeout, this, &AdTabPage::checkAdType);

    // C# updateLoggedInStatus: every 5 seconds while the tab is shown
    this->m_pollTimer = new QTimer(this);
    this->m_pollTimer->setInterval(5000);
    connect(this->m_pollTimer, &QTimer::timeout, this, &AdTabPage::pollLoggedInStatus);

    this->setState(State::Unavailable);
}

AdTabPage::~AdTabPage()
{
    delete this->ui;
}

QString AdTabPage::GetTitle() const
{
    return tr("Active Directory Users");
}

bool AdTabPage::IsApplicableForObjectType(XenObjectType objectType) const
{
    // MainWindow adds it for pools and live standalone servers (C# isPoolOrLiveStandaloneHost)
    return objectType == XenObjectType::Pool || objectType == XenObjectType::Host;
}

void AdTabPage::OnPageShown()
{
    BaseTabPage::OnPageShown();
    this->checkAdType();
    this->m_pollTimer->start();
    this->pollLoggedInStatus();
}

void AdTabPage::OnPageHidden()
{
    BaseTabPage::OnPageHidden();
    this->m_pollTimer->stop();
}

void AdTabPage::refreshContent()
{
    this->checkAdType();
}

void AdTabPage::removeObject()
{
    this->m_pollTimer->stop();
    if (this->m_connection && this->m_connection->GetCache())
        disconnect(this->m_connection->GetCache(), &XenCache::itemChanged, this, &AdTabPage::onCacheChanged);
    OperationManager* operations = OperationManager::instance();
    disconnect(operations, &OperationManager::recordAdded, this->m_refreshTimer, nullptr);
    disconnect(operations, &OperationManager::recordUpdated, this->m_refreshTimer, nullptr);
    this->m_loggedInSids.clear();
    this->m_statusKnown = false;
    this->m_expanded.clear();
}

void AdTabPage::updateObject()
{
    if (!this->m_connection || !this->m_connection->GetCache())
        return;
    connect(this->m_connection->GetCache(), &XenCache::itemChanged, this, &AdTabPage::onCacheChanged, Qt::UniqueConnection);
    // C# History_CollectionChanged: follow Enable/Disable AD actions
    OperationManager* operations = OperationManager::instance();
    connect(operations, &OperationManager::recordAdded, this->m_refreshTimer, QOverload<>::of(&QTimer::start), Qt::UniqueConnection);
    connect(operations, &OperationManager::recordUpdated, this->m_refreshTimer, QOverload<>::of(&QTimer::start), Qt::UniqueConnection);
}

void AdTabPage::onCacheChanged(XenConnection* connection, XenObjectType type, const QString& ref)
{
    Q_UNUSED(ref);
    if (connection != this->m_connection.data())
        return;
    if (type == XenObjectType::Host || type == XenObjectType::Pool || type == XenObjectType::Subject || type == XenObjectType::Role)
        this->m_refreshTimer->start();
}

bool AdTabPage::isPool() const
{
    return this->m_connection && this->m_connection->GetCache() && this->m_connection->GetCache()->GetPool();
}

QString AdTabPage::connectionName(int length) const
{
    return elide(this->m_connection ? this->m_connection->GetName() : QString(), length);
}

QString AdTabPage::domain() const
{
    XenCache* cache = this->m_connection ? this->m_connection->GetCache() : nullptr;
    QSharedPointer<Pool> pool = cache ? cache->GetPoolOfOne() : QSharedPointer<Pool>();
    QSharedPointer<Host> coordinator = pool ? pool->GetMasterHost() : QSharedPointer<Host>();
    return coordinator ? coordinator->ExternalAuthServiceName() : tr("Unknown");
}

bool AdTabPage::adActionRunning() const
{
    // C#: an Enable/Disable AD action not yet finished for this connection
    for (OperationManager::OperationRecord* record : OperationManager::instance()->GetRecords())
    {
        if (!record || !record->operation || record->operation->GetConnection() != this->m_connection.data())
            continue;
        if (record->state != AsyncOperation::NotStarted && record->state != AsyncOperation::Running)
            continue;
        if (qobject_cast<EnableAdAction*>(record->operation) || qobject_cast<DisableAdAction*>(record->operation))
            return true;
    }
    return false;
}

void AdTabPage::checkAdType()
{
    // C# checkAdType
    XenCache* cache = this->m_connection ? this->m_connection->GetCache() : nullptr;
    QSharedPointer<Pool> pool = cache ? cache->GetPoolOfOne() : QSharedPointer<Pool>();
    QSharedPointer<Host> coordinator = pool ? pool->GetMasterHost() : QSharedPointer<Host>();

    if (!coordinator)
        this->setState(State::Unavailable);
    else if (this->adActionRunning())
        this->setState(State::Configuring);
    else if (coordinator->ExternalAuthType().isEmpty())
        this->setState(State::Disabled);
    else
        this->setState(State::Enabled);  // C# assumes AD for any other auth type too
}

void AdTabPage::setState(State state)
{
    const bool enabledBefore = this->m_state == State::Enabled;
    this->m_state = state;
    const bool enabled = state == State::Enabled;

    switch (state)
    {
        case State::Unavailable:
            this->ui->labelBlurb->setText(tr("Please wait while %1 establishes your current external authentication configuration.").arg(XENADMIN_BRANDING_NAME));
            this->ui->buttonJoinLeave->setEnabled(false);
            break;
        case State::Disabled:
            this->ui->labelBlurb->setText(this->isPool()
                ? tr("AD is not currently configured for pool '%1'. To enable AD authentication, click Join Domain.").arg(this->connectionName(70))
                : tr("AD is not currently configured for server '%1'. To enable AD authentication, click Join Domain.").arg(this->connectionName(70)));
            this->ui->buttonJoinLeave->setText(tr("&Join Domain..."));
            this->ui->buttonJoinLeave->setEnabled(true);
            break;
        case State::Configuring:
            this->ui->labelBlurb->setText(this->isPool()
                ? tr("AD authentication is currently being configured for pool '%1'.").arg(this->connectionName(70))
                : tr("AD authentication is currently being configured for server '%1'.").arg(this->connectionName(70)));
            this->ui->buttonJoinLeave->setEnabled(false);
            break;
        case State::Enabled:
            this->ui->labelBlurb->setText(this->isPool()
                ? tr("Pool '%1' belongs to domain '%2'.").arg(this->connectionName(70), elide(this->domain(), 30))
                : tr("Server '%1' belongs to domain '%2'.").arg(this->connectionName(70), elide(this->domain(), 30)));
            this->ui->buttonJoinLeave->setText(tr("L&eave Domain..."));
            this->ui->buttonJoinLeave->setEnabled(true);
            break;
    }

    // C# SetSubjectListEnable
    this->ui->buttonAdd->setEnabled(enabled);
    this->ui->listStack->setCurrentWidget(enabled ? this->ui->pageSubjects : this->ui->pageDisabled);
    if (enabled)
    {
        if (!enabledBefore)
            this->m_expanded.insert(QString());  // C# expands the root row when the list appears
        this->repopulateList();
    } else
    {
        this->ui->tableSubjects->setRowCount(0);
        this->ui->buttonRemove->setEnabled(false);
        this->ui->buttonLogout->setEnabled(false);
        this->ui->buttonChangeRoles->setEnabled(false);
    }
}

QString AdTabPage::labelOf(const QSharedPointer<Subject>& subject)
{
    if (!subject->DisplayName().isEmpty())
        return subject->DisplayName();
    return !subject->SubjectName().isEmpty() ? subject->SubjectName() : subject->SubjectIdentifier();
}

QString AdTabPage::refAt(int row) const
{
    QTableWidgetItem* item = this->ui->tableSubjects->item(row, ColSubject);
    return item ? item->data(ROLE_REF).toString() : QString();
}

void AdTabPage::repopulateList()
{
    // C# RepopulateListBox: the root row, then every subject; keep selection and expansion
    if (this->m_state != State::Enabled || !this->m_connection || !this->m_connection->GetCache())
        return;

    QTableWidget* table = this->ui->tableSubjects;
    QSet<QString> selected;
    for (const QModelIndex& index : table->selectionModel()->selectedRows())
        selected.insert(this->refAt(index.row()));

    this->m_filling = true;
    table->setSortingEnabled(false);
    table->setRowCount(0);

    QStringList refs{QString()};
    for (const QSharedPointer<Subject>& subject : this->m_connection->GetCache()->GetAll<Subject>(XenObjectType::Subject))
    {
        if (subject)
            refs.append(subject->OpaqueRef());
    }

    for (const QString& ref : refs)
    {
        const int row = table->rowCount();
        table->insertRow(row);
        for (int column = ColExpand; column <= ColLoggedIn; ++column)
        {
            auto* item = new SubjectItem();
            item->setData(ROLE_REF, ref);
            table->setItem(row, column, item);
        }
        this->refreshRow(row);
    }

    table->setSortingEnabled(true);
    table->sortItems(table->horizontalHeader()->sortIndicatorSection(), table->horizontalHeader()->sortIndicatorOrder());
    for (int row = 0; row < table->rowCount(); ++row)
    {
        if (selected.contains(this->refAt(row)) && !this->refAt(row).isEmpty())
            table->selectRow(row);
    }
    if (table->selectionModel()->selectedRows().isEmpty() && table->rowCount() > 0)
        table->selectRow(0);
    table->resizeColumnToContents(ColSubject);
    table->resizeColumnToContents(ColRoles);
    table->resizeRowsToContents();
    this->m_filling = false;
    this->enableButtons();
}

void AdTabPage::refreshRow(int row)
{
    // C# AdSubjectRow.RefreshCellContent
    QTableWidget* table = this->ui->tableSubjects;
    const QString ref = this->refAt(row);
    const bool expanded = this->m_expanded.contains(ref);
    QSharedPointer<Subject> subject = ref.isEmpty() ? QSharedPointer<Subject>()
        : this->m_connection->GetCache()->ResolveObject<Subject>(XenObjectType::Subject, ref);
    if (!ref.isEmpty() && !subject)
        return;

    QString info, roles;
    bool group = false;
    int topRank = -2;
    QString loggedIn = "-";
    if (!subject)
    {
        info = expanded ? tr("Local root account") + "\n\n" + tr("(Always granted access)") : tr("Local root account");
    } else
    {
        group = subject->IsGroup();
        if (expanded)
        {
            QStringList lines;
            for (const auto& kvp : Subject::ExtractKvpInfo(subject->GetOtherConfig()))
                lines.append(kvp.second.isEmpty() ? kvp.first : kvp.first + kvp.second);
            info = lines.join("\n");
        } else
        {
            info = !subject->DisplayName().isEmpty() ? subject->DisplayName() : subject->SubjectName();
        }

        const QList<QSharedPointer<Role>> subjectRoles = rolesOf(subject);
        QStringList names;
        for (const QSharedPointer<Role>& role : subjectRoles)
            names.append(role->FriendlyName());
        if (!subjectRoles.isEmpty())
            topRank = subjectRoles.first()->Rank();
        roles = expanded ? names.join("\n")
              : names.isEmpty() ? QString() : names.size() > 1 ? names.first() + "..." : names.first();

        if (!group && this->m_statusKnown)
            loggedIn = this->m_loggedInSids.contains(subject->SubjectIdentifier()) ? tr("Yes") : tr("No");
    }

    table->item(row, ColExpand)->setIcon(QIcon(expanded ? ":/icons/expanded_triangle.png" : ":/icons/contracted_triangle.png"));
    table->item(row, ColType)->setIcon(QIcon(group ? ":/icons/user_group_16.png" : ":/icons/user_16.png"));
    table->item(row, ColType)->setData(ROLE_SORT, group ? 1 : 0);
    table->item(row, ColSubject)->setText(info);
    // C# NameCompare is on the subject name (it negates it, offset by its initial descending sort)
    table->item(row, ColSubject)->setData(ROLE_SORT, subject ? subject->SubjectName() : QString());
    table->item(row, ColRoles)->setText(roles);
    table->item(row, ColRoles)->setData(ROLE_SORT, topRank);
    table->item(row, ColLoggedIn)->setText(loggedIn);
    table->item(row, ColLoggedIn)->setData(ROLE_SORT, loggedIn == tr("Yes") ? 1 : 0);
}

void AdTabPage::onCellClicked(int row, int column)
{
    // C#: the expander column, or a double click, toggles a row's details
    if (column != ColExpand)
        return;
    const QString ref = this->refAt(row);
    if (this->m_expanded.contains(ref))
        this->m_expanded.remove(ref);
    else
        this->m_expanded.insert(ref);
    this->refreshRow(row);
    this->ui->tableSubjects->resizeRowToContents(row);
}

QList<QSharedPointer<Subject>> AdTabPage::selectedSubjects(bool* rootSelected) const
{
    QList<QSharedPointer<Subject>> subjects;
    if (rootSelected)
        *rootSelected = false;
    for (const QModelIndex& index : this->ui->tableSubjects->selectionModel()->selectedRows())
    {
        const QString ref = this->refAt(index.row());
        if (ref.isEmpty())
        {
            if (rootSelected)
                *rootSelected = true;
            continue;
        }
        if (QSharedPointer<Subject> subject = this->m_connection->GetCache()->ResolveObject<Subject>(XenObjectType::Subject, ref))
            subjects.append(subject);
    }
    return subjects;
}

void AdTabPage::enableButtons()
{
    // C# EnableButtons
    if (this->m_state != State::Enabled)
        return;
    bool rootSelected = false;
    const QList<QSharedPointer<Subject>> subjects = this->selectedSubjects(&rootSelected);
    if (subjects.isEmpty() && !rootSelected)
        return;

    bool allLoggedIn = this->m_statusKnown;
    for (const QSharedPointer<Subject>& subject : subjects)
        allLoggedIn &= this->m_loggedInSids.contains(subject->SubjectIdentifier());

    this->ui->buttonChangeRoles->setEnabled(!rootSelected);
    this->ui->buttonRemove->setEnabled(!rootSelected);
    this->ui->buttonLogout->setEnabled(!rootSelected && allLoggedIn);
}

void AdTabPage::onContextMenu(const QPoint& pos)
{
    if (this->m_state != State::Enabled)
        return;
    QMenu menu(this);
    menu.addAction(tr("Add..."), this, &AdTabPage::onAdd);
    menu.addAction(tr("&Change Role..."), this, &AdTabPage::onChangeRoles)->setEnabled(this->ui->buttonChangeRoles->isEnabled());
    menu.addAction(tr("&Logout User"), this, &AdTabPage::onLogout)->setEnabled(this->ui->buttonLogout->isEnabled());
    menu.addAction(tr("Remo&ve"), this, &AdTabPage::onRemove)->setEnabled(this->ui->buttonRemove->isEnabled());
    menu.exec(this->ui->tableSubjects->viewport()->mapToGlobal(pos));
}

void AdTabPage::pollLoggedInStatus()
{
    // C# updateLoggedInStatus, on a pool thread with the connection's session (the event
    // poller has its own), the result applied on the GUI thread
    if (this->m_state != State::Enabled || this->m_pollInFlight || !this->m_connection || !this->isVisible())
        return;
    XenAPI::Session* session = this->m_connection->GetSession();
    if (!session || !session->IsLoggedIn())
        return;

    this->m_pollInFlight = true;
    QPointer<AdTabPage> self(this);
    QPointer<XenConnection> connection = this->m_connection;
    QThreadPool::globalInstance()->start([self, connection, session]()
    {
        QStringList sids;
        bool ok = true;
        try
        {
            sids = XenAPI::SessionAPI::get_all_subject_identifiers(session);
        } catch (const std::exception& e)
        {
            ok = false;
            qWarning() << "AdTabPage: can't get the logged-in users:" << e.what();
        }
        QMetaObject::invokeMethod(qApp, [self, connection, sids, ok]()
        {
            if (!self)
                return;
            self->m_pollInFlight = false;
            if (connection != self->m_connection)
                return;
            self->m_statusKnown = ok;  // C# SetStatusLost on errors
            self->m_loggedInSids = QSet<QString>(sids.begin(), sids.end());
            for (int row = 0; row < self->ui->tableSubjects->rowCount(); ++row)
                self->refreshRow(row);
            self->enableButtons();
        }, Qt::QueuedConnection);
    });
}

void AdTabPage::onJoinLeave()
{
    if (!this->m_connection)
        return;

    if (this->m_state == State::Disabled)
    {
        // Joining; keep the domain and user name for the next attempt (C#)
        AdPasswordPrompt prompt(true, QString(), this);
        prompt.SetDomain(this->m_storedDomain);
        prompt.SetUsername(this->m_storedUsername);
        const int result = prompt.exec();
        this->m_storedDomain = prompt.Domain();
        this->m_storedUsername = prompt.Username();
        if (result != QDialog::Accepted)
            return;
        (new EnableAdAction(this->m_connection, prompt.Domain(), prompt.Username(), prompt.Password()))->RunAsync(true);
        this->checkAdType();
        return;
    }

    // Leaving: warn when the user will be disconnected
    XenAPI::Session* session = this->m_connection->GetSession();
    if (!session)
        return;
    QString message;
    if (session->IsLocalSuperuser())
    {
        message = tr("You are about to remove '%1' from the AD domain '%2'. Any users authenticated using AD will be disconnected.\n\nDo you want to continue?")
                      .arg(this->connectionName(50), elide(this->domain(), 30));
    } else
    {
        message = this->isPool()
            ? tr("You are currently connected to pool '%1' using Active Directory authentication.\nIf you leave the AD domain '%2', you will be disconnected from the pool.\n"
                 "To reconnect, you will need to supply the local root account credentials.\n\nDo you want to continue?")
            : tr("You are currently connected to server '%1' using Active Directory authentication.\nIf you leave the AD domain '%2', you will be disconnected from the server.\n"
                 "To reconnect, you will need to supply the local root account credentials.\n\nDo you want to continue?");
        message = message.arg(this->connectionName(50), elide(this->domain(), 30));
    }
    if (WarningDialog::ShowYesNo(message, tr("AD Authentication"), this) != WarningDialog::Result::Yes)
        return;

    AdPasswordPrompt prompt(false, this->domain(), this);
    const int result = prompt.exec();
    if (result == QDialog::Rejected)
        return;
    QVariantMap creds;
    if (result != AdPasswordPrompt::Ignored)
    {
        creds["user"] = prompt.Username();
        creds["pass"] = prompt.Password();
    }
    (new DisableAdAction(this->m_connection, creds))->RunAsync(true);
    this->checkAdType();
}

void AdTabPage::onAdd()
{
    if (!this->ui->buttonAdd->isEnabled() || !this->m_connection)
        return;
    ResolvingSubjectsDialog dialog(this->m_connection, this);
    dialog.exec();
}

void AdTabPage::onRemove()
{
    // C# ButtonRemove_Click
    if (!this->ui->buttonRemove->isEnabled())
        return;
    const QList<QSharedPointer<Subject>> subjects = this->selectedSubjects();
    if (subjects.isEmpty())
        return;

    QString message = subjects.size() == 1
        ? tr("Are you sure you want to remove the user '%1'?").arg(!subjects.first()->DisplayName().isEmpty() ? subjects.first()->DisplayName() : subjects.first()->SubjectName())
        : tr("Are you sure you want to remove these %1 users?").arg(subjects.size());

    // Removing pool admins: advise changing the root password and rotating the pool secret
    if (Host::StockholmOrGreater(this->m_connection))
    {
        int admins = 0;
        for (const QSharedPointer<Subject>& subject : subjects)
        {
            const QList<QSharedPointer<Role>> roles = rolesOf(subject);
            if (std::any_of(roles.begin(), roles.end(), [](const QSharedPointer<Role>& r) { return r->GetName() == "pool-admin"; }))
                ++admins;
        }
        QString adminMessage;
        if (admins > 0 && admins == subjects.size())
            adminMessage = admins == 1 ? tr("The user you are about to remove is a Pool Administrator.")
                                       : tr("The users you are about to remove are Pool Administrators.");
        else if (admins > 0)
            adminMessage = admins == 1 ? tr("One of the users you are about to remove is a Pool Administrator.")
                                       : tr("%1 of the users you are about to remove are Pool Administrators.").arg(admins);
        if (!adminMessage.isEmpty())
            message = QString("%1\n\n%2 %3").arg(message, adminMessage,
                tr("To reduce the risk of unauthorized access to your system, it is recommended that you also change the server root password and rotate the pool secret."));
    }

    if (WarningDialog::ShowYesNo(message, tr("AD Authentication"), this) != WarningDialog::Result::Yes)
        return;

    // Warn when revoking the credentials in use
    XenAPI::Session* session = this->m_connection->GetSession();
    const QString sessionSubject = session ? session->GetSessionSubject() : QString();
    for (const QSharedPointer<Subject>& subject : subjects)
    {
        if (sessionSubject.isEmpty() || subject->OpaqueRef() != sessionSubject)
            continue;
        const QString name = !subject->DisplayName().isEmpty() || !subject->SubjectName().isEmpty()
            ? elide(labelOf(subject), 256) : subject->SubjectIdentifier();
        const QString text = subject->IsGroup()
            ? tr("You are about to revoke the permissions for group '%1'.\n\nThe user you are currently logged into '%2' with is a member of '%1'. "
                 "Proceeding will disconnect you and any other members of this group from '%2'. To reconnect, you will need to supply the local root account credentials, "
                 "or the credentials for another authorized AD user.\n\nDo you want to continue?")
            : tr("You are about to revoke the permissions for user '%1'.\n\nYou are currently logged in as '%1' on '%2', and doing this will disconnect you from '%2'. "
                 "To reconnect, you will need to supply the local root account credentials, or the credentials for another authorized AD user.\n\nDo you want to continue?");
        if (WarningDialog::ShowYesNo(text.arg(name, this->connectionName(50)), tr("AD Authentication"), this) != WarningDialog::Result::Yes)
            return;
        break;
    }

    auto* action = new AddRemoveSubjectsAction(this->m_connection, {}, subjects);
    auto* progress = new ActionProgressDialog(action, this);
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->show();
    action->RunAsync(true);
}

bool AdTabPage::confirmLogout(bool logSelfOut, bool isRoleChange, const QList<QSharedPointer<Subject>>& subjects)
{
    // C# ConfirmLogout
    if (logSelfOut)
    {
        QString text = isRoleChange
            ? (subjects.size() > 1
                ? tr("You are currently logged in as one of the selected users. If you assign a new role, you will be logged out of '%1'. "
                     "All other selected users that are logged in will also be logged out.\n\nAre you sure you want to continue?")
                : tr("You are currently logged in as the selected user. If you assign a new role, you will be logged out of '%1'.\n\nAre you sure you want to continue?"))
            : (subjects.size() > 1
                ? tr("You are currently logged in as one of the selected users. If you continue, you will be logged out of '%1'. "
                     "All other selected users will also be logged out.\n\nAre you sure you want to continue?")
                : tr("You are currently logged in as the selected user. If you continue, you will be logged out of '%1'.\n\nAre you sure you want to continue?"));
        return WarningDialog::ShowYesNo(text.arg(this->connectionName(50)), tr("AD Authentication"), this) == WarningDialog::Result::Yes;
    }

    const QString first = subjects.isEmpty() ? QString()
        : (!subjects.first()->DisplayName().isEmpty() ? subjects.first()->DisplayName() : subjects.first()->SubjectName());
    const QString text = isRoleChange
        ? (subjects.size() == 1
            ? tr("If you assign a new role to user '%1', the user will be logged out.\n\nAre you sure you want to continue?").arg(first)
            : tr("If you assign a new role to logged in users, they will be logged out.\n\nAre you sure you want to continue?"))
        : (subjects.size() == 1
            ? tr("Are you sure you want to log out the user '%1'?").arg(first)
            : tr("Are you sure you want to log out these %1 users?").arg(subjects.size()));
    return WarningDialog::ShowYesNo(text, tr("AD Authentication"), this) == WarningDialog::Result::Yes;
}

void AdTabPage::onLogout()
{
    // C# ButtonLogout_Click
    if (!this->ui->buttonLogout->isEnabled() || !this->m_connection || !this->m_connection->GetSession())
        return;
    const QString sessionSubject = this->m_connection->GetSession()->GetSessionSubject();
    QList<QSharedPointer<Subject>> toLogout;
    bool logSelfOut = false;
    for (const QSharedPointer<Subject>& subject : this->selectedSubjects())
    {
        if (!this->m_loggedInSids.contains(subject->SubjectIdentifier()))
            continue;
        toLogout.append(subject);
        logSelfOut |= !sessionSubject.isEmpty() && subject->OpaqueRef() == sessionSubject;
    }
    if (toLogout.isEmpty() || !this->confirmLogout(logSelfOut, false, toLogout))
        return;
    this->logoutSubjects(toLogout);
}

void AdTabPage::logoutSubjects(const QList<QSharedPointer<Subject>>& subjects)
{
    // C# LogoutSubjects: the others first, our own session last
    XenAPI::Session* session = this->m_connection ? this->m_connection->GetSession() : nullptr;
    if (!session)
        return;
    const QString userSid = session->GetUserSid();
    QSharedPointer<Subject> current;
    QList<QSharedPointer<Subject>> others;
    for (const QSharedPointer<Subject>& subject : subjects)
    {
        if (!userSid.isEmpty() && subject->SubjectIdentifier() == userSid)
            current = subject;
        else
            others.append(subject);
    }

    if (others.isEmpty())
    {
        if (current)
            this->logOutCurrentSubject(current);
        return;
    }

    QList<AsyncOperation*> actions;
    QStringList names;
    for (const QSharedPointer<Subject>& subject : others)
    {
        names.append(labelOf(subject));
        const QString sid = subject->SubjectIdentifier();
        auto* op = new DelegatedAsyncOperation(this->m_connection, tr("Terminating session for user '%1'").arg(labelOf(subject)),
            tr("In progress"), [sid](DelegatedAsyncOperation* self)
            {
                XenAPI::SessionAPI::logout_subject_identifier(self->GetSession(), sid);
            });
        op->AddApiMethodToRoleCheck("session.logout_subject_identifier");
        actions.append(op);
    }
    const QString title = others.size() > 1 ? tr("Terminating session for users '%1'").arg(names.join(", "))
                                            : tr("Terminating session for user '%1'").arg(names.join(", "));
    auto* multiple = new MultipleAction(this->m_connection, title, tr("In progress"), tr("Completed"), actions, false, true, true);
    // The action deletes itself; read it on its thread and hand the result over
    QPointer<AdTabPage> self(this);
    connect(multiple, &AsyncOperation::completed, multiple, [self, multiple, current]()
    {
        const bool cancelled = multiple->IsCancelled();
        QMetaObject::invokeMethod(qApp, [self, cancelled, current]()
        {
            if (!self || cancelled)
                return;
            if (current)
                self->logOutCurrentSubject(current);
            else
                self->pollLoggedInStatus();  // C# pulses the status updater
        }, Qt::QueuedConnection);
    }, Qt::DirectConnection);
    multiple->RunAsync(true);
}

void AdTabPage::logOutCurrentSubject(const QSharedPointer<Subject>& subject)
{
    // C# LogOutCurrentSubject: logout_subject_identifier spares the calling session, so
    // disconnect explicitly afterwards
    const QString sid = subject->SubjectIdentifier();
    QPointer<XenConnection> connection = this->m_connection;
    auto* op = new DelegatedAsyncOperation(this->m_connection, tr("Terminating session for user '%1'").arg(labelOf(subject)),
        tr("In progress"), [sid](DelegatedAsyncOperation* self)
        {
            XenAPI::SessionAPI::logout_subject_identifier(self->GetSession(), sid);
        });
    op->AddApiMethodToRoleCheck("session.logout_subject_identifier");
    connect(op, &AsyncOperation::completed, op, [op, connection]()
    {
        const bool cancelled = op->IsCancelled();
        QMetaObject::invokeMethod(qApp, [cancelled, connection]()
        {
            if (cancelled || !connection || !AdTabPage::DisconnectConnection)
                return;
            AdTabPage::DisconnectConnection(connection);
        }, Qt::QueuedConnection);
    }, Qt::DirectConnection);
    op->RunAsync(true);
}

void AdTabPage::onChangeRoles()
{
    // C# ButtonChangeRoles_Click
    if (!this->ui->buttonChangeRoles->isEnabled() || !this->m_connection)
        return;
    const QList<QSharedPointer<Subject>> subjects = this->selectedSubjects();
    if (subjects.isEmpty())
        return;

    RoleSelectionDialog dialog(this->m_connection, subjects, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QStringList newRoles = dialog.SelectedRoleRefs();
    newRoles.sort();
    const QString sessionSubject = this->m_connection->GetSession() ? this->m_connection->GetSession()->GetSessionSubject() : QString();
    QList<QSharedPointer<Subject>> toLogout;
    bool logSelfOut = false;
    for (const QSharedPointer<Subject>& subject : subjects)
    {
        if (!this->m_loggedInSids.contains(subject->SubjectIdentifier()))
            continue;
        QStringList current = subject->RoleRefs();
        current.sort();
        if (current != newRoles)
            toLogout.append(subject);
        logSelfOut |= !sessionSubject.isEmpty() && subject->OpaqueRef() == sessionSubject;
    }
    if (!toLogout.isEmpty() && !this->confirmLogout(logSelfOut, true, toLogout))
        return;

    // One action per subject; afterwards log out the logged-in ones whose roles changed
    QList<AsyncOperation*> actions;
    auto succeeded = std::make_shared<QSet<QString>>();
    QStringList names;
    for (const QSharedPointer<Subject>& subject : subjects)
    {
        names.append(labelOf(subject));
        auto* action = new AddRemoveRolesAction(this->m_connection, subject, dialog.SelectedRoleRefs());
        const QString ref = subject->OpaqueRef();
        connect(action, &AsyncOperation::completed, action, [action, succeeded, ref]()
        {
            if (!action->IsCancelled() && !action->IsFailed())
                succeeded->insert(ref);
        }, Qt::DirectConnection);
        actions.append(action);
    }
    const QString title = subjects.size() > 1 ? tr("Assigning new roles to subjects '%1'").arg(names.join(", "))
                                              : tr("Assigning new roles to subject '%1'").arg(names.join(", "));
    auto* multiple = new MultipleAction(this->m_connection, title, tr("In progress"), tr("Completed"), actions, true, true, true);
    QPointer<AdTabPage> self(this);
    connect(multiple, &AsyncOperation::completed, multiple, [self, multiple, succeeded, toLogout]()
    {
        const bool cancelled = multiple->IsCancelled();
        QMetaObject::invokeMethod(qApp, [self, cancelled, succeeded, toLogout]()
        {
            if (!self || cancelled)
                return;
            QList<QSharedPointer<Subject>> logout;
            for (const QSharedPointer<Subject>& subject : toLogout)
            {
                if (succeeded->contains(subject->OpaqueRef()))
                    logout.append(subject);
            }
            if (!logout.isEmpty())
                self->logoutSubjects(logout);
        }, Qt::QueuedConnection);
    }, Qt::DirectConnection);
    multiple->RunAsync(true);
}
