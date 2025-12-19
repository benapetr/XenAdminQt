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

#include "notificationsview.h"
#include "ui_notificationsview.h"

NotificationsView::NotificationsView(QWidget* parent)
    : QWidget(parent), ui(new Ui::NotificationsView), m_alertsCount(0), m_eventsCount(0), m_alertsItem(nullptr), m_eventsItem(nullptr)
{
    ui->setupUi(this);

    // Set up custom delegate
    m_delegate = new NotificationsSubModeItemDelegate(this);
    ui->subModeList->setItemDelegate(m_delegate);

    // Initialize list items
    initializeItems();

    // Connect item click signal
    connect(ui->subModeList, &QListWidget::itemClicked,
            this, &NotificationsView::onItemClicked);

    // Select Alerts by default
    ui->subModeList->setCurrentRow(0);
}

NotificationsView::~NotificationsView()
{
    delete ui;
}

void NotificationsView::initializeItems()
{
    // Set list to flow top-down and not center items
    ui->subModeList->setFlow(QListView::TopToBottom);
    ui->subModeList->setLayoutMode(QListView::SinglePass);
    ui->subModeList->setResizeMode(QListView::Fixed);
    ui->subModeList->setSpacing(0);
    ui->subModeList->setUniformItemSizes(true);

    // Add Alerts item
    m_alertsItem = new QListWidgetItem();
    NotificationsSubModeItemData alertsData;
    alertsData.subMode = NavigationPane::Alerts;
    alertsData.unreadEntries = 0;
    m_alertsItem->setData(NotificationsSubModeRole, QVariant::fromValue(alertsData));
    ui->subModeList->addItem(m_alertsItem);

    // Add Events item
    m_eventsItem = new QListWidgetItem();
    NotificationsSubModeItemData eventsData;
    eventsData.subMode = NavigationPane::Events;
    eventsData.unreadEntries = 0;
    m_eventsItem->setData(NotificationsSubModeRole, QVariant::fromValue(eventsData));
    ui->subModeList->addItem(m_eventsItem);
}

void NotificationsView::onItemClicked(QListWidgetItem* item)
{
    if (!item)
        return;

    QVariant data = item->data(NotificationsSubModeRole);
    if (!data.canConvert<NotificationsSubModeItemData>())
        return;

    NotificationsSubModeItemData itemData = data.value<NotificationsSubModeItemData>();
    emit notificationsSubModeChanged(itemData.subMode);
}

QListWidgetItem* NotificationsView::getItemForSubMode(NavigationPane::NotificationsSubMode subMode) const
{
    switch (subMode)
    {
    case NavigationPane::Alerts:
        return m_alertsItem;
    case NavigationPane::Events:
        return m_eventsItem;
    default:
        return nullptr;
    }
}

void NotificationsView::selectNotificationsSubMode(NavigationPane::NotificationsSubMode subMode)
{
    QListWidgetItem* item = getItemForSubMode(subMode);
    if (item)
    {
        ui->subModeList->setCurrentItem(item);
        
        // Emit signal manually since setCurrentItem doesn't trigger itemClicked
        // This ensures the MainWindow receives the notification when switching programmatically
        emit notificationsSubModeChanged(subMode);
    }
}

void NotificationsView::updateEntries(NavigationPane::NotificationsSubMode mode, int entries)
{
    QListWidgetItem* item = getItemForSubMode(mode);
    if (!item)
        return;

    // Update stored count
    if (mode == NavigationPane::Alerts)
        m_alertsCount = entries;
    else if (mode == NavigationPane::Events)
        m_eventsCount = entries;

    // Update item data
    QVariant data = item->data(NotificationsSubModeRole);
    if (!data.canConvert<NotificationsSubModeItemData>())
        return;

    NotificationsSubModeItemData itemData = data.value<NotificationsSubModeItemData>();
    itemData.unreadEntries = entries;
    item->setData(NotificationsSubModeRole, QVariant::fromValue(itemData));

    // Force repaint - Qt5 compatibility: indexFromItem is protected in Qt5
    // Use row() to get index instead
    ui->subModeList->update(ui->subModeList->model()->index(ui->subModeList->row(item), 0));
}

int NotificationsView::getTotalEntries() const
{
    return m_alertsCount + m_eventsCount;
}
