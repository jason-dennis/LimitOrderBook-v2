//
// Created by denni on 3/21/2026.
//

#include "UI/OrdersPanel.h"
#include "imgui.h"
#include <iostream>
void OrdersPanel::Render(const std::string& Symbol) {

    auto Orders = Engine_.GetOrders(Symbol);
    auto trades = Engine_.GetTradesHistory(Symbol);
    ImGui::BeginTabBar("TABS");
    if (ImGui::BeginTabItem("Orders")) {
        ImGui::BeginTable("ORDERS",4);
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Side");
        ImGui::TableSetupColumn("Info");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();
        for (auto& order:Orders) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            char ID[32];
            snprintf(ID,sizeof(ID),"%d",order.GetOrderID());
            ImGui::Text("%s",ID);
            ImGui::TableNextColumn();

            auto Side = ToString(order.GetSide());
            ImGui::Text("%s",Side);
            ImGui::TableNextColumn();

            char InfoId[150];
            snprintf(InfoId,sizeof(InfoId),"Info##%d",order.GetOrderID());
            if (ImGui::Button(InfoId)) {
                SelectedOrder_ = std::make_unique<Order>(order) ;
            }

            ImGui::TableNextColumn();

            auto Status = ToString(order.GetStatus());
            if (strcmp(Status,"FILLED")==0) {
                ImGui::BeginDisabled(true);
                ImGui::Text("FILLED");
                ImGui::EndDisabled();
            }
            else if (strcmp(Status,"CANCELED")==0) {
                ImGui::BeginDisabled(true);
                ImGui::Text("CANCELED");
                ImGui::EndDisabled();
            }
            else {
                char CancelId[150];
                snprintf(CancelId,sizeof(CancelId),"Cancel##%d",order.GetOrderID());
                if (ImGui::Button(CancelId)) {
                    Engine_.CancelOrder(order.GetOrderID(),Symbol);
                }
            }

        }
        ImGui::EndTable();
        if (SelectedOrder_) {
            ImGui::OpenPopup("OrderInfo");
        }
        if (ImGui::BeginPopupModal("OrderInfo",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            if (SelectedOrder_) {
                ImGui::Text("Order ID: %d",SelectedOrder_->GetOrderID());
                ImGui::Text("Trader ID: %d",SelectedOrder_->GetTraderID());
                ImGui::Text("Side: %s",ToString(SelectedOrder_->GetSide()));
                ImGui::Text("Type: %s",ToString(SelectedOrder_->GetType()));
                ImGui::Text("Price: %.2f",SelectedOrder_->GetPrice()/100.0f);
                ImGui::Text("Quantity: %d",SelectedOrder_->GetQuantity());
                ImGui::Text("Status: %s", ToString(SelectedOrder_->GetStatus()));
                auto tp=SelectedOrder_->GetTime();
                std::time_t t =std::chrono::system_clock::to_time_t(tp);
                std::tm bt;
#ifdef _WIN32
                localtime_s(&bt,&t);
#else
                localtime_r(&t,&bt);
#endif

                auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(
                        tp.time_since_epoch()%std::chrono::seconds(1)).count();

                char TimeStr[80];
                strftime(TimeStr,sizeof(TimeStr), "%H:%M:%S", &bt);

                char FullTime[100];
                snprintf(FullTime,sizeof(FullTime),"%s.%03d",TimeStr,(int)ms);
                ImGui::Text("Time: %s",FullTime);
            }
            if (ImGui::Button("Close")) {
                SelectedOrder_=nullptr;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Trades")) {
        ImGui::BeginTable("TRADES",2);
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Info");
        ImGui::TableHeadersRow();

        for (auto& trade :trades) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            char ID[32];
            snprintf(ID,sizeof(ID),"%d",trade.GetID());
            ImGui::Text("%s",ID);
            ImGui::TableNextColumn();

            char InfoId[150];
            snprintf(InfoId,sizeof(InfoId),"Info##%d",trade.GetID());
            if (ImGui::Button(InfoId)) {
                SelectedTrade_= std::make_unique<Trade>(trade);
            }

        }
        ImGui::EndTable();

        if (SelectedTrade_) {
            ImGui::OpenPopup("TradeInfo");
        }
        if (ImGui::BeginPopupModal("TradeInfo",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            if (SelectedTrade_) {

                ImGui::Text("ID: %d",SelectedTrade_->GetID());
                ImGui::Text("Maker ID: %d",SelectedTrade_->GetMakerID());
                ImGui::Text("Taker ID: %d",SelectedTrade_->GetTakerID());
                ImGui::Text("Price: %.2f",SelectedTrade_->GetPrice()/100.0f);
                ImGui::Text("Quantity: %d",SelectedTrade_->GetQuantity());
                auto tp=SelectedTrade_->GetTimestamp();
                std::time_t t =std::chrono::system_clock::to_time_t(tp);
                std::tm bt;
#ifdef _WIN32
                localtime_s(&bt,&t);
#else
                localtime_r(&t,&bt);
#endif

                auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(
                        tp.time_since_epoch()%std::chrono::seconds(1)).count();

                char TimeStr[80];
                strftime(TimeStr,sizeof(TimeStr), "%H:%M:%S", &bt);

                char FullTime[100];
                snprintf(FullTime,sizeof(FullTime),"%s.%03d",TimeStr,(int)ms);
                ImGui::Text("Time: %s",FullTime);
            }
            if (ImGui::Button("Close")) {
                SelectedTrade_=nullptr;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::EndTabItem();

    }
    ImGui::EndTabBar();

}
