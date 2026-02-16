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

#include "commanderrordialog.h"
#include "ui_commanderrordialog.h"
#include "../iconmanager.h"
#include "xen/xenobject.h"
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QIcon>
#include <QPixmap>
#include <QStyle>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTimer>
#include <QShowEvent>
#include <QResizeEvent>

// Constructor with simple string map
CommandErrorDialog::CommandErrorDialog(const QString& title,
                                       const QString& text,
                                       const QMap<QString, QString>& cantRunReasons,
                                       DialogMode mode,
                                       QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::CommandErrorDialog)
    , m_mode(mode)
    , m_currentSortColumn(1)  // Default sort by Name column
    , m_currentSortOrder(Qt::AscendingOrder)
{
    this->ui->setupUi(this);
    this->setupDialog(title, text, mode);

    // Add rows without icons
    for (auto it = cantRunReasons.constBegin(); it != cantRunReasons.constEnd(); ++it)
    {
        this->addRow(QString(), it.key(), it.value());
    }

    // Sort by name column initially
    this->ui->tableWidget->sortItems(this->m_currentSortColumn, this->m_currentSortOrder);
    this->scheduleRowResize();
}

// Constructor with icon data
CommandErrorDialog::CommandErrorDialog(const QString& title,
                                       const QString& text,
                                       const QMap<QString, QPair<QString, QString>>& cantRunReasons,
                                       DialogMode mode,
                                       QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::CommandErrorDialog)
    , m_mode(mode)
    , m_currentSortColumn(1)  // Default sort by Name column
    , m_currentSortOrder(Qt::AscendingOrder)
{
    this->ui->setupUi(this);
    this->setupDialog(title, text, mode);

    // Add rows with icons
    for (auto it = cantRunReasons.constBegin(); it != cantRunReasons.constEnd(); ++it)
    {
        const QString& name = it.key();
        const QPair<QString, QString>& data = it.value();
        this->addRow(data.first, name, data.second);  // data.first = icon path, data.second = reason
    }

    // Sort by name column initially
    this->ui->tableWidget->sortItems(this->m_currentSortColumn, this->m_currentSortOrder);
    this->scheduleRowResize();
}

// Constructor with XenObject pointers (matches C# version)
CommandErrorDialog::CommandErrorDialog(const QString& title,
                                       const QString& text,
                                       const QHash<QSharedPointer<XenObject>, QString>& cantRunReasons,
                                       DialogMode mode,
                                       QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::CommandErrorDialog)
    , m_mode(mode)
    , m_currentSortColumn(1)  // Default sort by Name column
    , m_currentSortOrder(Qt::AscendingOrder)
{
    this->ui->setupUi(this);
    this->setupDialog(title, text, mode);

    // Add rows with XenObject icons and names
    // Sort objects by name first (matches C# RowComparer behavior)
    QList<QSharedPointer<XenObject>> sortedObjects = cantRunReasons.keys();
    std::sort(sortedObjects.begin(), sortedObjects.end(),
              [](const QSharedPointer<XenObject>& a, const QSharedPointer<XenObject>& b) {
                  return a->GetName() < b->GetName();
              });

    for (const auto& xenObject : sortedObjects)
    {
        if (!xenObject)
            continue;

        QIcon icon = IconManager::instance().GetIconForObject(xenObject.data());
        QString name = xenObject->GetName();
        QString reason = cantRunReasons.value(xenObject);
        
        this->addRow(icon, name, reason);
    }

    // Sort by name column initially (already sorted in insertion order)
    this->ui->tableWidget->sortItems(this->m_currentSortColumn, this->m_currentSortOrder);
    this->scheduleRowResize();
}

CommandErrorDialog::~CommandErrorDialog()
{
    delete this->ui;
}

void CommandErrorDialog::setupDialog(const QString& title, const QString& text, DialogMode mode)
{
    this->setWindowTitle(title);
    this->ui->textLabel->setText(text);

    // Set error icon using Qt standard icon
    QIcon errorIcon = this->style()->standardIcon(QStyle::SP_MessageBoxCritical);
    this->ui->iconLabel->setPixmap(errorIcon.pixmap(32, 32));

    // Configure button box based on mode
    this->ui->buttonBox->clear();
    if (mode == DialogMode::OKCancel)
    {
        QPushButton* okButton = this->ui->buttonBox->addButton(QDialogButtonBox::Ok);
        QPushButton* cancelButton = this->ui->buttonBox->addButton(QDialogButtonBox::Cancel);
        
        connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    }
    else  // DialogMode::Close
    {
        QPushButton* closeButton = this->ui->buttonBox->addButton(QDialogButtonBox::Close);
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    }

    // Configure table widget
    this->ui->tableWidget->setColumnCount(3);
    this->ui->tableWidget->setHorizontalHeaderLabels(QStringList() << "" << tr("Name") << tr("Reason"));

    // Allow multiline cell content like C# grid, and grow row heights to fit text.
    this->ui->tableWidget->setWordWrap(true);
    this->ui->tableWidget->setTextElideMode(Qt::ElideNone);
    this->ui->tableWidget->setIconSize(QSize(16, 16));
    this->ui->tableWidget->verticalHeader()->setDefaultSectionSize(20);
    this->ui->tableWidget->verticalHeader()->setMinimumSectionSize(20);

    // Set column widths/sizing.
    this->ui->tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    this->ui->tableWidget->setColumnWidth(0, 22);  // Icon column
    this->ui->tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    this->ui->tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    // Hide row numbers
    this->ui->tableWidget->verticalHeader()->setVisible(false);
    
    // Enable sorting
    this->ui->tableWidget->setSortingEnabled(true);
    
    // Connect header click for custom sorting
    connect(this->ui->tableWidget->horizontalHeader(), &QHeaderView::sectionClicked,
            this, &CommandErrorDialog::onTableHeaderClicked);
    connect(this->ui->tableWidget->horizontalHeader(), &QHeaderView::sectionResized,
            this, [this](int, int, int) { this->scheduleRowResize(); });
    
    // Set selection behavior
    this->ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
}

void CommandErrorDialog::addRow(const QString& iconPath, const QString& name, const QString& reason)
{
    int row = this->ui->tableWidget->rowCount();
    this->ui->tableWidget->insertRow(row);

    // Column 0: Icon
    QTableWidgetItem* iconItem = new QTableWidgetItem();
    if (!iconPath.isEmpty())
    {
        QIcon icon(iconPath);
        iconItem->setIcon(icon);
    }
    iconItem->setTextAlignment(Qt::AlignTop | Qt::AlignHCenter);
    iconItem->setFlags(iconItem->flags() & ~Qt::ItemIsEditable);  // Read-only
    this->ui->tableWidget->setItem(row, 0, iconItem);

    // Column 1: Name
    QTableWidgetItem* nameItem = new QTableWidgetItem(name);
    nameItem->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);  // Read-only
    this->ui->tableWidget->setItem(row, 1, nameItem);

    // Column 2: Reason
    QTableWidgetItem* reasonItem = new QTableWidgetItem(reason);
    reasonItem->setFlags(reasonItem->flags() & ~Qt::ItemIsEditable);  // Read-only
    reasonItem->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);
    this->ui->tableWidget->setItem(row, 2, reasonItem);

}

void CommandErrorDialog::addRow(const QIcon& icon, const QString& name, const QString& reason)
{
    int row = this->ui->tableWidget->rowCount();
    this->ui->tableWidget->insertRow(row);

    // Column 0: Icon
    QTableWidgetItem* iconItem = new QTableWidgetItem();
    if (!icon.isNull())
    {
        iconItem->setIcon(icon);
    }
    iconItem->setTextAlignment(Qt::AlignTop | Qt::AlignHCenter);
    iconItem->setFlags(iconItem->flags() & ~Qt::ItemIsEditable);  // Read-only
    this->ui->tableWidget->setItem(row, 0, iconItem);

    // Column 1: Name
    QTableWidgetItem* nameItem = new QTableWidgetItem(name);
    nameItem->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);  // Read-only
    this->ui->tableWidget->setItem(row, 1, nameItem);

    // Column 2: Reason
    QTableWidgetItem* reasonItem = new QTableWidgetItem(reason);
    reasonItem->setFlags(reasonItem->flags() & ~Qt::ItemIsEditable);  // Read-only
    reasonItem->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);
    this->ui->tableWidget->setItem(row, 2, reasonItem);

}

void CommandErrorDialog::onTableHeaderClicked(int logicalIndex)
{
    // Matches C# m_dataGridView_ColumnHeaderMouseClick behavior
    if (logicalIndex < 1 || logicalIndex > 2)
        return;  // Only allow sorting on Name and Reason columns

    // Toggle sort order if clicking same column, otherwise default to ascending
    if (logicalIndex == this->m_currentSortColumn)
    {
        this->m_currentSortOrder = (this->m_currentSortOrder == Qt::AscendingOrder)
                                    ? Qt::DescendingOrder
                                    : Qt::AscendingOrder;
    }
    else
    {
        this->m_currentSortColumn = logicalIndex;
        this->m_currentSortOrder = Qt::AscendingOrder;
    }

    this->ui->tableWidget->sortItems(this->m_currentSortColumn, this->m_currentSortOrder);
    this->scheduleRowResize();
}

void CommandErrorDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    this->scheduleRowResize();
}

void CommandErrorDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    this->scheduleRowResize();
}

void CommandErrorDialog::scheduleRowResize()
{
    if (this->m_rowResizePending || !this->ui || !this->ui->tableWidget)
        return;

    this->m_rowResizePending = true;
    QTimer::singleShot(0, this, [this]() {
        this->m_rowResizePending = false;
        if (!this->ui || !this->ui->tableWidget)
            return;
        this->ui->tableWidget->resizeRowsToContents();
    });
}
