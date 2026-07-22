#include "Networking/TcpServer.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <string>

TcpServer::~TcpServer() {
    Stop();
}

void TcpServer::Start() {
    struct addrinfo hints, *res;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    std::string portStr = std::to_string(port_);
    int status = getaddrinfo(NULL, portStr.c_str(), &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("socket");
        freeaddrinfo(res);
        return;
    }
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind");
        freeaddrinfo(res);
        close(sockfd);
        return;
    }

    freeaddrinfo(res);

    if (listen(sockfd, 64) == -1) {
        perror("listen");
        close(sockfd);
        return;
    }

    listenSocket_ = sockfd;

    printf("Server is running on port %d...\n", port_);

    running_.store(true);
    acceptThread_ = std::thread(&TcpServer::AcceptLoop, this);
}

void TcpServer::Stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    int sock = listenSocket_.load();
    if (sock != -1) {
        close(sock);
        listenSocket_.store(-1);
    }

    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }
}

void TcpServer::AcceptLoop() {
    while (running_.load()) {
        struct sockaddr_storage their_addr;
        socklen_t addr_size = sizeof their_addr;

        int new_fd = accept(listenSocket_.load(), (struct sockaddr*)&their_addr, &addr_size);

        if (new_fd == -1) {
            if (running_.load()) {
                perror("accept");
            }
            break;
        }

        printf("Connection accepted!\n");
        std::thread(&TcpServer::HandleConnection, this, new_fd).detach();
    }
}

void TcpServer::HandleConnection(int clientSocket) {
    std::vector<uint8_t> buffer;

    while(true){
        uint8_t t_buffer[1024];
        ssize_t bytes = recv(clientSocket,t_buffer,sizeof(t_buffer),0);

        if(bytes <= 0)
            break;

        buffer.insert(buffer.end(),t_buffer,t_buffer + bytes);

        while(true) {

            if (buffer.size() < 20) break;
            uint8_t SymLength = buffer[19];
            size_t ProtocolSize = 20 + static_cast<size_t>(SymLength);
            if (buffer.size() < ProtocolSize) break;

            auto start = buffer.begin();
            auto end = buffer.begin() + ProtocolSize;
            std::vector<uint8_t> ProtocolBytes (start,end);

            auto decode = Decode(ProtocolBytes);
            if(decode.has_value()) {
                Engine_.CreateOrder(decode->Price,
                                    decode->Quantity,
                                    decode->Type,
                                    decode->Symbol,
                                    decode->TIF,
                                    decode->TraderID,
                                    decode->Side);
            }
            buffer.erase(buffer.begin(),buffer.begin() + ProtocolSize);
        }
    }

    close(clientSocket);
}