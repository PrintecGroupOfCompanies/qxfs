#include <QDataStream>
#include <QEventLoop>
#include <QMutex>
#include <QUuid>
#include <QDebug>

#include "qxfsstream.h"

/**
 * @brief Global mutex protecting device capabilities map.
 */
Q_GLOBAL_STATIC(QMutex, capabilitiesMutex)

QMap<QString, QVariantMap> QXfsStream::m_capabilities;

/**
 * @brief Global mutex protecting device list.
 */
Q_GLOBAL_STATIC(QMutex, deviceMutex)

/**
 * @brief Global list of all QXfsStream instances.
 */
Q_GLOBAL_STATIC(QList<QXfsStream *>, devices)

QXfsStream::QXfsStream(QIODevice *io,
                       const QString &deviceId,
                       const QString &strClass, QObject *parent) :
    QObject{parent},
    m_io(io),
    m_strClass(strClass.toUpper())
{
    Q_ASSERT(m_strClass.length() == 3);
    Q_ASSERT(m_io);

    setObjectName(deviceId);

    {
        QMutexLocker lock(deviceMutex);
        devices->append(this);
    }

    m_statusCategory = "WFS_INF_" + m_strClass + "_STATUS";
    m_capabilitiesCategory = "WFS_INF_" + m_strClass + "_CAPABILITIES";

    connect(this, &QXfsStream::message,
    [this](QVariantMap msg)
    {
        QString message = msg["message"].toString();

        if (message == "WFS_SERVICE_EVENT")
        {
            serviceEvent(msg);
            emit serviceEventRecieved(msg);
        }
        else if (message == "WFS_USER_EVENT")
        {
            userEvent(msg);
            emit userEventRecieved(msg);
        }
        else if (message == "WFS_SYSTEM_EVENT")
        {
            QString dwCommand;
            QVariant lpCmdData;

            {
                const QPair<QString, QVariant> &cmd = currentCommand();

                dwCommand = cmd.first;
                lpCmdData = cmd.second;
            }

            systemEvent(msg);
            emit systemEventRecieved(msg, dwCommand, lpCmdData);
        }
    });

    connect(m_io, SIGNAL(readyRead()), SLOT(readyRead()));
}

QXfsStream::~QXfsStream()
{
    {
        QMutexLocker lock(deviceMutex);
        devices->removeOne(this);
    }
}

void
QXfsStream::readyRead()
{
    for (;;)
    {
        QVariantMap msg;
        QDataStream ds(m_io);

        ds.setByteOrder(QDataStream::BigEndian);
        ds.startTransaction();
        ds >> msg;

        if (!ds.commitTransaction())
            return;

        emit message(msg);
    }
}

/**
 * @typedef QJsCommandMap
 * @brief Map storing commands per device.
 *
 * Key: device object name.
 * Value: list of command pairs <dwCommand, lpCmdData>.
 */
using QJsCommandMap = QMap<QString, QList<QPair<QString, QVariant>>>;
Q_GLOBAL_STATIC(QJsCommandMap, commands);

/**
 * @brief Mutex protecting command map.
 */
Q_GLOBAL_STATIC(QMutex, commandMutex)

QPair<QString, QVariant>
QXfsStream::currentCommand() const
{
    QMutexLocker lock(commandMutex);

    if (!(*commands)[objectName()].isEmpty())
        return (*commands)[objectName()].first();

    return {QString(), QVariant()};
}

void
QXfsStream::appendCommand(const QPair<QString, QVariant> &cmd) const
{
    QMutexLocker lock(commandMutex);

    (*commands)[objectName()].append(cmd);
}

void
QXfsStream::finishCommand() const
{
    QMutexLocker lock(commandMutex);

    if (!(*commands)[objectName()].isEmpty())
        (*commands)[objectName()].removeFirst();
}

QString
QXfsStream::execute(const QString &dwCommand, const QVariant &lpCmdData)
{
    QString msgid = send("WFSExecute", dwCommand, lpCmdData);

    if (msgid.isEmpty())
        return QString();

    QMetaObject::Connection *c = new QMetaObject::Connection;

    *c = connect(this, &QXfsStream::message, this,
    [this, msgid, c, dwCommand, lpCmdData](QVariantMap msg)
    {
        if (msg["msgid"] != msgid)
            return;

        {
            QMutexLocker lock(deviceMutex);

            foreach (QXfsStream *device, *devices)
            {
                if (device->objectName() == objectName())
                {
                    emit device->executeEventBroadcasted(
                        msg, dwCommand, lpCmdData);
                }
            }
        }

        if (msg["hResult"] == "WFS_SUCCESS")
        {
            if (msg.contains("message"))
            {
                QString message = msg["message"].toString();

                if (message == "WFS_EXECUTE_COMPLETE")
                {
                    Q_ASSERT(dwCommand == msg["dwCommandCode"]);
                    done(msgid);
                    disconnect(*c);
                    delete c;
                    emit executeComplete(msg);
                }
                else if (message == "WFS_EXECUTE_EVENT")
                    emit executeEventRecieved(msg);
            }
            else
            {
                Q_ASSERT(dwCommand == msg["dwCommandCode"]);
                appendCommand({dwCommand, lpCmdData});
            }
        }
        else
        {
            done(msgid);
            disconnect(*c);
            delete c;
            emit executeComplete(msg);
        }
    });

    return msgid;
}

QString
QXfsStream::send(const QString &function, const QString &dwCommand,
                    const QVariant &lpCmdData)
{
    if (!connectToServer(m_io))
        return QString();

    QDataStream ds(m_io);
    ds.setByteOrder(QDataStream::BigEndian);

    QString msgid = QUuid::createUuid().toString();
    QVariantMap cmd
    {
        {"dwCommand", dwCommand},
        {"function", function},
        {"lpCmdData", lpCmdData},
        {"msgid", msgid}
    };
    ds << cmd;

    m_pending[msgid] = dwCommand;

    return msgid;
}

void
QXfsStream::done(const QString &msgid)
{
    finishCommand();
    m_pending.remove(msgid);
}

QVariantMap
QXfsStream::getInfo(const QString &category, const QVariant &queryDetails)
{
    QVariantMap rv;
    QString msgid = send("WFSGetInfo", category, queryDetails);

    if (msgid.isEmpty())
        return rv;

    QEventLoop loop;

    QMetaObject::Connection c = connect(this, &QXfsStream::message, this,
    [&](QVariantMap msg)
    {
        if (msg["msgid"] != msgid)
            return;

        const QString &hResult = msg["hResult"].toString();

        if (hResult == "WFS_SUCCESS")
        {
            Q_ASSERT(!msg.contains("message") ||
                     msg["message"] == "WFS_GETINFO_COMPLETE");

            if (!msg.contains("message"))
                return;
        }
        else
        {
            qWarning() << objectName() << " - " << category
                   << " command failed with " << hResult;
        }

        done(msgid);
        rv = msg;
        loop.exit(0);
    });

    loop.exec();
    disconnect(c);

    return rv;
}

QString
QXfsStream::cancel(const QString &reqMsgId)
{
    if (!connectToServer(m_io))
        return QString();

    QString msgid = QUuid::createUuid().toString();
    QMetaObject::Connection *c = new QMetaObject::Connection;

    *c = connect(this, &QXfsStream::message,
    [this, msgid, c](QVariantMap msg)
    {
        if (msg["msgid"] != msgid)
            return;

        done(msgid);
        disconnect(*c);
        delete c;
        emit cancelComplete(msg);
    });

    m_pending[msgid] = "";

    QDataStream ds(m_io);
    ds.setByteOrder(QDataStream::BigEndian);

    QVariantMap cmd
    {
        {"function", "WFSCancel"},
        {"msgid", msgid}
    };

    if (!reqMsgId.isEmpty() )
        cmd.insert("RequestID", reqMsgId);

    ds << cmd;

    return msgid;
}

bool
QXfsStream::syncCancel(const QString &reqMsgId)
{
    QString msgid = cancel(reqMsgId);

    if (msgid.isEmpty())
        return false;

    QEventLoop loop;
    bool finish = reqMsgId.isEmpty();

    QMetaObject::Connection c1 = connect(this, &QXfsStream::cancelComplete,
    [&](QVariantMap msg)
    {
        if (msg["msgid"] != msgid)
            return;

        disconnect(c1);

        if (finish || msg["hResult"] != "WFS_SUCCESS")
            loop.exit();
        else
            finish = true;
    });

    QMetaObject::Connection c2 = connect(this, &QXfsStream::executeComplete,
    [&, reqMsgId](QVariantMap msg)
    {
        if (msg["msgid"] != reqMsgId)
            return;

        disconnect(c2);

        if (finish)
            loop.exit();
        else
            finish = true;
    });

    loop.exec();

    return true;
}

QVariantMap
QXfsStream::capabilities()
{
    QMutexLocker lock(capabilitiesMutex);
    QVariantMap caps = m_capabilities[objectName()];

    if (caps.isEmpty())
        getCapabilities();

    return m_capabilities[objectName()];
}

void
QXfsStream::getCapabilities()
{
    QVariantMap data = getInfo(m_capabilitiesCategory);

    if (data.contains("lpBuffer"))
        m_capabilities[objectName()] = data["lpBuffer"].toMap();
}

QVariantMap
QXfsStream::getStatus()
{
    return getInfo(m_statusCategory)["lpBuffer"].toMap();
}
