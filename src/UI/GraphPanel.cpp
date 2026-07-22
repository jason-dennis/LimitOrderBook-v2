//
// Created by denni on 3/21/2026.
//

#include "UI/GraphPanel.h"
#include "implot.h"
void GraphPanel::Render(const std::string& Symbol) {

    auto trades =Engine_.GetTradesHistory(Symbol);
    std::sort(trades.begin(), trades.end(), [](const auto& a, const auto& b) {
        return a.GetTimestamp() < b.GetTimestamp();
    });
    std::vector<double> times;
    std::vector <double> prices;

    for (auto& trade: trades) {
        prices.push_back(trade.GetPrice()/100.0);
        times.push_back(std::chrono::duration<double>(trade.GetTimestamp().time_since_epoch()).count());
    }
    if (ImPlot::BeginPlot("##PricePlot", ImGui::GetContentRegionAvail())) {
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        ImPlot::PlotLine("Price",times.data(),prices.data(),(int)times.size());
        ImPlot::EndPlot();
    }
}
