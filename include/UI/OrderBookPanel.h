//
// Created by denni on 3/21/2026.
//

#ifndef LIMITORDERBOOK_ORDERBOOKPANEL_H
#define LIMITORDERBOOK_ORDERBOOKPANEL_H
#include "../../include/Engine/CoreEngine.h"

/**
 * @class OrderBookPanel
 * @brief UI component responsible for displaying the Market Data (Order Book).
 * * This panel fetches real-time bid and ask data from the CoreEngine and
 * renders them
 */
class OrderBookPanel {
private:
    /** @brief Reference to the central engine*/
    CoreEngine& Engine_;

public:

    /**
     * @brief Constructs an OrderBookPanel.
     * @param Engine Reference to the application's CoreEngine.
     */
    OrderBookPanel(CoreEngine& Engine): Engine_(Engine){}
    ~OrderBookPanel() = default;

    /**
     * @brief render method for the order book.
     * * Orchestrates the drawing of the entire panel, usually calling
     * RenderBids and RenderAsks internally.
     * @param Symbol The ticker symbol to display (e.g., "NVDA").
     */
    void Render(std::string& Symbol);

    /**
     * @brief Renders the "Bids" side of the book.
     * * Displays buy orders, usually sorted by price in descending order
     * (highest bid at the top).
     * @param Symbol The ticker symbol to filter data for.
     */
    void RenderBids(std::string& Symbol);

    /**
     * @brief Renders the "Asks" side of the book.
     * * Displays sell orders, usually sorted by price in ascending order
     * (lowest ask at the top).
     * @param Symbol The ticker symbol to filter data for.
     */
    void RenderAsks(std::string& Symbol);

};

#endif //LIMITORDERBOOK_ORDERBOOKPANEL_H