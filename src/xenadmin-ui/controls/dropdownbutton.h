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

// dropdownbutton.h - Button with dropdown menu indicator
// C# equivalent: xenadmin/XenAdmin/Controls/DropDownButton.cs

#ifndef DROPDOWNBUTTON_H
#define DROPDOWNBUTTON_H

#include <QPushButton>
#include <QMenu>

/*!
 * \brief Button with dropdown menu indicator
 *
 * C# equivalent: XenAdmin.Controls.DropDownButton
 * (xenadmin/XenAdmin/Controls/DropDownButton.cs)
 *
 * A button that displays a down-arrow triangle on the right side,
 * indicating that clicking it will show a dropdown menu.
 *
 * Usage example:
 * \code
 * DropDownButton* btn = new DropDownButton("Choose Columns", this);
 * QMenu* menu = new QMenu(this);
 * menu->addAction("Column 1");
 * menu->addAction("Column 2");
 * btn->setMenu(menu);
 * \endcode
 */
class DropDownButton : public QPushButton
{
    Q_OBJECT

    public:
        /*!
         * \brief Constructor
         * \param text Button text
         * \param parent Parent widget
         *
         * C# equivalent: DropDownButton() constructor (DropDownButton.cs line 42)
         */
        explicit DropDownButton(const QString& text = QString(), QWidget* parent = nullptr);

        /*!
         * \brief Constructor
         * \param parent Parent widget
         */
        explicit DropDownButton(QWidget* parent = nullptr);

        /*!
         * \brief Destructor
         */
        ~DropDownButton() override;

        /*!
         * \brief Set the dropdown menu
         * \param menu Menu to show when button is clicked
         *
         * C# equivalent: ContextMenuStrip property
         *
         * The button takes ownership of the menu and will show it
         * below the button when clicked.
         */
        void SetMenu(QMenu* menu);

        /*!
         * \brief Get the dropdown GetMenu
         * \return Current GetMenu or nullptr
         */
        QMenu* GetMenu() const { return this->m_menu; }

    protected:
        /*!
         * \brief Handle button click to show menu
         * \param event Mouse event
         *
         * C# equivalent: OnClick override (DropDownButton.cs lines 95-104)
         */
        void mousePressEvent(QMouseEvent* event) override;

        /*!
         * \brief Paint the button with dropdown triangle
         * \param event Paint event
         *
         * Draws the down-arrow triangle on the right side of the button.
         */
        void paintEvent(QPaintEvent* event) override;

    private slots:
        /*!
         * \brief Handle menu closing
         *
         * C# equivalent: _contextMenuStrip_Closed event handler
         * (DropDownButton.cs lines 84-90)
         *
         * Prevents immediate re-opening if user clicks button while menu is open.
         */
        void onMenuAboutToHide();

    private:
        QMenu* m_menu;              ///< Dropdown menu
        bool m_ignoreNextClick;     ///< Flag to prevent menu re-opening
};

#endif // DROPDOWNBUTTON_H
