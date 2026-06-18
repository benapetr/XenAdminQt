/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 */

#ifndef DOWNLOADFILEACTION_H
#define DOWNLOADFILEACTION_H

#include "../xen/asyncoperation.h"
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

class XENLIB_EXPORT DownloadFileAction : public AsyncOperation
{
    public:
        explicit DownloadFileAction(const QUrl& sourceUrl,
                                    const QString& destPath,
                                    QObject* parent = nullptr);

        QString OutputPath() const { return this->m_destPath; }

    protected:
        void run() override;
        void onCancel() override;

    private:
        QUrl m_sourceUrl;
        QString m_destPath;
        QNetworkReply* m_reply;
};

#endif // DOWNLOADFILEACTION_H
