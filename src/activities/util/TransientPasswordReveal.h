#pragma once

// Tiny state machine for password visibility. It deliberately stores no text:
// callers retain ownership of the credential and only ask whether rendering is
// temporarily allowed. Every cancellation/lifecycle path converges on reset().
class TransientPasswordReveal {
 public:
  bool begin() {
    if (visible_) return false;
    visible_ = true;
    return true;
  }

  bool reset() {
    if (!visible_) return false;
    visible_ = false;
    return true;
  }

  bool visible() const { return visible_; }

 private:
  bool visible_ = false;
};
