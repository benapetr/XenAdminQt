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

#include "pdsection.h"
#include "../commands/command.h"
#include <QHeaderView>
#include <QMouseEvent>
#include <QClipboard>
#include <QApplication>
#include <QTableWidgetItem>
#include <QHBoxLayout>
#include <QFrame>
#include <QEvent>
#include <QStyle>

PDSection::PDSection(QWidget* parent)
    : QWidget(parent)
    , mainLayout_(new QVBoxLayout(this))
    , headerPanel_(new QWidget(this))
    , titleLabel_(new QLabel(tr("Title"), this))
    , chevronButton_(new QPushButton(this))
    , tableWidget_(new QTableWidget(0, 3, this))
    , contextMenu_(new QMenu(this))
    , copyAction_(new QAction(tr("Copy"), this))
    , isExpanded_(true)
    , inLayout_(false)
    , disableFocusEvent_(false)
    , chevronHot_(false)
{
    this->setMinimumHeight(0);
    
    // Setup header panel
    QHBoxLayout* headerLayout = new QHBoxLayout(headerPanel_);
    headerLayout->setContentsMargins(5, 3, 5, 3);
    headerLayout->setSpacing(5);
    
    titleLabel_->setStyleSheet("font-weight: bold;");
    chevronButton_->setFlat(true);
    chevronButton_->setFixedSize(16, 16);
    chevronButton_->setCursor(Qt::PointingHandCursor);
    
    headerLayout->addWidget(this->titleLabel_);
    headerLayout->addStretch();
    headerLayout->addWidget(this->chevronButton_);
    
    // Use palette defaults so dark mode remains readable
    this->headerPanel_->setAutoFillBackground(false);
    
    // Setup table
    this->tableWidget_->setColumnCount(3);
    this->tableWidget_->setHorizontalHeaderLabels({"Key", "Value", "Notes"});
    this->tableWidget_->horizontalHeader()->setVisible(false);
    this->tableWidget_->verticalHeader()->setVisible(false);
    this->tableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->tableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    this->tableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->tableWidget_->setShowGrid(false);
    this->tableWidget_->setFrameStyle(QFrame::NoFrame);
    this->tableWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
    this->tableWidget_->setMouseTracking(true);
    
    // Column sizing
    this->tableWidget_->horizontalHeader()->setStretchLastSection(false);
    this->tableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    this->tableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    this->tableWidget_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    this->tableWidget_->setColumnWidth(0, 150); // Default key column width
    
    // Setup context menu
    this->contextMenu_->addAction(this->copyAction_);
    
    // Layout
    this->mainLayout_->setContentsMargins(0, 0, 0, 0);
    this->mainLayout_->setSpacing(1);
    this->mainLayout_->addWidget(this->headerPanel_);
    this->mainLayout_->addWidget(this->tableWidget_);
    
    // Use palette defaults so dark mode remains readable
    
    // Connect signals
    connect(this->chevronButton_, &QPushButton::clicked, this, &PDSection::onChevronClicked);
    connect(this->tableWidget_, &QTableWidget::cellClicked, this, &PDSection::onTableCellClicked);
    connect(this->tableWidget_, &QTableWidget::itemSelectionChanged, this, &PDSection::onTableSelectionChanged);
    connect(this->tableWidget_, &QTableWidget::customContextMenuRequested, this, &PDSection::onTableContextMenuRequested);
    connect(this->tableWidget_, &QTableWidget::cellEntered, this, &PDSection::onTableCellEntered);
    connect(this->copyAction_, &QAction::triggered, this, &PDSection::onCopyMenuItemTriggered);
    
    // Enable header click-to-toggle
    this->headerPanel_->installEventFilter(this);
    this->titleLabel_->installEventFilter(this);
    
    // Initial state
    this->Collapse();
    this->refreshChevron();
}

PDSection::~PDSection()
{
}

QString PDSection::GetSectionTitle() const
{
    return this->titleLabel_->text();
}

void PDSection::SetSectionTitle(const QString& title)
{
    this->titleLabel_->setText(title);
}

bool PDSection::IsEmpty() const
{
    return this->tableWidget_->rowCount() == 0;
}

bool PDSection::IsExpanded() const
{
    return this->isExpanded_;
}

bool PDSection::HasNoSelection() const
{
    return this->tableWidget_->selectedItems().isEmpty();
}

QRect PDSection::GetSelectedRowBounds() const
{
    if (this->HasNoSelection())
        return QRect(0, 0, 0, 0);
    
    int row = this->tableWidget_->currentRow();
    if (row < 0)
        return QRect(0, 0, 0, 0);
    
    int x = this->tableWidget_->x();
    int y = this->tableWidget_->visualItemRect(this->tableWidget_->item(row, 0)).y() + this->tableWidget_->y();
    int w = this->tableWidget_->width();
    int h = this->tableWidget_->rowHeight(row);
    
    return QRect(x, y, w, h);
}

void PDSection::SetDisableFocusEvent(bool disable)
{
    this->disableFocusEvent_ = disable;
}

void PDSection::SetShowCellToolTips(bool show)
{
    Q_UNUSED(show);
    // Qt handles tooltips per item, so we'll store this for future item creation
    // For now, we can't retroactively change existing items
}

bool PDSection::GetShowCellToolTips() const
{
    return true; // Qt default
}

void PDSection::Expand()
{
    this->toggleExpandedState(true);
}

void PDSection::Collapse()
{
    this->toggleExpandedState(false);
}

void PDSection::AddEntry(const QString& key, const QString& value, const QList<QAction*>& contextMenuItems)
{
    this->addRow(this->createKeyText(key), value, QString(), false, false, nullptr, true, contextMenuItems);
}

void PDSection::AddEntry(const QString& key, const QString& value, const QColor& fontColor, const QList<QAction*>& contextMenuItems)
{
    this->addRow(this->createKeyText(key), value, QString(), false, false, nullptr, true, contextMenuItems, fontColor);
}

void PDSection::AddEntryLink(const QString& key, const QString& value, Command* command, const QList<QAction*>& contextMenuItems)
{
    auto action = [command]() {
        if (command)
            command->Run();
    };
    this->addRow(this->createKeyText(key), value, QString(), true, false, action, true, contextMenuItems);
}

void PDSection::AddEntryLink(const QString& key, const QString& value, std::function<void()> action, const QList<QAction*>& contextMenuItems)
{
    this->addRow(this->createKeyText(key), value, QString(), true, false, action, true, contextMenuItems);
}

void PDSection::AddEntryWithNoteLink(const QString& key, const QString& value, const QString& note, 
                                     std::function<void()> action, bool enabled,
                                     const QList<QAction*>& contextMenuItems)
{
    this->addRow(this->createKeyText(key), value, note, false, true, action, enabled, contextMenuItems);
}

void PDSection::AddEntryWithNoteLink(const QString& key, const QString& value, const QString& note,
                                     std::function<void()> action, const QColor& fontColor,
                                     const QList<QAction*>& contextMenuItems)
{
    this->addRow(this->createKeyText(key), value, note, false, true, action, true, contextMenuItems, fontColor);
}

void PDSection::UpdateEntryValueWithKey(const QString& key, const QString& newValue, bool visible)
{
    for (int row = 0; row < this->tableWidget_->rowCount(); ++row)
    {
        QTableWidgetItem* keyItem = this->tableWidget_->item(row, 0);
        if (keyItem && keyItem->text().contains(key))
        {
            QTableWidgetItem* valueItem = this->tableWidget_->item(row, 1);
            if (valueItem)
            {
                valueItem->setText(newValue);
                this->tableWidget_->setRowHidden(row, !visible);
                
                if (this->tableWidget_->isRowHidden(row) != !visible)
                    this->refreshHeight();
                
                return;
            }
        }
    }
}

void PDSection::FixFirstColumnWidth(int width)
{
    this->tableWidget_->setColumnWidth(0, width);
}

void PDSection::ClearData()
{
    if (this->tableWidget_->currentRow() >= 0)
    {
        QTableWidgetItem* keyItem = this->tableWidget_->item(this->tableWidget_->currentRow(), 0);
        if (keyItem)
            this->previousSelectionKey_ = keyItem->text();
    }
    
    this->tableWidget_->clearContents();
    this->tableWidget_->setRowCount(0);
    this->rowData_.clear();
}

void PDSection::RestoreSelection()
{
    if (this->previousSelectionKey_.isEmpty())
        return;
    
    for (int row = 0; row < this->tableWidget_->rowCount(); ++row)
    {
        QTableWidgetItem* keyItem = this->tableWidget_->item(row, 0);
        if (keyItem && keyItem->text() == this->previousSelectionKey_)
        {
            this->tableWidget_->selectRow(row);
            return;
        }
    }
}

void PDSection::PauseLayout()
{
    this->inLayout_ = true;
}

void PDSection::StartLayout()
{
    this->inLayout_ = false;
    this->refreshHeight();
}

void PDSection::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    
    if (this->disableFocusEvent_)
        return;
    
    if (!this->isExpanded_)
        this->Expand();
}

bool PDSection::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == this->headerPanel_ || watched == this->titleLabel_) &&
        event->type() == QEvent::MouseButtonRelease)
    {
        if (this->isExpanded_)
            this->Collapse();
        else
        {
            this->Expand();
            this->tableWidget_->setFocus();
        }
        return true;
    }

    return QWidget::eventFilter(watched, event);
}

void PDSection::onHeaderDoubleClicked()
{
    if (this->isExpanded_)
        this->Collapse();
    else
    {
        this->Expand();
        this->tableWidget_->setFocus();
    }
}

void PDSection::onChevronClicked()
{
    if (this->isExpanded_)
        this->Collapse();
    else
    {
        this->Expand();
        this->tableWidget_->setFocus();
    }
}

void PDSection::onTableCellClicked(int row, int column)
{
    this->runCellCommandOrAction(row, column);
}

void PDSection::onTableSelectionChanged()
{
    if (this->inLayout_)
        return;
    
    emit this->ContentChangedSelection(this);
}

void PDSection::onTableContextMenuRequested(const QPoint& pos)
{
    int row = this->tableWidget_->rowAt(pos.y());
    if (row < 0)
        return;
    
    this->tableWidget_->selectRow(row);
    
    // Clear and rebuild context menu
    this->contextMenu_->clear();
    this->contextMenu_->addAction(this->copyAction_);
    
    // Add row-specific context menu items
    if (this->rowData_.contains(row) && !this->rowData_[row].contextMenuItems.isEmpty())
    {
        this->contextMenu_->addSeparator();
        for (QAction* action : this->rowData_[row].contextMenuItems)
            this->contextMenu_->addAction(action);
    }
    
    this->contextMenu_->exec(this->tableWidget_->mapToGlobal(pos));
}

void PDSection::onTableCellEntered(int row, int column)
{
    if (row < 0 || column < 0)
        return;
    
    if (this->rowData_.contains(row))
    {
        bool isLink = (column == 1 && this->rowData_[row].isValueLink) ||
                     (column == 2 && this->rowData_[row].isNoteLink);
        
        if (isLink)
            this->tableWidget_->setCursor(Qt::PointingHandCursor);
        else
            this->tableWidget_->setCursor(Qt::ArrowCursor);
    } else
    {
        this->tableWidget_->setCursor(Qt::ArrowCursor);
    }
}

void PDSection::onCopyMenuItemTriggered()
{
    int row = this->tableWidget_->currentRow();
    if (row < 0)
        return;
    
    QTableWidgetItem* valueItem = this->tableWidget_->item(row, 1);
    if (valueItem)
        QApplication::clipboard()->setText(valueItem->text());
}

QString PDSection::createKeyText(const QString& key) const
{
    if (!key.isEmpty())
        return key + ":"; // Using : as separator (Messages.GENERAL_PAGE_KVP_SEPARATOR)
    return key;
}

void PDSection::addRow(const QString& keyText, const QString& valueText, const QString& noteText,
                      bool isValueLink, bool isNoteLink, std::function<void()> linkAction,
                      bool enabled, const QList<QAction*>& contextMenuItems, const QColor& fontColor)
{
    int row = this->tableWidget_->rowCount();
    this->tableWidget_->insertRow(row);
    
    // Key column
    QTableWidgetItem* keyItem = new QTableWidgetItem(keyText);
    keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
    this->tableWidget_->setItem(row, 0, keyItem);
    
    // Value column
    QTableWidgetItem* valueItem = new QTableWidgetItem(valueText);
    valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
    if (fontColor.isValid())
    {
        valueItem->setForeground(QBrush(fontColor));
    }
    if (isValueLink)
    {
        QFont linkFont = valueItem->font();
        linkFont.setUnderline(true);
        valueItem->setFont(linkFont);
        valueItem->setForeground(QBrush(this->palette().color(QPalette::Link)));
    }
    this->tableWidget_->setItem(row, 1, valueItem);
    
    // Notes column
    QTableWidgetItem* noteItem = new QTableWidgetItem(noteText);
    noteItem->setFlags(noteItem->flags() & ~Qt::ItemIsEditable);
    if (isNoteLink && !noteText.isEmpty())
    {
        QFont linkFont = noteItem->font();
        linkFont.setUnderline(true);
        noteItem->setFont(linkFont);
        if (enabled)
            noteItem->setForeground(QBrush(this->palette().color(QPalette::Link)));
        else
            noteItem->setForeground(QBrush(this->palette().color(QPalette::Disabled, QPalette::Text)));
    }
    this->tableWidget_->setItem(row, 2, noteItem);
    
    // Store row data
    RowData data;
    data.isValueLink = isValueLink;
    data.isNoteLink = isNoteLink;
    data.linkAction = linkAction;
    data.contextMenuItems = contextMenuItems;
    this->rowData_[row] = data;
    
    // Set row enabled state
    if (!enabled)
    {
        for (int col = 0; col < 3; ++col)
        {
            QTableWidgetItem* item = this->tableWidget_->item(row, col);
            if (item)
            {
                item->setForeground(QBrush(this->palette().color(QPalette::Disabled, QPalette::Text)));
            }
        }
    }
    
    if (!this->inLayout_)
        this->refreshHeight();
}

void PDSection::toggleExpandedState(bool expand)
{
    if (this->isExpanded_ == expand)
        return;
    
    this->isExpanded_ = expand;
    this->tableWidget_->setVisible(expand);
    this->refreshHeight();
    this->refreshChevron();
    
    emit this->ExpandedChanged(this);
}

void PDSection::refreshHeight()
{
    const int headerHeight = this->headerPanel_->sizeHint().height();
    if (this->isExpanded_)
    {
        int contentHeight = 0;
        for (int row = 0; row < this->tableWidget_->rowCount(); ++row)
        {
            if (!this->tableWidget_->isRowHidden(row))
                contentHeight += this->tableWidget_->rowHeight(row);
        }
        
        // Add small padding for scrollbar if needed
        int totalHeight = headerHeight + contentHeight + 3; // 3px for borders
        this->setFixedHeight(totalHeight);
    } else
    {
        this->setFixedHeight(headerHeight + 2); // 2px for borders
    }
}

void PDSection::refreshChevron()
{
    QStyle::StandardPixmap iconType = this->isExpanded_
        ? QStyle::SP_ArrowUp
        : QStyle::SP_ArrowDown;
    this->chevronButton_->setIcon(this->style()->standardIcon(iconType));
    this->chevronButton_->setText(QString());
}

void PDSection::runCellCommandOrAction(int row, int column)
{
    if (!this->rowData_.contains(row))
        return;
    
    const RowData& data = this->rowData_[row];
    
    bool shouldRun = (column == 1 && data.isValueLink) ||
                     (column == 2 && data.isNoteLink);
    
    if (shouldRun && data.linkAction)
        data.linkAction();
}
