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

#include "metricupdater.h"
#include "xen/network/connection.h"
#include "xen/session.h"
#include <QXmlStreamReader>
#include <QDateTime>
#include <QUrl>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslConfiguration>

// C# Equivalent: XenAdmin.XenSearch.MetricUpdater implementation
// C# Reference: xenadmin/XenModel/XenSearch/MetricUpdater.cs

MetricUpdater::MetricUpdater(XenConnection* connection, QObject* parent)
    : QObject(parent), m_connection(connection), m_updateTimer(new QTimer(this)), m_networkManager(new QNetworkAccessManager(this)), m_running(false), m_paused(false), m_requestId(-1)
{
    m_updateTimer->setInterval(UPDATE_INTERVAL_MS);
    connect(m_updateTimer, &QTimer::timeout, this, &MetricUpdater::updateMetrics);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &MetricUpdater::onNetworkReplyFinished);
}

MetricUpdater::~MetricUpdater()
{
    stop();
}

void MetricUpdater::start()
{
    if (m_running)
        return;

    qDebug() << "MetricUpdater: Starting metric updates";
    m_running = true;
    m_paused = false;

    // Immediate first update
    updateMetrics();

    // Start periodic updates
    m_updateTimer->start();
}

void MetricUpdater::stop()
{
    if (!m_running)
        return;

    qDebug() << "MetricUpdater: Stopping metric updates";
    m_running = false;
    m_paused = false;
    m_updateTimer->stop();

    QMutexLocker locker(&m_metricsMutex);
    m_metricsCache.clear();
}

void MetricUpdater::pause()
{
    if (!m_running)
        return;

    qDebug() << "MetricUpdater: Pausing updates";
    m_paused = true;
    m_updateTimer->stop();
}

void MetricUpdater::resume()
{
    if (!m_running || !m_paused)
        return;

    qDebug() << "MetricUpdater: Resuming updates";
    m_paused = false;

    // Immediate update after resume
    updateMetrics();

    m_updateTimer->start();
}

void MetricUpdater::prod()
{
    if (!m_running)
        return;

    qDebug() << "MetricUpdater: Forcing immediate update";

    // Reset timer and trigger immediate update
    m_updateTimer->stop();
    updateMetrics();

    if (!m_paused)
        m_updateTimer->start();
}

double MetricUpdater::getValue(const QString& objectType, const QString& objectUuid,
                               const QString& metricName) const
{
    QString key = QString("%1:%2").arg(objectType, objectUuid);

    QMutexLocker locker(&m_metricsMutex);

    if (!m_metricsCache.contains(key))
        return 0.0;

    const MetricValues& metrics = m_metricsCache[key];
    return metrics.values.value(metricName, 0.0);
}

bool MetricUpdater::hasMetrics(const QString& objectType, const QString& objectUuid) const
{
    QString key = QString("%1:%2").arg(objectType, objectUuid);

    QMutexLocker locker(&m_metricsMutex);
    return m_metricsCache.contains(key) && !m_metricsCache[key].values.isEmpty();
}

void MetricUpdater::updateMetrics()
{
    if (!m_running || m_paused)
        return;

    if (!m_connection || !m_connection->isConnected())
    {
        qDebug() << "MetricUpdater: Connection not available, skipping update";
        return;
    }

    fetchRrdData();
}

void MetricUpdater::fetchRrdData()
{
    // C# Equivalent: ValuesFor(Host host) -> HTTPHelper.GET(GetUri(session, host), ...)
    // C# Reference: MetricUpdater.cs lines 173-189
    //
    // Builds URL: http(s)://host:port/rrd_updates?session_id=xxx&start=timestamp&cf=AVERAGE&interval=5&host=true

    QString url = buildRrdUrl();
    if (url.isEmpty())
    {
        qDebug() << "MetricUpdater: Failed to build RRD URL";
        return;
    }

    qDebug() << "MetricUpdater: Fetching RRD data from:" << url;

    // Create network request
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "XenAdmin-Qt/1.0");

    // Configure SSL if HTTPS
    if (url.startsWith("https"))
    {
        QSslConfiguration sslConfig = request.sslConfiguration();
        sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone); // Accept self-signed certificates
        request.setSslConfiguration(sslConfig);
    }

    // Send HTTP GET request
    QNetworkReply* reply = m_networkManager->get(request);
    reply->setProperty("requestTime", QDateTime::currentMSecsSinceEpoch());
}

QString MetricUpdater::buildRrdUrl() const
{
    // C# Equivalent: GetUri(Session session, Host host)
    // C# Reference: MetricUpdater.cs lines 343-353
    //
    // Format: /rrd_updates?session_id={0}&start={1}&cf={2}&interval={3}&host=true
    // - session_id: Session opaque ref
    // - start: Unix timestamp (current time - 10 seconds)
    // - cf: Consolidation function ("AVERAGE")
    // - interval: Data point interval (5 seconds)
    // - host: Include host metrics (true)

    if (!m_connection || !m_connection->getSession())
        return QString();

    QString sessionId = m_connection->getSession()->getSessionId();
    if (sessionId.isEmpty())
        return QString();

    qint64 startTime = getStartTimestamp();

    // Build query parameters manually to avoid encoding the colon in OpaqueRef:xxx
    // C# does NOT percent-encode the session_id - it's passed directly
    QString query = QString("session_id=%1&start=%2&cf=AVERAGE&interval=%3&host=true")
                        .arg(sessionId) // Do NOT percent-encode - XenServer expects "OpaqueRef:xxx"
                        .arg(startTime)
                        .arg(RRD_INTERVAL_SECONDS);

    int port = m_connection->getPort();
    bool useSSL = (port == 443); // Default XenServer SSL port

    QUrl url;
    url.setScheme(useSSL ? "https" : "http");
    url.setHost(m_connection->getHostname());
    url.setPort(port);
    url.setPath("/rrd_updates");
    url.setQuery(query);

    return url.toString();
}

qint64 MetricUpdater::getStartTimestamp() const
{
    // C# Equivalent: DateTime.UtcNow.Ticks - (host.Connection.ServerTimeOffset.Ticks + TicksInTenSeconds)
    // C# Reference: MetricUpdater.cs line 351
    //
    // Request data from 10 seconds ago (to account for clock skew and ensure we get recent data)

    QDateTime now = QDateTime::currentDateTimeUtc();
    qint64 tenSecondsAgo = now.toSecsSinceEpoch() - 10;

    // TODO: Account for server time offset if available
    // For now, use local UTC time

    return tenSecondsAgo;
}

void MetricUpdater::parseRrdXml(const QByteArray& xmlData)
{
    // C# Equivalent: AllValues(Stream httpstream)
    // C# Reference: MetricUpdater.cs lines 192-226
    //
    // XML Format:
    // <xport>
    //   <meta>
    //     <legend>
    //       <entry>AVERAGE:host:uuid:cpu0</entry>
    //       <entry>AVERAGE:vm:uuid:memory</entry>
    //       ...
    //     </legend>
    //   </meta>
    //   <data>
    //     <row>
    //       <v>0.123</v>  <!-- value for first entry -->
    //       <v>1234567</v>  <!-- value for second entry -->
    //       ...
    //     </row>
    //   </data>
    // </xport>

    QXmlStreamReader xml(xmlData);
    QStringList metricKeys;
    QMap<QString, MetricValues> newMetrics;
    qint64 timestamp = QDateTime::currentSecsSinceEpoch();

    QString currentElement;
    int valueIndex = 0;

    while (!xml.atEnd())
    {
        xml.readNext();

        if (xml.isStartElement())
        {
            currentElement = xml.name().toString();

            if (currentElement == "row")
                valueIndex = 0;
        } else if (xml.isCharacters() && !xml.isWhitespace())
        {
            QString text = xml.text().toString().trimmed();
            if (text.isEmpty())
                continue;

            if (currentElement == "entry")
            {
                // Add metric key from legend
                metricKeys.append(text);
            } else if (currentElement == "v")
            {
                // Parse metric value
                if (valueIndex < metricKeys.size())
                {
                    QString key = metricKeys[valueIndex];
                    bool ok;
                    double value = text.toDouble(&ok);

                    if (ok)
                    {
                        // Parse key: "AVERAGE:host:uuid:metric_name" or "AVERAGE:vm:uuid:metric_name"
                        QStringList parts = key.split(':');
                        if (parts.size() >= 4)
                        {
                            QString objectType = parts[1].toLower(); // "host" or "vm"
                            QString uuid = parts[2];
                            QString metricName = parts.mid(3).join(':'); // Handle metrics with ':' in name

                            QString cacheKey = QString("%1:%2").arg(objectType, uuid);

                            if (!newMetrics.contains(cacheKey))
                            {
                                MetricValues mv;
                                mv.lastUpdate = timestamp;
                                newMetrics[cacheKey] = mv;
                            }

                            newMetrics[cacheKey].values[metricName] = value;
                        }
                    }

                    valueIndex++;
                }
            }
        }
    }

    if (xml.hasError())
    {
        qWarning() << "MetricUpdater: XML parsing error:" << xml.errorString();
        return;
    }

    // Update cache with new metrics
    {
        QMutexLocker locker(&m_metricsMutex);
        m_metricsCache = newMetrics;
    }

    qDebug() << "MetricUpdater: Updated metrics for" << newMetrics.size() << "objects";
    emit metricsUpdated();
}

void MetricUpdater::onRrdDataReceived(const QByteArray& data)
{
    parseRrdXml(data);
}

void MetricUpdater::onRrdRequestFailed(const QString& error)
{
    qWarning() << "MetricUpdater: RRD request failed:" << error;
}

void MetricUpdater::onNetworkReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    qint64 requestTime = reply->property("requestTime").toLongLong();
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - requestTime;

    if (reply->error() != QNetworkReply::NoError)
    {
        QString errorMsg = QString("Network error: %1").arg(reply->errorString());
        qWarning() << "MetricUpdater:" << errorMsg << "(took" << elapsed << "ms)";
        onRrdRequestFailed(errorMsg);
        return;
    }

    QByteArray data = reply->readAll();
    qDebug() << "MetricUpdater: Received" << data.size() << "bytes (took" << elapsed << "ms)";

    if (data.isEmpty())
    {
        qWarning() << "MetricUpdater: Empty response from RRD endpoint";
        return;
    }

    onRrdDataReceived(data);
}
