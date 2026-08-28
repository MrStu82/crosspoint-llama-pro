#pragma once

class HalGPIO {
 public:
  bool deviceIsX3() const { return x3; }
  bool x3 = false;
};

inline HalGPIO gpio;
