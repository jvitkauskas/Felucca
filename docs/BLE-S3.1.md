<!-- SPDX-License-Identifier: GPL-3.0-only -->
# BLE S3.1 — frozen r5

Implemented on `ble-controller` above 85cd513 (S2.1/S3 code 6fb69c7 and the
owner's bench records). No commits, pushes, device/USB/desktop access or flash
operations were performed. This report describes software verification; the
S3.1 image has not run on the FM-1. Prior hardware results remain the owner's
three supplied bench records, not results for this image.

## Build and artifacts

```
FELUCCA_BLE=1 FELUCCA_BLE_STAGE=3 FELUCCA_BLE_S31=1 \
JIELI_TOOLCHAIN=/root/felucca-ble-env/jieli/toolchain \
AC79_SDK=/root/felucca-ble-env/sdk-min ./build.sh
python3 tools/ble_report.py s3.1
```

Use `out/felucca-ble-s3.1.fwsc`, `out/felucca-ble-s3.1.sha256`,
`out/ble-s3.1-build.log` and `out/BLE-S3.1-BUILD.txt`. The S3.1 switch defaults
to zero. Stage 2 and stage 3 builds without it retain their original command
sets and pairing profiles; the measurement and bond-store corrections also
apply to those configurations. S1's original native driver tests remain usable.
Target debug symbols/disassembly are preserved as `build/felucca-ble-s3.1.*`.
The report contains image/RAM/pool sizes, stack reservations and the exact hash.

## Module map against r5

- **03 §§1.3, 3.6 / D-6:** `ble_privacy.c/.h` holds four ordered entries with
  identity, peer/local IRKs, network/device mode, enable state and counters.
  `ble_controller.c` implements fixed command lengths, six CC handlers, bitmap
  octets 34=78, 35=02, 39=04, errors and Reset. Unsupported 202B/C/E stay unknown;
  LL features remain LE Encryption only. List mutations and busy-state checks
  share exclusion with CONNECT_IND acceptance.
- **03 §6.1 / 02 connection path:** `ble_ll_create` rejects a known identity
  in network mode while resolution is enabled; HAL performs that check before
  changing connection registers. Address comparisons run in the acceptance
  interrupt; one peer-IRK `ah` operation per executor delivery item resolves the
  RPA before the creation event. The original OTA address remains in the LL
  connection. Unmatched/disabled/failing AES resolution reports the received
  address. There is no host LTK lookup shim.
- **03 §4 and Core §7.7.65:** legacy Connection Complete carries identity with
  type 00/01, using r5's permitted Core fallback. When bit 9 is unmasked, Enhanced
  replaces it, uses 02/03 for a resolved identity, carries the peer RPA and zero
  local RPA. The general LE Meta event mask still applies. This public-address
  peripheral implements peer privacy only; full controller privacy is D-6.
- **04 §8.2 / K-11:** `ble_crypto.c` adds `ble_ah`, explicitly HCI/air LSO-first.
  Startup crypto selftest includes the sample hash; failure takes the existing
  seed FAILED path. Native tests additionally check the complete AES result
  against the Core sample and inject AES failure. The physical AES port still
  needs the owner's startup observation.
- **03 §1.3 / S-13:** `ble_host.c` derives a non-zero boot-stable local IRK from
  SHA-256(domain || public address). It has no secrecy claim under the mandated
  public local address. The startup callback resets pinned NimBLE's privacy
  cache so that HCI Reset restores enable/list state, as well as cold startup.
  Before requesting security, the application checks the live connection's
  encryption state, the one-link host SM procedure count and the LL phase.
  EALREADY increments a benign counter and never updates the host error or pair
  failure count. In-progress/encrypted skips have a separate counter.
- **04 §7:** `ble_bonds.c` retains the compatible CRC journal (30 × 128-byte
  slots at 0x96000; last 256 bytes untouched). It accepts canonical identities,
  normalizes identity tags, refuses private random keys and counts old private
  records dropped on load. Flash writes compare final state to verified durable
  state, including deletion/recreation; equal values do not append. Compaction
  drops orphan CCCDs, preserves current security/CCCD records and resets the
  baseline only after successful erase/program/readback. Flash remains deferred
  until radio idle with no link. Two-sector seed persistence is unchanged.
- **04 §0.8 / 02 §§4.3, 7.3a / F-27:** `ble_timing.c/.h` owns the shared nesting
  depth, preemption accumulator, own clock, pass/item budgets and refill split.
  `fm1_isr.S` brackets audio, TIMER5 and both BLE handlers; nested preemptions are
  counted once. Fatal exception handlers never resume a pass. The executor
  captures the first pending stamp and entry inside one IRQ-disabled section,
  clears it, uses signed difference and rejects/counts negative samples. Own
  time gates work; individual service, security, RX/control, housekeeping,
  delivery, driver and HCI command items report their maximum. Refill records
  total, summed actual CCM calls and remainder; the threshold is 80 us.
- **Diagnostics:** `ble_console.c` reports list enable/capacity/entries, matches,
  unresolved/failing resolution and network refusals; net/store migration/drop
  counters; per-link identity, known-record flag, method and SC flag. Method/SC
  are looked up afresh in that identity's own-security record. Missing records
  report unknown (0), even if another peer's latest write was SC. No keys are
  printed; the new 2027 trace is redacted too.

## Verification

All native suites use ASan/UBSan, compiler warnings as errors, and no device or
network I/O. Commands and retained logs:

- `tests/run_ble_s1_tests.sh` → `out/ble-s3.1-s1-tests.log`: original S1 suite,
  default and measured-power profiles, PASS.
- `python3 tests/run_ble_s2_tests.py` → `out/ble-s3.1-s2-tests.log`: full S2 logic
  and actual NimBLE GATT/MIDI/update/credits/reconnect/virtual-ten-minute suite,
  PASS.
- `python3 tests/run_ble_s3_tests.py` → `out/ble-s3.1-s3-tests.log`: original S3
  crypto/seed/bond/phase suites, S2 host regression, actual legacy and SC pairing,
  encrypted MIDI, persistent reconnect and MIC close, PASS.
- `python3 tests/run_ble_s3_tests.py --s31` → `out/ble-s3.1-tests.log`: all above
  with privacy enabled, plus `ble_s31_test.c` and actual host legacy/RPA+Reset
  integration, PASS. H-22..H-25 exercise command framing/bitmap, errors,
  duplicates (including non-zero IRK), capacity, zero IRKs, Reset, state guards,
  public/random identities, matched/unmatched/off/AES-failed resolution,
  network/device modes and conditional Enhanced event. A four-nonzero-IRK list
  resolves the last entry over four work items. An injected CONNECT_IND immediately
  before the poll lock proves resolution precedes event formatting across that
  acceptance race. F-24 simulation checks decoded
  CONNECT_IND with the Core sample RPA, then actual persisted-IRK/host-LTK
  reconnect following HCI Reset. S-13 tests both an active pairing and an
  already encrypted reconnect; another peer's SC record cannot change the
  current legacy diagnostic.
- F-25 simulation tests two peers, 80 unchanged delete/recreate cycles with no
  writes, absent/present/absent cancellation, real changes forcing compaction,
  reload of both security directions, old RPA migration/refusal, orphan removal,
  untouched sector tail and failed-readback retry.
- F-27 arithmetic tests signed wrap/error handling, separate live/all-state
  dispatch maxima, nested handler time counted once, own-time budget and item
  overruns, and CCM/remainder attribution. These are timing-logic tests, not CPU
  timing measurements. The EALREADY race branch is reviewed, not forced by a
  concurrent host (the native host is single threaded).
- Target build checks validate linker capacities, DMA placement, call-free RAM
  flash routines and MMIO confined to HAL. `tests/target_budget.py` passes for
  the S3.1 target DSP code; this static check is not an IRQ timing proof.

## Metrics and limits for the owner

Use `executor_dispatch_live_max_us` for G <=7500 us. The all-state
`executor_dispatch_max_us` and worker timer gap can include idle flash writes.
Require zero `executor_dispatch_negative`. `executor_wall_max_us` is
informative; the requirements are `executor_item_max_us` <=100, no item overrun,
no `executor_over_250us_plus_item`, and own pass <=250 us plus the observed
longest item. `executor_own_over_250us` alone is informative. Separate maxima
may come from different refills: do not add `refill_ccm_max_us` and
`refill_other_max_us` and assume they describe the maximum-total refill.
Require total <=80 us and no `refill_over_80us`.

IRQ entry/exit instrumentation has finite overhead; target own-time/item and
refill maxima, dispatch under audio/USB load, cumulative masking, stack fill,
ECC preemption and RF receive timing are unmeasured. An item overrun is counted;
there is no claim that native virtual time proves W_item <=100 us on silicon.
No temperature/range/power qualification is added.

The bond journal still has one-sector compaction: power loss during erase or
rewrite can lose bonds, and power loss before deferred flush loses net changes.
Seed records use their separate sectors and contract. FIRST_BOOT/CHAINED/
UNPERSISTED randomness claims and D-1 MIC-tail limitations remain as documented
in BLE-S3.md. Legacy identity reporting loses the OTA RPA in the host's view;
repeat pairing that needs OTA address provenance should be captured on the
bench. No full Core conformance is claimed.

## Owner bench checklist

1. **F-24:** power the desktop adapter off for the phone run. Retain the phone
   bond, boot the S3.1 image and confirm KAT/seed readiness, local+phone list
   entries and resolution enabled. Use a changed phone RPA (Bluetooth toggle or
   >16 minutes). Require 2027/204E status 0, identity in Connection Complete,
   201A rather than 201B, encryption and MIDI both ways without a new consent.
   Repeat ten cycles, an FM-1 power cycle, and `ble host` for HCI Reset/resync.
   Rerun the BlueZ encrypted reconnect and ten-minute regression.
2. **F-25:** with two retained peers, alternate 40 subscribe/unsubscribe sessions;
   unchanged net state must not append. Use existing journal history or genuine
   persistent CCCD changes to reach compaction. Wait for bond_pending=0 before
   power cycling. Confirm both bonds survive; inspect the sector for canonical
   live keys and no surviving orphan CCCD after compaction. Historical obsolete
   slots remain until erase; private_dropped can be nonzero when loading S3.
3. **F-26 / B23/B24:** clean bonds on both sides; consent delays 1/5/15/25 s.
   Capture SMP AuthReq, compare the phone identity's method/SC diagnostic and
   record. On timeout, freeze the trace before reboot. This implementation does
   not claim to resolve the Android legacy choice or slow-consent stall.
4. **F-27:** ten minutes encrypted at 15 ms and 7.5 ms with audio/USB/UI load;
   record the metrics and thresholds above, audio late/shed counters, IRQ maxima
   and separate idle-flash gaps. Qualify cumulative masking and nested stack
   margin independently. Exercise the full burst cases of F-16.
5. **A9 / H-25 / K-11:** capture phone-initiated disconnect with a sniffer to
   establish whether LL_TERMINATE_IND reaches the FM-1; do not equate the
   phone's local reason 16 with the FM-1's expected peer reason. A scriptable
   central should confirm network/device mode acceptance and conditional
   Enhanced events on air. Observe startup K-11 success on the physical AES
   block. Existing coalesced encryption boundaries and D-1 RF-tail probes remain
   pending as in BLE-S3.md.
