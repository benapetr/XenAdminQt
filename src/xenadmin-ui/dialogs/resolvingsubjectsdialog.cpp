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

#include "resolvingsubjectsdialog.h"
#include "ui_resolvingsubjectsdialog.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/ad/adactions.h"
#include <QCloseEvent>
#include <QHeaderView>
#include <QRegularExpression>

ResolvingSubjectsDialog::ResolvingSubjectsDialog(XenConnection* connection, QWidget* parent)
    : QDialog(parent), ui(new Ui::ResolvingSubjectsDialog), m_connection(connection)
{
    this->ui->setupUi(this);
    QString name = connection ? connection->GetName() : QString();
    if (name.length() > 80)
        name = name.left(79) + QChar(0x2026);
    this->ui->labelTopBlurb->setText(this->ui->labelTopBlurb->text().arg(name));

    this->ui->entryList->setVisible(false);
    this->ui->progressBar->setVisible(false);
    this->ui->labelStatus->setVisible(false);
    this->ui->buttonGrantAccess->setEnabled(false);

    connect(this->ui->textBoxUserNames, &QLineEdit::textChanged, this, [this](const QString& text)
    {
        this->ui->buttonGrantAccess->setEnabled(!text.isEmpty());
    });
    connect(this->ui->buttonGrantAccess, &QPushButton::clicked, this, &ResolvingSubjectsDialog::beginResolve);
    connect(this->ui->buttonCancel, &QPushButton::clicked, this, &ResolvingSubjectsDialog::reject);
}

ResolvingSubjectsDialog::~ResolvingSubjectsDialog()
{
    delete this->ui;
}

QStringList ResolvingSubjectsDialog::SplitNames(const QString& text)
{
    QStringList names;
    for (const QString& part : text.split(QRegularExpression("[,;]")))
    {
        const QString clean = part.trimmed();
        if (!clean.isEmpty() && !names.contains(clean))
            names.append(clean);
    }
    return names;
}

void ResolvingSubjectsDialog::beginResolve()
{
    // C# BeginResolve
    const QStringList names = SplitNames(this->ui->textBoxUserNames->text());
    if (names.isEmpty() || !this->m_connection)
        return;

    this->ui->entryList->setVisible(true);
    this->ui->progressBar->setVisible(true);
    this->ui->labelStatus->setVisible(true);
    this->ui->textBoxUserNames->setEnabled(false);
    this->ui->buttonGrantAccess->setEnabled(false);

    for (const QString& name : names)
    {
        Entry entry;
        entry.enteredName = name;
        entry.item = new QTreeWidgetItem(this->ui->entryList, QStringList{name});
        this->updateEntry(entry);
        this->m_entries.append(entry);
    }
    this->ui->entryList->header()->resizeSections(QHeaderView::ResizeToContents);

    auto* action = new AddRemoveSubjectsAction(this->m_connection, names, {});
    this->m_action = action;
    connect(action, &AddRemoveSubjectsAction::nameResolveComplete, this, &ResolvingSubjectsDialog::onNameResolved, Qt::QueuedConnection);
    connect(action, &AddRemoveSubjectsAction::allResolveComplete, this, [this]()
    {
        this->ui->labelStatus->setText(tr("Adding resolved names to access list..."));
    }, Qt::QueuedConnection);
    connect(action, &AddRemoveSubjectsAction::subjectAddComplete, this, &ResolvingSubjectsDialog::onSubjectAdded, Qt::QueuedConnection);
    connect(action, &AsyncOperation::progressChanged, this->ui->progressBar, &QProgressBar::setValue, Qt::QueuedConnection);
    connect(action, &AsyncOperation::completed, this, &ResolvingSubjectsDialog::onCompleted, Qt::QueuedConnection);
    action->RunAsync(true);
}

void ResolvingSubjectsDialog::onNameResolved(const QString& enteredName, const QString& resolvedName, const QString& sid, const QString& error)
{
    for (Entry& entry : this->m_entries)
    {
        if (entry.enteredName == enteredName)
        {
            entry.resolvedName = resolvedName;
            entry.sid = sid;
            entry.resolveFailed = !error.isEmpty();
            this->updateEntry(entry);
            break;
        }
    }
}

void ResolvingSubjectsDialog::onSubjectAdded(const QString& sid, const QString& name, const QString& error)
{
    Q_UNUSED(name);
    for (Entry& entry : this->m_entries)
    {
        if (entry.sid == sid)
        {
            entry.added = error.isEmpty();
            entry.addFailed = !error.isEmpty();
            this->updateEntry(entry);
            break;
        }
    }
}

void ResolvingSubjectsDialog::onCompleted()
{
    // C# addAction_Completed
    this->m_finished = true;
    const bool cancelled = this->m_action && this->m_action->IsCancelled();
    bool anyFailed = false;
    for (Entry& entry : this->m_entries)
    {
        if (cancelled)
        {
            // C# ListViewItemSubjectWrapper.Cancel
            if (entry.sid.isEmpty() && !entry.resolveFailed)
                entry.resolveFailed = true;
            if (!entry.added && !entry.addFailed)
                entry.addFailed = true;
            this->updateEntry(entry);
        }
        anyFailed |= entry.resolveFailed || entry.addFailed;
    }

    this->ui->labelStatus->setText(cancelled ? tr("Canceled by user")
                                   : anyFailed ? tr("Completed, with some errors encountered. See the application log files for more information.")
                                               : tr("Completed"));
    this->ui->progressBar->setValue(this->ui->progressBar->maximum());
    // C# SwitchCloseToCancel
    this->ui->buttonCancel->setText(tr("Close"));
    this->ui->buttonCancel->setDefault(true);
}

void ResolvingSubjectsDialog::updateEntry(Entry& entry)
{
    // C# resolveStatus / grantStatus
    QString resolve;
    if (!entry.sid.isEmpty())
        resolve = tr("Resolved as %1").arg(entry.resolvedName.isEmpty() ? tr("<unknown AD user>") : entry.resolvedName);
    else if (entry.resolveFailed)
        resolve = tr("Subject could not be resolved in your AD");
    else
        resolve = tr("Resolving...");

    QString grant;
    if (entry.resolveFailed || entry.addFailed)
        grant = tr("Failed to add to access list");
    else if (!entry.sid.isEmpty())
        grant = entry.added ? tr("Added to access list") : tr("Adding to access list...");

    entry.item->setText(1, resolve);
    entry.item->setText(2, grant);
}

void ResolvingSubjectsDialog::cancelAction()
{
    // C# OnClosing cancels the action if it's still going
    if (this->m_action && !this->m_finished && !this->m_action->IsCancelled())
        this->m_action->Cancel();
}

void ResolvingSubjectsDialog::closeEvent(QCloseEvent* event)
{
    this->cancelAction();
    QDialog::closeEvent(event);
}

void ResolvingSubjectsDialog::reject()
{
    this->cancelAction();
    QDialog::reject();
}
