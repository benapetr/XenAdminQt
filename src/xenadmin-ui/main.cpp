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
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>
#include "globals.h"
#include "mainwindow.h"
#include "settingsmanager.h"

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

    // Set application properties
    app.setApplicationName(XENADMIN_BRANDING_APP_NAME);
    app.setApplicationVersion(XENADMIN_VERSION);
    app.setOrganizationName(XENADMIN_BRANDING_ORG_NAME);
    SettingsManager::instance().ApplyProxySettings();

    MainWindow w;
    w.show();

    return app.exec();
}
