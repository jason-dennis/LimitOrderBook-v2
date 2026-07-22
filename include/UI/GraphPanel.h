//
// Created by denni on 3/21/2026.
//
#ifndef LIMITORDERBOOK_GRAPHPANEL_H
#define LIMITORDERBOOK_GRAPHPANEL_H

#include "Engine/CoreEngine.h"
#include "imgui.h"

/**
 * @class GraphPanel
 * @brief UI component responsible for rendering visual price charts.
 * * This panel interacts with the CoreEngine to retrieve historical and
 * real-time time-series data, rendering it as a graphical chart
 */
class GraphPanel {
private:
    /** @brief Reference to the central engine*/
    CoreEngine& Engine_;

public:

    GraphPanel(CoreEngine& Engine): Engine_(Engine){}
    ~GraphPanel() = default;

    /**
     * @brief Renders the graphical chart for a specific instrument.
     * * Fetches the necessary plot points from the engine and draws
     * the axes, grids, and price action to the screen.
     * @param Symbol The ticker symbol to be charted (e.g., "MSFT").
     */
    void Render(const std::string& Symbol);
};

#endif //LIMITORDERBOOK_GRAPHPANEL_H
