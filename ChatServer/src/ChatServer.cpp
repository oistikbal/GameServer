#include <iostream>
#include <stdexcept>
#include <thread>

#include "chatserver.h"
#include <MSWSock.h>

namespace chatserver
{
    struct server
    {
        static constexpr size_t MAX_CONNECTIONS = 64;
        static constexpr size_t BUFFER_SIZE = 128;

        SOCKET listen_socket = INVALID_SOCKET;
        HANDLE iocp_handle = nullptr;
        SSL_CTX* ssl_context = nullptr;
    };
}

static chatserver::server g_server = {};

namespace chatserver
{
    static void send(SOCKET clientSocket, const char* message)
    {
        WSABUF wsabuf;
        wsabuf.buf = (char*)message;
        wsabuf.len = static_cast<ULONG>(strlen(message));

        DWORD bytesSent = 0;
        DWORD flags = 0;
        OVERLAPPED* overlapped = new OVERLAPPED();

        ZeroMemory(overlapped, sizeof(OVERLAPPED));

        int result = WSASend(clientSocket, &wsabuf, 1, &bytesSent, flags, overlapped, NULL);
        if (result == SOCKET_ERROR) {
            int errCode = WSAGetLastError();
            if (errCode != WSA_IO_PENDING) {
                std::cerr << "WSASend failed with error: " << errCode << std::endl;
            }
            else {
                std::cout << "WSASend is pending, waiting for completion..." << std::endl;
            }
        }
        else {
            std::cout << "Sent data immediately, bytes sent: " << bytesSent << std::endl;
        }

        delete overlapped;
    }


    static void receive(SOCKET clientSocket) 
    {
        WSABUF buffer;
        buffer.buf = new char[g_server.BUFFER_SIZE];
        buffer.len = g_server.BUFFER_SIZE;

        DWORD flags = 0;
        OVERLAPPED* overlapped = new OVERLAPPED();
        ZeroMemory(overlapped, sizeof(OVERLAPPED));

        DWORD bytesReceived = 0;

        int result = WSARecv(clientSocket, &buffer, 1, &bytesReceived, &flags, overlapped, nullptr);
        if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            std::cerr << "WSARecv failed with error: " << WSAGetLastError() << std::endl;
            delete[] buffer.buf;
            delete overlapped;
            return;
        }

        std::cout << "Received " << bytesReceived << " bytes." << std::endl;

        if (bytesReceived > 0) {
            buffer.buf[bytesReceived] = '\0';

            std::cout << "Received Data: " << (const char*)buffer.buf << std::endl;
            chatserver::send(clientSocket, buffer.buf);
        }

        delete[] buffer.buf;
        delete overlapped;
    }

    static void worker_thread() 
    {
        DWORD bytesTransferred;
        ULONG_PTR completionKey;
        OVERLAPPED* overlapped;
        SOCKET clientSocket;

        while (true) {
            BOOL result = GetQueuedCompletionStatus(g_server.iocp_handle, &bytesTransferred, &completionKey, &overlapped, INFINITE);
            if (result == 0 || bytesTransferred == 0) {
                std::cerr << "Error or client disconnected" << std::endl;
                break;
            }

            receive((SOCKET)completionKey);
        }
    }

    bool initialize()
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            return false;
        }

        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
        SSL_library_init();

        g_server.ssl_context = SSL_CTX_new(TLS_server_method());
        if (!g_server.ssl_context) {
            std::cerr << "SSL context creation failed" << std::endl;
            return false;
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

    void run()
    {
        std::thread t(worker_thread);
        t.detach();

        while (true) 
        {
            sockaddr_in clientAddr;
            int clientAddrSize = sizeof(clientAddr);
            SOCKET clientSocket = accept(g_server.listen_socket, (sockaddr*)&clientAddr, &clientAddrSize);
            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "Failed to accept client connection" << std::endl;
                return;
            }

            std::cout << "Connection accepted " << clientSocket << std::endl;

            CreateIoCompletionPort((HANDLE)clientSocket, g_server.iocp_handle, (ULONG_PTR)clientSocket, 0);

            receive(clientSocket);
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
        if (g_server.ssl_context) {
            SSL_CTX_free(g_server.ssl_context);
        }

        WSACleanup();
    }
}