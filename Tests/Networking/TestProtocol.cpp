//
// Created by Ognean Jason Dennis on 15.07.2026.
//

#include <gtest/gtest.h>

#include "Networking/Protocol.h"

namespace {

    OrderRequest MakeRequest(float price, int quantity, const std::string& type,
                             const std::string& symbol, const std::string& tif,
                             int traderId, const std::string& side) {
        OrderRequest req;
        req.Price = price;
        req.Quantity = quantity;
        req.Type = type;
        req.Symbol = symbol;
        req.TIF = tif;
        req.TraderID = traderId;
        req.Side = side;
        return req;
    }

} // namespace

// ══════════════════════════════════════════════════════════════
// Round-trip: Encode urmat de Decode trebuie sa dea inapoi
// exact acelasi OrderRequest, pentru fiecare camp in parte.
// ══════════════════════════════════════════════════════════════

TEST(ProtocolTest, RoundTripPreservesAllFieldsForTypicalOrder) {
auto original = MakeRequest(105.50f, 25, "LIMIT", "AAPL", "GTC", 42, "BUY");

auto bytes = Encode(original);
auto decoded = Decode(bytes);

ASSERT_TRUE(decoded.has_value());
EXPECT_NEAR(decoded->Price, original.Price, 0.01f);
EXPECT_EQ(decoded->Quantity, original.Quantity);
EXPECT_EQ(decoded->Type, original.Type);
EXPECT_EQ(decoded->Symbol, original.Symbol);
EXPECT_EQ(decoded->TIF, original.TIF);
EXPECT_EQ(decoded->TraderID, original.TraderID);
EXPECT_EQ(decoded->Side, original.Side);
}

TEST(ProtocolTest, RoundTripPreservesLimitType) {
auto original = MakeRequest(100.0f, 10, "LIMIT", "AAPL", "GTC", 1, "BUY");
auto decoded = Decode(Encode(original));
ASSERT_TRUE(decoded.has_value());
EXPECT_EQ(decoded->Type, "LIMIT");
}

TEST(ProtocolTest, RoundTripPreservesMarketType) {
auto original = MakeRequest(100.0f, 10, "MARKET", "AAPL", "GTC", 1, "BUY");
auto decoded = Decode(Encode(original));
ASSERT_TRUE(decoded.has_value());
EXPECT_EQ(decoded->Type, "MARKET");
}

TEST(ProtocolTest, RoundTripPreservesBuySide) {
auto original = MakeRequest(100.0f, 10, "LIMIT", "AAPL", "GTC", 1, "BUY");
auto decoded = Decode(Encode(original));
ASSERT_TRUE(decoded.has_value());
EXPECT_EQ(decoded->Side, "BUY");
}

TEST(ProtocolTest, RoundTripPreservesSellSide) {
auto original = MakeRequest(100.0f, 10, "LIMIT", "AAPL", "GTC", 1, "SELL");
auto decoded = Decode(Encode(original));
ASSERT_TRUE(decoded.has_value());
EXPECT_EQ(decoded->Side, "SELL");
}

TEST(ProtocolTest, RoundTripPreservesGtcTif) {
auto original = MakeRequest(100.0f, 10, "LIMIT", "AAPL", "GTC", 1, "BUY");
auto decoded = Decode(Encode(original));
ASSERT_TRUE(decoded.has_value());
EXPECT_EQ(decoded->TIF, "GTC");
}

TEST(ProtocolTest, RoundTripPreservesIocTif) {
auto original = MakeRequest(100.0f, 10, "LIMIT", "AAPL", "IOC", 1, "BUY");
auto decoded = Decode(Encode(original));
ASSERT_TRUE(decoded.has_value());
EXPECT_EQ(decoded->TIF, "IOC");
}

TEST(ProtocolTest, RoundTripPreservesPriceWithDecimals) {
// Testeaza exact bug-ul gasit: impartirea la Tick facuta pe intregi
// trunchia zecimalele. Preturi cu ".50" sau ".99" ar fi picat inainte.
auto original = MakeRequest(105.50f, 10, "LIMIT", "AAPL", "GTC", 1, "BUY");
auto decoded = Decode(Encode(original));
ASSERT_TRUE(decoded.has_value());
EXPECT_NEAR(decoded->Price, 105.50f, 0.01f);
}

TEST(ProtocolTest, RoundTripPreservesZeroQuantity) {
auto original = MakeRequest(100.0f, 0, "LIMIT", "AAPL", "GTC", 1, "BUY");
auto decoded = Decode(Encode(original));
ASSERT_TRUE(decoded.has_value());
EXPECT_EQ(decoded->Quantity, 0);
}

TEST(ProtocolTest, RoundTripPreservesShortSymbol) {
auto original = MakeRequest(100.0f, 10, "LIMIT", "A", "GTC", 1, "BUY");
auto decoded = Decode(Encode(original));
ASSERT_TRUE(decoded.has_value());
EXPECT_EQ(decoded->Symbol, "A");
}

TEST(ProtocolTest, RoundTripPreservesLongerSymbol) {
auto original = MakeRequest(100.0f, 10, "LIMIT", "GOOGL", "GTC", 1, "BUY");
auto decoded = Decode(Encode(original));
ASSERT_TRUE(decoded.has_value());
EXPECT_EQ(decoded->Symbol, "GOOGL");
}

TEST(ProtocolTest, RoundTripPreservesLargeTraderID) {
auto original = MakeRequest(100.0f, 10, "LIMIT", "AAPL", "GTC", 999999, "BUY");
auto decoded = Decode(Encode(original));
ASSERT_TRUE(decoded.has_value());
EXPECT_EQ(decoded->TraderID, 999999);
}

// ══════════════════════════════════════════════════════════════
// Comportament defensiv: Decode nu trebuie sa crape la bytes
// incomplete/malformate, ci sa intoarca nullopt explicit --
// exact scenariul unei citiri partiale de pe socket.
// ══════════════════════════════════════════════════════════════

TEST(ProtocolTest, DecodeReturnsNulloptForEmptyBytes) {
std::vector<uint8_t> empty;
auto result = Decode(empty);
EXPECT_FALSE(result.has_value());
}

TEST(ProtocolTest, DecodeReturnsNulloptForTooFewBytesForFixedFields) {
std::vector<uint8_t> tooFew = {0, 1, 2, 3, 4}; // mult sub minimul de 20
auto result = Decode(tooFew);
EXPECT_FALSE(result.has_value());
}

TEST(ProtocolTest, DecodeReturnsNulloptForTruncatedSymbol) {
// Simuleaza o citire partiala de pe socket: mesaj valid, dar caruia
// ii lipsesc ultimele caractere ale simbolului (byte-ul de lungime
// spune, de exemplu, 4 caractere, dar au sosit doar 2).
auto original = MakeRequest(100.0f, 10, "LIMIT", "AAPL", "GTC", 1, "BUY");
auto fullBytes = Encode(original);

std::vector<uint8_t> truncated(fullBytes.begin(), fullBytes.end() - 2);
auto result = Decode(truncated);

EXPECT_FALSE(result.has_value())
<< "Decode ar trebui sa esueze explicit, nu sa citeasca memorie invalida";
}

TEST(ProtocolTest, DecodeReturnsNulloptWhenSymbolLengthByteIsMissing) {
// Exact la limita: toate campurile fixe prezente, dar lipseste chiar
// byte-ul de lungime al simbolului.
auto original = MakeRequest(100.0f, 10, "LIMIT", "AAPL", "GTC", 1, "BUY");
auto fullBytes = Encode(original);

// Pastreaza doar campurile fixe (19 bytes), fara byte-ul de lungime
// al simbolului si fara simbolul insusi.
std::vector<uint8_t> onlyFixedFields(fullBytes.begin(), fullBytes.begin() + 19);
auto result = Decode(onlyFixedFields);

EXPECT_FALSE(result.has_value());
}