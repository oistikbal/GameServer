#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <thread>
#include <winsock2.h>
#include <string>
#include <conio.h>



void receive_messages(SOCKET client_socket) {
    char buffer[128];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            std::cerr << "\nServer disconnected." << std::endl;
            closesocket(client_socket);
            return;
        }

        std::cout << "\r";
        std::cout << "Server: " << buffer << "\n";
        std::cout << "> " << std::flush;
    }
}

void send_message(SOCKET client_socket, const std::string& message) {
    if (message.empty()) {
        std::cout << "Message is empty, nothing to send" << std::endl;
        return;
    }

    WSABUF wsabuf;
    wsabuf.buf = (char*)message.c_str();
    wsabuf.len = static_cast<ULONG>(message.length());

    std::cout << "Sending: '" << message << "' (" << wsabuf.len << " bytes)" << std::endl;

    DWORD bytesSent = 0;
    DWORD flags = 0;

    int result = WSASend(client_socket, &wsabuf, 1, &bytesSent, flags, NULL, NULL);
    if (result == SOCKET_ERROR) {
        int errCode = WSAGetLastError();
        if (errCode == WSA_IO_PENDING) {
            std::cout << "WSASend is pending, waiting for completion..." << std::endl;
        }
        else {
            std::cerr << "WSASend failed with error: " << errCode << std::endl;
        }
    }
    else {
        std::cout << "Sent data immediately, result: " << result << ", bytes sent: " << bytesSent << std::endl;
        struct sockaddr_in addr;
        int addr_len = sizeof(addr);
        if (getpeername(client_socket, (struct sockaddr*)&addr, &addr_len) == 0) {
            std::cout << "Connected to: " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << std::endl;
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    SOCKET client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(3000);

    if (connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed." << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server. Start chatting!" << std::endl;
    std::thread(receive_messages, client_socket).detach();

    std::string message;
    while (true) {
        std::cout << "> " << std::flush;
        std::getline(std::cin, message);
        if (message == "exit") {
            break;
        }
        send_message(client_socket, message);
    }


    closesocket(client_socket);
    WSACleanup();
    return 0;
}
