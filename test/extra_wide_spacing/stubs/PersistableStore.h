#pragma once

class PersistableStoreBase {
 protected:
  void requestResave() {}
};

template <typename T>
class PersistableStore : public PersistableStoreBase {
 public:
  static T& getInstance() {
    static T instance;
    return instance;
  }

  bool saveToFile() const { return true; }
};
