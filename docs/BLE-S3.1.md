<!-- SPDX-License-Identifier: GPL-3.0-only -->
# BLE S3.1d — round 4, frozen r7

Uncommitted on `ble-controller` above **0fb0475**. S3.1d fixes long-session
connection-event processing and missing-instant handling, fully clears link
runtime at close/new CONNECT_IND, and adds pending/ignored/update diagnostics.
No device, flashing, commit or push operation occurred. Hardware qualification
of S3.1d remains outstanding.

## Root cause and scope

The original HAL and LL kept testing the original first-window timestamp for
an entire link. The signed 24-MHz comparison flips at **89.478485 s**, suppressing
event-close work during alternate halves of the tick wrap. The stuck dump's
29825/17896 event counters closely match the fifth boundary/three serviced
halves. A baseline native reproduction fails; the fixed 600-second grid serves
every close. The original exact-I-1-only instant action could then remain
pending forever without counting late, blocking later instant procedures.

Core Vol 6 Part B §5.1.1 confirms that **new-interval** WinSize/WinOffset
validation was correct and remains unchanged. Which historical LL PDU was
accepted/ignored cannot be proved from HCI-only btsnoop. The original close
already cleared instant_kind; the next-link 1.89-ms physical event-counter
rate is not proved to be caused by that flag. Timer-period 6000 in the stuck
dump is the normal 1-ms backstop, so cumulative worker calls do not establish
an immediate-work busy loop. Full proved/hypothesis analysis and decoded HCI
times: [round-4 report](../out/BLE-S3.1d-CONNUPDATE-ANALYSIS.md).

The fix retires the first-anchor gate after the first real close; detects
skipped/expired unprogrammed instants with reason 28; keeps far instants in
event units; holds latency off; protects a future window from old-event RX;
and clears the complete procedure record on completion/close. New creation
clears all connection-local runtime and deferred indexes, plus HAL delayed-slot
flags. Stop/settle/quiesce/reclaim/report and epoch ordering remain in place.
At I-1 the hardware programming order and r7 values are preserved. Software
parameters change at the instant; supervision retains its ordered instant/
window reset events. Physical A_INSTANT/A_INTERVAL/A_WIN_OFFSET latching and
RF window edges are still A1/A7 bench questions.

## Diagnostics and native verification

`ble` now prints pending_proc (kind 0 none/1 update/2 map, instant, received
event, programmed, params/map), control_last_ignored (opcode/reason/receipt
event), per-reason counters and four conn_update attempt records. History
survives ordinary reconnects; HCI Reset clears it. Sort ring records by serial;
epoch distinguishes links. Update outcomes: 0 empty, 1 pending, 2 programmed,
3 applied, 4 invalid ignored, 5 busy ignored, 6 instant passed, 7 closed.
Ignore reason IDs: 1 length, 2 params, 3 map, 4 pending, 5 role, 6 unexpected,
7 repeated version, 8 reserved/empty (opcode 255 unavailable), 9 encrypted.

Full ASan/UBSan suites (warnings as errors):

```sh
bash tests/run_ble_s1_tests.sh
python3 tests/run_ble_s2_tests.py
python3 tests/run_ble_s3_tests.py
python3 tests/run_ble_s3_tests.py --s31
```

Logs: out/ble-s3.1d-{s1,s2,s3}-tests.log and out/ble-s3.1d-tests.log.
The inherited TinyCrypt AES shift-base exception remains; upstream is unchanged.
`ble_conn_update_test.c` uses the existing virtual-radio ports: 48 window/
interval/near/far cases plus 48 immediate following updates, 12 encrypted
central cases, invalid limits, late/skipped/missing-close handling, I-1 receipt,
seven closing causes and fresh-link timing, and 600 seconds across tick wraps.
Existing event-counter-wrap maps, actual NimBLE pairing/repeat/RPA, encryption,
Reset/credit/ordering and ten-minute suites also pass. Baseline reproduction:
out/ble-s3.1d-baseline-repro.log. Two test helpers now allow 64 Reset passes;
production clear slices remain 128 bytes with the original timing thresholds.

Models observe port writes, software anchors/parameters and events. They cannot
prove silicon latch behavior, physical RF timing, actual 1.89-ms-rate recovery,
or IRQ/executor timing. Per-kind F-27, A1/A7/B22 and prior unresolved hardware
qualification continue to apply.

## Build and audit

```sh
FELUCCA_BLE=1 FELUCCA_BLE_STAGE=3 FELUCCA_BLE_S31=1 \
JIELI_TOOLCHAIN=/root/felucca-ble-env/jieli/toolchain \
AC79_SDK=/root/felucca-ble-env/sdk-min ./build.sh
python3 tools/ble_report.py s3.1d
python3 tests/target_budget.py
python3 tools/ble_s31_audit.py
sha256sum -c out/felucca-ble-s3.1d.sha256
```

Image: out/felucca-ble-s3.1d.fwsc (609849 B).
SHA256: `50b9b698f1ec94af1ff867e18e7c643aabdb51aa145e08fc6586e7711e238a16`.
App 528844 / 581564 B; static RAM 97256 / 98304 B (1048 B free);
pool including DMA 331268 / 344064 B (12796 B free). DMA 4340 B at 01C6FD10;
stacks 24320 / 7936 B, physical high water unmeasured.
Build/layout: out/ble-s3.1d-build.log and out/BLE-S3.1D-BUILD.txt.
Stage-2/stage-3 option checks use out/ble-s3.1d-{s2,s3}-build.log; target DSP
comparison: out/ble-s3.1d-target-budget.log. Input/provenance/changed-file/
artifact manifests use out/BLE-S3.1d-; audit log out/ble-s3.1d-audit.log.
Historical artifacts and manifests are retained. Audit details:
[BLE-S3.1-AUDIT.md](BLE-S3.1-AUDIT.md). r7 ambiguities are appended to
[SPEC-GAPS-S3.1.md](SPEC-GAPS-S3.1.md); the spec itself is untouched.

## Owner desktop bench — not run by the implementer

Prepare the S3.1d image using the owner's normal flash workflow, retain a
BlueZ bond and verify encrypted MIDI. Close other serial clients and stop
other scripts that issue connection updates. On the Linux desktop run:

```sh
sudo python3 tools/ble_conn_update_bench.py \
  --address FD:60:52:C0:DF:6C --console /dev/ttyACM0 --index 0 \
  --output out/s31d-owner-01
```

Replace the address/console/index with the owner's values. Requires Python 3,
btmon, bluetoothctl, raw-HCI privileges and an existing bond; the script does
not pair/remove bonds or flash. Its output directory must be new. It captures
btmon text and btsnoop, starts a fresh link, normalizes to 15 ms if needed,
and measures 15 ms before and after a default **95-second soak**. It then sends
one raw HCI command at a time: **15->7.5, 7.5->15, 15->30, 30->15 ms**. Each
must get desktop Complete 00, exactly one FM-1 conn_updates increment, no
pending procedure, and an independent 10-second wall-clock measurement using
both console event counters. It then disconnects/reconnects, requires a new
epoch, checks a fresh 15-ms measurement and disconnects. Keep console-*.txt,
capture.btsnoop, btmon.txt, measurements.json and terminal output. Failure
keeps partial evidence; inspect it before restarting. Help/parser/HCI/counter
fixtures (`python3 tests/ble_conn_update_bench_test.py`) were checked offline; the script has not been exercised on a device.

Require measured intervals close to 7.5/15/30 ms, both event deltas agreeing,
zero new supervision/late failures and no growing pending state. The script
uses an 8% console-timing allowance, not an RF accuracy claim. Repeat a
10-minute encrypted loaded run at 7.5 and 30 ms, retaining F-27 and audio
counters. Where available, capture LL with a common-clock sniffer: actual
WinSize/WinOffset/Instant and the transition window cannot be inferred from
desktop HCI or this native model.

<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Archived S3.1c — round 3, frozen r7

Uncommitted on `ble-controller` above c5973a1 (round-2 code b242adc).
`out/BLE-S3.1b-BENCH-R1.txt` records the supplied baseline: F-24/F-28 and
ten-minute encrypted link pass; Reset 2589 ticks (107.875 us), control 2525
ticks (105.208 us). Those are S3.1b measurements. S3.1c is not bench-qualified.
No device, flashing, commit or push operation was performed.

## Round 3 result and artifacts

Use the build command below with `python3 tools/ble_report.py s3.1c`.
Run `python3 tests/target_budget.py` and `python3 tools/ble_s31_audit.py`.
Artifacts are `out/felucca-ble-s3.1c.fwsc`, its `.sha256`,
`out/BLE-S3.1C-BUILD.txt`, `out/ble-s3.1c-build.log`,
`out/ble-s3.1c-target-budget.log`, and `build/felucca-ble-s3.1c.*`.
Stage-2/stage-3 option logs use `out/ble-s3.1c-{s2,s3}-build.log`.
Input/provenance/changed-file/artifact manifests use `out/BLE-S3.1c-`;
the audit log is `out/ble-s3.1c-audit.log`. Historical manifests are retained.

Package SHA256:
`c287bfe52f2ba203ddc1cc89cebc30777ffd5e9d860d61dcd77c84c2f2d2d800`.
Application 527036 / 581564 B; static RAM 97064 / 98304 B (1240 B free);
pool including DMA 331108 / 344064 B (12956 B free). DMA is 4340 B at
0x01C6FC70; stack reservations remain 24320 B user / 7936 B supervisor.
Physical stack high water is unmeasured.

## Round 3 Reset implementation

Reset now begins by inhibiting acceptance, LL worker/delivery and TX picking,
then requesting hardware stop. No live DMA buffer is reclaimed. Further
executor calls perform these items, and each checks the pass budget:

1. `reset_stats`: platform/timing and entropy counters. Clear timing first so
   later Reset slices remain visible in F-27.
2. `stop`: one poll of the existing stop state machine, retaining its deadline,
   instance masking/ACK and DMA ownership rules. The inherited latched stop
   failure keeps quarantine and returns Hardware Failure.
3. `reset_clear`: at most **128 bytes** of queues or link state per item; then
   a separate security-clear/cancel item. Completed random output is erased;
   an unfinished job finishes rekey and discards its output after Reset.
4. `reset_defaults`: masks, advertising, random address, empty/disabled
   resolving list and controller counters. Public identity and TX power persist.
5. `reset_complete`: successful 0C03 Command Complete, only after all work.

The Reset guard is outside the objects being cleared. All queue/link clears
follow hardware stop; the final clear installs the new epoch. There is no old
connection/LTK/encryption/NCP/disconnection/timer event after completion.
DRBG state/counter, readiness and seed records survive; no flash access occurs.
Started commands finish first, cancelled queued work still receives exactly
one reply, and advertising preparation completes before a following Reset.
Reset never waits on main-loop or host work.

The linked target has **2584 bytes of queues** (21 slices), **340 bytes of
link state** (3 slices), **120 bytes of security state**, and **248 bytes of
random-job state**. The final security item clears at most the last two fixed
objects (368 bytes); unfinished random work is cancelled, not hashed here.
These clears contain no AES/SHA and no variable queue drain. The injected
**3 ticks per copied/cleared byte** model gives queue slices **384 ticks / 16
us**, defaults **1005 / 41.875 us**, stats **1056 / 44 us**, completion **132 /
5.5 us**. Cancelling output ready but unconsumed gives the largest secure clear,
**1152 ticks / 48 us**. All steps are <=70 us in that model. The old 108-us
aggregate provides no individual target slice measurement. Small memory
slices should have margin; unchanged stop work previously ran in housekeeping
(61.04-us maximum). Target confirmation is still required.

## Round 3 control attribution and split

**The historical control maximum's opcode is unknowable from the retained
record.** Session-key AES and random SHA already ran in `key` and entropy
items, outside `control`. The concrete expensive candidate is the encrypted
response/refill path, including **0x06 LL_START_ENC_RSP**: input CCM (5 AES
blocks), output response CCM (5), then a waiting 27-byte ACL CCM (7), all in
one old item: **three CCM calls / 17 AES blocks**. Encrypted pause request
(0x0A), feature/version and other replying controls could combine CCM too.

The common path now has a `control_rx` authentication/staging item, a
`control` dispatch item, and separate `refill` items for **one TX slot/CCM
each**. RX interrupts and ordinary refill callers retain two-slot behavior.
The limited HAL refill still checks both slots for completion and retains
boundary-buffer exclusion, the original snapshot rules and DMA barriers.
`control_rx` and `control` record decoded LL opcodes in the global/per-kind
maximum fields; refills record their causing opcode. Markers use 0100 + ID
(0101 RX_ON, 0102 RX_OFF, 0103 REJECT_ACK); FFFF means unavailable, including
failed authentication. Ciphertext is never reported as an authenticated
opcode. Existing first-maximum tie behavior and console reporting apply.

No later PDU/marker passes staged control. ENC_RSP queuing precedes LTK Request;
LTK Command Complete precedes key/reject work. The original critical section
still covers TX enable plus the first encrypted response's queue insertion:
the response takes TX counter zero before ACL. Start/pause/reject barriers,
stream-position RX_ON/OFF changes and refresh event selection are retained.
Every pending stage explicitly repends the executor, including between slot
refills. The loop allows four steps per old PDU to retain drain capacity, with
the existing 100-us LL-loop and 250-us pass checks between items. A yield waits
for executor dispatch G, never main or the next connection event. Instant,
supervision, terminate and central response deadlines are unchanged; no new
peripheral LTK timer is armed. There is no unrequested host-dependent wait.

Qualification estimates: authentication is one CCM (old RX maximum 52.92 us),
dispatch has no AES/SHA, and one-slot refill is one CCM plus bookkeeping. The
26-us CCM deadline plus the old 38-us refill remainder suggests **about 64 us**
for refill. The **8 us/AES block + 1 tick/controller memory byte** model
observes authentication **1014 ticks / 42.25 us**, dispatch **1 tick**, refill
**1444 / 60.167 us**, verifying block counts **5 / 0 / 5 / 7** across four
items. TinyCrypt emulates the peripheral; its internal software copies are
excluded from controller-memory charges. Models and estimates are not proof
of the physical W_item bound.

## Round 3 validation and owner bench checklist

Complete ASan/UBSan suites (warnings as errors):
`tests/run_ble_s1_tests.sh`, `python3 tests/run_ble_s2_tests.py`,
`python3 tests/run_ble_s3_tests.py`, and the latter with `--s31`.
Logs are `out/ble-s3.1c-{s1,s2,s3}-tests.log` and
`out/ble-s3.1c-tests.log`. The inherited TinyCrypt AES shift-base-only exception
remains. Upstream sources are unchanged; target guards and DSP comparison run.

`tests/ble_s31_steps.c` forces pass yields between authentication, dispatch
and each refill. It checks AES counts, opcodes, <=70-us modeled items, the
counter-zero response before ACL, pause marker order and unavailable opcode
on MIC failure. Reset is tested at eleven encryption stages: queued/decoded
ENC_REQ, entropy request/reseed, LTK pending/delivered, reply pending, loaded
START_ENC_REQ, pending RX_ON, staged START_ENC_RSP and response/refill pending;
also entropy-output readiness, advertising preparation/active advertising,
and mid-resolution. Tests hold stop busy and assert loaded-slot retention,
exercise stale markers/timers, preserve public/power/DRBG state, prohibit
flash writes and old events, and confirm random-service progress afterwards.
Queued cancellation replies/defaults are checked. Existing real pinned-host
F-24/F-28 legacy/SC, privacy/store, restart/reject/barrier/MIC, and ten-minute
virtual-link suites are retained. S1/S2 tests now await resumed completion.

1. **F-27:** capture boot, Reset, legacy/SC start and restart diagnostics,
   retaining every `executor_kind` count/max/overrun/opcode plus global fields.
   Exercise pending ACL at 06 with two TX slots available. Check `command`
   0C03, `stop`, all `reset_*`, `control_rx`, `control`, `refill`, and entropy
   kinds. Require each <=2400 ticks (100 us), preferably <=1680 (70 us), zero
   item/pass overruns, own pass <=250 us + longest item, dispatch <=7.5 ms/no
   negatives, refill <=80 us. Retain CCM/remainder separately. Run ten minutes
   each at encrypted 15 ms and 7.5 ms under audio/USB/UI load; save audio
   late/shed, canary and stack evidence. Capture before another Reset clears
   the counters; do not sum unrelated per-kind maxima.
2. **F-24 regression:** restore both bonds/list after Reset and `ble host`;
   fresh-RPA reconnects without prompts/negative LTK, correct Enhanced identity
   + peer RPA and encrypted MIDI both ways. Repeat four cycles and power cycle.
   Confirm sync LE mask `1f 02 00 00 00 00 00 00` and restored list entries.
3. **F-28 regression:** retain DUT bond, forget on phone only, pair again;
   successful 2028 before 2027, one entry per peer, no Confirm Value/DHKey
   failure, then three fresh-RPA encrypted reconnects. Repeat changed IRK/new
   RPA and desktop SC where possible, plus live unpair/replacement.

The round-2 snoop answers B23/B24; no LL/HCI version change is included.
Remaining F-25/F-26, on-air H-22..H-25, A9, D-1/D-6, journal crash window,
uncredited entropy and physical stack/RF qualification retain their status.
See `docs/SPEC-GAPS-S3.1.md`, **Round 3 / r7**.

## Archived round-2 / r6 implementation report

The following is the historical pre-bench report; its old status statements
and artifacts describe round 2, not the current round-3 tree.

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
