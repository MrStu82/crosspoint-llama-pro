#pragma once
#include <HalStorage.h>

#include <iostream>

namespace serialization {
template <typename T>
void writePod(std::ostream& os, const T& value) {
  os.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
void writePod(HalFile& file, const T& value) {
  file.write(reinterpret_cast<const uint8_t*>(&value), sizeof(T));
}

template <typename T>
void readPod(std::istream& is, T& value) {
  is.read(reinterpret_cast<char*>(&value), sizeof(T));
}

template <typename T>
void readPod(HalFile& file, T& value) {
  file.read(reinterpret_cast<uint8_t*>(&value), sizeof(T));
}

inline void writeString(std::ostream& os, const std::string& s) {
  const uint32_t len = s.size();
  writePod(os, len);
  os.write(s.data(), len);
}

inline void writeString(HalFile& file, const std::string& s) {
  const uint32_t len = s.size();
  writePod(file, len);
  file.write(reinterpret_cast<const uint8_t*>(s.data()), len);
}

// Upper bound for a serialized string; a larger length means a corrupt cache.
constexpr uint32_t MAX_STRING_LEN = 4096;

inline void readString(std::istream& is, std::string& s) {
  uint32_t len = 0;
  readPod(is, len);
  if (!is || len > MAX_STRING_LEN) {
    s.clear();
    return;
  }
  s.resize(len);
  is.read(&s[0], len);
  s.resize(is.gcount());
}

inline void readString(HalFile& file, std::string& s) {
  uint32_t len = 0;
  if (file.read(&len, sizeof(len)) != static_cast<int>(sizeof(len)) || len > MAX_STRING_LEN) {
    s.clear();
    return;
  }
  s.resize(len);
  const int got = file.read(&s[0], len);
  s.resize(got > 0 ? got : 0);
}
}  // namespace serialization
