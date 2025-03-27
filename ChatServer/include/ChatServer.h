#pragma once

#define WIN32_LEAN_AND_MEAN

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <WinSock2.h>
#include <vector>
#include <atomic>
#include <memory>
#include <array>
#include <algorithm>

namespace chatserver
{
    bool initialize();
    bool start_listening(const char* ip, int port);
    void run();
    void shutdown();
}