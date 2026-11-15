# picco-fw-1115 roadmap

Picco (Onyx BOOX Tiles, ESP32-S3) as a second board behind the existing
FREEINK_DEVICE_<DEV> switch. Stock partition table, bootloader, otadata and
safeboot are never written; the app runs from ota_0 at 0xE0000 (size 0xE70000).

- Phase A: two builds from one tree. X4 Pro ELF section-identical to base;
  Picco app-only image, upstream network OTA killed, dual-slot SD/boot-switch
  paths refuse. Deliver bare app .bin for 0xE0000.
- Phase B (on word): single binary detecting the running board.
- Phase C (analysis done, research/picco-fw-1115/PHASE-C.md): SD update via
  stock safeboot /update/firmware.bin; build only on word.
