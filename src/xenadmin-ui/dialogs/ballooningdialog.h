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

#ifndef BALLOONINGDIALOG_H
#define BALLOONINGDIALOG_H

#include <QDialog>
#include <QVariantMap>

class XenLib;
class ChangeMemorySettingsAction;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class BallooningDialog;
}
QT_END_NAMESPACE

/**
 * @brief Dialog for editing VM memory (ballooning) settings
 *
 * Allows users to configure VM memory allocation, choosing between:
 * - Fixed allocation: Single static memory value
 * - Dynamic allocation: Memory range (min/max) with DMC support
 *
 * Matches C# XenAdmin BallooningDialog.cs functionality.
 * Uses ChangeMemorySettingsAction for applying changes.
 */
class BallooningDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Construct ballooning dialog
     * @param vmRef VM opaque reference
     * @param xenLib XenLib instance for API access
     * @param parent Parent widget
     */
    explicit BallooningDialog(const QString& vmRef, XenLib* xenLib, QWidget* parent = nullptr);
    ~BallooningDialog();

private slots:
    void onFixedRadioToggled(bool checked);
    void onDynamicRadioToggled(bool checked);
    void onFixedValueChanged(double value);
    void onDynMinValueChanged(double value);
    void onDynMaxValueChanged(double value);
    void onAccepted();

private:
    Ui::BallooningDialog* ui;
    QString m_vmRef;
    XenLib* m_xenLib;
    QVariantMap m_vmData;

    bool m_hasBallooning; // Whether VM supports DMC
    bool m_isTemplate;

    qint64 m_originalStaticMin;
    qint64 m_originalStaticMax;
    qint64 m_originalDynamicMin;
    qint64 m_originalDynamicMax;

    void populateControls();
    void updateDMCAvailability();
    void updateSpinnerRanges();
    void setSpinnerEnabled(bool fixed, bool dynamic);

    qint64 bytesToMB(qint64 bytes) const;
    qint64 mbToBytes(double mb) const;

    bool validateMemorySettings() const;
    bool applyMemoryChanges();
};

#endif // BALLOONINGDIALOG_H
