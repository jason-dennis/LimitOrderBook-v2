//
// Created by denni on 3/21/2026.
//

#include "../../include/UI/OrderBookPanel.h"
#include "imgui.h"

void OrderBookPanel::Render(std::string& Symbol) {

   ImVec2 available = ImGui::GetContentRegionAvail();
   float headerHeight = ImGui::GetTextLineHeightWithSpacing()+10;
   float halfHeight = (available.y-headerHeight)*0.5f;

   ImGui::BeginChild("Asks", ImVec2(0, halfHeight), false, ImGuiWindowFlags_NoScrollbar);
         RenderAsks(Symbol);
   ImGui::EndChild();
   if (ImGui::BeginTable("Header", 3)) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      float cellWidth = ImGui::GetColumnWidth();
      float textWidth = ImGui::CalcTextSize("Price").x;
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cellWidth - textWidth) * 0.5f);
      ImGui::Text("Price");
      ImGui::TableNextColumn();

      auto bestAsks = Engine_.GetBestAsks(1,Symbol);
      auto bestBids = Engine_.GetBestBids(1,Symbol);

      char midStr[32] = "---";
      if (!bestAsks.empty() && !bestBids.empty()) {
         float mid = (bestAsks[0].GetPrice() + bestBids[0].GetPrice()) / 2.0f / 100.0f;
         snprintf(midStr, sizeof(midStr), "%.2f", mid);
      }

      cellWidth = ImGui::GetColumnWidth();
      textWidth = ImGui::CalcTextSize(midStr).x;
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cellWidth - textWidth) * 0.5f);
      ImGui::Text("%s", midStr);
      ImGui::TableNextColumn();

      cellWidth = ImGui::GetColumnWidth();
      textWidth = ImGui::CalcTextSize("Quantity").x;
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cellWidth - textWidth) * 0.5f);
      ImGui::Text("Quantity");
      ImGui::TableNextColumn();
      ImGui::EndTable();
   }

   ImGui::BeginChild("Bids", ImVec2(0, halfHeight), false, ImGuiWindowFlags_NoScrollbar);
         RenderBids(Symbol);
   ImGui::EndChild();
}

void OrderBookPanel::RenderBids(std::string& Symbol) {

   auto bids = Engine_.GetBestBids(10,Symbol);
   if (ImGui::BeginTable("Bids",3)) {
      for (auto &bid : bids) {
         ImGui::TableNextRow();

         //Price Column
         ImGui::TableNextColumn();
         float cellWidth = ImGui::GetColumnWidth();
         char PriceStr[32];
         snprintf(PriceStr,sizeof(PriceStr),"%.2f",bid.GetPrice()/100.0f);
         float textWidth = ImGui::CalcTextSize(PriceStr).x;
         ImGui::SetCursorPosX(ImGui::GetCursorPosX()+ (cellWidth-textWidth)* 0.5f);
         ImGui::TextColored(ImVec4(0.3f,1.0f,0.3f,1.0f),"%s",PriceStr);

         //Empty Column
         ImGui::TableNextColumn();

         //Quantity Column
         ImGui::TableNextColumn();
         cellWidth = ImGui::GetColumnWidth();
         char QtyStr[32];
         snprintf(QtyStr,sizeof(QtyStr),"%d",bid.GetQuantity());
         textWidth = ImGui::CalcTextSize(QtyStr).x;
         ImGui::SetCursorPosX(ImGui::GetCursorPosX()+ (cellWidth-textWidth)* 0.5f);
         ImGui::Text("%s",QtyStr);
      }
      ImGui::EndTable();
   }
}

void OrderBookPanel::RenderAsks(std::string& Symbol) {

   auto asks = Engine_.GetBestAsks(10, Symbol);
   std::vector<Order> reversedAsks(asks.rbegin(), asks.rend());
   float tableHeight = asks.size() * ImGui::GetTextLineHeightWithSpacing();
   float emptySpace = ImGui::GetContentRegionAvail().y - tableHeight;
   if (emptySpace > 0)
      ImGui::Dummy(ImVec2(0, emptySpace));

   if (ImGui::BeginTable("Asks",3)) {

      for (auto &ask : reversedAsks) {
         ImGui::TableNextRow();

         //Price Column
         ImGui::TableNextColumn();
         float cellWidth = ImGui::GetColumnWidth();
         char PriceStr[32];
         snprintf(PriceStr,sizeof(PriceStr),"%.2f",ask.GetPrice()/100.0f);
         float textWidth = ImGui::CalcTextSize(PriceStr).x;
         ImGui::SetCursorPosX(ImGui::GetCursorPosX()+ (cellWidth-textWidth)* 0.5f);
         ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),"%s",PriceStr);

         //Empty Column
         ImGui::TableNextColumn();

         //Quantity Column
         ImGui::TableNextColumn();
         cellWidth = ImGui::GetColumnWidth();
         char QtyStr[32];
         snprintf(QtyStr,sizeof(QtyStr),"%d",ask.GetQuantity());
         textWidth = ImGui::CalcTextSize(QtyStr).x;
         ImGui::SetCursorPosX(ImGui::GetCursorPosX()+ (cellWidth-textWidth)* 0.5f);
         ImGui::Text("%s",QtyStr);
      }
      ImGui::EndTable();
   }
}
