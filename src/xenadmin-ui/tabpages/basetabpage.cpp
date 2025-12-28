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

#include "basetabpage.h"
#include "xenlib.h"
BaseTabPage::BaseTabPage(QWidget* parent) : QWidget(parent), m_xenLib(nullptr)
{
}

BaseTabPage::~BaseTabPage()
{
}

void BaseTabPage::SetXenObject(const QString& objectType, const QString& objectRef, const QVariantMap& objectData)
{
    this->m_objectType = objectType;
    this->m_objectRef = objectRef;
    this->m_objectData = objectData;
    this->refreshContent();
}

void BaseTabPage::setXenLib(XenLib* xenLib)
{
    this->m_xenLib = xenLib;
    if (!this->m_connection && xenLib)
        this->m_connection = xenLib->getConnection();
}

void BaseTabPage::SetConnection(XenConnection *conn)
{
    this->m_connection = conn;
}

void BaseTabPage::onPageShown()
{
    // Default implementation does nothing
    // Subclasses can override to start timers, etc.
}

void BaseTabPage::onPageHidden()
{
    // Default implementation does nothing
    // Subclasses can override to stop timers, etc.
}

void BaseTabPage::refreshContent()
{
    // Default implementation does nothing
    // Subclasses must override to display content
}
