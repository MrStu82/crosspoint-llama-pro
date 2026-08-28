#pragma once

class HalDisplay {
 public:
  enum RefreshMode { FAST_REFRESH, HALF_REFRESH, FULL_REFRESH };
  bool isInverted() const { return inverted; }
  void setInverted(bool value) { inverted = value; }
  bool inverted = false;
};

inline HalDisplay display;
