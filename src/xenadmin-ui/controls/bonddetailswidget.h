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

#ifndef BONDDETAILSWIDGET_H
#define BONDDETAILSWIDGET_H

#include <QWidget>
#include <QSharedPointer>
#include <QStringList>
#include "ui_bonddetailswidget.h"

class Host;
class Pool;
class PIF;

class BondDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BondDetailsWidget(QWidget* parent = nullptr);

    void SetHost(const QSharedPointer<Host>& host);
    void SetPool(const QSharedPointer<Pool>& pool);
    void Refresh();

    bool IsValid() const;
    bool CanCreateBond(QWidget* parent);

    QString BondName() const;
    QString BondMode() const;
    QString HashingAlgorithm() const;
    qint64 MTU() const;
    bool AutoPlug() const;
    QStringList SelectedPifRefs() const;

signals:
    void ValidChanged(bool valid);

private slots:
    void onInputsChanged();

private:
    void populateBondNics();
    void refreshSelectionState();
    void updateMtuBounds();
    void updateLacpVisibility();
    int bondSizeLimit() const;

    QSharedPointer<Host> coordinatorHost() const;

    Ui::BondDetailsWidget ui;
    QSharedPointer<Host> m_host;
    QSharedPointer<Pool> m_pool;
    bool m_populatingBond = false;
    bool m_valid = false;
};

#endif // BONDDETAILSWIDGET_H
