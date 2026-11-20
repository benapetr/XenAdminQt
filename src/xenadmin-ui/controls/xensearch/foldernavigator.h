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

// foldernavigator.h - Breadcrumb navigation widget for folder paths
// C# equivalent: xenadmin/XenAdmin/Controls/XenSearch/FolderNavigator.cs

#ifndef FOLDERNAVIGATOR_H
#define FOLDERNAVIGATOR_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QList>
#include <QRect>
#include <QPixmap>

/*!
 * \brief Breadcrumb navigation widget for folder paths
 *
 * C# equivalent: XenAdmin.Controls.XenSearch.FolderNavigator
 * (xenadmin/XenAdmin/Controls/XenSearch/FolderNavigator.cs)
 *
 * Displays a folder path as a clickable breadcrumb trail, where each
 * component can be clicked to navigate to that folder level.
 *
 * Example: "Pool > Folder1 > Folder2"
 *
 * The widget auto-hides when the folder path is empty.
 *
 * Usage example:
 * \code
 * FolderNavigator* nav = new FolderNavigator(this);
 * nav->SetFolder("OpaqueRef:pool/Folder1/Folder2");
 * \endcode
 */
class FolderNavigator : public QWidget
{
    Q_OBJECT

    public:
        /*!
         * \brief Constructor
         * \param parent Parent widget
         *
         * C# equivalent: FolderNavigator() constructor (FolderNavigator.cs line 39)
         */
        explicit FolderNavigator(QWidget* parent = nullptr);

        /*!
         * \brief Destructor
         */
        ~FolderNavigator() override;

        /*!
         * \brief Set the folder path to display
         * \param folder Folder path (e.g. "OpaqueRef:pool/Folder1/Folder2")
         *
         * C# equivalent: Folder property setter (FolderNavigator.cs lines 44-57)
         *
         * If the path is empty, the widget is hidden. Otherwise, the path
         * is parsed and displayed as a clickable breadcrumb trail.
         */
        void SetFolder(const QString& folder);

        /*!
         * \brief Get the current folder path
         * \return Current folder path
         */
        QString GetFolder() const { return this->folder_; }

    signals:
        /*!
         * \brief Emitted when user clicks a folder component
         * \param folderPath Path to the clicked folder level
         *
         * C# equivalent: Calls Program.MainWindow.SearchForFolder()
         * (FolderListItem.cs line 317)
         */
        void folderClicked(const QString& folderPath);

    protected:
        /*!
         * \brief Paint the breadcrumb trail
         * \param event Paint event
         *
         * C# equivalent: OnPaint() override (FolderNavigator.cs lines 59-64)
         */
        void paintEvent(QPaintEvent* event) override;

        /*!
         * \brief Handle mouse movement for hover effects
         * \param event Mouse event
         *
         * C# equivalent: OnMouseMove() override (FolderNavigator.cs lines 66-71)
         */
        void mouseMoveEvent(QMouseEvent* event) override;

        /*!
         * \brief Handle mouse leaving for hover cleanup
         * \param event Leave event
         *
         * C# equivalent: OnMouseLeave() override (FolderNavigator.cs lines 73-78)
         */
        void leaveEvent(QEvent* event) override;

        /*!
         * \brief Handle mouse clicks on breadcrumb components
         * \param event Mouse event
         *
         * C# equivalent: OnMouseClick() override (FolderNavigator.cs lines 80-85)
         */
        void mousePressEvent(QMouseEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    public:
        //! The whole path (C# FolderListItem.PreferredSize)
        QSize sizeHint() const override;
        //! Narrower widths truncate the path
        QSize minimumSizeHint() const override;

    private:
        //! One drawn piece of the path: a component, or the ellipsis replacing leading ones
        struct Part
        {
            QString text;
            QRect rect;
            bool separatorBefore = false;
            QString searchPath;  //!< folder searched for on click; empty if not clickable
        };

        /*!
         * \brief Lays the path out in the widget's width
         *
         * C# FolderListItem.CalcSizeAndTrunc and DrawSelf: when the path does not fit, leading
         * components are replaced by an ellipsis, then the last component is shortened.
         */
        void calculateLayout();

        //! Width of the whole path untruncated (C# FolderListItem.PreferredSize)
        int fullWidth() const;

        int partAt(const QPoint& pos) const;

        QString folder_;
        QStringList pathComponents_;
        QList<Part> parts_;
        int hoveredPart_ = -1;
        QPixmap separator_;

        static const int INNER_PADDING = 9;     ///< Padding between components
        static const int IMAGE_OFFSET = 4;      ///< Vertical offset for separator icon
        static const int LEFT_MARGIN = 3;       ///< C# DrawSelf: compensates for unpadded text
};

#endif // FOLDERNAVIGATOR_H
