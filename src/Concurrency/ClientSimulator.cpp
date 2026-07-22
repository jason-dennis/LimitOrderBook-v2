//
// Created by Ognean Jason Dennis on 13.07.2026.
//
#include "Concurrency/ClientSimulator.h"
#include <random>
#include <chrono>
#include <thread>

void ClientSimulator::run() {
    std::random_device rd;
    std::mt19937 generator(rd());

    std::uniform_real_distribution<float> priceDist(90.0f, 110.0f);
    std::uniform_int_distribution<int> qtyDist(1, 100);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<int> sleepDist(200, 800);

    while (Running_.load()) {

        auto Price = priceDist(generator);
        auto Quantity = qtyDist(generator);
        auto Type = "LIMIT";
        auto Symbol = "AAPL";
        auto TIF = "GTC";
        auto TraderID = ClientId_;
        auto Side = (sideDist(generator) == 0) ? "BUY" : "SELL";

        Engine_.CreateOrder(Price, Quantity,Type,Symbol,
                            TIF,TraderID,Side);

        std::this_thread::sleep_for(std::chrono::milliseconds(sleepDist(generator)));
    }
}

void ClientSimulator::stop() {
    Running_.store(false);
}