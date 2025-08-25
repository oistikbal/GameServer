#define WIN32_LEAN_AND_MEAN

#include <WinSock2.h>
#include <iostream>
#include <process.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace chatserver
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
} // namespace chatserver

namespace
{
chatserver::server g_server = {};
std::vector<chatserver::client *> g_clients;
std::mutex g_clients_mutex;
} // namespace

namespace
{
void receive(chatserver::io_context *context)
{
    DWORD flags = 0;
    DWORD bytes = 0;

    context->kind = chatserver::op_type::recv;
    context->buffer.buf = context->data;
    context->buffer.len = chatserver::server::BUFFER_SIZE;

    int result = WSARecv(context->client->socket, &context->buffer, 1, &bytes, &flags, &context->overlapped, NULL);

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        std::cerr << "WSARecv failed with error: " << WSAGetLastError() << std::endl;
    }
}

void send(chatserver::io_context *context, const char *msg, size_t len)
{
    DWORD bytes = 0;

    memcpy(context->data, msg, len);
    context->buffer.buf = context->data;
    context->buffer.len = static_cast<ULONG>(len);
    context->kind = chatserver::op_type::send;

    int result = WSASend(context->client->socket, &context->buffer, 1, &bytes, 0, &context->overlapped, NULL);

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        std::cerr << "WSASend failed with error: " << WSAGetLastError() << std::endl;
    }
}

void post_receive()
{
}

void post_send()
{
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

        auto *context = reinterpret_cast<chatserver::io_context *>(overlapped);
        if (!context)
            continue;

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

        if (context->kind == chatserver::op_type::recv)
        {
            std::string message(context->data, context->data + bytesTransferred);
            std::cout << "Received: " << message << std::endl;

            // Echo back to all clients
            std::lock_guard<std::mutex> lock(g_clients_mutex);
            for (auto *c : g_clients)
            {
                auto *send_ctx = new chatserver::io_context{};
                send_ctx->client = c;
                send(send_ctx, message.c_str(), message.size());
            }

            ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
            receive(context);
        }
        else if (context->kind == chatserver::op_type::send)
        {
            delete context;
        }
    }
}

} // namespace

namespace chatserver
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

    return true;
}

void run(int thread_count)
{
    for (int i = 0; i < thread_count; i++)
    {
        std::thread(worker_thread).detach();
    }

    while (true)
    {
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);

        SOCKET clientSocket = accept(g_server.listen_socket, (sockaddr *)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET)
        {
            std::cerr << "Failed to accept client connection" << std::endl;
            continue;
        }

        auto *c = new chatserver::client{clientSocket};
        {
            std::lock_guard<std::mutex> lock(g_clients_mutex);
            g_clients.push_back(c);
        }

        std::cout << "Client connected! Total online: " << ++g_server.online_connections << std::endl;

        CreateIoCompletionPort((HANDLE)c->socket, g_server.iocp_handle, 0, 0);

        auto *context = new chatserver::io_context{};
        context->client = c;
        ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
        receive(context);
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
} // namespace chatserver