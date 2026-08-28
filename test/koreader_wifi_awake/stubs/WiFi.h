#pragma once

class WiFiClass {
 public:
  bool getSleep() const;
  bool setSleep(bool enabled);
};

extern WiFiClass WiFi;
