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

#include "snapshottreeview.h"
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QMouseEvent>
#include <QScrollBar>
#include <QPalette>
#include <QApplication>
#include <QDebug>
#include <cmath>

//==============================================================================
// SnapshotTreeView Implementation
//==============================================================================

SnapshotTreeView::SnapshotTreeView(QWidget* parent)
    : QListWidget(parent),
      root_(nullptr),
      linkLineColor_(QColor(169, 169, 169)), // SystemColors.ControlDark equivalent
      linkLineWidth_(2.0f),
      hGap_(50),
      vGap_(20),
      isEmpty_(false),
      treeMode_(true)
{
    // Configure list widget for custom drawing
    this->setViewMode(QListWidget::IconMode);
    this->setIconSize(QSize(32, 32));
    this->setSpacing(0);
    this->setResizeMode(QListWidget::Fixed);
    this->setMovement(QListWidget::Free);
    this->setSelectionMode(QAbstractItemView::SingleSelection);
    this->setSelectionRectVisible(false);
    this->setUniformItemSizes(true);
    this->setDragDropMode(QAbstractItemView::NoDragDrop);
    this->setWrapping(false);
    
    // Enable custom rendering
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    this->initializeImageList();
}

SnapshotTreeView::~SnapshotTreeView()
{
    this->Clear();
}

QPixmap SnapshotTreeView::GetImage(int index) const
{
    if (index < 0 || index >= this->imageList_.size())
        return QPixmap();

    return this->imageList_.at(index);
}

void SnapshotTreeView::UpdateIcon(SnapshotIcon* icon)
{
    if (!icon)
        return;

    QPixmap pixmap = this->GetImage(icon->GetImageIndex());
    if (!pixmap.isNull())
        icon->setIcon(QIcon(pixmap));
}

void SnapshotTreeView::SetTreeMode(bool enabled)
{
    if (this->treeMode_ == enabled)
        return;

    this->treeMode_ = enabled;
    if (this->treeMode_)
    {
        this->setViewMode(QListWidget::IconMode);
        this->setResizeMode(QListWidget::Fixed);
        this->setMovement(QListWidget::Free);
    }
    else
    {
        this->setViewMode(QListWidget::ListMode);
        this->setResizeMode(QListWidget::Adjust);
        this->setMovement(QListWidget::Static);
    }

    this->doItemsLayout();
    if (this->treeMode_)
        this->performLayout();
    this->update();
}
void SnapshotTreeView::setLinkLineColor(const QColor& color)
{
    this->linkLineColor_ = color;
    this->update();
}

void SnapshotTreeView::setLinkLineWidth(float width)
{
    this->linkLineWidth_ = width;
    this->update();
}

void SnapshotTreeView::setHGap(int gap)
{
    if (gap < 4 * STRAIGHT_LINE_LENGTH)
        gap = 4 * STRAIGHT_LINE_LENGTH;
    this->hGap_ = gap;
    this->performLayout();
}

void SnapshotTreeView::setVGap(int gap)
{
    if (gap < 0)
        gap = 0;
    this->vGap_ = gap;
    this->performLayout();
}

void SnapshotTreeView::initializeImageList()
{
    // Initialize image list with icons matching C# version
    // Matches: xenadmin/XenAdmin/Controls/SnapshotTreeView.cs InitializeComponent()
    
    QStringList iconPaths = {
        ":/tree-icons/vm_highlight_32.png",                      // 0: VMImageIndex
        ":/tree-icons/vm_template_32.png",                       // 1: Template
        ":/tree-icons/vm_template_32.png",                       // 2: CustomTemplate (same as Template)
        ":/tree-icons/snapshot_disk_32.png",                     // 3: DiskSnapshot
        ":/tree-icons/snapshot_disk_memory_32.png",              // 4: DiskAndMemorySnapshot
        ":/tree-icons/snapshot_scheduled_disk_32.png",           // 5: ScheduledDiskSnapshot
        ":/tree-icons/snapshot_scheduled_disk_memory_32.png",    // 6: ScheduledDiskMemorySnapshot
        ":/tree-icons/spinning_frame_0.png",                     // 7: SpinningFrame0
        ":/tree-icons/spinning_frame_1.png",                     // 8: SpinningFrame1
        ":/tree-icons/spinning_frame_2.png",                     // 9: SpinningFrame2
        ":/tree-icons/spinning_frame_3.png",                     // 10: SpinningFrame3
        ":/tree-icons/spinning_frame_4.png",                     // 11: SpinningFrame4
        ":/tree-icons/spinning_frame_5.png",                     // 12: SpinningFrame5
        ":/tree-icons/spinning_frame_6.png",                     // 13: SpinningFrame6
        ":/tree-icons/spinning_frame_7.png"                      // 14: SpinningFrame7
    };

    for (const QString& iconPath : iconPaths)
    {
        QPixmap pixmap(iconPath);
        if (pixmap.isNull())
        {
            qWarning() << "SnapshotTreeView: Failed to load icon:" << iconPath;
            // Create fallback colored pixmap
            pixmap = QPixmap(32, 32);
            pixmap.fill(QColor(200, 200, 200)); // Gray fallback
        }
        this->imageList_.append(pixmap);
    }
}

SnapshotIcon* SnapshotTreeView::AddSnapshot(SnapshotIcon* snapshot)
{
    if (!snapshot)
        return nullptr;

    if (snapshot->GetParent() != nullptr)
    {
        snapshot->GetParent()->AddChild(snapshot);
        snapshot->GetParent()->Invalidate();
    }
    else if (this->root_ != nullptr)
    {
        qWarning() << "SnapshotTreeView::AddSnapshot: Attempting to add a new root when one already exists!";
        return nullptr;
    }
    else
    {
        this->root_ = snapshot;
    }

    // If this is a VM node, sort parents to make the path to VM the first branch
    if (snapshot->GetImageIndex() == SnapshotIcon::VMImageIndex)
    {
        SnapshotIcon* current = snapshot;
        while (current->GetParent() != nullptr)
        {
            QList<SnapshotIcon*>& siblings = current->GetParent()->GetChildren();
            if (siblings.count() > 1)
            {
                int indexCurrent = siblings.indexOf(current);
                if (indexCurrent > 0)
                {
                    // Swap to make current the first child
                    SnapshotIcon* temp = siblings[0];
                    siblings[0] = current;
                    siblings[indexCurrent] = temp;
                }
            }
            current->SetIsInVMBranch(true);
            current = current->GetParent();
        }
    }

    this->addItem(snapshot);
    this->isEmpty_ = false;
    this->performLayout();

    return snapshot;
}

void SnapshotTreeView::RemoveSnapshot(SnapshotIcon* snapshot)
{
    if (!snapshot)
        return;

    if (snapshot->GetParent() != nullptr)
    {
        QList<SnapshotIcon*>& siblings = snapshot->GetParent()->GetChildren();
        int pos = siblings.indexOf(snapshot);
        siblings.removeOne(snapshot);

        // Add our children in our place
        for (SnapshotIcon* child : snapshot->GetChildren())
        {
            siblings.insert(pos++, child);
            child->SetParent(snapshot->GetParent());
        }
        snapshot->GetParent()->Invalidate();
        snapshot->SetParent(nullptr);
    }
    else
    {
        this->root_ = nullptr;
    }

    this->takeItem(this->row(snapshot));
    this->performLayout();
}

void SnapshotTreeView::Clear()
{
    QListWidget::clear();
    this->root_ = nullptr;
    this->isEmpty_ = true;
    this->update();
}

void SnapshotTreeView::ChangeVMToSpinning(bool spinning, const QString& message)
{
    this->spinningMessage_ = message;
    
    for (int i = 0; i < this->count(); ++i)
    {
        QListWidgetItem* item = this->item(i);
        SnapshotIcon* snapshotIcon = dynamic_cast<SnapshotIcon*>(item);
        if (snapshotIcon && 
            (snapshotIcon->GetImageIndex() == SnapshotIcon::VMImageIndex || 
             snapshotIcon->GetImageIndex() > SnapshotIcon::UnknownImage))
        {
            snapshotIcon->ChangeSpinningIcon(spinning, message);
            return;
        }
    }
}

void SnapshotTreeView::performLayout()
{
    if (!this->treeMode_ || !this->root_ || !this->parentWidget())
        return;

    this->origin_ = this->getOrigin();
    this->root_->InvalidateAll();

    // Center the tree in the view
    int x = qMax(this->hGap_, this->width() / 2 - this->root_->GetSubtreeWidth() / 2);
    int y = qMax(this->vGap_, this->height() / 2 - this->root_->GetSubtreeHeight() / 2);

    this->positionSnapshots(this->root_, x, y);
    this->update();
}

void SnapshotTreeView::positionSnapshots(SnapshotIcon* icon, int x, int y)
{
    if (!icon)
        return;

    QSize iconSize = icon->GetDefaultSize();

    // Position the current icon
    QPoint newPoint(x, y + icon->GetCentreHeight() - iconSize.height() / 2);
    icon->SetPosition(QPoint(newPoint.x() + this->origin_.x(), newPoint.y() + this->origin_.y()));
    const QModelIndex index = this->indexFromItem(icon);
    if (index.isValid())
        this->setPositionForIndex(icon->GetPosition(), index);

    // Position children to the right
    x += iconSize.width() + this->hGap_;
    for (SnapshotIcon* child : icon->GetChildren())
    {
        this->positionSnapshots(child, x, y);
        y += child->GetSubtreeHeight();
    }
}

QPoint SnapshotTreeView::getOrigin() const
{
    // Get scroll offset
    int xOffset = this->horizontalScrollBar() ? this->horizontalScrollBar()->value() : 0;
    int yOffset = this->verticalScrollBar() ? this->verticalScrollBar()->value() : 0;
    return QPoint(-xOffset, -yOffset);
}

void SnapshotTreeView::paintEvent(QPaintEvent* event)
{
    // First, let Qt draw the items
    QListWidget::paintEvent(event);

    if (!this->treeMode_)
        return;

    QPainter painter(this->viewport());
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw empty message if no snapshots
    if (this->isEmpty_ || this->count() == 0)
    {
        QString text = tr("There are no snapshots for this VM.");
        QFont font = this->font();
        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(QRect(0, 0, 275, 1000), Qt::TextWordWrap, text);
        
        int x = this->width() / 2 - textRect.width() / 2;
        int y = this->height() / 2 - textRect.height() / 2;
        
        painter.fillRect(QRect(x, y, textRect.width(), textRect.height()), this->palette().base());
        painter.setPen(this->palette().text().color());
        painter.drawText(QRect(x, y, textRect.width(), textRect.height()), Qt::TextWordWrap, text);
        return;
    }

    // Draw connection lines
    this->drawConnectionLines(painter);
}

void SnapshotTreeView::drawConnectionLines(QPainter& painter)
{
    if (!this->root_)
        return;

    // Recursively draw lines for all nodes
    QList<SnapshotIcon*> queue;
    queue.append(this->root_);

    while (!queue.isEmpty())
    {
        SnapshotIcon* icon = queue.takeFirst();
        
        // Draw date/time label below icon
        this->drawDate(painter, icon);

        // Draw lines to children
        for (SnapshotIcon* child : icon->GetChildren())
        {
            this->paintLine(painter, icon, child, child->IsInVMBranch());
            queue.append(child);
        }
    }
}

void SnapshotTreeView::paintLine(QPainter& painter, SnapshotIcon* parent, SnapshotIcon* child, bool highlight)
{
    if (!parent || !child)
        return;

    QPoint parentPos = parent->GetPosition();
    QPoint childPos = child->GetPosition();
    QSize parentSize = parent->GetDefaultSize();
    QSize childSize = child->GetDefaultSize();

    // Calculate line endpoints
    int left = parentPos.x() + parentSize.width() + 6;
    int right = childPos.x();
    int mid = (left + right) / 2;
    
    QPoint start(left, parentPos.y() + parentSize.height() / 2);
    QPoint end(right, childPos.y() + childSize.height() / 2);
    
    QPoint curveStart = start + QPoint(STRAIGHT_LINE_LENGTH, 0);
    QPoint curveEnd = end - QPoint(STRAIGHT_LINE_LENGTH, 0);
    QPoint control1(mid + STRAIGHT_LINE_LENGTH, start.y());
    QPoint control2(mid - STRAIGHT_LINE_LENGTH, end.y());

    // Set pen style
    QColor lineColor = highlight ? QColor(34, 139, 34) : this->linkLineColor_; // ForestGreen if highlight
    float lineWidth = highlight ? 2.5f : this->linkLineWidth_;
    
    QPen pen(lineColor, lineWidth);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    // Draw the path
    QPainterPath path;
    path.moveTo(start);
    path.lineTo(curveStart);
    path.cubicTo(control1, control2, curveEnd);
    path.lineTo(end);

    painter.drawPath(path);

    // Draw arrow at the end
    const int arrowSize = 4;
    QPointF arrowP1 = end + QPointF(-arrowSize, -arrowSize);
    QPointF arrowP2 = end + QPointF(-arrowSize, arrowSize);
    
    QPainterPath arrow;
    arrow.moveTo(end);
    arrow.lineTo(arrowP1);
    arrow.moveTo(end);
    arrow.lineTo(arrowP2);
    
    painter.drawPath(arrow);
}

void SnapshotTreeView::drawDate(QPainter& painter, SnapshotIcon* icon)
{
    if (!icon)
        return;

    QString timeText = icon->GetLabelCreationTime();
    if (timeText.isEmpty())
        return;

    QPoint pos = icon->GetPosition();
    QSize size = icon->GetDefaultSize();

    // Draw time below icon
    QFont font = this->font();
    font.setPointSize(font.pointSize() - 1);
    painter.setFont(font);

    QFontMetrics fm(font);
    QRect timeRect(pos.x(), pos.y() + size.height(), size.width(), fm.height() * 2);
    
    painter.setPen(this->palette().text().color());
    painter.drawText(timeRect, Qt::AlignHCenter | Qt::TextWordWrap, timeText);
}

void SnapshotTreeView::resizeEvent(QResizeEvent* event)
{
    QListWidget::resizeEvent(event);
    this->performLayout();
}

void SnapshotTreeView::mousePressEvent(QMouseEvent* event)
{
    QListWidgetItem* item = this->itemAt(event->pos());
    
    // Adjust hit testing like C# version (check slightly above click point)
    if (!item)
    {
        QPoint adjustedPos = event->pos() - QPoint(0, 23);
        item = this->itemAt(adjustedPos);
        if (item)
        {
            this->setCurrentItem(item);
            return;
        }
    }

    QListWidget::mousePressEvent(event);
}

void SnapshotTreeView::scrollContentsBy(int dx, int dy)
{
    QListWidget::scrollContentsBy(dx, dy);
    this->update(); // Redraw connection lines after scrolling
}

//==============================================================================
// SnapshotIcon Implementation
//==============================================================================

SnapshotIcon::SnapshotIcon(const QString& name, const QString& creationTime, 
                           SnapshotIcon* parent, SnapshotTreeView* treeView, int imageIndex)
    : QObject(),
      QListWidgetItem(),
      name_(name.length() > 35 ? name.left(32) + "..." : name),
      creationTime_(creationTime),
      parent_(parent),
      treeView_(treeView),
      imageIndex_(imageIndex),
      defaultSize_(70, 64),
      isInVMBranch_(false),
      subtreeWidth_(0),
      subtreeHeight_(0),
      subtreeWeight_(0),
      centreHeight_(0),
      spinningTimer_(nullptr),
      currentSpinningFrame_(7)
{
    this->setText(this->name_);
    this->setToolTip(QString("%1 %2").arg(name, creationTime));
    this->setSizeHint(this->defaultSize_);
    if (this->treeView_)
        this->treeView_->UpdateIcon(this);

    // Set up spinning animation for VM nodes
    if (imageIndex == VMImageIndex)
    {
        this->spinningTimer_ = new QTimer(this);
        this->spinningTimer_->setInterval(150);
        connect(this->spinningTimer_, &QTimer::timeout, 
                this, &SnapshotIcon::onSpinningTimerTick);
    }
}

SnapshotIcon::~SnapshotIcon()
{
    if (this->spinningTimer_)
    {
        this->spinningTimer_->stop();
        delete this->spinningTimer_;
    }

    // Don't delete children - they're managed by the tree view
}

bool SnapshotIcon::IsSelectable() const
{
    // Only actual snapshots are selectable (not VM or template nodes)
    return this->imageIndex_ == DiskSnapshot || 
           this->imageIndex_ == DiskAndMemorySnapshot ||
           this->imageIndex_ == ScheduledDiskSnapshot ||
           this->imageIndex_ == ScheduledDiskMemorySnapshot;
}

void SnapshotIcon::AddChild(SnapshotIcon* child)
{
    if (!child)
        return;

    this->children_.append(child);
    this->Invalidate();
}

void SnapshotIcon::Remove()
{
    if (this->treeView_)
    {
        this->treeView_->RemoveSnapshot(this);
    }
}

void SnapshotIcon::Invalidate()
{
    // Reset cached dimensions to force recalculation
    this->subtreeWidth_ = 0;
    this->subtreeHeight_ = 0;
    this->subtreeWeight_ = 0;
    this->centreHeight_ = 0;

    if (this->parent_)
        this->parent_->Invalidate();
}

void SnapshotIcon::InvalidateAll()
{
    this->subtreeWidth_ = 0;
    this->subtreeHeight_ = 0;
    this->subtreeWeight_ = 0;
    this->centreHeight_ = 0;

    for (SnapshotIcon* child : this->children_)
    {
        child->InvalidateAll();
    }
}

int SnapshotIcon::GetSubtreeWidth()
{
    if (this->subtreeWidth_ == 0)
    {
        int currentWidth = this->defaultSize_.width() + this->treeView_->getHGap();
        
        if (!this->children_.isEmpty())
        {
            int maxWidth = 0;
            for (SnapshotIcon* child : this->children_)
            {
                int childSubtree = child->GetSubtreeWidth();
                if (currentWidth + childSubtree > maxWidth)
                    maxWidth = currentWidth + childSubtree;
            }
            if (maxWidth > currentWidth)
                this->subtreeWidth_ = maxWidth;
            else
                this->subtreeWidth_ = currentWidth;
        }
        else
        {
            this->subtreeWidth_ = currentWidth;
        }
    }
    return this->subtreeWidth_;
}

int SnapshotIcon::GetSubtreeHeight()
{
    if (this->subtreeHeight_ == 0)
    {
        this->subtreeHeight_ = this->defaultSize_.height() + this->treeView_->getVGap();
        
        if (!this->children_.isEmpty())
        {
            int height = 0;
            for (SnapshotIcon* child : this->children_)
            {
                height += child->GetSubtreeHeight();
            }
            if (height > this->subtreeHeight_)
                this->subtreeHeight_ = height;
        }
    }
    return this->subtreeHeight_;
}

int SnapshotIcon::GetSubtreeWeight()
{
    if (this->subtreeWeight_ == 0)
    {
        int weight = 1; // this node
        for (SnapshotIcon* child : this->children_)
        {
            weight += child->GetSubtreeWeight();
        }
        this->subtreeWeight_ = weight;
    }
    return this->subtreeWeight_;
}

int SnapshotIcon::GetCentreHeight()
{
    if (this->centreHeight_ == 0)
    {
        int top = 0;
        int totalWeight = 0;
        int weightedCentre = 0;

        for (SnapshotIcon* child : this->children_)
        {
            int iconWeight = child->GetSubtreeWeight();
            totalWeight += iconWeight;
            weightedCentre += iconWeight * (top + child->GetCentreHeight());
            top += child->GetSubtreeHeight();
        }

        if (totalWeight > 0)
            this->centreHeight_ = weightedCentre / totalWeight;
        else
            this->centreHeight_ = (top + this->GetSubtreeHeight()) / 2;
    }
    return this->centreHeight_;
}

void SnapshotIcon::ChangeSpinningIcon(bool enabled, const QString& message)
{
    if (this->imageIndex_ > UnknownImage || this->imageIndex_ == VMImageIndex)
    {
        this->imageIndex_ = enabled ? SpinningFrame0 : VMImageIndex;
        this->setText(enabled ? message : "Now");
        if (this->treeView_)
            this->treeView_->UpdateIcon(this);

        if (enabled && this->spinningTimer_)
        {
            this->currentSpinningFrame_ = SpinningFrame0;
            this->spinningTimer_->start();
        }
        else if (this->spinningTimer_)
        {
            this->spinningTimer_->stop();
        }

        // Update display
        if (this->treeView_)
            this->treeView_->update();
    }
}

void SnapshotIcon::onSpinningTimerTick()
{
    if (this->currentSpinningFrame_ <= SpinningFrame7)
    {
        this->imageIndex_ = this->currentSpinningFrame_++;
    }
    else
    {
        this->currentSpinningFrame_ = SpinningFrame0;
        this->imageIndex_ = this->currentSpinningFrame_;
    }

    // Update display
    if (this->treeView_)
    {
        this->treeView_->UpdateIcon(this);
        this->treeView_->update();
    }
}
