#pragma once
#define WIN32_LEAN_AND_MEAN
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <WinSock2.h>
#include <Windows.h>
#include <vector>
#include <atomic>
#include <memory>
#include <array>
#include <algorithm>

namespace ChatServer
{
    // Fundamental DoD approach: Separate arrays for each data type
    struct ServerState {
        static constexpr size_t MAX_CONNECTIONS = 16;
        static constexpr size_t BUFFER_SIZE = 128;

        std::array<SOCKET, MAX_CONNECTIONS> sockets;
        std::array<SSL*, MAX_CONNECTIONS> ssl_connections;
        std::array<char, MAX_CONNECTIONS* BUFFER_SIZE> receive_buffers;
        std::array<size_t, MAX_CONNECTIONS> buffer_sizes;
        std::array<std::atomic<bool>, MAX_CONNECTIONS> connection_active;
        std::array<OVERLAPPED, MAX_CONNECTIONS> overlapped_structs;

        SOCKET listen_socket = INVALID_SOCKET;
        HANDLE iocp_handle = nullptr;
        SSL_CTX* ssl_context = nullptr;

        std::atomic<size_t> active_connection_count{ 0 };
        std::atomic<size_t> next_connection_index{ 0 };
    };

    bool Initialize(ServerState& state);
    bool StartListening(ServerState& state, const char* ip, int port);
    void Run(ServerState& state);
    void Shutdown(ServerState& state);

    size_t FindFreeConnectionSlot(ServerState& state);
    void AcceptNewConnection(ServerState& state);
    void ProcessReceivedData(ServerState& state, size_t connection_index, DWORD bytes_transferred);
}