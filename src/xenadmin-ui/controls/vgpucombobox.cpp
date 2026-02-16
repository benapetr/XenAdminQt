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

#include "vgpucombobox.h"

#include <QAbstractItemView>
#include <QPainter>
#include <QStandardItemModel>
#include <QStyledItemDelegate>

namespace
{
    constexpr int ROLE_TUPLE = Qt::UserRole;
    constexpr int ROLE_HEADER = Qt::UserRole + 1;
    constexpr int ROLE_SUBITEM = Qt::UserRole + 2;

    class VgpuComboDelegate : public QStyledItemDelegate
    {
        public:
            explicit VgpuComboDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent)
            {
            }

            void paint(QPainter* painter, const QStyleOptionViewItem& option,
                       const QModelIndex& index) const override
            {
                QStyleOptionViewItem opt(option);
                this->initStyleOption(&opt, index);

                const bool isHeader = index.data(ROLE_HEADER).toBool();
                const bool isSubitem = index.data(ROLE_SUBITEM).toBool();

                if (isHeader)
                {
                    opt.font.setBold(true);
                }

                if (isSubitem)
                {
                    opt.text = QStringLiteral("    ") + opt.text;
                }

                QStyledItemDelegate::paint(painter, opt, index);
            }
    };
} // namespace

VgpuComboBox::VgpuComboBox(QWidget* parent) : QComboBox(parent)
{
    this->setInsertPolicy(QComboBox::NoInsert);
    this->setEditable(false);
    this->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    this->setItemDelegate(new VgpuComboDelegate(this));
}

void VgpuComboBox::ClearTuples()
{
    this->clear();
}

void VgpuComboBox::AddTuple(const GpuTuple& tuple)
{
    this->addItem(tuple.displayName, QVariant::fromValue(tuple));

    QStandardItemModel* standardModel = qobject_cast<QStandardItemModel*>(this->model());
    if (!standardModel)
        return;

    QStandardItem* item = standardModel->item(this->count() - 1);
    if (!item)
        return;

    item->setData(tuple.isGpuHeaderItem, ROLE_HEADER);
    item->setData(tuple.isVgpuSubitem, ROLE_SUBITEM);
    item->setEnabled(!tuple.isGpuHeaderItem && tuple.enabled);
}

GpuTuple VgpuComboBox::CurrentTuple() const
{
    const QVariant value = this->currentData(ROLE_TUPLE);
    return value.value<GpuTuple>();
}

bool VgpuComboBox::SetCurrentTuple(const GpuTuple& tuple)
{
    for (int i = 0; i < this->count(); ++i)
    {
        const QVariant value = this->itemData(i, ROLE_TUPLE);
        if (value.isValid() && value.value<GpuTuple>() == tuple)
        {
            this->setCurrentIndex(i);
            return true;
        }
    }

    return false;
}

void VgpuComboBox::showPopup()
{
    if (this->count() > 0)
    {
        QStandardItemModel* standardModel = qobject_cast<QStandardItemModel*>(this->model());
        if (standardModel)
        {
            const int current = this->currentIndex();
            if (current < 0 || !standardModel->item(current)->isEnabled())
            {
                for (int i = 0; i < this->count(); ++i)
                {
                    QStandardItem* item = standardModel->item(i);
                    if (item && item->isEnabled())
                    {
                        this->setCurrentIndex(i);
                        break;
                    }
                }
            }
        }
    }

    QComboBox::showPopup();
}
