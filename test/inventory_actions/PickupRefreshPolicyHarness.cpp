#include <cstdlib>
#include <iostream>

#include "GameControlRefreshPolicy.h"

namespace {
void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
}  // namespace

int main() {
  // A resolved Action pickup has one normal game-frame panel transaction and
  // no separate down/up feedback transactions. Before this policy it was
  // three blocking transactions (down + up + game frame).
  const int pickupPanelTransactions =
      (game::refreshControlFeedbackImmediately(0) ? 2 : 0) + 1;
  require(pickupPanelTransactions == 1, "pickup must issue exactly one panel transaction");

  // Menu does not render a game frame after the tap, so retain its down/up
  // feedback rather than making the control appear unresponsive.
  const int menuFeedbackTransactions =
      game::refreshControlFeedbackImmediately(1) ? 2 : 0;
  require(menuFeedbackTransactions == 2, "Menu feedback must remain intact");

  std::cout << "Pickup refresh policy: PASS (Action 3->1 panel transactions; Menu 2 unchanged)\n";
  return 0;
}
