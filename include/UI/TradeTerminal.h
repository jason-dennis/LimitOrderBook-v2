//
// Created by denni on 3/20/2026.
//

#ifndef LIMITORDERBOOK_TRADETERMINAL_H
#define LIMITORDERBOOK_TRADETERMINAL_H
#include "../../include/Engine/CoreEngine.h"
#include "UI/OrderBookPanel.h"
#include  "UI/GraphPanel.h"
#include "UI/OrderEntryPanel.h"
#include "UI/OrdersPanel.h"
#include <iostream>

/**
 * @class TradeTerminal
 * @brief The main interface class that manages and orchestrates the trading terminal UI.
 * * This class acts as a central hub, bringing together various UI panels
 * (Order Book, Graph, Order Entry, and Orders) and connecting them
 * to the underlying Core Engine.
 */
class TradeTerminal {
private:
    CoreEngine& Engine_;           /**< Reference to the main core trading engine. */
    OrderBookPanel OrderBook_;     /**< Panel responsible for displaying the order book. */
    GraphPanel Graph_;             /**< Panel for rendering price  charts. */
    OrderEntryPanel OrderEntry_;   /**< Panel handling user input for new orders and exit commands. */
    OrdersPanel Orders_;           /**< Panel displaying orders and trades. */
public:
    /**
         * @brief Constructs a new Trade Terminal object.
         * * Initializes all UI panels and binds them to the provided core engine.
         * * @param Engine A reference to the application's CoreEngine.
         */
    TradeTerminal(CoreEngine& Engine): Engine_(Engine),
    OrderBook_(Engine),Graph_(Engine),
    OrderEntry_(Engine), Orders_(Engine){}
    ~TradeTerminal() = default;

    /**
         * @brief Renders the entire trading terminal.
         * * This method is called on every frame/tick to update
         * and draw all the individual panels comprising the terminal.
         */
    void Render();

    /**
     * @brief Checks the running state of the terminal.
     * * Determines if the terminal should continue running by checking
     * the exit state of the Order Entry panel.
     * * @return true If the application is still running.
     * @return false If an exit command has been triggered.
     */
    bool IsRunning() const {
        return !OrderEntry_.IsExit();
    }
};

#endif //LIMITORDERBOOK_TRADETERMINAL_H