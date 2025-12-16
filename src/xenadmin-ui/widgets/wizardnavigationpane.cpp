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

#include "wizardnavigationpane.h"
#include "ui_wizardnavigationpane.h"

#include <QListWidget>
#include <QSignalBlocker>

WizardNavigationPane::WizardNavigationPane(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::WizardNavigationPane)
{
    ui->setupUi(this);
    ui->stepsList->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->stepsList->setFocusPolicy(Qt::NoFocus);
    ui->stepsList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->stepsList->viewport()->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->stepsList->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->stepsList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->stepsList->setFrameShape(QFrame::NoFrame);
    ui->stepsList->setSpacing(2);

    ui->brandIcon->setVisible(false);

    setMinimumWidth(180);
}

WizardNavigationPane::~WizardNavigationPane()
{
    delete ui;
}

void WizardNavigationPane::setSteps(const QVector<Step>& steps)
{
    m_steps = steps;
    ui->stepsList->clear();

    for (const Step& step : steps)
    {
        auto* item = new QListWidgetItem(step.icon, step.title);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->stepsList->addItem(item);
    }

    if (!steps.isEmpty())
        ui->stepsList->setCurrentRow(0);
}

void WizardNavigationPane::setCurrentStep(int index)
{
    if (index < 0 || index >= ui->stepsList->count())
        return;

    QSignalBlocker blocker(ui->stepsList);
    ui->stepsList->setCurrentRow(index);
}

void WizardNavigationPane::setBranding(const QString& text, const QPixmap& pixmap)
{
    ui->brandLabel->setText(text);
    if (!pixmap.isNull())
    {
        ui->brandIcon->setPixmap(pixmap);
        ui->brandIcon->setVisible(true);
    }
    else
    {
        ui->brandIcon->clear();
        ui->brandIcon->setVisible(false);
    }
}
