/* Copyright (c) 2024 Petr Bena
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef METRICUPDATER_H
#define METRICUPDATER_H

#include <QObject>
#include <QTimer>
#include <QMap>
#include <QString>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class XenConnection;

// C# Equivalent: XenAdmin.XenSearch.MetricUpdater
// C# Reference: xenadmin/XenModel/XenSearch/MetricUpdater.cs
//
// Purpose: Fetches and caches RRD (Round-Robin Database) metrics from XenServer
// for real-time performance monitoring of VMs and hosts.
//
// Metrics Available:
// - CPU: "cpu0", "cpu1", ... (per-vCPU utilization 0-1)
// - Memory: "memory" (total bytes), "memory_internal_free" (free KB)
// - Network: "vif_0_rx", "vif_0_tx", ... (bytes/sec per VIF)
// - Disk: "vbd_0_read", "vbd_0_write", ... (bytes/sec per VBD)
// - Host: "memory_total_kib", "memory_free_kib", "cpu0", "cpu1", ...
//
// Usage:
//   double cpuUsage = metricUpdater->getValue("vm", vmUuid, "cpu0");
//   double memFree = metricUpdater->getValue("vm", vmUuid, "memory_internal_free");

class MetricUpdater : public QObject
{
    Q_OBJECT

    public:
        explicit MetricUpdater(XenConnection* connection);
        ~MetricUpdater();

        // Start periodic metric updates (every 30 seconds)
        void start();

        // Stop metric updates
        void stop();

        // Pause updates temporarily
        void pause();

        // Resume paused updates
        void resume();

        // Force immediate update (skip wait)
        void prod();

        // Get cached metric value for object
        // objectType: "vm" or "host"
        // objectUuid: UUID of VM/host
        // metricName: "cpu0", "memory", "vif_0_rx", etc.
        // Returns: Metric value, or 0.0 if not available
        double getValue(const QString& objectType, const QString& objectUuid,
                        const QString& metricName) const;

        // Check if metrics are available for object
        bool hasMetrics(const QString& objectType, const QString& objectUuid) const;

    signals:
        // Emitted after each successful metrics update
        void metricsUpdated();

    private slots:
        void updateMetrics();
        void onRrdDataReceived(const QByteArray& data);
        void onRrdRequestFailed(const QString& error);
        void onNetworkReplyFinished(QNetworkReply* reply);

    private:
        struct MetricValues
        {
            QMap<QString, double> values; // metric_name -> value
            qint64 lastUpdate;            // timestamp of last update
        };

        void fetchRrdData();
        void parseRrdXml(const QByteArray& xmlData);
        QString buildRrdUrl() const;
        qint64 getStartTimestamp() const;

        XenConnection* m_connection;
        QTimer* m_updateTimer;
        QNetworkAccessManager* m_networkManager;

        // Cache: objectType:uuid -> metrics
        mutable QMutex m_metricsMutex;
        QMap<QString, MetricValues> m_metricsCache; // "vm:uuid" or "host:uuid" -> values

        bool m_running;
        bool m_paused;
        int m_requestId;

        static const int UPDATE_INTERVAL_MS = 30000; // 30 seconds (matches C#)
        static const int RRD_INTERVAL_SECONDS = 5;   // 5-second data points
};

#endif // METRICUPDATER_H
