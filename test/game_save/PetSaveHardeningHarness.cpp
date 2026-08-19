// Focused regression gate for companion save hardening. Build under ASan/UBSan
// with the same host stubs as the existing game-save harnesses.
#include <HalStorage.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include "GameState.h"
#include "GameTypes.h"

namespace { int failures = 0; }
#define CHECK(c, m) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s\n", m); ++failures; } } while (0)
struct PetV6 { bool active; uint8_t speciesId; char name[16]; uint8_t hpBase, attackBase, defenseBase; game::Item gear; bool hasGear; };
static const char* kPath = "/tmp/pet_save_gate/.crosspoint/game/save.bin";
static std::vector<uint8_t> bytes() { std::ifstream f(kPath, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
static void put(const std::vector<uint8_t>& b) { std::ofstream f(kPath, std::ios::binary | std::ios::trunc); f.write((const char*)b.data(), b.size()); }
static void seed() { system("rm -rf /tmp/pet_save_gate; mkdir -p /tmp/pet_save_gate"); GAME_STATE.newGame(7); GAME_STATE.pet.active=true; GAME_STATE.pet.speciesId=1; std::strcpy(GAME_STATE.pet.name,"Pip"); CHECK(GAME_STATE.saveToFile(), "seed save"); }
int main() {
  seed(); auto b=bytes(); const size_t o=1+sizeof(game::Player);
  // Invalid species must reject atomically.
  auto bad=b; bad[o+offsetof(game::Pet,speciesId)]=game::PET_SPECIES_COUNT; put(bad); CHECK(!GAME_STATE.loadFromFile(), "invalid species rejected");
  // Unterminated stored name is sanitised, not read past its buffer.
  auto unter=b; std::memset(unter.data()+o+offsetof(game::Pet,name), 'X', 16); put(unter); CHECK(GAME_STATE.loadFromFile(), "unterminated valid save loads"); CHECK(GAME_STATE.pet.name[15]=='\0', "loaded name terminated");
  // v6 layout migrates and receives a valid map position from the player.
  seed(); b=bytes(); PetV6 p6{}; const auto& p=GAME_STATE.pet; p6.active=p.active; p6.speciesId=p.speciesId; std::memcpy(p6.name,p.name,16); p6.hpBase=p.hpBase; p6.attackBase=p.attackBase; p6.defenseBase=p.defenseBase; p6.gear=p.gear; p6.hasGear=p.hasGear;
  std::vector<uint8_t> v6(b.begin(), b.begin()+o); v6[0]=6; v6.insert(v6.end(),(uint8_t*)&p6,(uint8_t*)&p6+sizeof(p6)); v6.insert(v6.end(),b.begin()+o+sizeof(game::Pet),b.end()); put(v6); CHECK(GAME_STATE.loadFromFile(), "v6 pet migrates"); CHECK(GAME_STATE.pet.x==GAME_STATE.player.x && GAME_STATE.pet.y==GAME_STATE.player.y, "v6 pet map position initialised");
  std::puts(failures ? "CHECKS FAILED" : "ALL CHECKS PASSED"); return failures ? 1 : 0;
}
