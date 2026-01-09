#include <QDataStream>
#include <QLocalSocket>
#include <QSslSocket>
#include <QSet>
#include <QMutex>

#include "qxfssocketstream.h"

Q_GLOBAL_STATIC(QSet<QString>, warnOnce)
Q_GLOBAL_STATIC(QMutex, warnOnceMutex)

QIODevice *
QXfsSocketStream::createSocket(const QString &deviceAddress,
                               const QString &deviceId)
{
    m_socket = nullptr;
    m_host = new QString();
    m_isLocal = (deviceAddress == "local");
    m_isSsl = false;

    if (m_isLocal)
    {
        QLocalSocket *socket = new QLocalSocket;

        m_socket = socket;

        socket->setServerName("printec.ndc.device." + deviceId);
        socket->connectToServer();
    }
    else
    {
        QSslSocket *socket;

        if (deviceAddress.startsWith("tcp://") ||
            (m_isSsl = deviceAddress.startsWith("ssl://")))
        {
            socket = new QSslSocket;
        }
        else
        {
bad_address:
            qCritical("%s: unknown device connection address %s",
                      qPrintable(deviceId), qPrintable(deviceAddress));
            return nullptr;
        }

        m_socket = socket;

        QStringList l = deviceAddress.mid(6).split(":");

        if (l.size() != 2)
            goto bad_address;

        bool ok;

        *m_host = l.at(0);
        m_port = l.at(1).toUInt(&ok);

        if (!ok)
            goto bad_address;

        if (m_isSsl)
            socket->connectToHostEncrypted(*m_host, m_port);
        else
            socket->connectToHost(*m_host, m_port);
    }

    return m_socket;
}

QXfsSocketStream::QXfsSocketStream(const QString &deviceAddress,
                                   const QString &deviceId,
                                   const QString &strClass,
                                   QObject *parent) :
    QXfsStream(createSocket(deviceAddress, deviceId), deviceId, strClass,
               parent)
{
    connect(m_socket, SIGNAL(disconnected()), SLOT(disconnected()));
}

QXfsSocketStream::~QXfsSocketStream()
{
    delete m_host;
    m_socket->disconnect();
    m_socket->deleteLater();
}

void
QXfsSocketStream::disconnected()
{
    QMap<QString, QString> commands = pending();
    QMap<QString, QString>::const_iterator it;
    QVariantMap msg = {{"hResult", "WFS_ERR_CONNECTION_LOST"}};

    for (it = commands.cbegin(); it != commands.cend(); it++)
    {
        msg["msgid"] = it.key();
        msg["dwCommandCode"] = it.value();

        emit message(msg);
    }
}

bool
QXfsSocketStream::connectToServer(QIODevice *io)
{
    QString errorString;

    if (m_isLocal)
    {
        QLocalSocket *socket = static_cast<QLocalSocket *>(io);

        if (socket->state() != QLocalSocket::ConnectedState)
        {
            socket->connectToServer();

            if (!socket->waitForConnected())
            {
                errorString = socket->errorString();
                goto conn_err;
            }
        }
    }
    else
    {
        QSslSocket *socket = static_cast<QSslSocket *>(io);
        bool connected;

        if (socket->state() != QAbstractSocket::ConnectedState ||
            (m_isSsl && !socket->isEncrypted()))
        {
            if (m_isSsl)
            {
                if (socket->state() == QAbstractSocket::UnconnectedState ||
                    socket->state() == QAbstractSocket::ClosingState)
                {
                    socket->connectToHostEncrypted(*m_host, m_port);
                }

                connected = socket->waitForEncrypted();
            }
            else
            {
                if (socket->state() == QAbstractSocket::UnconnectedState ||
                    socket->state() == QAbstractSocket::ClosingState)
                {
                    socket->connectToHost(*m_host, m_port);
                }

                connected = socket->waitForConnected();
            }

            if (!connected)
            {
                errorString = socket->errorString();
                goto conn_err;
            }
        }
    }

    return true;

conn_err:
    {
        QMutexLocker lock(warnOnceMutex);
        const auto &on {objectName()};

        if (!warnOnce->contains(on))
        {
            warnOnce->insert(on);

            qWarning() << on << " - unable to connect to device server: "
                       << errorString;
        }
    }

    return false;
}
