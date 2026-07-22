//
// Created by Ognean Jason Dennis on 13.07.2026.
//

#ifndef LIMITORDERBOOK_CLIENTSIMULATOR_H
#define LIMITORDERBOOK_CLIENTSIMULATOR_H
#include "Concurrency/OrderQueue.h"
#include "Engine/CoreEngine.h"

class ClientSimulator {
private:
    CoreEngine& Engine_;
    int ClientId_;
    std::atomic<bool> Running_{true};

public:
    ClientSimulator(CoreEngine& Engine, int clientId):
            Engine_(Engine),
            ClientId_(clientId) {}

    void run();
    void stop();
};


#endif //LIMITORDERBOOK_CLIENTSIMULATOR_H
