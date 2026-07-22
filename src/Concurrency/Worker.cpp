//
// Created by Ognean Jason Dennis on 13.07.2026.
//
#include "Concurrency/Worker.h"

void Worker::run() {
    while (auto msg = Queue_.Pop()) {
        Engine_.CreateOrder(msg->Price, msg->Quantity, msg->Type,
                            msg->Symbol, msg->TIF, msg->TraderID, msg->Side);
    }
}
