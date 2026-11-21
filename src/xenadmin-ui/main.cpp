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

#include <QApplication>
#include <QFormLayout>
#include <QProxyStyle>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QTextStream>
#include <QTimer>
#include "globals.h"
#include "mainwindow.h"
#include "settingsmanager.h"
#include "xenlib/otherconfig/otherconfigandtagswatcher.h"
#include "xenlib/customfields/customfieldsmanager.h"
#include "xenlib/folders/foldersmanager.h"
#include "xenlib/xenlib.h"
#include "xenlib/utils/filelogger.h"

namespace
{
    /*!
     * macOS's style keeps form fields at their natural width and centres the form, so most
     * dialogs (laid out for Windows, like C#) end up with narrow text boxes floating in the
     * middle. Use the Windows and Linux form behaviour; everything else stays native.
     */
    class FormLayoutStyle : public QProxyStyle
    {
        public:
            int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget, QStyleHintReturn* returnData) const override
            {
                switch (hint)
                {
                    case SH_FormLayoutFieldGrowthPolicy:
                        return QFormLayout::AllNonFixedFieldsGrow;
                    case SH_FormLayoutFormAlignment:
                        return int(Qt::AlignLeft | Qt::AlignTop);
                    default:
                        return QProxyStyle::styleHint(hint, option, widget, returnData);
                }
            }
    };
}

int main(int argc, char* argv[])
{
    {
        QCoreApplication coreApp(argc, argv);
        QCoreApplication::setApplicationName(XENADMIN_BRANDING_APP_NAME);
        QCoreApplication::setApplicationVersion(XENADMIN_VERSION);
        QCoreApplication::setOrganizationName(XENADMIN_BRANDING_ORG_NAME);

        QCommandLineParser parser;
        parser.setApplicationDescription("XenAdmin Qt");

        QCommandLineOption confOption(QStringList() << "c" << "conf", "Use alternative configuration directory path.", "path");
        QCommandLineOption versionOption(QStringList() << "V" << "version", "Print version and exit.");
        parser.addOption(confOption);
        parser.addOption(versionOption);
        parser.addHelpOption();

        parser.process(coreApp);

        if (parser.isSet(versionOption))
        {
            QTextStream(stdout) << coreApp.applicationName() << " " << coreApp.applicationVersion() << "\n";
            return 0;
        }

        if (parser.isSet(confOption))
        {
            SettingsManager::SetConfigDir(parser.value(confOption));
        }
    }

    QApplication app(argc, argv);
#ifdef Q_OS_MACOS
    app.setStyle(new FormLayoutStyle());
#endif
    XenLib::RegisterMetaTypes();

    // Set application properties
    app.setApplicationName(XENADMIN_BRANDING_APP_NAME);
    app.setApplicationVersion(XENADMIN_VERSION);
    app.setOrganizationName(XENADMIN_BRANDING_ORG_NAME);

    // Application log file (C# log4net); configured before anything else logs much
    SettingsManager::instance().ApplyFileLoggingSettings();
    FileLogger::instance()->Install();
    qInfo().noquote() << QString("%1 %2 started").arg(app.applicationName(), app.applicationVersion());

    SettingsManager::instance().ApplyProxySettings();

    // C# parity startup services (watchers + folders + custom fields)
    OtherConfigAndTagsWatcher::instance()->RegisterEventHandlers();
    FoldersManager::instance()->RegisterEventHandlers();
    (void)CustomFieldsManager::instance();
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []()
    {
        FoldersManager::instance()->DeregisterEventHandlers();
        OtherConfigAndTagsWatcher::instance()->DeregisterEventHandlers();
    });

    int result = 0;
    {
        MainWindow w;
        w.show();
        result = app.exec();
    }

    // The handler stays installed (the debug console chains to it), only the buffer is written out
    qInfo().noquote() << QString("%1 exiting").arg(app.applicationName());
    FileLogger::instance()->Flush();
    return result;
}
