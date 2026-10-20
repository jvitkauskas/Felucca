<!-- SPDX-License-Identifier: GPL-3.0-only -->
# BLE S3.1b — round 2, frozen r6

Changes are uncommitted on `ble-controller` above ee63586 (implementation
round 1: 94eb8a8). The owner’s `out/BLE-S3.1-BENCH-R1.txt` was read first.
It establishes the old image’s F-24 success, first compaction survival and
164/226-us item failures; these are not measurements of S3.1b. No device,
flash, commit or push operation was performed in round 2.

## Result and artifacts

Build with the existing pinned execution/packaging toolchain:

```
FELUCCA_BLE=1 FELUCCA_BLE_STAGE=3 FELUCCA_BLE_S31=1 \
JIELI_TOOLCHAIN=/root/felucca-ble-env/jieli/toolchain \
AC79_SDK=/root/felucca-ble-env/sdk-min ./build.sh
python3 tools/ble_report.py s3.1b
```

- `out/felucca-ble-s3.1b.fwsc` and `out/felucca-ble-s3.1b.sha256`.
- `out/ble-s3.1b-build.log`, `out/BLE-S3.1B-BUILD.txt` and
  `out/ble-s3.1b-target-budget.log`; ELF/disassembly/bin retained in
  `build/felucca-ble-s3.1b.*`.
- `out/BLE-S3.1b-INPUT-SHA256SUMS`,
  `out/BLE-S3.1b-TOOLING-PROVENANCE-SHA256SUMS`,
  `out/BLE-S3.1b-CHANGED-FILES.txt`, `out/ble-s3.1b-audit.log`.
- Historical stage-2 and stage-3 target option checks:
  `out/ble-s3.1b-s2-build.log`, `out/ble-s3.1b-s3-build.log`.
  S1/default and measured-power native configurations remain supported.
  S2.1 shares the stage-2 target option; S3.1 remains opt-in.

Final package SHA256:
`414a2a86e6afeae68021b43c21bbbb9acfed4cef448ee92868ff80e06afdc4e5`.
Application 525900 / 581564 B; static RAM 96888 / 98304 B (1416 B free);
pool including DMA 331060 / 344064 B (13004 B free). Stack reservations remain
24320 B user / 7936 B supervisor; physical high water is unmeasured.

## r6 changes and choices

`docs/SPEC-GAPS-S3.1.md`, **Round 2 / r6**, records the Core/NimBLE review,
withdrawn legacy fallback, integration choices and remaining objections.
No specification or vendored NimBLE/TinyCrypt source was changed.

- `ble_link.c` retains the on-air peer address in legacy Connection Complete,
  including a resolved RPA with type 01. `ble_controller.c` converts it to
  Enhanced v1 only when LE mask bit 9 is set: resolved identity 02/03, peer RPA,
  local RPA zero. Unresolved Enhanced events retain the received address and
  have both RPA fields zero. There is exactly one connection event; the outer
  LE Meta mask still applies, including when bit 9 is set and bit 0 is clear.
- `ble_host.c` sends **LE Set Event Mask 0x21F through NimBLE’s synchronous
  command path after every host sync**, before advertising. Failure requests
  host reset without advertising. The existing stable local IRK and startup
  privacy-cache reset still restore resolution enable/local and stored peer
  entries after HCI Reset and console `ble host`. HCI/LL versions remain 4.0;
  no speculative B23 version change is included.
- `ble_host_delete_bond` removes the resolving-list entry through the pinned
  host privacy path (2028), tolerates an absent entry, then deletes store
  records. It never calls the terminating `ble_gap_unpair`. Repeat pairing,
  application unpair and overflow replacement share this helper; failed
  deletion does not return RETRY. An application store-write adapter also
  removes the old bond before persisting a replacement OUR_SEC key. This
  handles a changed peer IRK/new unresolved RPA, for which NimBLE cannot
  recognize repeated pairing until identity distribution. Pinned NimBLE
  persists OUR_SEC before PEER_SEC; the adapter preserves both new directions
  and makes the subsequent 2027 succeed. CCCD-only deletes remain ordinary
  store operations. No journal format or persistent-seed change is required.
- `ble_timing.c/.h` adds a global maximum’s kind/opcode plus per-kind raw-tick
  maxima, sample counts, overruns and opcode at maximum. The console’s
  `executor_kind` lines label every kind. Resolution and event delivery are
  separate items, including acceptance immediately before the poll lock.
  Failed work remains counted. RX/control, key service, entropy sampling,
  command, stop, advertising preparation, service/driver and housekeeping
  are distinguished; nested-preemption and own-time accounting are retained.
- Executor random generation is resumable with **one SHA-256 compression per
  step**. Reseed, output and rekey have separate kinds. The original framing,
  generator counter and rekey construction are preserved. HCI LE Rand waits
  for its job; LL_ENC_REQ requests SKDs+IVs together as 12 bytes. Owners cannot
  steal one another’s output; a cancelled link job finishes rekey and discards
  the output. Close/Reset cancels stale link work. Entropy-sample hashing and
  AES key service are also separate items.
- Advertising startup keeps the DMA instance closed across **24 calls**:
  15 zeroing slices of at most 256 bytes, one software-frame build, six frame
  publication slices of at most 64 bytes at the original access widths, one
  payload/remap step and one final hardware publication. HCI enable completes
  only after publication; subsequent commands, including Reset, remain queued.
  Restart service uses the same steps. No radio is published partially ready.

## Verification

All four complete native suites passed with ASan/UBSan and warnings as errors:

```
tests/run_ble_s1_tests.sh                     # out/ble-s3.1b-s1-tests.log
python3 tests/run_ble_s2_tests.py             # out/ble-s3.1b-s2-tests.log
python3 tests/run_ble_s3_tests.py             # out/ble-s3.1b-s3-tests.log
python3 tests/run_ble_s3_tests.py --s31       # out/ble-s3.1b-tests.log
```

The inherited test build disables only UBSan shift-base checking for the
unmodified TinyCrypt AES source; other ASan/UBSan checks remain enabled.
Tests use deterministic simulated NOR/AES/central, without device/network I/O.

- **F-28 real pinned NimBLE SMP, legacy and SC:** first create and reconnect a
  bond, then peer forgets only its LTK while DUT retains the listed IRK. Repeat
  pairing verifies c1 or f5/f6 against the actual OTA address; 2028 precedes
  2027, both return success, and no 0406 occurs. Repeat with a changed IRK over
  the old RPA, and a changed IRK/new RPA unresolved at connection creation.
  Each of the three pairings has three fresh-RPA encrypted reconnects (nine
  per method). Verify the mask and restored list after console resume and
  three normal host HCI-error resets. A real capacity overflow exercises the
  installed replacement callback; explicit unpair and an absent-entry repeat
  preserve encrypted ATT communication on the live link.
- **Corrected H-25:** resolved public/random identities in Enhanced, legacy
  OTA RPA, off/unmatched/AES-failed resolution, zero RPA fields when unresolved,
  device/network modes, Enhanced selected with legacy masked, outer mask
  suppression, one-entry resolution steps through a full list, acceptance race.
- **F-27 attribution:** signed wrap/negative dispatch, nested preemptions,
  own/pass/item thresholds, changing maximum kinds/opcodes, independent
  per-kind counters and reset, and the original refill CCM/remainder checks.
  Link-time wrappers count actual TinyCrypt compressions, verify synchronous
  and async random bytes match, and require exactly one compression per step.
  The deliberately injected **60 us/compression model** records per-kind
  maxima of 60 us, normal generation in three steps and reseeded generation
  in five; this is a scheduling test, not measured target performance.
- Original crypto/seed/failure/phase/barrier/MIC suites, F-25 migration/churn/
  compaction/reload/no-op persistence, S-13 security request suppression,
  actual GATT/MIDI/ACL credits, ten-minute virtual link and reconnect suites pass.
- S1 also checks deferred advertising publication, Reset FIFO and preparation
  failure. HAL slice sizes/access widths were reviewed; native hardware mocks
  do not execute the real MMIO implementation. Target linker/DMA/RAM-text/MMIO
  guards and DSP budget checks pass; stage 2 and stage 3 options build.

## W_item findings and remaining qualification

**The old 164-us boot and 226-us session samples cannot be identified exactly
from the retained record:** it contains no kind, opcode or individual trace
correlation. Assigning either to a particular call would invent evidence.
The call graph does narrow them:

- K-11 and the AES/SHA startup KATs run in `ble_seed_boot`, before the executor.
  Bond load/flush/preparation and ECC run in main. They therefore contributed
  **zero to the old executor W_item counter**, even if they cost main time.
- Delivery to the host copies at most 70 bytes into the application ring;
  NimBLE allocation/SMP/ECC run later in main. Its new `delivery` maximum can
  qualify the actual copy cost; it is not an ECC item.
- Old **2018 LE Rand** did three SHA compressions in a normal command, or five
  with reseeding. Old **LL_ENC_REQ** generated 8 and 4 bytes in separate calls:
  eight compressions after connection reseed, six without. These are concrete
  multi-compression executor paths. At the explicitly hypothetical 60-us
  model, that is 180/300 us for Rand and 480/360 us for ENC_REQ, before overhead;
  new items execute one compression each (60 us in that model). The model is
  deliberately a test input, not a calibration to either historical maximum.
- Old **200A advertising enable** combined the 3690-byte clear, complete frame
  build/stores, payload and publication. It now yields across the 24 slices
  above. Target elapsed cost per slice is unmeasured. A single AES transaction
  retains its 26-us HAL deadline; previously observed refill maximum was 59 us
  on the old image. Neither proves the new item/refill maxima on silicon.

Require a fresh boot diagnostic before pairing, a capture after pairing and
F-27 load captures with `executor_item_max_kind`, `executor_item_max_opcode`
and every `executor_kind` row. Kind raw ticks / 24 are microseconds. Global and
per-kind maxima keep the first sample on ties. Counters reset with HCI Reset;
freeze/capture them before issuing `ble host`. Do not sum unrelated per-kind
maxima to infer pass time. Requirements remain item <=100 us, own pass <=250 us
plus its longest item, zero over-budget passes, dispatch <=7500 us/no negatives,
and refill <=80 us. Physical timing and 25% nested-stack margin remain open.

D-1 MIC-tail and D-6 peer-only privacy deviations, single-sector bond journal
power-loss risk, unqualified entropy credit, A9 disconnect provenance and B24
slow-consent behavior remain. B23 is only the r6 LL-4.0 hypothesis. This image
has not been flashed or bench-qualified.

## Owner bench checklist

1. **F-28:** retain DUT bond/list, forget on phone only, pair again; require
   Enhanced identity + correct RPA, successful 2028 before 2027, no Confirm
   Value Failed/DHKey Check Failed, then three fresh-RPA encrypted reconnects.
   Repeat with changed IRK/new RPA where possible; check one entry per peer
   plus local. Repeat the desktop SC path. Test app unpair/replacement without
   a link termination caused by the deletion helper.
2. **F-24 recheck:** changed RPA after HCI Reset/`ble host`, four further cycles
   and DUT power cycle, no prompt or negative LTK reply, encrypted MIDI both
   ways. Confirm sync’s 2001 payload `1f 02 00 00 00 00 00 00` and list restore.
   Retain both phone and desktop bonds and recheck after compaction (F-25).
3. **F-27:** capture kind/opcode maxima immediately at boot, after each legacy/
   SC pairing and each restart; then ten minutes each at encrypted 15 ms and
   7.5 ms with audio/USB/UI load. Require all thresholds above; save per-kind
   counts/overruns, refill split, audio late/shed and stack-fill evidence.
   If any kind exceeds 100 us, record its opcode and split that actual path
   further before claiming the bound. Keep idle-flash timer gaps separate.
4. **B23 phone HCI snoop:** full snoop mode, both sides forgotten, fresh pairing;
   compare SMP Request/Response AuthReq SC bits with this peer’s stored method.
   A separately authorized test-only LL-4.2 build can test the hypothesis;
   S3.1b retains LL/HCI 4.0. A release version change needs its own conformance
   review. Recheck F-26 at consent delays 1/5/15/25 s and freeze on B24 timeout.
5. Scriptable-central H-22..H-25, A9 sniffer/termination provenance, D-1 RF-tail,
   cumulative interrupt masking and range/temperature qualification remain
   the existing hardware checklist; native tests do not replace them.
