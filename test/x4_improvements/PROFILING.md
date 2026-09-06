# Bounded reader profiling
Opt in only on a diagnostic build with -DX4_READER_DIAGNOSTICS=1; default release is disabled. No settings or visible UI is added. No device flash authorized in the current task.

Stages (numeric IDs): 0 recoverable persistence incl. lock/readback/renames, 1 TXT initialization/cold open, 2 TXT text layout incl. reads, 3 EPUB render incl. nested stages, 4 render-lock acquisition, 5 synchronous full-buffer refresh, 6 EPUB incremental pagination, 7 TXT content reads. Durations are inclusive and may overlap: do NOT add them together. Async/gray/window refreshes are not instrumented by stage5. 200 samples per stage maximum, one first-sample/cold and one n=200 summary; p50/p95/max microseconds. Diagnostic logging has overhead. No optimization is justified by a single sample.

ESP output adds internal-heap minimum, largest free internal block minimum and calling-task stack low-water mark IN WORDS. Host sentinel UINT32_MAX means unavailable, not enormous RAM headroom. No credentials or book text are logged by this profiler.

Use fixed checked-in EPUB fixture(s) and generated 1MiB TXT corpus, source/SDK/build flags/card/settings/font hashes. Capture cold opens and at least 200 forward turns per format. Record accepted page transitions, not only injected inputs. Compare only like-for-like hardware or simulator runs; simulator wall time is NOT e-ink/SD/battery performance. Hash book and selected font once at initialization; Content identity log records bytes/ms; host digest benchmark isolates CPU/memory cost, excludes SD.

After collecting repeatable distributions, target only a dominant measured stage, rerun durability/preservation gates, and reject regressions. This task does not introduce batching, background tasks, new cache layers or an unmeasured optimization.
