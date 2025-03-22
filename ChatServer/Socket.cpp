#include "Socket.h"

Socket::Socket() : sock(INVALID_SOCKET), ssl(nullptr), ctx(nullptr) {}

Socket::~Socket() { Close(); }

bool Socket::Initialize()
{
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
}

bool Socket::Connect(const std::string& host, int port)
{
    struct sockaddr_in serverAddr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        return false;

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr);

    return connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == 0;
}

bool Socket::Bind(int port)
{
    struct sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        return false;

    return bind(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == 0;
}

bool Socket::Listen(int backlog)
{
    return listen(sock, backlog) == 0;
}

Socket* Socket::Accept()
{
    SOCKET clientSock = accept(sock, nullptr, nullptr);
    if (clientSock == INVALID_SOCKET)
        return nullptr;

    Socket* client = new Socket();
    client->sock = clientSock;
    return client;
}

bool Socket::Send(const std::string& data)
{
    return send(sock, data.c_str(), static_cast<int>(data.size()), 0) != SOCKET_ERROR;
}

std::string Socket::Receive()
{
    char buffer[4096];
    int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
    return (bytesReceived > 0) ? std::string(buffer, bytesReceived) : "";
}

void Socket::Close()
{
    if (sock != INVALID_SOCKET)
    {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
}