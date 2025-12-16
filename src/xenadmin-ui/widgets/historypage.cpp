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

#include "historypage.h"

#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDateTime>
#include <QLocale>

HistoryPage::HistoryPage(QWidget* parent)
    : QWidget(parent),
      m_tree(new QTreeWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tree->setColumnCount(5);
    QStringList headers;
    headers << tr("Started")
            << tr("Finished")
            << tr("Operation")
            << tr("Status")
            << tr("Details");
    m_tree->setHeaderLabels(headers);
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(4, QHeaderView::Stretch);

    layout->addWidget(m_tree);

    auto manager = OperationManager::instance();
    connect(manager, &OperationManager::recordAdded, this, &HistoryPage::onRecordAdded);
    connect(manager, &OperationManager::recordUpdated, this, &HistoryPage::onRecordUpdated);
    connect(manager, &OperationManager::recordRemoved, this, &HistoryPage::onRecordRemoved);

    const auto& existing = manager->records();
    for (auto* record : existing)
        onRecordAdded(record);
}

void HistoryPage::onRecordAdded(OperationManager::OperationRecord* record)
{
    if (!record || m_items.contains(record))
        return;

    auto* item = new QTreeWidgetItem(m_tree);
    m_items.insert(record, item);
    updateItem(record, item);
}

void HistoryPage::onRecordUpdated(OperationManager::OperationRecord* record)
{
    auto it = m_items.find(record);
    if (it == m_items.end())
        return;

    updateItem(record, it.value());
}

void HistoryPage::onRecordRemoved(OperationManager::OperationRecord* record)
{
    auto it = m_items.find(record);
    if (it == m_items.end())
        return;

    QTreeWidgetItem* item = it.value();
    m_items.erase(it);

    if (!item)
        return;

    // When removing items managed by QTreeWidget, detach them from the tree
    // before deleting to avoid the widget deleting them a second time later.
    int topLevelIndex = m_tree->indexOfTopLevelItem(item);
    if (topLevelIndex >= 0)
    {
        delete m_tree->takeTopLevelItem(topLevelIndex);
    } else
    {
        delete item;
    }
}

void HistoryPage::updateItem(OperationManager::OperationRecord* record, QTreeWidgetItem* item)
{
    if (!record || !item)
        return;

    const auto locale = QLocale();
    item->setText(0, record->started.isValid() ? locale.toString(record->started.toLocalTime(), QLocale::ShortFormat) : QString());
    item->setText(1, record->finished.isValid() ? locale.toString(record->finished.toLocalTime(), QLocale::ShortFormat) : QString());
    item->setText(2, record->title);
    item->setText(3, statusText(record));
    item->setText(4, detailText(record));
}

QString HistoryPage::statusText(OperationManager::OperationRecord* record) const
{
    switch (record->state)
    {
    case AsyncOperation::NotStarted:
        return tr("Pending");
    case AsyncOperation::Running:
        return tr("Running (%1%)").arg(record->progress);
    case AsyncOperation::Completed:
        return tr("Completed");
    case AsyncOperation::Cancelled:
        return tr("Cancelled");
    case AsyncOperation::Failed:
        return tr("Failed");
    default:
        return tr("Unknown");
    }
}

QString HistoryPage::detailText(OperationManager::OperationRecord* record) const
{
    if (!record->errorMessage.isEmpty())
        return record->errorMessage;
    if (!record->description.isEmpty())
        return record->description;
    return QString();
}
