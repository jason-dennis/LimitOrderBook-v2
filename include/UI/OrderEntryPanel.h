//
// Created by denni on 3/21/2026.
//

#ifndef LIMITORDERBOOK_ORDERENTRYPANEL_H
#define LIMITORDERBOOK_ORDERENTRYPANEL_H
#include "Engine/CoreEngine.h"

/**
 * @class OrderEntryPanel
 * @brief UI component for configuring and submitting Buy/Sell orders.
 * * This panel manages the input state for order parameters such as price,
 * quantity, Time-In-Force (TIF), and order type. It also handles the
 * global application exit state.
 */
class OrderEntryPanel {
private:
    CoreEngine& Engine_;    /**< Reference to the central trading engine. */
    int SymbolIndex;        /**< Index of the currently selected ticker in SymbolOptions. */

    /** @name Order Parameters
     * Input values for Buy and Sell sides.
     * @{ */
    float PriceBuy;         /**< Current input price for a Buy order. */
    float PriceSell;        /**< Current input price for a Sell order. */
    int QtyBuy;             /**< Current input quantity for a Buy order. */
    int QtySell;            /**< Current input quantity for a Sell order. */
    int TifBuy;             /**< Selected index for Buy Time-In-Force (GTC, IOC, FOK). */
    int TifSell;            /**< Selected index for Sell Time-In-Force. */
    int TypeBuy;            /**< Selected index for Buy order type (Limit, Market). */
    int TypeSell;           /**< Selected index for Sell order type. */
    /** @} */

    /** @name UI Configuration Constants
     * Static options displayed in comboboxes.
     * @{ */
    static constexpr char* SymbolOptions[] = {"AAPL", "NVDA", "MSFT", "GOOGL", "AMZN", "META", "AVGO", "MELI", "ISRG", "TSLA"};
    static constexpr const char* TIFOptions[] = {"GTC", "IOC", "FOK"};
    static constexpr const char* TypeOptions[] = {"LIMIT", "MARKET"};
    /** @} */

    bool PriceBuyInitialized_ = false;  /**< Guard to ensure Buy price is set from market data only once. */
    bool PriceSellInitialized_ = false; /**< Guard to ensure Sell price is set from market data only once. */
    bool Exit;                          /**< Flag indicating if the user has requested to close the application. */

public:
    /**
     * @brief Constructs the Order Entry Panel.
     * Initializes all numerical inputs to zero and the exit flag to false.
     * @param Engine Reference to the application CoreEngine.
     */
    OrderEntryPanel(CoreEngine& Engine): Engine_(Engine), SymbolIndex(0),PriceBuy(0),PriceSell(0),
    QtyBuy(0),QtySell(0),TifBuy(0),TifSell(0),TypeBuy(0),TypeSell(0),Exit(false){}
    ~OrderEntryPanel() = default;
    /**
         * @brief Renders the Order Entry form to the screen.
         * Handles user input, updates internal state, and dispatches orders to the Engine.
         */
    void Render();

    /**
     * @brief Gets the string representation of the currently selected symbol.
     * @return const char* The ticker symbol (e.g., "AAPL").
     */
    const char* GetSelectedSymbol() const { return SymbolOptions[SymbolIndex]; }

    /**
     * @brief Checks if the exit command has been triggered.
     * @return true If the user clicked on exit button.
     */
    bool IsExit() const {return Exit;}

};

#endif //LIMITORDERBOOK_ORDERENTRYPANEL_H