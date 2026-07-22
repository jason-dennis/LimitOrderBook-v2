//
// Created by denni on 3/20/2026.
//

#include "../../include/UI/TradeTerminal.h"

#include "imgui.h"


void TradeTerminal::Render() {

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos( viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);




    ImGui::Begin("TradeTerminal" ,nullptr, ImGuiWindowFlags_NoDecoration);
    ImVec2 available = ImGui::GetContentRegionAvail();
    std::string Symbol = OrderEntry_.GetSelectedSymbol();

    ImGui::BeginChild("Graph + Orders",ImVec2(available.x*0.40f,available.y*1.0f),false,ImGuiWindowFlags_NoScrollbar);
        ImGui::BeginChild("Graph",ImVec2(0,available.y*0.49f),true,ImGuiWindowFlags_NoScrollbar);
        Graph_.Render(Symbol);
        ImGui::EndChild();

        ImGui::BeginChild("Orders",ImVec2(0,available.y*0.49f),true,ImGuiWindowFlags_NoScrollbar);
        Orders_.Render(Symbol);
        ImGui::EndChild();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("OrderBook",ImVec2(available.x * 0.30f,available.y*1.0f),true,ImGuiWindowFlags_NoScrollbar);
    OrderBook_.Render(Symbol);
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("Panel",ImVec2(available.x * 0.30f,available.y*1.0f),true,ImGuiWindowFlags_NoScrollbar);
    OrderEntry_.Render();
    ImGui::EndChild();



    ImGui::End();


}
