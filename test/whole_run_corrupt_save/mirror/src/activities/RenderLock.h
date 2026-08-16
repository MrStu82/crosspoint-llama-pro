#pragma once
// Shadow of src/activities/RenderLock.h, reused verbatim from
// test/sponsor_hp_clamp/mirror/src/activities/RenderLock.h. The real header only
// declares its methods (bodies live in a RenderLock.cpp a repo-wide search
// could not locate), so this stub matches the real public shape with trivial
// inline bodies -- the harness constructs one RenderLock{} to pass into
// render(RenderLock&&) and never touches locking semantics at all.
class Activity;

class RenderLock {
  bool isLocked = false;

 public:
  explicit RenderLock() : isLocked(true) {}
  explicit RenderLock(Activity&) : isLocked(true) {}
  RenderLock(const RenderLock&) = delete;
  RenderLock& operator=(const RenderLock&) = delete;
  ~RenderLock() = default;
  void unlock() { isLocked = false; }
  static bool peek() { return false; }
};
