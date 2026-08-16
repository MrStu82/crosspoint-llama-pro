#pragma once

// Shared validity classification for on-disk save data (whole-game save.bin and
// per-level level_NN.bin files). GameSave::validateLevel() and
// GameState::validateSaveFile() both return this type, and both are implemented
// as a thin wrapper around the exact same parse/validate code path their
// respective load functions use -- there is intentionally only one definition
// of "valid" for each file kind, reused by both the load-boundary check and the
// Save Data Audit menu scan.
struct SaveValidity {
  enum class Status : uint8_t { NotPresent, Valid, Invalid };
  Status status = Status::NotPresent;
  // Populated only when status == Invalid. One of "version", "truncated", "seed",
  // "bad index" -- the fixed reason vocabulary the Save Data Audit screen displays.
  // Never blank when status == Invalid.
  const char* reason = "";
};
