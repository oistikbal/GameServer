#include <Windows.h>
#include <iostream>

#include "login_server.h"

int main(int argc, char *argv[])
{
    constexpr char address[] = "127.0.0.1";
    constexpr int port = 3000;

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    int threadCount = sysInfo.dwNumberOfProcessors;

    for (int i = 1; i < argc; i++)
    {
        if (CompareString(LOCALE_USER_DEFAULT, 0, argv[i], -1, "-threadcount", -1) == CSTR_EQUAL && i + 1 < argc)
        {
            threadCount = atoi(argv[i + 1]);
            if (threadCount <= 0)
            {
                std::cout << ("Invalid thread count. Defaulting to 1.\n");
                threadCount = 1;
            }
            i++;
        }
    }

    if (!login_server::initialize())
    {
        std::cerr << "LoginServer failed to initialize";
        return 1;
    }
    if (!login_server::start_listening(address, port))
    {
        std::cerr << "LoginServer failed to listen";
        return 1;
    }

    std::cout << "LoginServer listening at " << address << ":" << port << " with " << threadCount << " threads"
              << std::endl;
    ;
    login_server::run(threadCount);

    login_server::shutdown();
    return 0;
}
