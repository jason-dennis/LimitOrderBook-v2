//
// Created by Ognean Jason Dennis on 15.07.2026.
//

#ifndef LIMITORDERBOOK_TCPSERVER_H
#define LIMITORDERBOOK_TCPSERVER_H
#include <atomic>
#include <thread>
#include "Concurrency/OrderQueue.h"
#include "Networking/Protocol.h"
#include "Engine/CoreEngine.h"
class TcpServer{
private:
    int port_;
    std::atomic<int> listenSocket_{-1};
    std::atomic<bool> running_{false};
    std::thread acceptThread_;
    CoreEngine& Engine_;

    void AcceptLoop();
    void HandleConnection(int clientSocket);
public:
    TcpServer(int port, CoreEngine& Engine): port_(port),
    Engine_(Engine){}
    ~TcpServer();
    void Start();
    void Stop();
};

#endif //LIMITORDERBOOK_TCPSERVER_H
