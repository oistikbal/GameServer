#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <WinSock2.h>
#include <MSWSock.h>
#include <windows.h>
#include <process.h>

#include "chatserver.h"

namespace chatserver
{
    struct server
    {
        static constexpr size_t MAX_CONNECTIONS = 64;
        static constexpr size_t BUFFER_SIZE = 128;

        SOCKET listen_socket = INVALID_SOCKET;
        HANDLE iocp_handle = nullptr;
        size_t online_connections;
    };

    struct io_context {
        OVERLAPPED overlapped;
        WSABUF buffer;
        char data[server::BUFFER_SIZE];
        SOCKET clientSocket;
    };
}

static chatserver::server g_server = {};
static chatserver::io_context g_contexts[chatserver::server::MAX_CONNECTIONS];

namespace chatserver
{
    static void send(size_t context_index, const char* message) {
        WSABUF wsabuf;
        wsabuf.buf = (char*)message;
        wsabuf.len = static_cast<ULONG>(strlen(message));

        DWORD bytesSent = 0;
        DWORD flags = 0;

        // Use the context's overlapped structure
        ZeroMemory(&g_contexts[context_index].overlapped, sizeof(OVERLAPPED));

        int result = WSASend(g_contexts[context_index].clientSocket, &wsabuf, 1, &bytesSent, flags, &g_contexts[context_index].overlapped, NULL);
        if (result == SOCKET_ERROR) {
            int errCode = WSAGetLastError();
            if (errCode != WSA_IO_PENDING) {
                std::cerr << "WSASend failed with error: " << errCode << std::endl;
            }
            else {
                std::cout << "WSASend is pending..." << std::endl;
            }
        }
        else {
            std::cout << "Sent data immediately, bytes sent: " << bytesSent << std::endl;
        }
    }


    static void receive(size_t context_index)
    {
        io_context* context = &g_contexts[context_index];
        context->buffer.buf = context->data;
        context->buffer.len = g_server.BUFFER_SIZE - 1;
        ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));

        DWORD flags = 0;
        DWORD bytesReceived = 0;

        int result = WSARecv(context->clientSocket, &context->buffer, 1, &bytesReceived, &flags, &context->overlapped, nullptr);
        if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            std::cerr << "WSARecv failed with error: " << WSAGetLastError() << std::endl;
        }
    }

    static unsigned int WINAPI worker_thread(void* lpParam) {
        while (true) {
            DWORD bytesTransferred;
            ULONG_PTR completionKey;
            OVERLAPPED* overlapped;

            BOOL result = GetQueuedCompletionStatus(g_server.iocp_handle, &bytesTransferred, &completionKey, &overlapped, INFINITE);
            if (result == FALSE) {
                DWORD error = GetLastError();
                if (error == ERROR_ABANDONED_WAIT_0) {
                    std::cerr << "Completion port closed. Exiting worker thread." << std::endl;
                    break;
                }
                if (overlapped == nullptr) {
                    std::cerr << "GetQueuedCompletionStatus failed with error: " << error << std::endl;
                    continue;
                }
            }

            io_context* context = &g_contexts[(size_t)completionKey];

            if (bytesTransferred == 0) {
                // Client disconnected
                if (context->clientSocket != INVALID_SOCKET) {
                    std::cerr << "Client disconnected (Index: " << completionKey << ")\n";
                    closesocket(context->clientSocket);
                    context->clientSocket = INVALID_SOCKET;
                }
                continue;
            }

            if (bytesTransferred > 0) {
                std::cout << "Received " << bytesTransferred << " bytes." << std::endl;
                // Null-terminate the received data
                context->buffer.buf[bytesTransferred] = '\0';
                std::cout << "Received Data: " << context->buffer.buf << std::endl;

                // Broadcast to all other connected clients
                for (int i = 0; i < server::MAX_CONNECTIONS; i++) {
                    if (g_contexts[i].clientSocket != INVALID_SOCKET && i != completionKey) {
                        send(i, context->buffer.buf);
                    }
                }

                // Post another receive operation for this client
                receive(completionKey);
            }
        }
        return 0;
    }

    bool initialize()
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            return false;
        }

        for (int i = 0; i < server::MAX_CONNECTIONS; i++) {
            g_contexts[i].clientSocket = INVALID_SOCKET;
            g_contexts[i].buffer.buf = g_contexts[i].data;
            g_contexts[i].buffer.len = server::BUFFER_SIZE - 1;
            ZeroMemory(&g_contexts[i].overlapped, sizeof(OVERLAPPED));
        }

        return true;
    }

    bool start_listening(const char* ipAddress, int port)
    {
        g_server.listen_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

        if (g_server.listen_socket == SOCKET_ERROR)
        {
            std::cerr << "Failed to create listen socket" << std::endl;
            return false;
        }

        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr(ipAddress);
        serverAddr.sin_port = htons(port);

        if (bind(g_server.listen_socket, reinterpret_cast<const sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) 
        {
            std::cerr << "Failed to bind listen socket" << std::endl;
            return false;
        }

        if (listen(g_server.listen_socket, SOMAXCONN) == SOCKET_ERROR) 
        {
            std::cerr << "Failed to listen socket" << std::endl;
            return false;
        }

        g_server.iocp_handle = CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_server.listen_socket), NULL, 0, 0);
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
            _beginthreadex(nullptr, 0, worker_thread, nullptr, 0, nullptr);
        }

        while (true) 
        {
            sockaddr_in clientAddr;
            int clientAddrSize = sizeof(clientAddr);
            size_t context_index = 0;
            for (size_t i = 0; i < server::MAX_CONNECTIONS; i++)
            {
                if (g_contexts[i].clientSocket == INVALID_SOCKET)
                {
                    context_index = i;
                    break;
                }
            }

            g_contexts[context_index].clientSocket = accept(g_server.listen_socket, (sockaddr*)&clientAddr, &clientAddrSize);
            if (g_contexts[context_index].clientSocket == INVALID_SOCKET) {
                std::cerr << "Failed to accept client connection" << std::endl;
                return;
            }

            std::cout << "Client Accepted (Index: " << context_index << ")\n";
            CreateIoCompletionPort((HANDLE)g_contexts[context_index].clientSocket, g_server.iocp_handle, (ULONG_PTR)context_index, 0);

            receive(context_index);
        }
    }

    void shutdown()
    {
        if (g_server.listen_socket != INVALID_SOCKET) {
            closesocket(g_server.listen_socket);
        }
        if (g_server.iocp_handle) {
            CloseHandle(g_server.iocp_handle);
        }

        WSACleanup();
    }
}