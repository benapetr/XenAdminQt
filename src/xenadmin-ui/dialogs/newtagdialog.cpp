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

#include "newtagdialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <algorithm>

namespace
{
QString cleanedTag(const QString& text)
{
    return text.trimmed();
}
}

NewTagDialog::NewTagDialog(QWidget* parent)
    : QDialog(parent)
{
    this->setWindowTitle(tr("Edit Tags"));
    this->resize(560, 420);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->addWidget(new QLabel(tr("Select tags to apply. Partially checked tags are preserved unchanged."), this));

    this->m_table = new QTableWidget(this);
    this->m_table->setColumnCount(2);
    this->m_table->setHorizontalHeaderLabels(QStringList() << tr("Selected") << tr("Tag"));
    this->m_table->verticalHeader()->setVisible(false);
    this->m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    this->m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    this->m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    root->addWidget(this->m_table, 1);
    connect(this->m_table, &QTableWidget::cellClicked, this, &NewTagDialog::onCellClicked);
    QShortcut* toggleShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this->m_table);
    connect(toggleShortcut, &QShortcut::activated, this, &NewTagDialog::onToggleSelection);

    QHBoxLayout* addRow = new QHBoxLayout();
    this->m_addLineEdit = new QLineEdit(this);
    this->m_addLineEdit->setPlaceholderText(tr("Add tag"));
    this->m_addButton = new QPushButton(tr("Add"), this);
    this->m_addButton->setEnabled(false);
    addRow->addWidget(this->m_addLineEdit, 1);
    addRow->addWidget(this->m_addButton);
    root->addLayout(addRow);

    connect(this->m_addLineEdit, &QLineEdit::textChanged, this, &NewTagDialog::onAddTextChanged);
    connect(this->m_addButton, &QPushButton::clicked, this, &NewTagDialog::onAddClicked);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void NewTagDialog::SetTags(const QStringList& allTags, const QStringList& selectedTags, const QStringList& indeterminateTags)
{
    this->m_table->setRowCount(0);

    QStringList checked = selectedTags;
    checked.removeDuplicates();
    checked.sort();

    QStringList partial = indeterminateTags;
    partial.removeDuplicates();
    partial.sort();

    QStringList all = allTags;
    for (const QString& tag : checked)
        if (!all.contains(tag))
            all.append(tag);
    for (const QString& tag : partial)
        if (!all.contains(tag))
            all.append(tag);
    all.removeDuplicates();
    all.sort();

    for (const QString& tag : all)
    {
        if (checked.contains(tag))
            this->addOrUpdateTag(tag, Qt::Checked);
        else if (partial.contains(tag))
            this->addOrUpdateTag(tag, Qt::PartiallyChecked);
        else
            this->addOrUpdateTag(tag, Qt::Unchecked);
    }

    this->resortRows();
}

QStringList NewTagDialog::GetSelectedTags() const
{
    QStringList tags;
    for (int row = 0; row < this->m_table->rowCount(); ++row)
    {
        QTableWidgetItem* checkItem = this->m_table->item(row, 0);
        QTableWidgetItem* tagItem = this->m_table->item(row, 1);
        if (!checkItem || !tagItem)
            continue;

        if (checkItem->checkState() == Qt::Checked)
            tags.append(tagItem->text());
    }

    tags.removeDuplicates();
    tags.sort();
    return tags;
}

QStringList NewTagDialog::GetIndeterminateTags() const
{
    QStringList tags;
    for (int row = 0; row < this->m_table->rowCount(); ++row)
    {
        QTableWidgetItem* checkItem = this->m_table->item(row, 0);
        QTableWidgetItem* tagItem = this->m_table->item(row, 1);
        if (!checkItem || !tagItem)
            continue;

        if (checkItem->checkState() == Qt::PartiallyChecked)
            tags.append(tagItem->text());
    }

    tags.removeDuplicates();
    tags.sort();
    return tags;
}

void NewTagDialog::onAddTextChanged(const QString& text)
{
    this->m_addButton->setEnabled(!cleanedTag(text).isEmpty());
}

void NewTagDialog::onAddClicked()
{
    const QString tag = cleanedTag(this->m_addLineEdit->text());
    if (tag.isEmpty())
        return;

    this->addOrUpdateTag(tag, Qt::Checked);
    this->resortRows();
    this->m_addLineEdit->clear();
}

void NewTagDialog::onCellClicked(int row, int column)
{
    if (row < 0 || row >= this->m_table->rowCount() || column != 0)
        return;

    QTableWidgetItem* item = this->m_table->item(row, 0);
    if (!item)
        return;

    // Match C# toggle behavior: Checked -> Unchecked, otherwise -> Checked.
    const Qt::CheckState next = item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked;
    item->setCheckState(next);
    this->resortRows();
}

void NewTagDialog::onToggleSelection()
{
    const QList<QTableWidgetSelectionRange> ranges = this->m_table->selectedRanges();
    if (ranges.isEmpty())
        return;

    bool allChecked = true;
    for (const QTableWidgetSelectionRange& range : ranges)
    {
        for (int row = range.topRow(); row <= range.bottomRow(); ++row)
        {
            QTableWidgetItem* item = this->m_table->item(row, 0);
            if (!item || item->checkState() != Qt::Checked)
            {
                allChecked = false;
                break;
            }
        }
        if (!allChecked)
            break;
    }

    const Qt::CheckState nextState = allChecked ? Qt::Unchecked : Qt::Checked;
    for (const QTableWidgetSelectionRange& range : ranges)
    {
        for (int row = range.topRow(); row <= range.bottomRow(); ++row)
        {
            QTableWidgetItem* item = this->m_table->item(row, 0);
            if (item)
                item->setCheckState(nextState);
        }
    }

    this->resortRows();
}

void NewTagDialog::addOrUpdateTag(const QString& tag, Qt::CheckState checkState)
{
    const QString cleaned = cleanedTag(tag);
    if (cleaned.isEmpty())
        return;

    for (int row = 0; row < this->m_table->rowCount(); ++row)
    {
        QTableWidgetItem* tagItem = this->m_table->item(row, 1);
        if (!tagItem)
            continue;
        if (tagItem->text() != cleaned)
            continue;

        QTableWidgetItem* checkItem = this->m_table->item(row, 0);
        if (checkItem)
            checkItem->setCheckState(checkState);
        return;
    }

    const int row = this->m_table->rowCount();
    this->m_table->insertRow(row);

    QTableWidgetItem* checkItem = new QTableWidgetItem();
    checkItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    checkItem->setCheckState(checkState);
    this->m_table->setItem(row, 0, checkItem);

    QTableWidgetItem* tagItem = new QTableWidgetItem(cleaned);
    tagItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    this->m_table->setItem(row, 1, tagItem);
}

void NewTagDialog::resortRows()
{
    struct RowData
    {
        QString tag;
        Qt::CheckState state = Qt::Unchecked;
    };

    QList<RowData> rows;
    rows.reserve(this->m_table->rowCount());
    for (int row = 0; row < this->m_table->rowCount(); ++row)
    {
        QTableWidgetItem* checkItem = this->m_table->item(row, 0);
        QTableWidgetItem* tagItem = this->m_table->item(row, 1);
        if (!checkItem || !tagItem)
            continue;

        rows.append({tagItem->text(), checkItem->checkState()});
    }

    std::sort(rows.begin(), rows.end(), [this](const RowData& a, const RowData& b)
    {
        const int pa = this->priorityForState(a.state);
        const int pb = this->priorityForState(b.state);
        if (pa != pb)
            return pa < pb;
        return QString::localeAwareCompare(a.tag, b.tag) < 0;
    });

    this->m_table->setRowCount(0);
    for (const RowData& row : rows)
        this->addOrUpdateTag(row.tag, row.state);
}

int NewTagDialog::priorityForState(Qt::CheckState state) const
{
    switch (state)
    {
        case Qt::Checked:
            return static_cast<int>(TagPriority::Checked);
        case Qt::PartiallyChecked:
            return static_cast<int>(TagPriority::Indeterminate);
        default:
            return static_cast<int>(TagPriority::Unchecked);
    }
}
