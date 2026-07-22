//
// Created by Ognean Jason Dennis on 15.07.2026.
//
#include "Networking/TcpServer.h"
#include "Engine/CoreEngine.h"
#include <iostream>

int main() {
    CoreEngine Engine;
    TcpServer server(8080,Engine);
    server.Start();

    std::cout << "Apasa Enter pentru a opri serverul...\n";
    std::cin.get();

    server.Stop();
    return 0;
}
