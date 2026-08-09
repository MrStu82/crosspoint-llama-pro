#include "SolitaireStore.h"

namespace {
void cardToJson(JsonObject obj, const SolitaireStore::Card& c) {
  obj["r"] = c.rank;
  obj["s"] = c.suit;
  obj["f"] = c.faceUp;
}

SolitaireStore::Card cardFromJson(JsonObjectConst obj) {
  SolitaireStore::Card c;
  c.rank = obj["r"] | 0;
  c.suit = obj["s"] | 0;
  c.faceUp = obj["f"] | false;
  return c;
}

void pileToJson(JsonArray arr, const std::vector<SolitaireStore::Card>& pile) {
  for (const auto& c : pile) {
    cardToJson(arr.add<JsonObject>(), c);
  }
}

void pileFromJson(JsonArrayConst arr, std::vector<SolitaireStore::Card>& pile) {
  pile.clear();
  pile.reserve(arr.size());
  for (JsonObjectConst obj : arr) {
    pile.push_back(cardFromJson(obj));
  }
}
}  // namespace

void SolitaireStore::toJson(JsonDocument& doc) const {
  pileToJson(doc["stock"].to<JsonArray>(), stock);
  pileToJson(doc["waste"].to<JsonArray>(), waste);
  JsonArray cols = doc["tableau"].to<JsonArray>();
  for (int col = 0; col < kColumns; col++) {
    pileToJson(cols.add<JsonArray>(), tableau[col]);
  }
  JsonArray foundations = doc["foundations"].to<JsonArray>();
  for (int f = 0; f < kFoundations; f++) {
    foundations.add(foundationTop[f]);
  }
}

bool SolitaireStore::fromJson(JsonVariantConst doc) {
  pileFromJson(doc["stock"].as<JsonArrayConst>(), stock);
  pileFromJson(doc["waste"].as<JsonArrayConst>(), waste);

  for (int col = 0; col < kColumns; col++) {
    tableau[col].clear();
  }
  JsonArrayConst cols = doc["tableau"].as<JsonArrayConst>();
  int col = 0;
  for (JsonArrayConst colArr : cols) {
    if (col >= kColumns) {
      break;
    }
    pileFromJson(colArr, tableau[col]);
    col++;
  }

  for (int f = 0; f < kFoundations; f++) {
    foundationTop[f] = 0;
  }
  JsonArrayConst foundations = doc["foundations"].as<JsonArrayConst>();
  int f = 0;
  for (JsonVariantConst v : foundations) {
    if (f >= kFoundations) {
      break;
    }
    foundationTop[f] = v.as<uint8_t>();
    f++;
  }

  return true;
}
