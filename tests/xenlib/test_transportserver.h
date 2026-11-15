#ifndef TEST_TRANSPORTSERVER_H
#define TEST_TRANSPORTSERVER_H

#include <QAtomicInt>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <utility>
#include <QJsonObject>
#include <QSemaphore>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QThread>
#include <QVector>

// Local TLS peer for transport tests. It counts received requests, so an
// accidental replay of a mutation is observable without a real XAPI server.
class TransportTestServer : public QThread
{
public:
    enum Reply { Close, PeerClose, Truncated, Malformed, HttpError, Chunked, ApiFailure };

    TransportTestServer(const QString& certPath, const QString& keyPath, QVector<Reply> replies)
        : m_replies(std::move(replies))
    {
        QFile cert(certPath), key(keyPath);
        if (cert.open(QIODevice::ReadOnly) && key.open(QIODevice::ReadOnly))
        {
            this->m_certificate = QSslCertificate(cert.readAll());
            this->m_key = QSslKey(key.readAll(), QSsl::Rsa);
        }
    }

    ~TransportTestServer() override
    {
        this->requestInterruption();
        this->wait();
    }

    int startListening()
    {
        if (this->m_certificate.isNull() || this->m_key.isNull())
            return 0;
        this->start();
        return this->m_ready.tryAcquire(1, 3000) ? this->m_port.loadAcquire() : 0;
    }

    int requests() const { return this->m_requests.loadAcquire(); }
    int connections() const { return this->m_connections.loadAcquire(); }

protected:
    void run() override
    {
        class Listener : public QTcpServer
        {
        public:
            QSslCertificate certificate;
            QSslKey key;
        protected:
            void incomingConnection(qintptr descriptor) override
            {
                auto* socket = new QSslSocket(this);
                socket->setLocalCertificate(this->certificate);
                socket->setPrivateKey(this->key);
                socket->setPeerVerifyMode(QSslSocket::VerifyNone);
                if (!socket->setSocketDescriptor(descriptor))
                {
                    delete socket;
                    return;
                }
                socket->startServerEncryption();
                this->addPendingConnection(socket);
            }
        } server;
        server.certificate = this->m_certificate;
        server.key = this->m_key;
        if (server.listen(QHostAddress::LocalHost, 0))
            this->m_port.storeRelease(server.serverPort());
        this->m_ready.release();

        while (server.isListening() && !this->isInterruptionRequested())
        {
            if (!server.hasPendingConnections() && !server.waitForNewConnection(100))
                continue;
            auto* socket = qobject_cast<QSslSocket*>(server.nextPendingConnection());
            if (!socket)
                continue;
            this->m_connections.fetchAndAddOrdered(1);
            if (!socket->isEncrypted() && !socket->waitForEncrypted(2000))
            {
                delete socket;
                continue;
            }
            QByteArray input;
            while (!this->isInterruptionRequested() && socket->state() == QAbstractSocket::ConnectedState)
            {
                if (socket->bytesAvailable() == 0 && !socket->waitForReadyRead(100))
                    continue;
                input += socket->readAll();
                const int end = input.indexOf("\r\n\r\n");
                if (end < 0)
                    continue;
                int length = 0;
                for (const QByteArray& line : input.left(end).split('\n'))
                {
                    if (line.toLower().startsWith("content-length:"))
                        length = line.mid(line.indexOf(':') + 1).trimmed().toInt();
                }
                if (input.size() < end + 4 + length)
                    continue;
                const QJsonObject request = QJsonDocument::fromJson(input.mid(end + 4, length)).object();
                input.remove(0, end + 4 + length);
                const int index = this->m_requests.fetchAndAddOrdered(1);
                const Reply reply = this->m_replies.value(index, Close);
                QByteArray body = QJsonDocument(QJsonObject{{"jsonrpc", "2.0"}, {"id", request.value("id")},
                                                           {"result", QJsonValue(QJsonValue::Null)}}).toJson(QJsonDocument::Compact);
                if (reply == Malformed)
                    body = "not JSON";
                if (reply == ApiFailure)
                    body = QJsonDocument(QJsonObject{{"jsonrpc", "2.0"}, {"id", request.value("id")},
                        {"error", QJsonObject{{"code", 1}, {"message", "HOST_NOT_ENOUGH_FREE_MEMORY"},
                                             {"data", QJsonArray{"1024", "512"}}}}}).toJson(QJsonDocument::Compact);

                QByteArray response = reply == HttpError ? "HTTP/1.1 503 Unavailable\r\n" : "HTTP/1.1 200 OK\r\n";
                response += (reply == PeerClose || reply == ApiFailure) ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
                if (reply == Chunked)
                {
                    response += "Transfer-Encoding: chunked\r\n\r\n";
                    response += QByteArray::number(body.size(), 16) + "\r\n" + body + "\r\n0\r\n\r\n";
                } else
                {
                    response += "Content-Length: " + QByteArray::number(body.size() + (reply == Truncated ? 20 : 0));
                    response += "\r\n\r\n" + body;
                }
                socket->write(response);
                while (socket->bytesToWrite() > 0 || socket->encryptedBytesToWrite() > 0)
                {
                    if (!socket->waitForBytesWritten(2000))
                        break;
                }
                if (reply != ApiFailure)
                {
                    socket->disconnectFromHost();
                    if (socket->state() != QAbstractSocket::UnconnectedState)
                        socket->waitForDisconnected(2000);
                    break;
                }
            }
            delete socket;
        }
    }

private:
    QVector<Reply> m_replies;
    QSslCertificate m_certificate;
    QSslKey m_key;
    QSemaphore m_ready;
    QAtomicInt m_port = 0;
    QAtomicInt m_requests = 0;
    QAtomicInt m_connections = 0;
};

#endif
