/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 */

#include "downloadfileaction.h"

#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

DownloadFileAction::DownloadFileAction(const QUrl& sourceUrl,
                                       const QString& destPath,
                                       QObject* parent)
    : AsyncOperation(tr("Download"),
                     tr("Downloading %1...").arg(sourceUrl.fileName().isEmpty()
                         ? sourceUrl.toString()
                         : sourceUrl.fileName()),
                     parent)
    , m_sourceUrl(sourceUrl)
    , m_destPath(destPath)
    , m_reply(nullptr)
{
}

void DownloadFileAction::onCancel()
{
    if (this->m_reply)
        QMetaObject::invokeMethod(this->m_reply, "abort", Qt::QueuedConnection);
}

void DownloadFileAction::run()
{
    this->SetCanCancel(true);

    QFile out(this->m_destPath);
    if (!out.open(QIODevice::WriteOnly))
    {
        this->setError(tr("Cannot create %1: %2").arg(this->m_destPath, out.errorString()));
        this->setState(Failed);
        return;
    }

    QNetworkAccessManager manager;
    QNetworkRequest request(this->m_sourceUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "XenAdminQt/1.0");

    QEventLoop loop;
    QTimer cancelTimer;
    cancelTimer.setInterval(100);

    this->m_reply = manager.get(request);

    connect(this->m_reply, &QNetworkReply::readyRead, this, [this, &out]()
    {
        if (this->m_reply)
            out.write(this->m_reply->readAll());
    });

    connect(this->m_reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total)
    {
        if (total > 0)
            this->setPercentCompleteSafe(qBound(0, static_cast<int>((received * 100) / total), 99));
        this->setDescriptionSafe(tr("Downloading %1 (%2/%3 bytes)...")
                                 .arg(this->m_sourceUrl.fileName())
                                 .arg(received)
                                 .arg(total));
    });

    connect(this->m_reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&cancelTimer, &QTimer::timeout, this, [this]()
    {
        if (this->IsCancelled() && this->m_reply)
            this->m_reply->abort();
    });

    cancelTimer.start();
    loop.exec();
    cancelTimer.stop();

    out.write(this->m_reply->readAll());
    out.close();

    const QNetworkReply::NetworkError err = this->m_reply->error();
    const QString errStr = this->m_reply->errorString();
    this->m_reply->deleteLater();
    this->m_reply = nullptr;

    if (this->IsCancelled() || err == QNetworkReply::OperationCanceledError)
    {
        QFile::remove(this->m_destPath);
        this->setState(Cancelled);
        return;
    }

    if (err != QNetworkReply::NoError)
    {
        QFile::remove(this->m_destPath);
        this->setError(tr("Download failed: %1").arg(errStr));
        this->setState(Failed);
        return;
    }

    if (QFileInfo(this->m_destPath).size() == 0)
    {
        QFile::remove(this->m_destPath);
        this->setError(tr("Download failed: empty response"));
        this->setState(Failed);
        return;
    }

    this->setPercentCompleteSafe(100);
    this->setState(Completed);
}
