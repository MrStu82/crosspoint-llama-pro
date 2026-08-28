#pragma once
inline void phase5aRenderLockSideEffect() { asm volatile("" ::: "memory"); }
class RenderLock {
 public:
  RenderLock() { phase5aRenderLockSideEffect(); }
  ~RenderLock() { phase5aRenderLockSideEffect(); }
};
