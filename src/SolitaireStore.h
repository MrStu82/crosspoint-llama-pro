#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <cstdint>
#include <vector>

// Persisted Klondike Solitaire game state, so a round survives an app
// restart. Deliberately a flat mirror of SolitaireActivity's own game state
// (its own Card struct is private) rather than something SolitaireActivity
// reads/writes directly -- the activity converts to/from this on save/load.
class SolitaireStore : public PersistableStore<SolitaireStore> {
 public:
  struct Card {
    uint8_t rank = 0;  // 1=Ace .. 13=King, 0 = invalid/unused
    uint8_t suit = 0;  // 0=Spades, 1=Hearts, 2=Diamonds, 3=Clubs
    bool faceUp = false;
  };

  static constexpr int kColumns = 7;
  static constexpr int kFoundations = 4;

  std::vector<Card> stock;
  std::vector<Card> waste;
  std::vector<Card> tableau[kColumns];
  uint8_t foundationTop[kFoundations] = {};  // 0 = empty, else highest rank placed

  static const char* getFilePath() { return "/.crosspoint/solitaire.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

 private:
  SolitaireStore() = default;
  ~SolitaireStore() = default;

  friend class PersistableStore<SolitaireStore>;
};

// Helper macro to access the solitaire save-game store
#define SOLITAIRE_STORE SolitaireStore::getInstance()
