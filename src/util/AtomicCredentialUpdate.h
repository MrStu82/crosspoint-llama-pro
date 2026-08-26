#pragma once

#include <string>

namespace AtomicCredentialUpdate {

// The persistence callback must atomically replace durable storage. If it
// fails, memory is rolled back and the previous durable file remains valid.
template <typename Persist>
bool replace(std::string& current, const std::string& replacement, Persist&& persist) {
  const std::string previous = current;
  current = replacement;
  if (persist()) return true;
  current = previous;
  return false;
}

}  // namespace AtomicCredentialUpdate
