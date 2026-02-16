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

#ifndef PERFMONDEFINITIONACTION_H
#define PERFMONDEFINITIONACTION_H

#include "../../asyncoperation.h"
#include "../../../xenlib_global.h"
#include <QList>
#include <QVariantMap>

class XenObject;
class Host;

class XENLIB_EXPORT PerfmonDefinitionAction : public AsyncOperation
{
    Q_OBJECT

    public:
        struct Definition
        {
            QString name;
            double threshold = 0.0;
            int durationSeconds = 0;
            int intervalSeconds = 0;
        };

        PerfmonDefinitionAction(XenConnection* connection,
                                const QString& objectRef,
                                const QString& objectType,
                                const QList<Definition>& definitions,
                                bool suppressHistory = true,
                                QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        static constexpr const char* PERFMON_KEY = "perfmon";
        static constexpr const char* PERFMON_DOM0_MEMORY = "mem_usage";
        static constexpr const char* PERFMON_PLUGIN = "perfmon";
        static constexpr const char* PERFMON_PLUGIN_REFRESH = "refresh";
        static constexpr const char* PERFMON_PLUGIN_START = "start";
        static constexpr const char* PERFMON_NOT_RUNNING_ERROR = "ERROR_111";

        void applyDefinitionsToObject(const QString& objectType,
                                      const QString& objectRef,
                                      const QList<Definition>& definitions);
        void setOtherConfigKey(const QString& objectType,
                               const QString& objectRef,
                               const QString& key,
                               const QString& value);
        void removeFromOtherConfig(const QString& objectType,
                                   const QString& objectRef,
                                   const QString& key);
        QVariantMap getCurrentOtherConfig(const QString& objectType, const QString& objectRef) const;
        void setOtherConfig(const QString& objectType, const QString& objectRef, const QVariantMap& otherConfig);

        void callApiVoid(const QString& method, const QVariantList& params);
        QList<Definition> parseDefinitions(const QString& perfmonXml) const;
        QString buildDefinitionsXml(const QList<Definition>& definitions) const;

        QSharedPointer<XenObject> resolveTargetObject() const;
        QList<QSharedPointer<Host>> collectRefreshHosts(const QSharedPointer<XenObject>& object) const;
        void refreshPerfmonOnHosts(const QList<QSharedPointer<Host>>& hosts);

        QString m_objectRef;
        QString m_objectType;
        QList<Definition> m_definitions;
};

#endif // PERFMONDEFINITIONACTION_H
