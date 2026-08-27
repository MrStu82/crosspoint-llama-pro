#pragma once

#include <cctype>
#include <string>

// One-shot import of a Hardcover personal access token handed to the device as a
// plain text file on the SD card root. This is the only entry path the firmware
// has: there is no on-device keyboard route to a 40-plus character opaque token,
// and the web settings API deliberately never returns or accepts it.
//
// The plaintext file is deleted as part of a successful import -- leaving a
// readable credential on a removable card would be a worse resting place than
// the obfuscated store it was just copied into.
namespace HardcoverTokenImport {

// Path checked on the SD card root.
inline constexpr const char* kSourcePath = "/hardcover_token.txt";

enum class Status {
  NoFile,        // nothing to import; not an error
  Accepted,      // stored, and the plaintext file was removed
  StoredNotWiped,// stored, but the plaintext file could not be deleted
  Unreadable,    // the file exists but could not be read
  Invalid,       // contents are not a Hardcover token
  StoreFailed,   // valid token, but it could not be written durably
};

// Strips leading/trailing ASCII whitespace, and stops at the first CR or LF so a
// file saved by a text editor with a trailing newline still yields the token.
// Inline and free of any filesystem or store dependency so the host test can
// exercise it without standing up a HAL.
inline std::string sanitise(const std::string& raw) {
  size_t begin = 0;
  while (begin < raw.size() && std::isspace(static_cast<unsigned char>(raw[begin]))) ++begin;
  size_t end = begin;
  while (end < raw.size() && raw[end] != '\r' && raw[end] != '\n') ++end;
  while (end > begin && std::isspace(static_cast<unsigned char>(raw[end - 1]))) --end;
  return raw.substr(begin, end - begin);
}

// Reads, validates, stores and wipes. Safe to call when no file is present.
// The caller owns the wording, so this stays free of the translation table.
Status run();

}  // namespace HardcoverTokenImport
