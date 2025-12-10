#ifndef QXFSSTREAM_H
#define QXFSSTREAM_H

#include <QIODevice>
#include <QVariantMap>

#include "qxfs_global.h"

/**
 * @class QNdcXfsStream
 * @brief Common base for XFS device proxies.
 *
 * @details
 * This class encapsulates:
 *  - Command execution, cancellation, and completion tracking.
 *  - Capability and status queries with caching options.
 *  - Emission of typed signals for command and event traffic.
 *
 * Subclasses should:
 *  - Provide high-level Q_INVOKABLE APIs for QML/JS consumers.
 *  - Optionally override event handlers to translate raw events into
 *    domain-specific signals or state transitions.
 */
class QXFS_EXPORT QXfsStream : public QObject
{
    Q_OBJECT

public:

    /**
     * @brief Constructs a device proxy bound to a device class id.
     *
     * @param io IO device to be used for communication with device warpper.
     * @param deviceId Logical identifier of the target device instance.
     * @param strClass Device class discriminator used by the backend.
     *
     * @note Construction does not guarantee immediate connectivity. The
     *       connectToServer() slot is used to establish a session.
     */
    explicit QXfsStream(QIODevice *io, const QString &deviceId,
                        const QString &strClass, QObject *parent = nullptr);

    /**
     * @brief Destroys the device proxy and releases resources.
     *
     * Ensures pending operations are finalized and the io device is closed.
     */
    ~QXfsStream();

    /**
     * @brief Executes a device command asynchronously.
     *
     * Serializes and enqueues a command identified by @p dwCommand with
     * optional @p lpCmdData payload. Returns a request message id that can
     * be used for correlation and cancellation.
     *
     * @param dwCommand Command name or code understood by the service.
     * @param lpCmdData Arbitrary data (often map or TLV) for the command.
     * @return QString Request message id used to track the operation.
     *
     * @signals executeComplete() is emitted upon completion with a result.
     * @signals executeEventRecieved() may be fired for intermediate updates.
     */
    Q_INVOKABLE QString execute(const QString &dwCommand,
                                const QVariant &lpCmdData = QVariant());

    /**
     * @brief Requests cancellation of a previously issued command.
     *
     * @param reqMsgId Request id returned by execute(). If empty, a best-
     *        effort cancel may apply to the current command context.
     * @return QString Request id of the cancel command (for correlation).
     *
     * Sends a WFSCancel request and sets up a slot to emit cancelComplete
     * when the cancellation is acknowledged by the device.
     */
    Q_INVOKABLE QString cancel(const QString &reqMsgId = QString());

    /**
     * @brief Synchronously cancels a previously issued command.
     *
     * Blocks execution until cancelComplete or executeComplete events
     * are received for the specified message.
     *
     * @param reqMsgId Request id returned by execute(). If empty, the
     *        implementation may target the current command.
     * @return bool True if cancellation succeeded; otherwise false.
     */
    Q_INVOKABLE bool syncCancel(const QString &reqMsgId = QString());

public slots:
    /**
     * @brief Retrieves device capabilities.
     *
     * @return QVariantMap Capability dictionary; structure is device-specific.
     *
     * If capabilities are not cached, fetches them from the device.
     *
     * @note Implementations may cache capabilities per device class.
     */
    QVariantMap capabilities();

    /**
     * @brief Retrieves current device status.
     *
     * @return QVariantMap Status dictionary representing live conditions.
     */
    QVariantMap status() { return getStatus(); }

    /**
     * @brief Queries information by category and optional filter.
     *
     * @param category Category identifier (e.g., "status", "caps", "counters").
     * @param queryDetails Optional parameters to narrow the query.
     * @return QVariantMap Result map with category-specific fields.
     */
    QVariantMap getInfo(const QString &category,
                        const QVariant &queryDetails = QVariant());

protected:
    const QMap<QString, QString> &pending() {return m_pending;}

    /**
     * @brief Queries the backend for a fresh status snapshot.
     *
     * @return QVariantMap The latest known status map.
     */
    QVariantMap getStatus();

    /**
     * @brief Initiates a capabilities refresh from the backend.
     *
     * Populates internal caches and may emit executeComplete() when done.
     */
    void getCapabilities();

    /**
     * @brief Hook for device/service-originated events.
     *
     * Override in subclasses to translate service events into higher-level
     * device signals or state changes.
     *
     * @param msg Event payload.
     */
    virtual void serviceEvent(const QVariantMap &msg)
    {
        Q_UNUSED(msg);
    }

    /**
     * @brief Hook for user-facing events and prompts.
     *
     * Override in subclasses to emit UX-related guidance (insert card, etc.).
     *
     * @param msg Event payload.
     */
    virtual void userEvent(const QVariantMap &msg)
    {
        Q_UNUSED(msg);
    }

    /**
     * @brief Hook for system-level events (lifecycle, policy, timeouts).
     *
     * Override in subclasses to handle framework/system notifications.
     *
     * @param msg Event payload.
     */
    virtual void systemEvent(const QVariantMap &msg)
    {
        Q_UNUSED(msg);
    }

    /**
     * @brief Attempts to establish a connection to the backend service.
     *
     * @return bool True on successful connection; otherwise false.
     */
    virtual bool connectToServer(QIODevice *) {return true;}

private:
    /**
     * @brief The underlying I/O transport. Typically a QLocalSocket or SSL.
     *
     * May be null until connectToServer() succeeds.
     */
    QIODevice *m_io;

    /**
     * @brief Device class discriminator used to segment capability caches.
     */
    const QString m_strClass;

    /**
     * @brief Category key used for status queries.
     */
    QString m_statusCategory;

    /**
     * @brief Category key used for capability queries.
     */
    QString m_capabilitiesCategory;

    /**
     * @brief Map of pending request ids to command names.
     *
     * Used to correlate inbound responses with outstanding operations.
     */
    QMap<QString, QString> m_pending;

    /**
     * @brief Static cache of capabilities per device class.
     *
     * Reduces repeated backend calls for shared, immutable capability data.
     */
    static QMap<QString, QVariantMap> m_capabilities;

private slots:

    /**
     * @brief Slot invoked when the socket has data to read.
     *
     * Parses inbound frames and routes them to the proper handlers or signals.
     */
    void readyRead();

signals:
    /**
     * @brief Emitted for generic messages or diagnostics.
     *
     * @param msg Arbitrary payload, typically for logging or tracing.
     */
    void message(QVariantMap msg);

    /**
     * @brief Emitted when an in-flight command is aborted by the system.
     *
     * @param msg Context describing the aborted operation.
     */
    void aborted(QVariantMap msg);

    /**
     * @brief Emitted when a command completes.
     *
     * @param msg Result payload containing status and data fields.
     */
    void executeComplete(const QVariantMap &msg);

    /**
     * @brief Emitted when a cancellation request settles.
     *
     * @param msg Result payload containing status and details.
     */
    void cancelComplete(const QVariantMap &msg);

    /**
     * @brief Emitted for intermediate execution events.
     *
     * @param msg Incremental update or progress notification.
     */
    void executeEventRecieved(const QVariantMap &msg);

    /**
     * @brief Emitted for broadcast events to all devices of a type.
     *
     * @param msg Broadcast payload from the backend.
     * @param dwCommand Command associated with the broadcast.
     * @param lpCmdData Additional data providing context.
     */
    void executeEventBroadcasted(const QVariantMap &msg,
                                 const QString &dwCommand,
                                 const QVariant &lpCmdData);

    /**
     * @brief Emitted for backend service-originated events.
     *
     * @param msg Payload describing device/service changes.
     */
    void serviceEventRecieved(const QVariantMap &msg);

    /**
     * @brief Emitted for user-centric prompts or guidance.
     *
     * @param msg Payload intended to inform UI/UX actions.
     */
    void userEventRecieved(const QVariantMap &msg);

    /**
     * @brief Emitted for system-level events tied to a command context.
     *
     * @param msg The event payload.
     * @param dwCommand The command associated with the event.
     * @param lpCmdData Contextual data supplied by the system.
     */
    void systemEventRecieved(const QVariantMap &msg, const QString &dwCommand,
                             const QVariant &lpCmdData);

private:
    /**
     * @brief Returns the command currently at the head of the queue.
     *
     * @return QPair&lt;QString, QVariant&gt; The command id and payload.
     */
    QPair<QString, QVariant> currentCommand() const;

    /**
     * @brief Appends a command to the pending queue.
     *
     * @param cmd Pair of command id and payload.
     */
    void appendCommand(const QPair<QString, QVariant> &cmd) const;

    /**
     * @brief Completes and removes the current command from the queue.
     */
    void finishCommand() const;

    /**
     * @brief Low-level send routine for commands and data.
     *
     * @param function High-level action name (execute/cancel/etc.).
     * @param dwCommand Command code or descriptor.
     * @param lpCmdData Arbitrary payload for the operation.
     * @return QString Request id generated for this transmission.
     */
    QString send(const QString &function, const QString &dwCommand,
                 const QVariant &lpCmdData);

    /**
     * @brief Marks a request as completed and performs cleanup.
     *
     * @param msgid The request id to finalize.
     */
    void done(const QString &msgid);
};

#endif // QXFSSTREAM_H
