#define WIN32_LEAN_AND_MEAN

#include <WinSock2.h>

#include <MSWSock.h>
#include <iostream>
#include <process.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace login_server
{
struct server
{
    static constexpr size_t MAX_CONNECTIONS = 64;
    static constexpr size_t BUFFER_SIZE = 1024;

    SOCKET listen_socket = INVALID_SOCKET;
    HANDLE iocp_handle = nullptr;
    std::atomic<size_t> online_connections{0};
};

struct client
{
    SOCKET socket;
};

enum class op_type : uint8_t
{
    recv,
    send
};

struct io_context
{
    OVERLAPPED overlapped{};
    WSABUF buffer{};
    char data[server::BUFFER_SIZE]{};
    client *client = nullptr;
    op_type kind{};
};

struct accept_context
{
    OVERLAPPED overlapped{};
    SOCKET acceptSocket = INVALID_SOCKET;
    char buffer[(sizeof(sockaddr_in) + 16) * 2]; // space for local + remote addresses
};
} // namespace login_server

namespace
{
login_server::server g_server = {};
std::vector<login_server::client *> g_clients;
std::mutex g_clients_mutex;
} // namespace

namespace
{
void receive(login_server::client *client)
{
    DWORD flags = 0;
    DWORD bytes = 0;

    login_server::io_context *context = new login_server::io_context();
    context->kind = login_server::op_type::recv;
    context->buffer.buf = context->data;
    context->buffer.len = login_server::server::BUFFER_SIZE;
    context->client = client;

    int result = WSARecv(context->client->socket, &context->buffer, 1, &bytes, &flags, &context->overlapped, NULL);

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        std::cerr << "WSARecv failed with error: " << WSAGetLastError() << std::endl;
    }
}

void send(login_server::client *client, const char *msg, size_t len)
{
    DWORD bytes = 0;

    login_server::io_context *context = new login_server::io_context();
    memcpy(context->data, msg, len);
    context->buffer.buf = context->data;
    context->buffer.len = static_cast<ULONG>(len);
    context->kind = login_server::op_type::send;
    context->client = client;

    int result = WSASend(context->client->socket, &context->buffer, 1, &bytes, 0, &context->overlapped, NULL);

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        std::cerr << "WSASend failed with error: " << WSAGetLastError() << std::endl;
    }
}

void post_receive(login_server::io_context *context, DWORD bytesTransferred)
{
    std::string message(context->data, context->data + bytesTransferred);
    std::cout << "Received: " << message << std::endl;

    // Echo back to all clients
    std::lock_guard<std::mutex> lock(g_clients_mutex);

    for (auto *client : g_clients)
    {

        send(client, message.c_str(), message.size());
    }

    receive(context->client);
    delete context;
}

void post_send(login_server::io_context *context)
{
    std::cout << "Send packet to: " << context->client << std::endl;
    delete context;
}

void accept()
{
    auto *ctx = new login_server::accept_context();
    ctx->acceptSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

    DWORD bytes = 0;
    BOOL result = AcceptEx(g_server.listen_socket, ctx->acceptSocket, ctx->buffer, 0, sizeof(sockaddr_in) + 16,
                           sizeof(sockaddr_in) + 16, &bytes, &ctx->overlapped);

    if (!result && WSAGetLastError() != ERROR_IO_PENDING)
    {
        std::cerr << "AcceptEx failed: " << WSAGetLastError() << std::endl;
        closesocket(ctx->acceptSocket);
        delete ctx;
    }
}

void post_accept(login_server::accept_context *context)
{
    auto *c = new login_server::client{context->acceptSocket};
    {
        std::lock_guard<std::mutex> lock(g_clients_mutex);
        g_clients.push_back(c);
    }

    setsockopt(context->acceptSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&g_server.listen_socket,
               sizeof(g_server.listen_socket));

    CreateIoCompletionPort((HANDLE)c->socket, g_server.iocp_handle, 0, 0);

    std::cout << "Client connected! Total online: " << ++g_server.online_connections << std::endl;

    receive(c);
    delete context;
    accept();
}

void worker_thread()
{
    while (true)
    {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        OVERLAPPED *overlapped = nullptr;

        BOOL result =
            GetQueuedCompletionStatus(g_server.iocp_handle, &bytesTransferred, &completionKey, &overlapped, INFINITE);

        if (!result && overlapped == nullptr)
        {
            std::cerr << "GetQueuedCompletionStatus failed: " << GetLastError() << std::endl;
            continue;
        }

        if ((SOCKET)completionKey != g_server.listen_socket)
        {
            auto *context = reinterpret_cast<login_server::io_context *>(overlapped);

            if (bytesTransferred == 0)
            {
                std::lock_guard<std::mutex> lock(g_clients_mutex);
                g_clients.erase(std::find(g_clients.begin(), g_clients.end(), context->client));
                closesocket(context->client->socket);
                delete context->client;
                delete context;
                g_server.online_connections--;
                std::cout << "Client disconnected! Total online: " << g_server.online_connections << std::endl;
                continue;
            }

            if (context->kind == login_server::op_type::recv)
            {
                post_receive(context, bytesTransferred);
            }
            else if (context->kind == login_server::op_type::send)
            {
                post_send(context);
            }
        }
        else if ((SOCKET)completionKey == g_server.listen_socket)
        {
            auto *context = reinterpret_cast<login_server::accept_context *>(overlapped);
            post_accept(context);
        }
        else
        {
            continue;
        }
    }
}

} // namespace

namespace login_server
{
bool initialize()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }
    return true;
}

bool start_listening(const char *ipAddress, int port)
{
    g_server.listen_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

    if (g_server.listen_socket == INVALID_SOCKET)
    {
        std::cerr << "Failed to create listen socket" << std::endl;
        return false;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(ipAddress);
    serverAddr.sin_port = htons(port);

    if (bind(g_server.listen_socket, reinterpret_cast<const sockaddr *>(&serverAddr), sizeof(serverAddr)) ==
        SOCKET_ERROR)
    {
        std::cerr << "Failed to bind listen socket" << std::endl;
        return false;
    }

    if (listen(g_server.listen_socket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "Failed to listen on socket" << std::endl;
        return false;
    }

    g_server.iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (!g_server.iocp_handle)
    {
        std::cerr << "IOCP creation failed" << std::endl;
        return false;
    }

    if (!CreateIoCompletionPort((HANDLE)g_server.listen_socket, g_server.iocp_handle, g_server.listen_socket, 0))
    {
        std::cerr << "Failed to associate listen socket with IOCP" << std::endl;
        return false;
    }

    return true;
}

void run(int thread_count)
{
    for (int i = 0; i < thread_count; i++)
    {
        std::thread(worker_thread).detach();
    }

    for (int i = 0; i < 4; i++)
    {
        accept();
    }

    while (true)
    {
        SwitchToThread();
    }
}

void shutdown()
{
    if (g_server.listen_socket != INVALID_SOCKET)
    {
        closesocket(g_server.listen_socket);
    }
    if (g_server.iocp_handle)
    {
        CloseHandle(g_server.iocp_handle);
    }

    WSACleanup();
}
} // namespace login_server