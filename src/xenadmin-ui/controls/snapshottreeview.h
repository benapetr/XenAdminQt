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

#ifndef SNAPSHOTTREEVIEW_H
#define SNAPSHOTTREEVIEW_H

#include <QListWidget>
#include <QListWidgetItem>
#include <QColor>
#include <QPainter>
#include <QTimer>
#include <QList>
#include <QScrollBar>

class SnapshotIcon;

/**
 * @brief Custom ListView-style widget for displaying snapshot hierarchy
 * 
 * This widget displays snapshots in a tree-like layout with connecting lines,
 * matching the C# XenAdmin SnapshotTreeView implementation. It uses custom
 * drawing to render snapshot icons with curved connecting lines showing
 * parent-child relationships.
 * 
 * Port of: xenadmin/XenAdmin/Controls/SnapshotTreeView.cs
 */
class SnapshotTreeView : public QListWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor linkLineColor READ getLinkLineColor WRITE setLinkLineColor)
    Q_PROPERTY(float linkLineWidth READ getLinkLineWidth WRITE setLinkLineWidth)
    Q_PROPERTY(int hGap READ getHGap WRITE setHGap)
    Q_PROPERTY(int vGap READ getVGap WRITE setVGap)

    public:
        explicit SnapshotTreeView(QWidget* parent = nullptr);
        ~SnapshotTreeView() override;

        // Property getters/setters
        QColor getLinkLineColor() const { return this->linkLineColor_; }
        void setLinkLineColor(const QColor& color);

        float getLinkLineWidth() const { return this->linkLineWidth_; }
        void setLinkLineWidth(float width);

        int getHGap() const { return this->hGap_; }
        void setHGap(int gap);

        int getVGap() const { return this->vGap_; }
        void setVGap(int gap);

        // Core functionality
        SnapshotIcon* AddSnapshot(SnapshotIcon* snapshot);
        void RemoveSnapshot(SnapshotIcon* snapshot);
        void Clear();

        // Spinning state for VM operations
        void ChangeVMToSpinning(bool spinning, const QString& message);
        QString GetSpinningMessage() const { return this->spinningMessage_; }
        QPixmap GetImage(int index) const;
        void UpdateIcon(SnapshotIcon* icon);
        void SetTreeMode(bool enabled);
        bool IsTreeMode() const { return this->treeMode_; }

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void scrollContentsBy(int dx, int dy) override;

    private:
        void initializeImageList();
        void performLayout();
        void positionSnapshots(SnapshotIcon* icon, int x, int y);
        void drawConnectionLines(QPainter& painter);
        void paintLine(QPainter& painter, SnapshotIcon* parent, SnapshotIcon* child, bool highlight);
        void drawDate(QPainter& painter, SnapshotIcon* icon);
        QPoint getOrigin() const;

        // Constants
        static constexpr int STRAIGHT_LINE_LENGTH = 8;

        // Member variables
        SnapshotIcon* root_;
        QColor linkLineColor_;
        float linkLineWidth_;
        int hGap_;
        int vGap_;
        QString spinningMessage_;
        QPoint origin_;
        bool isEmpty_;
        bool treeMode_;

        // Image list for icons
        QList<QPixmap> imageList_;
};

/**
 * @brief Represents a single snapshot node in the tree
 * 
 * Encapsulates snapshot rendering, icon selection, and hierarchy management.
 * Each SnapshotIcon tracks its parent and children to form the tree structure.
 * 
 * Note: This inherits from both QObject (for signals/slots) and QListWidgetItem.
 * 
 * Port of: SnapshotIcon class inside SnapshotTreeView.cs
 */
class SnapshotIcon : public QObject, public QListWidgetItem
{
    Q_OBJECT
    public:
        // Image index constants (must match C# version)
        enum ImageIndex
        {
            VMImageIndex = 0,
            Template = 1,
            CustomTemplate = 2,
            DiskSnapshot = 3,
            DiskAndMemorySnapshot = 4,
            ScheduledDiskSnapshot = 5,
            ScheduledDiskMemorySnapshot = 6,
            UnknownImage = 6,
            // Spinning frames 7-14
            SpinningFrame0 = 7,
            SpinningFrame1 = 8,
            SpinningFrame2 = 9,
            SpinningFrame3 = 10,
            SpinningFrame4 = 11,
            SpinningFrame5 = 12,
            SpinningFrame6 = 13,
            SpinningFrame7 = 14
        };

        SnapshotIcon(const QString& name, const QString& creationTime, 
                     SnapshotIcon* parent, SnapshotTreeView* treeView, int imageIndex);
        ~SnapshotIcon();

        // Property getters/setters
        QString GetLabelCreationTime() const { return this->creationTime_; }
        QString GetLabelName() const { return this->name_; }
        SnapshotIcon* GetParent() const { return this->parent_; }
        void SetParent(SnapshotIcon* parent) { this->parent_ = parent; }
        QList<SnapshotIcon*>& GetChildren() { return this->children_; }
        const QList<SnapshotIcon*>& GetChildren() const { return this->children_; }
        
        bool IsSelectable() const;
        bool IsInVMBranch() const { return this->isInVMBranch_; }
        void SetIsInVMBranch(bool value) { this->isInVMBranch_ = value; }

        int GetImageIndex() const { return this->imageIndex_; }
        void SetImageIndex(int index) { this->imageIndex_ = index; }

        QSize GetDefaultSize() const { return this->defaultSize_; }
        QPoint GetPosition() const { return this->position_; }
        void SetPosition(const QPoint& pos) { this->position_ = pos; }

        // Hierarchy management
        void AddChild(SnapshotIcon* child);
        void Remove();

        // Layout/dimension calculations (cached)
        int GetSubtreeWidth();
        int GetSubtreeHeight();
        int GetSubtreeWeight();
        int GetCentreHeight();

        // Invalidation for layout recalculation
        void Invalidate();
        void InvalidateAll();

        // Spinning animation for VM operations
        void ChangeSpinningIcon(bool enabled, const QString& message);

    private slots:
        void onSpinningTimerTick();

    private:
        QString name_;
        QString creationTime_;
        SnapshotIcon* parent_;
        SnapshotTreeView* treeView_;
        QList<SnapshotIcon*> children_;
        int imageIndex_;
        QSize defaultSize_;
        QPoint position_;
        bool isInVMBranch_;

        // Cached dimensions (0 means not calculated)
        int subtreeWidth_;
        int subtreeHeight_;
        int subtreeWeight_;
        int centreHeight_;

        // Spinning animation
        QTimer* spinningTimer_;
        int currentSpinningFrame_;
};

#endif // SNAPSHOTTREEVIEW_H
