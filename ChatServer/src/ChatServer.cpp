#include <iostream>
#include <stdexcept>

#include "ChatServer.h"

namespace ChatServer
{
    bool Initialize(ServerState& state) 
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            return false;
        }

        state.iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (!state.iocp_handle) {
            std::cerr << "IOCP creation failed" << std::endl;
            return false;
        }

        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
        SSL_library_init();

        state.ssl_context = SSL_CTX_new(TLS_server_method());
        if (!state.ssl_context) {
            std::cerr << "SSL context creation failed" << std::endl;
            return false;
        }

        std::fill(state.connection_active.begin(), state.connection_active.end(), false);
        std::fill(state.sockets.begin(), state.sockets.end(), INVALID_SOCKET);

        return true;
    }

    size_t FindFreeConnectionSlot(ServerState& state) 
    {
        for (size_t i = 0; i < ServerState::MAX_CONNECTIONS; ++i) {
            bool expected = false;
            if (state.connection_active[i].compare_exchange_strong(expected, true)) {
                return i;
            }
        }
        return ServerState::MAX_CONNECTIONS;
    }

    void AcceptNewConnection(ServerState& state) 
    {
        size_t slot = FindFreeConnectionSlot(state);
        if (slot == ServerState::MAX_CONNECTIONS) {
            std::cerr << "Max connections reached" << std::endl;
            return;
        }

        state.sockets[slot] = accept(state.listen_socket, NULL, NULL);
        if (state.sockets[slot] == INVALID_SOCKET) {
            std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
            state.connection_active[slot] = false;
            return;
        }

        CreateIoCompletionPort(
            reinterpret_cast<HANDLE>(state.sockets[slot]),
            state.iocp_handle,
            slot,
            0
        );

        state.ssl_connections[slot] = SSL_new(state.ssl_context);
        SSL_set_fd(state.ssl_connections[slot], state.sockets[slot]);
        SSL_accept(state.ssl_connections[slot]);

        ++state.active_connection_count;
    }

    void ProcessReceivedData(ServerState& state, size_t connection_index, DWORD bytes_transferred) 
    {
        if (bytes_transferred == 0) {
            closesocket(state.sockets[connection_index]);
            SSL_free(state.ssl_connections[connection_index]);
            state.connection_active[connection_index] = false;
            --state.active_connection_count;
            return;
        }

        char* buffer = &state.receive_buffers[connection_index * ServerState::BUFFER_SIZE];
        state.buffer_sizes[connection_index] = bytes_transferred;

        std::cout << "Received " << bytes_transferred
            << " bytes from connection " << connection_index << std::endl;
    }

    void Run(ServerState& state) 
    {
        while (true) {
            DWORD bytes_transferred = 0;
            ULONG_PTR completion_key = 0;
            OVERLAPPED* overlapped = nullptr;

            BOOL result = GetQueuedCompletionStatus(
                state.iocp_handle,
                &bytes_transferred,
                &completion_key,
                &overlapped,
                INFINITE
            );

            if (!result) {
                if (overlapped == nullptr) {
                    std::cerr << "IOCP error: " << GetLastError() << std::endl;
                    break;
                }
            }

            if (completion_key == 0) {
                AcceptNewConnection(state);
            }
            else {
                ProcessReceivedData(state, completion_key, bytes_transferred);
            }
        }
    }

    bool StartListening(ServerState& state, const char* ipAddress, int port) 
    {
        state.listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (state.listen_socket == INVALID_SOCKET) {
            std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
            return false;
        }

        sockaddr_in service;
        service.sin_family = AF_INET;
        service.sin_addr.s_addr = inet_addr(ipAddress);
        service.sin_port = htons(port);

        if (bind(state.listen_socket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
            std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
            closesocket(state.listen_socket);
            return false;
        }

        if (listen(state.listen_socket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
            closesocket(state.listen_socket);
            return false;
        }

        CreateIoCompletionPort(
            reinterpret_cast<HANDLE>(state.listen_socket),
            state.iocp_handle,
            0,
            0
        );

        return true;
    }

    void Shutdown(ServerState& state) 
    {
        for (size_t i = 0; i < ServerState::MAX_CONNECTIONS; ++i) {
            if (state.connection_active[i]) {
                closesocket(state.sockets[i]);
                SSL_free(state.ssl_connections[i]);
            }
        }

        if (state.listen_socket != INVALID_SOCKET) {
            closesocket(state.listen_socket);
        }

        if (state.ssl_context) {
            SSL_CTX_free(state.ssl_context);
        }

        WSACleanup();
    }
}