<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S2.1 / S3 clean-room input audit

Date: 2026-10-19. Worktree /root/fm1/Felucca-ble, ble-controller, uncommitted
above 3581aba. S1/S2 audits and manifests are historical; their then-current
snapshot paths are not permission to reopen them. This task used frozen r4.
No device, USB, network host, flashing or commit operation was performed. No
subagents were spawned. Every shell command had an explicit allowed workdir;
the supplied /root/fm1-toolchain cwd was not inspected or used.

`out/BLE-S3-INPUT-SHA256SUMS` hashes 312 allowed external input files. It covers
content read, searched, copied, compiled, packaged or hashed; some normative
files/reviews were read by section. Allowed-directory filename inventories are
not claims that each file's content was used as an implementation reference.
The tool refuses to reopen superseded snapshots. Original host imports remain
in `out/BLE-S2-NIMBLE-INPUT-SHA256SUMS` (129 files); S3's TinyCrypt additions are
in `out/BLE-S3-TINYCRYPT-INPUT-SHA256SUMS` (23 files). All imported upstream code
is byte-identical to those original hashes; PINNED.md is our provenance note.

## External inputs

- All seven files of `/root/wl82-ble-spec-snapshots/r4/`: RULES.md,
  00-overview.md, 01-radio-bringup.md, 02-baseband.md, 03-link-layer-hci.md,
  04-platform.md and 05-acceptance-tests.md. r3 and older snapshots were not
  reopened. No reference link into the live specification or reconstruction
  was followed.
- Permitted review files under `/root/wl82-ble-spec-review/`:
  IMPLEMENTABILITY-gpt-6.1-sol.md, IMPLEMENTABILITY-r2-gpt-6.1-sol.md,
  IMPLEMENTABILITY-r3-gpt-6.1-sol.md, RESPONSES-r1.md, RESPONSES-r2.md,
  RESPONSES-r3.md and PLATFORM-felucca.md. Hashing the permitted prior inputs
  is included in the new manifest. A review's references to forbidden trees
  were not followed.
- `/root/nimble-src` at da7e3256da3ba80b232df880f40d8359311cc62e:
  the original 129-file host/services/transport/porting subset; the 23-file
  TinyCrypt header/AES/CMAC/SHA256/P-256 subset with LICENSE/AUTHORS; host
  syscfg.yml and porting Makefile.defs/Makefile.tinycrypt; NPL Linux header and
  the bounded porting/nimble/src C-file search for startup/ECC hooks. No
  NimBLE controller implementation is linked. The byte-order/SMP reference
  inspected during this continuation was the allowed imported host source.
- `/root/fm1-app-re/nimble/hci-usage-device.txt` only. No other app-re file.
- Public register headers under `/root/sdk-blemidi/include_lib/driver/` only:
  the inherited explicit S2 header-search list is rehashed; no other SDK tree
  was read. No Bluetooth SDK source/library was used.
- `/root/felucca-ble-env/`: the JieLi compiler/linker/objcopy/objdump tools,
  compiler builtin headers found by dependency generation, and the three
  already-pinned sdk-min packaging assets (uboot.boot, cfg_tool.bin,
  cfg/eq_cfg_hw.bin). Their inclusion is packaging provenance, not BLE code.
  No vendor archive, bitcode, IR, object or ELF was read or disassembled.

The parent's S2 bench results in the brief and existing worktree logs were
accepted as supplied. They were not independently remeasured. The platform
implementation/reference was the permitted worktree itself. The read-only
Felucca checkout was not modified. Git metadata was used only for identity,
status and diff checks. Target objdump listings and compiler IR are exclusively
newly built Felucca/NimBLE GPL/open-source products, used by existing build
checks, never vendor firmware or forbidden material.

No live-spec, trace, patch-work, fm1-toolchain, unrelated fm1 directory, other
app-re material or vendor Bluetooth binary/source was accessed. There was no
browser/web use. Missing hardware facts are recorded in SPEC-GAPS-S2.1.md and
SPEC-GAPS-S3.md.

## Local validation evidence

- `out/ble-s21-s1-tests.log`, `out/ble-s21-tests.log`: native regression,
  including actual NimBLE GATT/MIDI, reconnects, updates and virtual ten minutes.
- `out/ble-s3-tests.log`: AES/CCM/seed/bond/epoch tests and actual NimBLE SC
  and legacy SMP, persistent-store reconnect and encrypted MIDI.
- `out/ble-s21-final-build.log`, `out/ble-s3-build.log`, corresponding BUILD
  reports and SHA files, plus DSP target-budget logs: target sizes and checks.
- `out/ble-stock-build.log`: stock app hash
  bbe649ac40d6573a548c52833bbd54cbe74957703732104e70b942853c8fa3ee,
  package hash afa34f78b27e5568a5696407560d66eb038114b9e2d40f0bf68a0da26ebd9e5a,
  identical to baseline. `build/felucca-stock.*` preserves the checked output.
- Connect-retry behavior was tested with fake async calls only; Python syntax
  checks/import did not invoke sockets, D-Bus, ALSA or device I/O.

The final stage-2 rebuild is e555e67b93de406f0aef538b3bfa7151459ada14653049610dc43ff62b01d7ad.
The first image handed to the parent (80c720dc...) is preserved as
out/felucca-ble-s2.1-first.fwsc with its original FIRST-BUILD report. The final
source rebuild changes application size by 80 bytes, with identical RAM/pool
allocations and S2.1 behavior. Only the final named image is the delivery SHA.
