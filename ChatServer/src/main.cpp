#include <Windows.h>
#include <iostream>

#include "chatserver.h"

int main(int argc, char* argv[])
{
    constexpr char address[]  = "127.0.0.1";
    constexpr int port = 3000;

    int threadCount = 1;

    for (int i = 1; i < argc; i++) {
        if (CompareString(LOCALE_USER_DEFAULT, 0, argv[i], -1, "-threadcount", -1) == CSTR_EQUAL && i + 1 < argc) {
            threadCount = atoi(argv[i + 1]);
            if (threadCount <= 0) {
                std::cout << ("Invalid thread count. Defaulting to 1.\n");
                threadCount = 1;
            }
            i++;
        }
    }

    if (!chatserver::initialize()) 
    {
        std::cerr << "ChatServer failed to initialize";
        return 1;
    }
    if (!chatserver::start_listening(address, port))
    {
        std::cerr << "ChatServer failed to listen";
        return 1;
    }

    std::cout << "ChatServer listening at " << address << ":" << port << " with " << threadCount << " threads" << std::endl; ;
    chatserver::run(threadCount);

    chatserver::shutdown();
    return 0;
}
