#ifndef QXFSSOCKETSTREAM_H
#define QXFSSOCKETSTREAM_H

#include "qxfsstream.h"
#include "qxfs_global.h"

class QXFS_EXPORT QXfsSocketStream : public QXfsStream
{
    Q_OBJECT

public:
    explicit QXfsSocketStream(const QString &deviceAddress,
                              const QString &deviceId,
                              const QString &strClass,
                              QObject *parent = nullptr);
    ~QXfsSocketStream();

protected:
    /**
     * @brief Attempts to establish a connection to the backend service.
     *
     * @return bool True on successful connection; otherwise false.
     */
    virtual bool connectToServer(QIODevice *io);

protected slots:
    /**
     * @brief Slot invoked when the socket disconnects.
     *
     * Performs cleanup and may emit diagnostics or state transitions.
     */
    void disconnected();

private:
    QIODevice *createSocket(const QString &deviceAddress,
                            const QString &deviceId);

private:

    /**
     * @brief The connection socket.
     */
    QIODevice *m_socket;

    /**
     * @brief True if the connection target is local (QLocalSocket).
     */
    bool m_isLocal;

    /**
     * @brief True if SSL/TLS is negotiated for the connection.
     */
    bool m_isSsl;

    /**
     * @brief Hostname or endpoint for remote connections.
     */
    QString *m_host;

    /**
     * @brief Port for remote TCP/SSL connections.
     */
    quint16 m_port;
};

#endif // QXFSSOCKETSTREAM_H
