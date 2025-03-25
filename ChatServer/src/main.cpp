#include <iostream>

#include "ChatServer.h"

int main() 
{
    ChatServer::ServerState serverState = {};

    if (!ChatServer::Initialize(serverState)) 
    {
        std::cerr << "ChatServer failed to initialize";
        return 1;
    }
    if (!ChatServer::StartListening(serverState, "localhost", 3000))
    {
        std::cerr << "ChatServer failed to listen";
        return 1;
    }

    std::cout << "ChatServer Initialized\n" ;
    ChatServer::Run(serverState);

    ChatServer::Shutdown(serverState);
    return 0;
}
