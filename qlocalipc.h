#ifndef QLOCALIPC_H
#define QLOCALIPC_H

#include <QLocalServer>
#include <QLocalSocket>
#include <QtGlobal>

#ifdef Q_OS_UNIX

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#endif

#ifdef Q_OS_WIN

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#endif

/**
 * @namespace QLocalIpc
 * @brief Inter process communication security helpers.
 */
namespace QLocalIpc
{

#ifdef Q_OS_UNIX

/**
 * @brief Checks that the peer of the socket runs as the current user,
 * unix domain sockets only.
 */
inline bool peerUidIsCurrentUser(QLocalSocket *socket)
{
    struct ucred cred;
    socklen_t len = sizeof(cred);

    if (getsockopt(socket->socketDescriptor(), SOL_SOCKET, SO_PEERCRED,
                   &cred, &len) != 0)
        return false;

    return cred.uid == getuid();
}

#endif

#ifdef Q_OS_WIN

/**
 * @brief Checks that a security identifier belongs to the current user.
 */
inline bool sidIsCurrentUser(PSID sid)
{
    HANDLE token = INVALID_HANDLE_VALUE;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;

    DWORD len = 0;

    GetTokenInformation(token, TokenUser, nullptr, 0, &len);

    QByteArray buffer(static_cast<int>(len), Qt::Uninitialized);
    auto self = reinterpret_cast<PTOKEN_USER>(buffer.data());

    bool rv = GetTokenInformation(token, TokenUser, self, len, &len) &&
              EqualSid(sid, self->User.Sid);

    CloseHandle(token);

    return rv;
}

/**
 * @brief Checks that a process runs as the current user.
 */
inline bool processUserIsCurrentUser(HANDLE process)
{
    HANDLE token = INVALID_HANDLE_VALUE;

    if (!OpenProcessToken(process, TOKEN_QUERY, &token))
        return false;

    DWORD len = 0;

    GetTokenInformation(token, TokenUser, nullptr, 0, &len);

    QByteArray buffer(static_cast<int>(len), Qt::Uninitialized);
    auto user = reinterpret_cast<PTOKEN_USER>(buffer.data());

    bool rv = GetTokenInformation(token, TokenUser, user, len, &len) &&
              sidIsCurrentUser(user->User.Sid);

    CloseHandle(token);

    return rv;
}

/**
 * @brief Checks that the process on the other end of the named pipe
 * runs as the current user, given the process id provided by the
 * pipe.
 */
inline bool peerProcessIsCurrentUser(HANDLE pipe,
                                      BOOL (WINAPI *peerPid)(HANDLE, PULONG))
{
    DWORD pid = 0;

    if (!peerPid(pipe, &pid))
        return false;

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                 FALSE, pid);

    if (process == nullptr)
        return false;

    bool rv = processUserIsCurrentUser(process);

    CloseHandle(process);

    return rv;
}

#endif

/**
 * @brief Restricts the local server to the current user.
 *
 * Must be called before QLocalServer::listen(). On platforms where
 * the local server does not support access options the call has no
 * effect.
 */
inline void restrictToUser(QLocalServer *server)
{
    server->setSocketOptions(QLocalServer::UserAccessOption);
}

/**
 * @brief Checks that the peer of a connected local socket runs as the
 * current user.
 *
 * The identity is taken from the operating system, not from the
 * connection itself: on Unix from the peer credentials of the unix
 * domain socket, on Windows from the process id of the named pipe
 * client, provided the client process can be opened.
 *
 * @return true when the peer runs as the current user.
 */
inline bool peerIsCurrentUser(QLocalSocket *socket)
{
#ifdef Q_OS_UNIX

    return peerUidIsCurrentUser(socket);

#elif defined(Q_OS_WIN)

    HANDLE pipe = reinterpret_cast<HANDLE>(
                static_cast<uintptr_t>(socket->socketDescriptor()));

    return peerProcessIsCurrentUser(pipe, GetNamedPipeClientProcessId);

#else

    Q_UNUSED(socket)

    return true;

#endif
}

/**
 * @brief Checks that the server a local socket client is connected to
 * runs as the current user.
 *
 * On Unix the peer credentials identify the server process at both
 * ends of the socket. On Windows the process id of the server is
 * queried through the named pipe and the security identifier in its
 * token is compared with the one of the current process.
 *
 * @return true when the server runs as the current user.
 */
inline bool serverIsCurrentUser(QLocalSocket *socket)
{
#ifdef Q_OS_UNIX

    return peerUidIsCurrentUser(socket);

#elif defined(Q_OS_WIN)

    HANDLE pipe = reinterpret_cast<HANDLE>(
                static_cast<uintptr_t>(socket->socketDescriptor()));

    return peerProcessIsCurrentUser(pipe, GetNamedPipeServerProcessId);

#else

    Q_UNUSED(socket)

    return true;

#endif
}

}

#endif // QLOCALIPC_H
