#include <QtTest>
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/xenobjecttype.h"
#include "xenlib/ovf/ovfpackage.h"
#include "test_helpers.h"
#include "test_transportserver.h"
#include "xenlib/xen/network/connectionworker.h"
#include "xenlib/xen/jsonrpcclient.h"
#include "xenlib/xen/failure.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/actions/vm/vmstartabstractaction.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/network/heartbeat.h"
#include "xenlib/xen/actions/vdi/movevirtualdiskaction.h"
#include "xenlib/xen/actions/vdi/creatediskaction.h"
#include "xenlib/xen/actions/vbd/vbdcreateandplugaction.h"
#include "xenlib/xen/actions/vdi/migratevirtualdiskaction.h"
#include <QTemporaryFile>
#include <QTextStream>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers: build minimal cache entries to exercise VM methods in isolation
// ─────────────────────────────────────────────────────────────────────────────

static QSharedPointer<VM> makeVm(const QString& ref, const QVariantMap& data)
{
    XenCache* cache = XenCache::GetDummy();
    cache->Update(XenObjectType::VM, ref, data);
    return QSharedPointer<VM>(new VM(nullptr, ref));
}

static QVariantMap normalVm(const QString& powerState = "Halted",
                             const QVariantList& allowedOps = QVariantList{"export"})
{
    QVariantMap d;
    d["power_state"]         = powerState;
    d["is_a_template"]       = false;
    d["is_control_domain"]   = false;
    d["allowed_operations"]  = allowedOps;
    d["name_label"]          = "Test VM";
    return d;
}

class HaTestStartAction : public VMStartAbstractAction
{
public:
    HaTestStartAction(QSharedPointer<VM> vm, WarningDialogHAInvalidConfig warning, bool isStart)
        : VMStartAbstractAction(vm, "Test HA start", warning, nullptr), m_isStart(isStart) {}
    bool IsStart() const override { return this->m_isStart; }
    VMStartAbstractAction* Clone() override { return nullptr; }
    bool performed = false;
protected:
    void run() override { this->startOrResumeVmWithHa(0, 100); }
    void doAction(int, int) override { this->performed = true; }
private:
    bool m_isStart;
};

class XenLibTests : public QObject
{
    Q_OBJECT

private slots:
    void diskDeviceSelection_data()
    {
        QTest::addColumn<bool>("create");
        QTest::addColumn<int>("devices");
        QTest::addColumn<bool>("existingBootable");
        QTest::addColumn<bool>("plugFails");
        QTest::newRow("create-fills-gap-before-dvd") << true << int(TransportTestServer::DiskDevices) << true << false;
        QTest::newRow("attach-fills-gap-before-dvd") << false << int(TransportTestServer::DiskDevices) << true << false;
        QTest::newRow("create-full-no-orphan-vdi") << true << int(TransportTestServer::EmptyDevices) << true << false;
        QTest::newRow("attach-full") << false << int(TransportTestServer::EmptyDevices) << true << false;
        QTest::newRow("first-created-disk-bootable") << true << int(TransportTestServer::BootDevice) << false << false;
        QTest::newRow("keep-existing-bootable-disk") << true << int(TransportTestServer::BootDevice) << true << false;
        QTest::newRow("first-attached-disk-bootable") << false << int(TransportTestServer::BootDevice) << false << false;
        QTest::newRow("propagate-hotplug-error") << true << int(TransportTestServer::DiskDevices) << true << true;
    }

    void diskDeviceSelection()
    {
        if (!QSslSocket::supportsSsl())
            QSKIP("TLS backend unavailable");
        QFETCH(bool, create);
        QFETCH(int, devices);
        QFETCH(bool, existingBootable);
        QFETCH(bool, plugFails);
        const bool full = devices == TransportTestServer::EmptyDevices;
        QVector<TransportTestServer::Reply> replies{TransportTestServer::Login, TransportTestServer::Close,
                                                   TransportTestServer::Reply(devices)};
        if (!full)
        {
            if (create)
                replies.append(TransportTestServer::CreatedVDI);
            replies.append(TransportTestServer::CreatedVBD);
            replies.append(plugFails ? TransportTestServer::PlugAllowed : TransportTestServer::Close);
            replies.append(plugFails ? TransportTestServer::ApiFailure : TransportTestServer::RunningVM);
        }
        TransportTestServer server(QFINDTESTDATA("transport-testdata/server-cert.pem"),
                                   QFINDTESTDATA("transport-testdata/server-key.pem"), replies);
        const int port = server.startListening();
        QVERIFY(port > 0);
        XenConnection connection;
        QVERIFY(connection.ConnectToHost("127.0.0.1", port, "test", "test"));
        QTRY_VERIFY_WITH_TIMEOUT(connection.IsTransportConnected(), 5000);
        XenAPI::Session session(&connection);
        QVERIFY(session.Login("test", "test"));
        session.SetOwnsSessionToken(false);
        connection.SetSession(&session);
        XenCache* cache = connection.GetCache();
        cache->Update(XenObjectType::VM, "OpaqueRef:disk-vm",
                      QVariantMap{{"name_label", "Disk VM"}, {"power_state", "Running"},
                                  {"VBDs", QStringList{"OpaqueRef:disk", "OpaqueRef:dvd"}}});
        cache->Update(XenObjectType::VBD, "OpaqueRef:disk",
                      QVariantMap{{"VM", "OpaqueRef:disk-vm"}, {"userdevice", devices == TransportTestServer::BootDevice ? "2" : "0"},
                                  {"type", "Disk"}, {"bootable", existingBootable}});
        cache->Update(XenObjectType::VBD, "OpaqueRef:dvd",
                      QVariantMap{{"VM", "OpaqueRef:disk-vm"}, {"userdevice", "3"}, {"type", "CD"}});
        const auto vm = cache->ResolveObject<VM>("OpaqueRef:disk-vm");
        QVariantMap vbd{{"VM", vm->OpaqueRef()}, {"VDI", "OpaqueRef:existing-vdi"},
                        {"type", "Disk"}, {"empty", false}, {"mode", "RW"}};
        CreateDiskAction createAction(QVariantMap{{"name_label", "New disk"}}, vbd, vm);
        VbdCreateAndPlugAction attachAction(vm, vbd, "Existing disk");
        QSignalSpy createInstructions(&createAction, &CreateDiskAction::showUserInstruction);
        QSignalSpy attachInstructions(&attachAction, &VbdCreateAndPlugAction::showUserInstruction);
        AsyncOperation* action = create ? static_cast<AsyncOperation*>(&createAction) : &attachAction;
        action->RunSync(&session);
        QCOMPARE(action->HasError(), full || plugFails);
        QCOMPARE(create ? createInstructions.count() : attachInstructions.count(), !full && !plugFails ? 1 : 0);
        QStringList methods;
        for (const QJsonObject& request : server.recordedRequests())
        {
            const QString method = request.value("method").toString();
            methods.append(method);
            if (method == "VBD.create")
            {
                const QJsonObject record = request.value("params").toArray().at(1).toObject();
                QCOMPARE(record.value("userdevice").toString(), devices == TransportTestServer::BootDevice ? QString("0") : QString("1"));
                QCOMPARE(record.value("bootable").toBool(), devices == TransportTestServer::BootDevice && (!create || !existingBootable));
            }
        }
        QCOMPARE(methods.count("VM.get_allowed_VBD_devices"), 1);
        QCOMPARE(methods.count("VDI.create"), create && !full ? 1 : 0);
        QCOMPARE(methods.count("VBD.create"), full ? 0 : 1);
        QCOMPARE(methods.count("VBD.async_plug"), plugFails ? 1 : 0);
        if (create && !full)
            QVERIFY(methods.indexOf("VM.get_allowed_VBD_devices") < methods.indexOf("VDI.create"));
        connection.SetSession(nullptr);
    }

    void protectedVmStart_data()
    {
        QTest::addColumn<bool>("haEnabled");
        QTest::addColumn<QString>("priority");
        QTest::addColumn<int>("agilityReply");
        QTest::addColumn<bool>("accept");
        QTest::addColumn<bool>("clearFails");
        QTest::addColumn<bool>("isStart");
        QTest::newRow("confirm-start") << true << QString("restart") << int(TransportTestServer::AgilityFailure) << true << false << true;
        QTest::newRow("confirm-resume") << true << QString("restart") << int(TransportTestServer::AgilityFailure) << true << false << false;
        QTest::newRow("cancel") << true << QString("restart") << int(TransportTestServer::AgilityFailure) << false << false << true;
        QTest::newRow("clear-fails") << true << QString("restart") << int(TransportTestServer::AgilityFailure) << true << true << true;
        QTest::newRow("agile") << true << QString("restart") << int(TransportTestServer::Close) << true << false << true;
        QTest::newRow("transport-error-is-not-non-agility") << true << QString("restart") << int(TransportTestServer::Truncated) << true << false << true;
        QTest::newRow("permission-error-is-not-non-agility") << true << QString("restart") << int(TransportTestServer::PermissionFailure) << true << false << true;
        QTest::newRow("ha-disabled") << false << QString("restart") << int(TransportTestServer::AgilityFailure) << true << false << true;
        QTest::newRow("best-effort") << true << QString("best-effort") << int(TransportTestServer::AgilityFailure) << true << false << true;
    }

    void protectedVmStart()
    {
        if (!QSslSocket::supportsSsl())
            QSKIP("TLS backend unavailable");
        QFETCH(bool, haEnabled);
        QFETCH(QString, priority);
        QFETCH(int, agilityReply);
        QFETCH(bool, accept);
        QFETCH(bool, clearFails);
        QFETCH(bool, isStart);
        const bool checksAgility = haEnabled && priority == "restart";
        const bool prompts = checksAgility && agilityReply == TransportTestServer::AgilityFailure;
        QVector<TransportTestServer::Reply> replies{TransportTestServer::Login, TransportTestServer::Close};
        if (checksAgility)
            replies.append(TransportTestServer::Reply(agilityReply));
        if (prompts && accept)
            replies.append(clearFails ? TransportTestServer::ApiFailure : TransportTestServer::Close);
        TransportTestServer server(QFINDTESTDATA("transport-testdata/server-cert.pem"),
                                   QFINDTESTDATA("transport-testdata/server-key.pem"), replies);
        const int port = server.startListening();
        QVERIFY(port > 0);
        XenConnection connection;
        QVERIFY(connection.ConnectToHost("127.0.0.1", port, "test", "test"));
        QTRY_VERIFY_WITH_TIMEOUT(connection.IsTransportConnected(), 5000);
        XenAPI::Session session(&connection);
        QVERIFY(session.Login("test", "test"));
        session.SetOwnsSessionToken(false);
        connection.SetSession(&session);
        XenCache* cache = connection.GetCache();
        cache->Update(XenObjectType::Pool, "OpaqueRef:test-pool", QVariantMap{{"ha_enabled", haEnabled}});
        cache->Update(XenObjectType::VM, "OpaqueRef:test-vm",
                      QVariantMap{{"name_label", "HA VM"}, {"power_state", isStart ? "Halted" : "Suspended"},
                                  {"ha_restart_priority", priority}});
        const auto vm = cache->ResolveObject<VM>("OpaqueRef:test-vm");
        bool warned = false;
        bool correctMode = false;
        HaTestStartAction action(vm, [&](QSharedPointer<VM> warnedVm, bool start) {
            warned = true;
            correctMode = warnedVm == vm && start == isStart;
            if (!accept)
                throw VMStartAbstractAction::CancelledByUser();
        }, isStart);
        action.RunSync(&session);
        QCOMPARE(warned, prompts);
        if (prompts)
            QVERIFY(correctMode);
        const bool cancelled = prompts && !accept;
        const bool failed = (checksAgility && agilityReply != TransportTestServer::Close && !prompts) || (prompts && accept && clearFails);
        QCOMPARE(action.GetState(), cancelled ? AsyncOperation::Cancelled : failed ? AsyncOperation::Failed : AsyncOperation::Completed);
        QCOMPARE(action.performed, !cancelled && !failed);

        int clears = 0;
        for (const QJsonObject& request : server.recordedRequests())
        {
            if (request.value("method").toString() == "VM.set_ha_restart_priority")
            {
                ++clears;
                const QJsonArray params = request.value("params").toArray();
                QCOMPARE(params.at(0).toString(), session.GetSessionID());
                QCOMPARE(params.at(1).toString(), vm->OpaqueRef());
                QCOMPARE(params.at(2).toString(), QString());
            }
        }
        QCOMPARE(clears, prompts && accept ? 1 : 0);
    }

    void jsonRpcStrictValidation()
    {
        using Xen::JsonRpcClient;
        QVERIFY(JsonRpcClient::parseJsonRpcResponseOrThrow(R"({"jsonrpc":"2.0","result":null,"id":1})").isNull());
        QCOMPARE(JsonRpcClient::parseJsonRpcResponseOrThrow(R"({"jsonrpc":"2.0","result":false,"id":1})").toBool(), false);
        for (const QByteArray& invalid : {QByteArray(), QByteArray("not JSON"), QByteArray("{}"),
             QByteArray(R"({"jsonrpc":"2.0","id":1})")})
        {
            QVERIFY_EXCEPTION_THROWN(JsonRpcClient::parseJsonRpcResponseOrThrow(invalid), std::runtime_error);
        }
        try
        {
            JsonRpcClient::parseJsonRpcResponseOrThrow(R"({"jsonrpc":"2.0","error":{"code":1,"message":"HOST_NOT_ENOUGH_FREE_MEMORY","data":["1024","512"]},"id":1})");
            QFAIL("API rejection was accepted");
        } catch (const Failure& failure)
        {
            QCOMPARE(failure.errorCode(), QString("HOST_NOT_ENOUGH_FREE_MEMORY"));
            QCOMPARE(failure.errorDescription().size(), 3);
            QCOMPARE(QString::fromUtf8(failure.what()), failure.message());
        }
    }

    void transportReconnectsBeforeNextRequest_data()
    {
        QTest::addColumn<int>("reply");
        QTest::newRow("announced-close") << int(TransportTestServer::Close);
        QTest::newRow("idle-peer-close") << int(TransportTestServer::PeerClose);
        QTest::newRow("chunked-close") << int(TransportTestServer::Chunked);
    }

    void transportReconnectsBeforeNextRequest()
    {
        if (!QSslSocket::supportsSsl())
            QSKIP("TLS backend unavailable");
        QFETCH(int, reply);
        TransportTestServer server(QFINDTESTDATA("transport-testdata/server-cert.pem"),
                                   QFINDTESTDATA("transport-testdata/server-key.pem"),
                                   {TransportTestServer::Reply(reply), TransportTestServer::Close});
        const int port = server.startListening();
        QVERIFY(port > 0);
        Xen::ConnectionWorker worker("127.0.0.1", port);
        worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(worker.IsTransportConnected(), 5000);
        const QByteArray request = Xen::JsonRpcClient::buildJsonRpcCall("VM.assert_can_boot_here", {}, 1);
        for (int i = 0; i < 2; ++i)
        {
            const int id = worker.QueueRequest(request, false);
            const QByteArray response = worker.WaitForResponse(id, 5000);
            QVERIFY(Xen::JsonRpcClient::parseJsonRpcResponseOrThrow(response).isNull());
            // Let the peer's close arrive before the next request.
            QTest::qWait(50);
        }
        QCOMPARE(server.requests(), 2);
        QCOMPARE(server.connections(), 2);
    }

    void transportFailureDoesNotReplayMutation_data()
    {
        QTest::addColumn<int>("reply");
        QTest::newRow("truncated-body") << int(TransportTestServer::Truncated);
        QTest::newRow("invalid-json") << int(TransportTestServer::Malformed);
        QTest::newRow("http-error") << int(TransportTestServer::HttpError);
    }

    void transportFailureDoesNotReplayMutation()
    {
        if (!QSslSocket::supportsSsl())
            QSKIP("TLS backend unavailable");
        QFETCH(int, reply);
        TransportTestServer server(QFINDTESTDATA("transport-testdata/server-cert.pem"),
                                   QFINDTESTDATA("transport-testdata/server-key.pem"),
                                   {TransportTestServer::Reply(reply), TransportTestServer::Close});
        const int port = server.startListening();
        QVERIFY(port > 0);
        Xen::ConnectionWorker worker("127.0.0.1", port);
        worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(worker.IsTransportConnected(), 5000);
        const QByteArray mutation = Xen::JsonRpcClient::buildJsonRpcCall("VM.start", {}, 1);
        const QByteArray failed = worker.WaitForResponse(worker.QueueRequest(mutation, false), 5000);
        const QJsonObject error = QJsonDocument::fromJson(failed).object().value("error").toObject();
        QCOMPARE(error.value("message").toString(), QString("TRANSPORT_ERROR"));
        QVERIFY(error.value("data").toArray().first().toString().contains("outcome is unknown"));
        QVERIFY_EXCEPTION_THROWN(Xen::JsonRpcClient::parseJsonRpcResponseOrThrow(failed), Failure);
        QVERIFY(!worker.IsTransportConnected());
        QCOMPARE(server.requests(), 1);

        const QByteArray read = Xen::JsonRpcClient::buildJsonRpcCall("VM.get_record", {}, 2);
        const QByteArray response = worker.WaitForResponse(worker.QueueRequest(read, false), 5000);
        QVERIFY(Xen::JsonRpcClient::parseJsonRpcResponseOrThrow(response).isNull());
        QCOMPARE(server.requests(), 2);
        QCOMPARE(server.connections(), 2);
    }

    void transportPreservesApiFailure()
    {
        if (!QSslSocket::supportsSsl())
            QSKIP("TLS backend unavailable");
        TransportTestServer server(QFINDTESTDATA("transport-testdata/server-cert.pem"),
                                   QFINDTESTDATA("transport-testdata/server-key.pem"),
                                   {TransportTestServer::ApiFailure, TransportTestServer::Close});
        const int port = server.startListening();
        QVERIFY(port > 0);
        Xen::ConnectionWorker worker("127.0.0.1", port);
        worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(worker.IsTransportConnected(), 5000);
        const QByteArray request = Xen::JsonRpcClient::buildJsonRpcCall("VM.assert_can_boot_here", {}, 1);
        const QByteArray response = worker.WaitForResponse(worker.QueueRequest(request, false), 5000);
        QCOMPARE(QJsonDocument::fromJson(response).object().value("error").toObject().value("message").toString(),
                 QString("HOST_NOT_ENOUGH_FREE_MEMORY"));
        QVERIFY(worker.IsTransportConnected());
        const QByteArray next = worker.WaitForResponse(worker.QueueRequest(request, false), 5000);
        QVERIFY(Xen::JsonRpcClient::parseJsonRpcResponseOrThrow(next).isNull());
        QCOMPARE(server.connections(), 1);
    }

    void timedOutQueuedRequestIsNeverSent()
    {
        if (!QSslSocket::supportsSsl())
            QSKIP("TLS backend unavailable");
        TransportTestServer server(QFINDTESTDATA("transport-testdata/server-cert.pem"),
                                   QFINDTESTDATA("transport-testdata/server-key.pem"), {TransportTestServer::Close});
        const int port = server.startListening();
        QVERIFY(port > 0);
        Xen::ConnectionWorker worker("127.0.0.1", port);
        const QByteArray request = Xen::JsonRpcClient::buildJsonRpcCall("VM.start", {}, 1);
        const QByteArray cancelled = worker.WaitForResponse(worker.QueueRequest(request, false), 1);
        QVERIFY(cancelled.contains("Request was not sent"));
        worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(worker.IsTransportConnected(), 5000);
        const QByteArray response = worker.WaitForResponse(worker.QueueRequest(request, false), 5000);
        QVERIFY(Xen::JsonRpcClient::parseJsonRpcResponseOrThrow(response).isNull());
        QCOMPARE(server.requests(), 1);
    }

    void connectionManagerWaitsForLogicalReadiness()
    {
        XenConnection connection;
        auto* manager = Xen::ConnectionsManager::instance();
        manager->AddConnection(&connection);
        QSignalSpy ready(manager, &Xen::ConnectionsManager::connectionStateChanged);

        // Simulate transport establishment without login/cache completion.
        const bool established = QMetaObject::invokeMethod(&connection, "onWorkerEstablished", Qt::DirectConnection);
        const int beforeLogin = ready.count();
        connection.ConnectionResult(true, QString());
        const int afterLogin = ready.count();
        connection.EndConnect();
        manager->RemoveConnection(&connection);

        QVERIFY(established);
        QCOMPARE(beforeLogin, 0);
        QCOMPARE(afterLogin, 1);
    }

    void heartbeatReportsTwoConsecutiveSessionFailures()
    {
        XenConnection connection;
        QVERIFY(QMetaObject::invokeMethod(&connection, "onWorkerEstablished", Qt::DirectConnection));
        // A connected transport without a logged-in session cannot create a probe.
        XenHeartbeat heartbeat(&connection, 15000);
        QSignalSpy lost(&heartbeat, &XenHeartbeat::connectionLost);
        heartbeat.start();
        QVERIFY(heartbeat.isRunning());

        QVERIFY(QMetaObject::invokeMethod(&heartbeat, "performHeartbeat", Qt::DirectConnection));
        QCOMPARE(lost.count(), 0);
        QVERIFY(heartbeat.isRunning());
        QVERIFY(QMetaObject::invokeMethod(&heartbeat, "performHeartbeat", Qt::DirectConnection));
        QCOMPARE(lost.count(), 1);
        QVERIFY(!heartbeat.isRunning());
    }

    void diskActionsUseCachedMetadataWithoutSession()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        const QString vdiRef = "OpaqueRef:metadata-vdi";
        const QString sourceSrRef = "OpaqueRef:metadata-source";
        const QString targetSrRef = "OpaqueRef:metadata-target";
        cache->Update(XenObjectType::SR, sourceSrRef, QVariantMap{{"name_label", "Source SR"}});
        cache->Update(XenObjectType::SR, targetSrRef, QVariantMap{{"name_label", "Target SR"}});
        cache->Update(XenObjectType::VDI, vdiRef,
                      QVariantMap{{"name_label", "Cached disk"}, {"SR", sourceSrRef}, {"type", "suspend"}});
        QVERIFY(!connection.GetSession());

        MoveVirtualDiskAction move(&connection, vdiRef, targetSrRef);
        MigrateVirtualDiskAction migrate(&connection, vdiRef, targetSrRef);
        for (const AsyncOperation* action : {static_cast<AsyncOperation*>(&move), static_cast<AsyncOperation*>(&migrate)})
        {
            QVERIFY(action->GetDescription().contains("Cached disk"));
            QVERIFY(action->GetDescription().contains("Source SR"));
            QVERIFY(action->GetDescription().contains("Target SR"));
        }
        QVERIFY(move.GetApiMethodsToRoleCheck().contains("vm.set_suspend_VDI"));
    }

    void diskActionMetadataFallsBackToReferences()
    {
        MoveVirtualDiskAction move(nullptr, "missing-vdi", "missing-sr");
        MigrateVirtualDiskAction migrate(nullptr, "missing-vdi", "missing-sr");
        for (const AsyncOperation* action : {static_cast<AsyncOperation*>(&move), static_cast<AsyncOperation*>(&migrate)})
        {
            QVERIFY(action->GetDescription().contains("missing-vdi"));
            QVERIFY(action->GetDescription().contains("missing-sr"));
        }
    }

    // ── cache round-trip ──────────────────────────────────────────────────────
    void loadCacheFromResource_populatesVmData()
    {
        XenCache* cache = LoadCacheFromEventJson(":/testdata/xenapi.json");
        QVERIFY(cache != nullptr);

        const QStringList vmRefs = cache->GetAllRefs(XenObjectType::VM);
        QVERIFY(!vmRefs.isEmpty());

        QSharedPointer<VM> vm = MakeObjectFromDummyCache<VM>(vmRefs.first());
        QVERIFY(!vm.isNull());
        QVERIFY(!vm->GetName().isEmpty());
    }

    // ── IsXvaExportable ───────────────────────────────────────────────────────

    void isXvaExportable_haltedWithExportOp_returnsTrue()
    {
        auto vm = makeVm("ref-1", normalVm("Halted"));
        QVERIFY(vm->IsXvaExportable());
    }

    void isXvaExportable_runningWithExportOp_returnsTrue()
    {
        // XVA export is allowed from any power state as long as "export" is present.
        auto vm = makeVm("ref-2", normalVm("Running"));
        QVERIFY(vm->IsXvaExportable());
    }

    void isXvaExportable_noExportOp_returnsFalse()
    {
        auto vm = makeVm("ref-3", normalVm("Halted", QVariantList{}));
        QVERIFY(!vm->IsXvaExportable());
    }

    void isXvaExportable_template_returnsFalse()
    {
        QVariantMap d = normalVm("Halted");
        d["is_a_template"] = true;
        auto vm = makeVm("ref-4", d);
        QVERIFY(!vm->IsXvaExportable());
    }

    void isXvaExportable_controlDomain_returnsFalse()
    {
        QVariantMap d = normalVm("Running");
        d["is_control_domain"] = true;
        auto vm = makeVm("ref-5", d);
        QVERIFY(!vm->IsXvaExportable());
    }

    // snapshot: is_a_template=true && power_state=Suspended in XenAPI convention;
    // we store is_a_template=true for snapshot records — it blocks export.
    void isXvaExportable_snapshot_returnsFalse()
    {
        QVariantMap d = normalVm("Suspended");
        d["is_a_snapshot"] = true;
        d["is_a_template"] = false;
        // allowed_operations for a snapshot won't include "export", but we also
        // check is_a_snapshot via IsSnapshot(). Either path blocks export.
        d["allowed_operations"] = QVariantList{"export"};
        auto vm = makeVm("ref-6", d);
        // Snapshots are filtered by IsSnapshot() → IsXvaExportable() returns false.
        QVERIFY(!vm->IsXvaExportable());
    }

    // ── IsOvfExportable ───────────────────────────────────────────────────────

    void isOvfExportable_haltedWithExportOp_returnsTrue()
    {
        auto vm = makeVm("ref-10", normalVm("Halted"));
        QVERIFY(vm->IsOvfExportable());
    }

    void isOvfExportable_suspendedWithExportOp_returnsTrue()
    {
        auto vm = makeVm("ref-11", normalVm("Suspended"));
        QVERIFY(vm->IsOvfExportable());
    }

    void isOvfExportable_running_returnsFalse()
    {
        // Running VMs cannot be captured into OVF (C# IsVmExportable filter).
        auto vm = makeVm("ref-12", normalVm("Running"));
        QVERIFY(!vm->IsOvfExportable());
    }

    void isOvfExportable_paused_returnsFalse()
    {
        auto vm = makeVm("ref-13", normalVm("Paused"));
        QVERIFY(!vm->IsOvfExportable());
    }

    void isOvfExportable_noExportOp_returnsFalse()
    {
        auto vm = makeVm("ref-14", normalVm("Halted", QVariantList{}));
        QVERIFY(!vm->IsOvfExportable());
    }

    void isOvfExportable_template_returnsFalse()
    {
        QVariantMap d = normalVm("Halted");
        d["is_a_template"] = true;
        auto vm = makeVm("ref-15", d);
        QVERIFY(!vm->IsOvfExportable());
    }

    void isOvfExportable_controlDomain_returnsFalse()
    {
        QVariantMap d = normalVm("Halted");
        d["is_control_domain"] = true;
        auto vm = makeVm("ref-16", d);
        QVERIFY(!vm->IsOvfExportable());
    }

    // ── IsRealVM ─────────────────────────────────────────────────────────────

    void isRealVM_normalVm_returnsTrue()
    {
        auto vm = makeVm("ref-20", normalVm());
        QVERIFY(vm->IsRealVM());
    }

    void isRealVM_template_returnsFalse()
    {
        QVariantMap d = normalVm();
        d["is_a_template"] = true;
        auto vm = makeVm("ref-21", d);
        QVERIFY(!vm->IsRealVM());
    }

    void isRealVM_controlDomain_returnsFalse()
    {
        QVariantMap d = normalVm();
        d["is_control_domain"] = true;
        auto vm = makeVm("ref-22", d);
        QVERIFY(!vm->IsRealVM());
    }

    // ── OvfPackage::XenConfigBySystem — Qt OvfWriter format ──────────────────
    // Format: <xen:Data><xen:Name>key</xen:Name><xen:Value>val</xen:Value></xen:Data>
    // Namespace: http://www.citrix.com/xencenter/2009/xen

    void ovfXenConfig_qtFormat_parsesHvmBootPolicy()
    {
        const QString xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:xen="http://www.citrix.com/xencenter/2009/xen"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="myvm">
    <Name>myvm</Name>
    <VirtualHardwareSection>
      <xen:Data ovf:required="false">
        <xen:Name>HVM_boot_policy</xen:Name>
        <xen:Value>BIOS order</xen:Value>
      </xen:Data>
      <xen:Data ovf:required="false">
        <xen:Name>platform</xen:Name>
        <xen:Value>nx=true;acpi=1</xen:Value>
      </xen:Data>
    </VirtualHardwareSection>
  </VirtualSystem>
</Envelope>)";

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        QVERIFY(cfg.contains("myvm"));
        QCOMPARE(cfg["myvm"]["HVM_boot_policy"], QString("BIOS order"));
        QCOMPARE(cfg["myvm"]["platform"], QString("nx=true;acpi=1"));
    }

    void ovfXenConfig_qtFormat_pvBootloader()
    {
        const QString xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:xen="http://www.citrix.com/xencenter/2009/xen"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="pvvm">
    <Name>pvvm</Name>
    <VirtualHardwareSection>
      <xen:Data ovf:required="false">
        <xen:Name>PV_bootloader</xen:Name>
        <xen:Value>pygrub</xen:Value>
      </xen:Data>
      <xen:Data ovf:required="false">
        <xen:Name>PV_args</xen:Name>
        <xen:Value>quiet splash</xen:Value>
      </xen:Data>
    </VirtualHardwareSection>
  </VirtualSystem>
</Envelope>)";

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        QVERIFY(cfg.contains("pvvm"));
        QCOMPARE(cfg["pvvm"]["PV_bootloader"], QString("pygrub"));
        QCOMPARE(cfg["pvvm"]["PV_args"], QString("quiet splash"));
        QVERIFY(!cfg["pvvm"].contains("HVM_boot_policy"));
    }

    // ── OvfPackage::XenConfigBySystem — C# XenAdmin format ───────────────────
    // Format: <citrix:VirtualSystemOtherConfigurationData Name="key"><Value>v</Value>...
    // Namespace: http://schemas.citrix.com/ovf/envelope/1

    void ovfXenConfig_csFormat_parsesBootPolicy()
    {
        const QString xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:citrix="http://schemas.citrix.com/ovf/envelope/1"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="csvm">
    <Name>csvm</Name>
    <VirtualHardwareSection>
      <citrix:VirtualSystemOtherConfigurationData Name="HVM_boot_policy">
        <Value>BIOS order</Value>
      </citrix:VirtualSystemOtherConfigurationData>
      <citrix:VirtualSystemOtherConfigurationData Name="HVM_boot_params">
        <Value>order=dc</Value>
      </citrix:VirtualSystemOtherConfigurationData>
    </VirtualHardwareSection>
  </VirtualSystem>
</Envelope>)";

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        QVERIFY(cfg.contains("csvm"));
        QCOMPARE(cfg["csvm"]["HVM_boot_policy"], QString("BIOS order"));
        QCOMPARE(cfg["csvm"]["HVM_boot_params"], QString("order=dc"));
    }

    void ovfXenConfig_csFormat_recommendationsKeyExcluded()
    {
        // The "recommendations" entry must be skipped by parseXenConfig (handled by parseRecommendations).
        const QString xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:citrix="http://schemas.citrix.com/ovf/envelope/1"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="vm1">
    <Name>vm1</Name>
    <VirtualHardwareSection>
      <citrix:VirtualSystemOtherConfigurationData Name="recommendations">
        <Value><restrictions><restriction field="vcpus-max" value="8"/></restrictions></Value>
      </citrix:VirtualSystemOtherConfigurationData>
      <citrix:VirtualSystemOtherConfigurationData Name="HVM_boot_policy">
        <Value>BIOS order</Value>
      </citrix:VirtualSystemOtherConfigurationData>
    </VirtualHardwareSection>
  </VirtualSystem>
</Envelope>)";

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        QVERIFY(cfg.contains("vm1"));
        QVERIFY(!cfg["vm1"].contains("recommendations"));
        QCOMPARE(cfg["vm1"]["HVM_boot_policy"], QString("BIOS order"));
    }

    // ── OvfPackage::XenConfigBySystem — no xen config section ────────────────

    void ovfXenConfig_noXenSection_returnsEmptyMap()
    {
        const QString xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="plainvm">
    <Name>plainvm</Name>
    <VirtualHardwareSection/>
  </VirtualSystem>
</Envelope>)";

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        // System is present (from VirtualSystem element) but config map is empty
        QVERIFY(cfg.contains("plainvm"));
        QVERIFY(cfg["plainvm"].isEmpty());
    }

    // ── OvfPackage::XenConfigBySystem — multi-VM package ─────────────────────

    void ovfXenConfig_multipleVms_configsAreSeparate()
    {
        const QString xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:xen="http://www.citrix.com/xencenter/2009/xen"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystemCollection ovf:id="myvapp">
    <VirtualSystem ovf:id="vm-hvm">
      <Name>vm-hvm</Name>
      <VirtualHardwareSection>
        <xen:Data ovf:required="false">
          <xen:Name>HVM_boot_policy</xen:Name>
          <xen:Value>BIOS order</xen:Value>
        </xen:Data>
      </VirtualHardwareSection>
    </VirtualSystem>
    <VirtualSystem ovf:id="vm-pv">
      <Name>vm-pv</Name>
      <VirtualHardwareSection>
        <xen:Data ovf:required="false">
          <xen:Name>PV_bootloader</xen:Name>
          <xen:Value>pygrub</xen:Value>
        </xen:Data>
      </VirtualHardwareSection>
    </VirtualSystem>
  </VirtualSystemCollection>
</Envelope>)";

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        QVERIFY(cfg.contains("vm-hvm"));
        QVERIFY(cfg.contains("vm-pv"));
        QCOMPARE(cfg["vm-hvm"]["HVM_boot_policy"], QString("BIOS order"));
        QVERIFY(!cfg["vm-hvm"].contains("PV_bootloader"));
        QCOMPARE(cfg["vm-pv"]["PV_bootloader"], QString("pygrub"));
        QVERIFY(!cfg["vm-pv"].contains("HVM_boot_policy"));
    }

    void ovfXenConfig_emptyValueAllowed()
    {
        // A key with an empty value should still be stored (it may override a default).
        const QString xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:xen="http://www.citrix.com/xencenter/2009/xen"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="uefivm">
    <Name>uefivm</Name>
    <VirtualHardwareSection>
      <xen:Data ovf:required="false">
        <xen:Name>HVM_boot_policy</xen:Name>
        <xen:Value></xen:Value>
      </xen:Data>
    </VirtualHardwareSection>
  </VirtualSystem>
</Envelope>)";

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        QVERIFY(cfg.contains("uefivm"));
        QVERIFY(cfg["uefivm"].contains("HVM_boot_policy"));
        QCOMPARE(cfg["uefivm"]["HVM_boot_policy"], QString(""));
    }
};

QTEST_GUILESS_MAIN(XenLibTests)
#include "test_main.moc"
