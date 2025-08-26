#pragma once

namespace login_server
{
    bool initialize();
    bool start_listening(const char* ip, int port);
    void run(int thread_count);
    void shutdown();
}