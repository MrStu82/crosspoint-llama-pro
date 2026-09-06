# X4 PRO physical power/wake acceptance — ready, NOT EXECUTED
Blocker: Trantor enumeration contains only Espressif 94:A9:90:E3:E1:3C, the previously identified Deck, NOT an authorized connected X4 Pro. No suitable current meter is identified/available. No serial device was opened; no hardware write or flash is authorized by this job.

Prerequisites for a later separately authorized physical session: confirm X4 Pro identity, approved already-installed source/image, instrument model/calibration/current range/burden voltage, regulated supply voltage and wiring agreed by owner. Do not extrapolate Deck readings. No wiring changes or flash under this dispatch.

Record firmware HEAD/SDK, battery/supply voltage, SD card/book/position, ambient conditions, meter range/sample rate, input-power measurement point and any USB-powered rail. Keep the same device/settings/book for baseline and candidate. Save raw current-versus-time CSV, not only screenshots.

Matrix: normal sleep and transparent sleep × USB absent/present × frontlight off/on immediately before sleep = eight cells. For each: record active reading current, trigger the existing touch-compatible sleep flow, wait 60 seconds, then record 120 seconds of settled current (mean/min/max). Repeat sleep/wake ten times per cell. Record successful wake source, latency, retained-frame correctness, first touch response, restored book position and frontlight state. Also record the existing full-off path and a cold boot without USB; never conflate off, deep sleep and USB standby.

Acceptance: all ten wake cycles/cell retain existing touch/resume/frame/frontlight behaviour; no unexplained sustained drain relative to the approved same-hardware baseline beyond instrument uncertainty. Establish any numeric current threshold from baseline measurements BEFORE judging the candidate. A transient first-second peak is not settled current. Report USB contribution separately; do not infer battery life from app size or a host test.

Template columns (CSV): device,source,sdk,baseline_or_candidate,mode,usb,frontlight_before_sleep,cycle,supply_V,settled_mean_mA,settled_min_mA,settled_max_mA,wake_source,wake_ms,touch_pass,frame_pass,position_pass,frontlight_pass,raw_trace_path,notes.

No battery improvement is claimed. Existing panel-park, wake/USB/frontlight mappings remain unchanged; the preservation runner reuses their deterministic tests. Electrical power loss and torn FAT sectors are outside host persistence mocks.
