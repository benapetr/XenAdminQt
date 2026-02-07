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

#include "archivemaintainer.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xencache.h"
#include <QEventLoop>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSet>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QXmlStreamReader>
#include <limits>
#include <cmath>

namespace
{
    struct ParsedPointUpdate
    {
        CustomDataGraph::ArchiveInterval interval = CustomDataGraph::ArchiveInterval::None;
        QString dataSourceId;
        qint64 timestampMs = 0;
        double value = 0.0;
    };

    using ParsedPointUpdates = QVector<ParsedPointUpdate>;

    qint64 maxPointsForInterval(CustomDataGraph::ArchiveInterval interval)
    {
        using namespace CustomDataGraph;
        switch (interval)
        {
            case ArchiveInterval::FiveSecond:
                return ArchiveMaintainer::FIVE_SECONDS_IN_TEN_MINUTES + 4;
            case ArchiveInterval::OneMinute:
                return ArchiveMaintainer::MINUTES_IN_TWO_HOURS;
            case ArchiveInterval::OneHour:
                return ArchiveMaintainer::HOURS_IN_ONE_WEEK;
            case ArchiveInterval::OneDay:
                return ArchiveMaintainer::DAYS_IN_ONE_YEAR;
            default:
                return 0;
        }
    }

    qint64 toSecondsForInterval(CustomDataGraph::ArchiveInterval interval)
    {
        using namespace CustomDataGraph;
        switch (interval)
        {
            case ArchiveInterval::FiveSecond:
                return 5;
            case ArchiveInterval::OneMinute:
                return 60;
            case ArchiveInterval::OneHour:
                return 3600;
            case ArchiveInterval::OneDay:
                return 86400;
            default:
                return 5;
        }
    }

    CustomDataGraph::ArchiveInterval intervalFromPdpPerRow(qint64 pdpPerRow)
    {
        using namespace CustomDataGraph;
        switch (pdpPerRow)
        {
            case 1:
                return ArchiveInterval::FiveSecond;
            case 12:
                return ArchiveInterval::OneMinute;
            case 720:
                return ArchiveInterval::OneHour;
            case 17280:
                return ArchiveInterval::OneDay;
            default:
                return ArchiveInterval::None;
        }
    }

    QString normalizeDataSourceIdForObject(const QString& rawId, const QString& objectType, const QString& objectUuid)
    {
        const QString id = rawId.trimmed();
        if (id.isEmpty())
            return id;

        const QStringList parts = id.split(':');
        if (parts.size() >= 3)
            return QStringLiteral("%1:%2:%3").arg(parts.at(0).toLower(), parts.at(1), parts.mid(2).join(QStringLiteral(":")));

        if (!objectType.isEmpty() && !objectUuid.isEmpty())
            return QStringLiteral("%1:%2:%3").arg(objectType, objectUuid, id);

        return id;
    }

    bool parseRrdNumericValue(const QString& rawValue, double* outValue)
    {
        if (!outValue)
            return false;

        const QString value = rawValue.trimmed();
        if (value.isEmpty())
            return false;

        if (value.compare(QStringLiteral("NaN"), Qt::CaseInsensitive) == 0)
        {
            *outValue = std::numeric_limits<double>::quiet_NaN();
            return true;
        }

        if (value.compare(QStringLiteral("Infinity"), Qt::CaseInsensitive) == 0
            || value.compare(QStringLiteral("+Infinity"), Qt::CaseInsensitive) == 0
            || value.compare(QStringLiteral("inf"), Qt::CaseInsensitive) == 0
            || value.compare(QStringLiteral("+inf"), Qt::CaseInsensitive) == 0)
        {
            *outValue = std::numeric_limits<double>::infinity();
            return true;
        }

        if (value.compare(QStringLiteral("-Infinity"), Qt::CaseInsensitive) == 0
            || value.compare(QStringLiteral("-inf"), Qt::CaseInsensitive) == 0)
        {
            *outValue = -std::numeric_limits<double>::infinity();
            return true;
        }

        bool ok = false;
        const double parsed = value.toDouble(&ok);
        if (!ok)
            return false;

        *outValue = parsed;
        return true;
    }

    double normalizeNonFiniteForGraph(double value)
    {
        // Match XenAdmin C#: DataSet.AddPoint maps NaN/Infinity to -1 sentinel.
        return std::isfinite(value) ? value : -1.0;
    }

    QByteArray httpGetBlocking(const QUrl& url)
    {
        if (!url.isValid())
            return QByteArray();

        QNetworkAccessManager manager;
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, "XenAdmin-Qt/1.0");

        if (url.scheme().startsWith(QStringLiteral("https"), Qt::CaseInsensitive))
        {
            QSslConfiguration sslConfig = request.sslConfiguration();
            sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
            request.setSslConfiguration(sslConfig);
        }

        QNetworkReply* reply = manager.get(request);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError)
        {
            reply->deleteLater();
            return QByteArray();
        }

        const QByteArray data = reply->readAll();
        reply->deleteLater();
        return data;
    }

    ParsedPointUpdates parseUpdateXmlToPoints(const QByteArray& xml,
                                              CustomDataGraph::ArchiveInterval interval,
                                              const QString& objectType,
                                              const QString& objectUuid,
                                              const QSet<QString>& selectedIds)
    {
        ParsedPointUpdates updates;
        if (xml.isEmpty())
            return updates;

        QXmlStreamReader reader(xml);
        QStringList entries;
        qint64 currentTimeMs = 0;
        int valueIndex = 0;

        while (!reader.atEnd())
        {
            reader.readNext();

            if (!reader.isStartElement())
                continue;

            const QString name = reader.name().toString();
            if (name == QLatin1String("entry"))
            {
                QString entry = reader.readElementText().trimmed();
                if (entry.startsWith(QStringLiteral("AVERAGE:"), Qt::CaseInsensitive))
                    entry = entry.mid(QStringLiteral("AVERAGE:").size());

                const QString normalized = normalizeDataSourceIdForObject(entry, objectType, objectUuid);
                if (selectedIds.isEmpty() || selectedIds.contains(normalized))
                    entries.append(normalized);
                else
                    entries.append(QString());
            } else if (name == QLatin1String("row"))
            {
                valueIndex = 0;
            } else if (name == QLatin1String("t"))
            {
                bool ok = false;
                const qint64 sec = reader.readElementText().toLongLong(&ok);
                if (ok)
                    currentTimeMs = sec * 1000;
            } else if (name == QLatin1String("v"))
            {
                const QString valueText = reader.readElementText().trimmed();
                double value = 0.0;
                const bool ok = parseRrdNumericValue(valueText, &value);

                if (ok && valueIndex >= 0 && valueIndex < entries.size() && currentTimeMs > 0)
                {
                    const QString& id = entries.at(valueIndex);
                    if (!id.isEmpty())
                    {
                        ParsedPointUpdate update;
                        update.interval = interval;
                        update.dataSourceId = id;
                        update.timestampMs = currentTimeMs;
                        update.value = normalizeNonFiniteForGraph(value);
                        updates.append(update);
                    }
                }

                ++valueIndex;
            }
        }

        return updates;
    }

    ParsedPointUpdates parseFullArchiveXmlToPoints(const QByteArray& xml,
                                                   const QString& objectType,
                                                   const QString& objectUuid,
                                                   const QSet<QString>& selectedIds)
    {
        ParsedPointUpdates updates;
        if (xml.isEmpty())
            return updates;

        QXmlStreamReader reader(xml);

        QStringList names;
        bool inRra = false;
        bool rraAverage = false;
        qint64 stepSize = 0;
        qint64 endTime = 0;
        qint64 currentInterval = 0;
        qint64 currentTimeMs = 0;
        int valueIndex = 0;

        while (!reader.atEnd())
        {
            reader.readNext();

            if (reader.isStartElement())
            {
                const QString node = reader.name().toString();

                if (node == QLatin1String("name"))
                {
                    const QString normalized = normalizeDataSourceIdForObject(reader.readElementText().trimmed(), objectType, objectUuid);
                    if (selectedIds.isEmpty() || selectedIds.contains(normalized))
                        names.append(normalized);
                    else
                        names.append(QString());
                } else if (node == QLatin1String("step"))
                {
                    stepSize = reader.readElementText().toLongLong();
                } else if (node == QLatin1String("lastupdate"))
                {
                    endTime = reader.readElementText().toLongLong();
                } else if (node == QLatin1String("rra"))
                {
                    inRra = true;
                    rraAverage = false;
                    currentInterval = 0;
                    currentTimeMs = 0;
                } else if (node == QLatin1String("cf") && inRra)
                {
                    rraAverage = reader.readElementText().trimmed().compare(QStringLiteral("AVERAGE"), Qt::CaseInsensitive) == 0;
                } else if (node == QLatin1String("pdp_per_row") && inRra)
                {
                    currentInterval = reader.readElementText().toLongLong();
                    if (stepSize > 0 && currentInterval > 0)
                    {
                        const qint64 modInterval = endTime % (stepSize * currentInterval);
                        qint64 stepCount = CustomDataGraph::ArchiveMaintainer::DAYS_IN_ONE_YEAR;
                        if (currentInterval == 1)
                            stepCount = CustomDataGraph::ArchiveMaintainer::FIVE_SECONDS_IN_TEN_MINUTES;
                        else if (currentInterval == 12)
                            stepCount = CustomDataGraph::ArchiveMaintainer::MINUTES_IN_TWO_HOURS;
                        else if (currentInterval == 720)
                            stepCount = CustomDataGraph::ArchiveMaintainer::HOURS_IN_ONE_WEEK;

                        currentTimeMs = (endTime - modInterval - stepSize * currentInterval * stepCount) * 1000;
                    }
                } else if (node == QLatin1String("row") && inRra)
                {
                    if (currentInterval > 0 && stepSize > 0)
                        currentTimeMs += (currentInterval * stepSize * 1000);
                    valueIndex = 0;
                } else if (node == QLatin1String("v") && inRra && rraAverage)
                {
                    const QString valueText = reader.readElementText().trimmed();
                    double value = 0.0;
                    const bool ok = parseRrdNumericValue(valueText, &value);
                    const CustomDataGraph::ArchiveInterval interval = intervalFromPdpPerRow(currentInterval);

                    if (ok && interval != CustomDataGraph::ArchiveInterval::None && valueIndex >= 0 && valueIndex < names.size() && currentTimeMs > 0)
                    {
                        const QString& id = names.at(valueIndex);
                        if (!id.isEmpty())
                        {
                            ParsedPointUpdate update;
                            update.interval = interval;
                            update.dataSourceId = id;
                            update.timestampMs = currentTimeMs;
                            update.value = normalizeNonFiniteForGraph(value);
                            updates.append(update);
                        }
                    }

                    ++valueIndex;
                }
            } else if (reader.isEndElement() && reader.name() == QLatin1String("rra"))
            {
                inRra = false;
                rraAverage = false;
                currentInterval = 0;
            }
        }

        return updates;
    }
}

namespace CustomDataGraph
{
    ArchiveMaintainer::ArchiveMaintainer(XenObject* xenObject, QObject* parent)
        : QObject(parent), m_xenObject(xenObject), m_timer(new QTimer(this))
    {
        if (this->m_xenObject)
            this->m_connection = this->m_xenObject->GetConnection();

        this->m_archives.insert(ArchiveInterval::FiveSecond, DataArchive(FIVE_SECONDS_IN_TEN_MINUTES + 4));
        this->m_archives.insert(ArchiveInterval::OneMinute, DataArchive(MINUTES_IN_TWO_HOURS));
        this->m_archives.insert(ArchiveInterval::OneHour, DataArchive(HOURS_IN_ONE_WEEK));
        this->m_archives.insert(ArchiveInterval::OneDay, DataArchive(DAYS_IN_ONE_YEAR));
        this->m_archives.insert(ArchiveInterval::None, DataArchive(0));

        this->m_timer->setInterval(5000);
        connect(this->m_timer, &QTimer::timeout, this, &ArchiveMaintainer::onSampleTick);
    }

    ArchiveMaintainer::~ArchiveMaintainer()
    {
        this->Stop();
        this->shutdownWorkerThread();
    }

    void ArchiveMaintainer::Start()
    {
        if (this->m_running)
            return;

        this->m_running = true;
        ++this->m_requestToken;
        this->m_initialLoadCompleted = false;
        this->m_initialLoadInProgress = false;
        this->m_updateInProgress = false;
        this->m_timer->stop();

        this->initialLoad();
    }

    void ArchiveMaintainer::Stop()
    {
        this->m_running = false;
        ++this->m_requestToken;
        this->m_initialLoadCompleted = false;
        this->m_initialLoadInProgress = false;
        this->m_updateInProgress = false;
        this->m_timer->stop();
    }

    void ArchiveMaintainer::SetDataSourceIds(const QStringList& dataSourceIds)
    {
        this->m_dataSourceIds = dataSourceIds;
    }

    QStringList ArchiveMaintainer::DataSourceIds() const
    {
        return this->m_dataSourceIds;
    }

    QDateTime ArchiveMaintainer::GraphNow() const
    {
        const qint64 offsetSeconds = this->m_connection ? this->m_connection->GetServerTimeOffsetSeconds() : 0;
        return QDateTime::currentDateTime().addSecs(-offsetSeconds - 15);
    }

    XenObject* ArchiveMaintainer::GetXenObject() const
    {
        return this->m_xenObject;
    }

    DataArchive& ArchiveMaintainer::Archive(ArchiveInterval interval)
    {
        return this->m_archives[interval];
    }

    DataArchive ArchiveMaintainer::Archive(ArchiveInterval interval) const
    {
        return this->m_archives.value(interval);
    }

    DataSet ArchiveMaintainer::GetDataSet(const QString& dataSourceId, ArchiveInterval interval) const
    {
        return this->m_archives.value(interval).Get(dataSourceId);
    }

    const DataSet* ArchiveMaintainer::TryGetDataSet(const QString& dataSourceId, ArchiveInterval interval) const
    {
        const auto it = this->m_archives.constFind(interval);
        if (it == this->m_archives.constEnd())
            return nullptr;

        return it.value().Find(dataSourceId);
    }

    void ArchiveMaintainer::onSampleTick()
    {
        this->collectUpdates();
    }

    QDateTime ArchiveMaintainer::serverNow() const
    {
        const qint64 offsetSeconds = this->m_connection ? this->m_connection->GetServerTimeOffsetSeconds() : 0;
        return QDateTime::currentDateTimeUtc().addSecs(-offsetSeconds);
    }

    qint64 ArchiveMaintainer::timeFromInterval(ArchiveInterval interval) const
    {
        QDateTime baseline;

        switch (interval)
        {
            case ArchiveInterval::FiveSecond:
                baseline = this->m_lastFiveSecondCollection;
                if (baseline.isValid())
                    return baseline.toSecsSinceEpoch() - 5;
                return this->serverNow().addSecs(-(10 * 60)).toSecsSinceEpoch();
            case ArchiveInterval::OneMinute:
                baseline = this->m_lastOneMinuteCollection;
                if (baseline.isValid())
                    return baseline.toSecsSinceEpoch() - 60;
                return this->serverNow().addSecs(-(2 * 60 * 60)).toSecsSinceEpoch();
            case ArchiveInterval::OneHour:
                baseline = this->m_lastOneHourCollection;
                if (baseline.isValid())
                    return baseline.toSecsSinceEpoch() - 3600;
                return this->serverNow().addSecs(-(7 * 24 * 60 * 60)).toSecsSinceEpoch();
            case ArchiveInterval::OneDay:
                baseline = this->m_lastOneDayCollection;
                if (baseline.isValid())
                    return baseline.toSecsSinceEpoch() - 86400;
                return this->serverNow().addDays(-366).toSecsSinceEpoch();
            default:
                return this->serverNow().addSecs(-5).toSecsSinceEpoch();
        }
    }

    QString ArchiveMaintainer::resolveRequestHostAddress() const
    {
        if (!this->m_connection || !this->m_xenObject)
            return QString();

        if (this->m_xenObject->GetObjectType() == XenObjectType::Host)
        {
            auto* host = dynamic_cast<Host*>(this->m_xenObject);
            if (host && !host->GetAddress().isEmpty())
                return host->GetAddress();
            return this->m_connection->GetHostname();
        }

        if (this->m_xenObject->GetObjectType() == XenObjectType::VM)
        {
            auto* vm = dynamic_cast<VM*>(this->m_xenObject);
            if (vm)
            {
                const QSharedPointer<Host> resident = vm->GetResidentOnHost();
                if (resident && !resident->GetAddress().isEmpty())
                    return resident->GetAddress();
            }

            XenCache* cache = this->m_connection->GetCache();
            if (cache)
            {
                const QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>();
                for (const QSharedPointer<Host>& host : hosts)
                {
                    if (host && host->IsMaster() && !host->GetAddress().isEmpty())
                        return host->GetAddress();
                }
            }

            return this->m_connection->GetHostname();
        }

        return this->m_connection->GetHostname();
    }

    QUrl ArchiveMaintainer::buildUpdateUri(ArchiveInterval interval) const
    {
        if (!this->m_connection || !this->m_connection->GetSession() || !this->m_xenObject)
            return QUrl();

        const QString sessionId = this->m_connection->GetSession()->GetSessionID();
        const QString hostAddress = this->resolveRequestHostAddress();
        if (sessionId.isEmpty() || hostAddress.isEmpty())
            return QUrl();

        const int port = this->m_connection->GetPort();
        QUrl url;
        url.setScheme(port == 443 ? QStringLiteral("https") : QStringLiteral("http"));
        url.setHost(hostAddress);
        url.setPort(port);
        url.setPath(QStringLiteral("/rrd_updates"));

        const qint64 start = this->timeFromInterval(interval);
        const qint64 duration = toSecondsForInterval(interval);

        QString query = QStringLiteral("session_id=%1&start=%2&cf=AVERAGE&interval=%3")
                            .arg(sessionId)
                            .arg(start)
                            .arg(duration);

        if (this->m_xenObject->GetObjectType() == XenObjectType::Host)
        {
            query.append(QStringLiteral("&host=true"));
        } else if (this->m_xenObject->GetObjectType() == XenObjectType::VM)
        {
            query.append(QStringLiteral("&vm_uuid=%1").arg(this->m_xenObject->GetUUID()));
        }

        url.setQuery(query);
        return url;
    }

    QUrl ArchiveMaintainer::buildRrdsUri() const
    {
        if (!this->m_connection || !this->m_connection->GetSession() || !this->m_xenObject)
            return QUrl();

        const QString sessionId = this->m_connection->GetSession()->GetSessionID();
        const QString hostAddress = this->resolveRequestHostAddress();
        if (sessionId.isEmpty() || hostAddress.isEmpty())
            return QUrl();

        const int port = this->m_connection->GetPort();
        QUrl url;
        url.setScheme(port == 443 ? QStringLiteral("https") : QStringLiteral("http"));
        url.setHost(hostAddress);
        url.setPort(port);

        QString query = QStringLiteral("session_id=%1").arg(sessionId);

        if (this->m_xenObject->GetObjectType() == XenObjectType::Host)
        {
            url.setPath(QStringLiteral("/host_rrds"));
        } else if (this->m_xenObject->GetObjectType() == XenObjectType::VM)
        {
            url.setPath(QStringLiteral("/vm_rrds"));
            query.append(QStringLiteral("&uuid=%1").arg(this->m_xenObject->GetUUID()));
        } else
        {
            return QUrl();
        }

        url.setQuery(query);
        return url;
    }

    void ArchiveMaintainer::ensureWorkerThread()
    {
        if (this->m_workerThread)
            return;

        this->m_workerThread = new QThread(this);
        this->m_workerContext = new QObject();
        this->m_workerContext->moveToThread(this->m_workerThread);
        connect(this->m_workerThread, &QThread::finished, this->m_workerContext, &QObject::deleteLater);
        this->m_workerThread->start();
    }

    void ArchiveMaintainer::shutdownWorkerThread()
    {
        if (!this->m_workerThread)
            return;

        this->m_workerThread->quit();
        this->m_workerThread->wait();
        this->m_workerThread = nullptr;
        this->m_workerContext = nullptr;
    }

    void ArchiveMaintainer::initialLoad()
    {
        if (!this->m_running || !this->m_connection || !this->m_connection->GetSession() || !this->m_xenObject)
            return;

        for (auto it = this->m_archives.begin(); it != this->m_archives.end(); ++it)
        {
            if (it.key() == ArchiveInterval::None)
                continue;

            it.value().SetMaxPoints(static_cast<int>(maxPointsForInterval(it.key())));
            it.value().ClearSets();
        }

        this->ensureWorkerThread();
        if (!this->m_workerContext)
        {
            this->m_initialLoadInProgress = false;
            return;
        }

        const quint64 token = this->m_requestToken;
        const QUrl rrdsUrl = this->buildRrdsUri();
        const QString objectType = this->m_xenObject->GetObjectType() == XenObjectType::Host ? QStringLiteral("host") : QStringLiteral("vm");
        const QString objectUuid = this->m_xenObject->GetUUID();
        const QSet<QString> selectedIds(this->m_dataSourceIds.begin(), this->m_dataSourceIds.end());
        this->m_initialLoadInProgress = true;

        QPointer<ArchiveMaintainer> self(this);
        QMetaObject::invokeMethod(this->m_workerContext, [self, token, rrdsUrl, objectType, objectUuid, selectedIds]()
        {
            const QByteArray xml = httpGetBlocking(rrdsUrl);
            const ParsedPointUpdates updates = parseFullArchiveXmlToPoints(xml, objectType, objectUuid, selectedIds);

            if (!self)
                return;

            QMetaObject::invokeMethod(self.data(), [self, token, updates]()
            {
                if (!self)
                    return;

                if (!self->m_running || token != self->m_requestToken)
                    return;

                qint64 newestTimestampMs = 0;
                for (int i = updates.size() - 1; i >= 0; --i)
                {
                    const ParsedPointUpdate& update = updates.at(i);
                    self->appendPoint(self->m_archives[update.interval], update.dataSourceId, update.timestampMs, update.value);
                    newestTimestampMs = qMax(newestTimestampMs, update.timestampMs);
                }

                if (self->m_connection && newestTimestampMs > 0)
                {
                    const qint64 localUtcNowSec = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
                    const qint64 serverDataNowSec = newestTimestampMs / 1000;
                    const qint64 derivedOffsetSec = localUtcNowSec - serverDataNowSec;

                    // Keep this as a fallback source when heartbeat hasn't provided server offset yet.
                    if (qAbs(derivedOffsetSec - self->m_connection->GetServerTimeOffsetSeconds()) >= 2)
                        self->m_connection->SetServerTimeOffsetSeconds(derivedOffsetSec);
                }

                const QDateTime now = self->serverNow();
                self->m_lastFiveSecondCollection = now;
                self->m_lastOneMinuteCollection = now;
                self->m_lastOneHourCollection = now;
                self->m_lastOneDayCollection = now;
                self->m_initialLoadInProgress = false;
                self->m_initialLoadCompleted = true;

                if (self->m_running)
                    self->m_timer->start();

                emit self->ArchivesUpdated();
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
    }

    void ArchiveMaintainer::collectUpdates()
    {
        if (!this->m_running || !this->m_connection || !this->m_connection->GetSession() || !this->m_initialLoadCompleted)
            return;

        if (this->m_updateInProgress || this->m_initialLoadInProgress)
            return;

        const QDateTime now = this->serverNow();
        QList<QPair<ArchiveInterval, QUrl>> requests;

        if (!this->m_lastFiveSecondCollection.isValid() || this->m_lastFiveSecondCollection.secsTo(now) >= 5)
        {
            requests.append(qMakePair(ArchiveInterval::FiveSecond, this->buildUpdateUri(ArchiveInterval::FiveSecond)));
            this->m_lastFiveSecondCollection = now;
        }

        if (!this->m_lastOneMinuteCollection.isValid() || this->m_lastOneMinuteCollection.secsTo(now) >= 60)
        {
            requests.append(qMakePair(ArchiveInterval::OneMinute, this->buildUpdateUri(ArchiveInterval::OneMinute)));
            this->m_lastOneMinuteCollection = now;
        }

        if (!this->m_lastOneHourCollection.isValid() || this->m_lastOneHourCollection.secsTo(now) >= 3600)
        {
            requests.append(qMakePair(ArchiveInterval::OneHour, this->buildUpdateUri(ArchiveInterval::OneHour)));
            this->m_lastOneHourCollection = now;
        }

        if (!this->m_lastOneDayCollection.isValid() || this->m_lastOneDayCollection.secsTo(now) >= 86400)
        {
            requests.append(qMakePair(ArchiveInterval::OneDay, this->buildUpdateUri(ArchiveInterval::OneDay)));
            this->m_lastOneDayCollection = now;
        }

        if (requests.isEmpty())
            return;

        this->ensureWorkerThread();
        if (!this->m_workerContext)
        {
            this->m_updateInProgress = false;
            return;
        }

        const quint64 token = this->m_requestToken;
        const QString objectType = this->m_xenObject->GetObjectType() == XenObjectType::Host ? QStringLiteral("host") : QStringLiteral("vm");
        const QString objectUuid = this->m_xenObject->GetUUID();
        const QSet<QString> selectedIds(this->m_dataSourceIds.begin(), this->m_dataSourceIds.end());
        this->m_updateInProgress = true;

        QPointer<ArchiveMaintainer> self(this);
        QMetaObject::invokeMethod(this->m_workerContext, [self, token, requests, objectType, objectUuid, selectedIds]()
        {
            ParsedPointUpdates updates;
            for (const QPair<ArchiveInterval, QUrl>& request : requests)
            {
                const QByteArray xml = httpGetBlocking(request.second);
                const ParsedPointUpdates parsed = parseUpdateXmlToPoints(xml, request.first, objectType, objectUuid, selectedIds);
                updates += parsed;
            }

            if (!self)
                return;

            QMetaObject::invokeMethod(self.data(), [self, token, updates]()
            {
                if (!self)
                    return;

                if (!self->m_running || token != self->m_requestToken)
                {
                    self->m_updateInProgress = false;
                    return;
                }

                for (int i = updates.size() - 1; i >= 0; --i)
                {
                    const ParsedPointUpdate& update = updates.at(i);
                    self->appendPoint(self->m_archives[update.interval], update.dataSourceId, update.timestampMs, update.value);
                }

                self->m_updateInProgress = false;
                emit self->ArchivesUpdated();
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
    }

    void ArchiveMaintainer::appendPoint(DataArchive& archive, const QString& dataSourceId, qint64 timestampMs, double value)
    {
        archive.InsertPointSortedDescendingByX(dataSourceId, DataPoint(timestampMs, value));
    }
}
