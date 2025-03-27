#include <iostream>

#include "chatserver.h"

int main(int argc, char* argv[])
{
    constexpr char address[]  = "127.0.0.1";
    constexpr int port = 3000;

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

    std::cout << "ChatServer listening at " << address << ":" << port << std::endl; ;
    chatserver::run();

    chatserver::shutdown();
    return 0;
}
