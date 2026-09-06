# Review fixes
Phase 1 active: recoverable persistence, pagination identity, stats write coalescing.
Phase 2 pending: subsystem review and tests. One final release build only.

Phase 1 implemented: previous committed backup restored on read/restart; temp readback; serialized stats mutations; complete EPUB render spec and effective TXT pagination identity; one book write per qualified EPUB turn; removed two internal no-caller methods after downstream scan. Native fault/state tests pass. No UI or SDK delta.
Phase 2 active: subsystem coverage and host suite; final release not yet built.
