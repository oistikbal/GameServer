#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string>

class Socket
{
public:
    Socket();
    ~Socket();

    bool Initialize();
    bool Connect(const std::string& host, int port);
    bool Bind(int port);
    bool Listen(int backlog = SOMAXCONN);
    Socket* Accept();
    bool Send(const std::string& data);
    std::string Receive();
    void Close();

private:
    SOCKET sock;
    SSL* ssl;
    SSL_CTX* ctx;

    bool InitSSL();
    void CleanupSSL();
};
