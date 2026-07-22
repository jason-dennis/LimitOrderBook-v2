//
// Created by denni on 3/21/2026.
//

#include "UI/OrderEntryPanel.h"
#include "imgui.h"
#include <iostream>


void OrderEntryPanel::Render() {

    // ######################################################################################
    // ############################ Symbol Zone ###############################################
    //######################################################################################
    ImGui::Dummy(ImVec2(0, 30));

    float width = ImGui::GetContentRegionAvail().x;
    float textW = ImGui::CalcTextSize("SYMBOL").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - textW) * 0.5f);
    ImGui::Text("SYMBOL");

    float comboWidth = width * 0.7f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - comboWidth) * 0.5f);
    ImGui::SetNextItemWidth(comboWidth);
    ImGui::Combo("##Symbol",&SymbolIndex,SymbolOptions,IM_ARRAYSIZE(SymbolOptions));
    ImGui::Dummy(ImVec2(0, 30));
    auto Symbol =std::string(SymbolOptions[SymbolIndex]);
    auto bestAsks = Engine_.GetBestAsks(1, Symbol);
    auto bestBids = Engine_.GetBestBids(1, Symbol);

    float bestAskPrice = bestAsks.empty() ? 0.0f : bestAsks[0].GetPrice() / 100.0f;
    float bestBidPrice = bestBids.empty() ? 0.0f : bestBids[0].GetPrice() / 100.0f;
    ImGui::Separator();

// ######################################################################################
// ############################ Buy Zone ###############################################
//######################################################################################
    ImGui::Dummy(ImVec2(0, 30));

    /// Title text
    width = ImGui::GetContentRegionAvail().x;
    textW = ImGui::CalcTextSize("BUY").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - textW) * 0.5f);
    ImGui::Text("BUY");

    // Price text
    width = ImGui::GetContentRegionAvail().x;
    textW = ImGui::CalcTextSize("Price").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - textW) * 0.16f);
    ImGui::Text("Price");

    /// Price Input
    if (!PriceBuyInitialized_) {
        PriceBuy = bestAskPrice;
        PriceBuyInitialized_=true;
    }
    float inputWidth = width * 0.7f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - inputWidth) * 0.5f);
    ImGui::SetNextItemWidth(inputWidth);
    ImGui::InputFloat("##PriceBuy",&PriceBuy,0.01f,1.0f,"%.2f");
    if (PriceBuy < 0) PriceBuy = 0;

    // Quantity Text
    width = ImGui::GetContentRegionAvail().x;
    textW = ImGui::CalcTextSize("Quantity").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - textW) * 0.17f);
    ImGui::Text("Quantity");

    /// Quantity Input
    inputWidth = width * 0.7f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - inputWidth) * 0.5f);
    ImGui::SetNextItemWidth(inputWidth);
    ImGui::InputInt("##QuantityBuy",&QtyBuy);
    if (QtyBuy < 0) QtyBuy = 0;

    /// Tif Combo
    comboWidth = width * 0.7f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - comboWidth) * 0.5f);
    ImGui::SetNextItemWidth(comboWidth);
    ImGui::Combo("##TifBuy",&TifBuy,TIFOptions,IM_ARRAYSIZE(TIFOptions));

    ///Type Combo
    comboWidth = width * 0.7f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - comboWidth) * 0.5f);
    ImGui::SetNextItemWidth(comboWidth);
    ImGui::Combo("##TypeBuy", &TypeBuy, TypeOptions,IM_ARRAYSIZE(TypeOptions));

    /// Button
    float buttonWidth = width * 0.5f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - buttonWidth) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    if (ImGui::Button("Buy",ImVec2(buttonWidth,35))) {
        if (PriceBuy > 0.0f && QtyBuy > 0) {
            Engine_.CreateOrder(
                PriceBuy,
                QtyBuy,
                TypeOptions[TypeBuy],
                SymbolOptions[SymbolIndex],
                TIFOptions[TifBuy],
                1,
                "BUY"
                );
        }
    }
    ImGui::PopStyleColor(2);
    ImGui::Dummy(ImVec2(0, 30));

    ImGui::Separator();

    // ######################################################################################
    // ############################ Sell Zone ###############################################
    //######################################################################################
    ImGui::Dummy(ImVec2(0, 30));


    /// Title text
    width = ImGui::GetContentRegionAvail().x;
    textW = ImGui::CalcTextSize("SELL").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - textW) * 0.5f);
    ImGui::Text("SELL");

    // Price text
    width = ImGui::GetContentRegionAvail().x;
    textW = ImGui::CalcTextSize("Price").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - textW) * 0.16f);
    ImGui::Text("Price");

    /// Price Input
    if (!PriceSellInitialized_) {
        PriceSell = bestBidPrice;
        PriceSellInitialized_=true;
    }
    inputWidth = width * 0.7f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - inputWidth) * 0.5f);
    ImGui::SetNextItemWidth(inputWidth);
    ImGui::InputFloat("##PriceSell",&PriceSell,0.01f,1.0f,"%.2f");
    if (PriceSell < 0) PriceSell = 0;

    // Quantity Text
    width = ImGui::GetContentRegionAvail().x;
    textW = ImGui::CalcTextSize("Quantity").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - textW) * 0.17f);
    ImGui::Text("Quantity");

    /// Quantity Input
    inputWidth = width * 0.7f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - inputWidth) * 0.5f);
    ImGui::SetNextItemWidth(inputWidth);
    ImGui::InputInt("##QuantitySell",&QtySell);
    if (QtySell < 0) QtySell = 0;

    /// Tif Combo
    comboWidth = width * 0.7f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - comboWidth) * 0.5f);
    ImGui::SetNextItemWidth(comboWidth);
    ImGui::Combo("##TifSell",&TifSell,TIFOptions,IM_ARRAYSIZE(TIFOptions));

    ///Type Combo
    comboWidth = width * 0.7f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - comboWidth) * 0.5f);
    ImGui::SetNextItemWidth(comboWidth);
    ImGui::Combo("##TypeSell", &TypeSell, TypeOptions,IM_ARRAYSIZE(TypeOptions));

    /// Button
    buttonWidth = width * 0.5f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - buttonWidth) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("Sell",ImVec2(buttonWidth,35))) {
        if (PriceSell > 0.0f && QtySell> 0) {
            Engine_.CreateOrder(
                PriceSell,
                QtySell,
                TypeOptions[TypeSell],
                SymbolOptions[SymbolIndex],
                TIFOptions[TifSell],
                1,
                "SELL"
                );
        }
    }
    ImGui::PopStyleColor(2);
    ImGui::Dummy(ImVec2(0, 30));

    ImGui::Separator();


    // ######################################################################################
    // ############################ Exit Zone ###############################################
    //######################################################################################

    ImGui::Dummy(ImVec2(0, 60));

    buttonWidth = width * 0.5f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - buttonWidth) * 0.5f);
    if (ImGui::Button("Exit",ImVec2(buttonWidth,35))) {
        Exit=true;
    }
}