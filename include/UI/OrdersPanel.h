//
// Created by denni on 3/21/2026.
//

#ifndef LIMITORDERBOOK_ORDERSPANEL_H
#define LIMITORDERBOOK_ORDERSPANEL_H
#include "Engine/CoreEngine.h"

/**
 * @class OrdersPanel
 * @brief UI component responsible for displaying and managing the user's orders and trades.
 * * This panel typically renders a table of  orders, historical orders,
 * and executed trades. It also tracks the currently selected order or trade to allow
 * for detailed inspection or actions(cancel).
 */
class OrdersPanel {
private:
    CoreEngine& Engine_;                      /**< Reference to the central trading engine. */
    std::unique_ptr<Order> SelectedOrder_;    /**< Pointer to the order currently selected by the user. */
    std::unique_ptr<Trade> SelectedTrade_;    /**< Pointer to the trade currently selected by the user. */
public:

    /**
     * @brief Constructs a new Orders Panel object.
     * * Initializes the panel with a reference to the core engine and sets
     * the initial selected order and trade pointers to null.
     * * @param Engine A reference to the application's CoreEngine.
     */
    OrdersPanel(CoreEngine& Engine):Engine_(Engine),SelectedOrder_(nullptr),SelectedTrade_(nullptr){}
    ~OrdersPanel() = default;

    /**
     * @brief Renders the orders and trades interface for a specific asset.
     * * Updates the UI elements to reflect the current state of the user's orders
     * and trades associated with the provided trading symbol.
     * * @param Symbol The ticker symbol (e.g., "AAPL") to display order data for.
     */
    void Render(const std::string& Symbol);
};
#endif //LIMITORDERBOOK_ORDERSPANEL_H