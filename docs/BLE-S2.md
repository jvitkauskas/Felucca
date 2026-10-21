<!-- SPDX-License-Identifier: MIT -->
# Felucca BLE S2

S2 adds one unencrypted peripheral connection, a real Apache NimBLE 1.9.0 host,
and BLE-MIDI to the S1 radio implementation. Changes are uncommitted on
`ble-controller`, above `48ee2a0`. S1 passed on the owner's FM-1. S2 has passing
local software tests and target builds; device/RF acceptance remains for the
parent session. No device, USB or network host was accessed during implementation.

The owner's first S2 bench passed boot, NimBLE sync, advertising and audio,
then crashed on connection acceptance. The focused correction and its evidence
are in [BLE-S2-BENCH-R1.md](BLE-S2-BENCH-R1.md). The rebuilt image still needs
the parent's physical connection retest; software simulation is not RF acceptance.

## Build and artifacts

From this worktree:

```sh
FELUCCA_BLE=1 \
JIELI_TOOLCHAIN=/root/felucca-ble-env/jieli/toolchain \
AC79_SDK=/root/felucca-ble-env/sdk-min ./build.sh
python3 tests/run_ble_s2_tests.py
tests/run_ble_s1_tests.sh
```

`FELUCCA_BLE=0` builds stock Felucca. The NimBLE source subset is vendored under
`firmware/third_party/nimble/`; builds use no downloads or vendor Bluetooth
objects. It is pinned to `nimble_1_9_0_tag`, commit
`da7e3256da3ba80b232df880f40d8359311cc62e`. Its Apache-2.0 LICENSE/NOTICE and
copyright notices are retained. The optional measured-power S1 command remains
conditional on `FELUCCA_BLE_TX_DBM`; no absolute power value is supplied here.

The delivered package is `out/felucca-ble-s2.fwsc`. Its SHA256, app SHA256,
exact flash/RAM sizes, stock comparison and validation results are in
`out/BLE-S2-BUILD.txt` and `out/felucca-ble-s2.sha256`. The package also contains
the existing independent update loader and the three unchanged packer inputs;
these are not linked into the controller.

## Design and module map

- `firmware/ble/ble_link_logic.c`: pure CONNECT_IND/parameter validation,
  channel-map/CSA#1 oracle, widening, modular instant arithmetic, RX classes,
  supervision and establishment deadlines. Maps to r3 02 §§3.4, 5.4–5.8, 6;
  03 §6.2. Time is raw 24 MHz TIMER4, with signed modular deadline comparisons.
- `firmware/ble/ble_link.c`: preallocated connection record, epochs, RX/TX
  ownership, control procedures, ACL credits, deferred LE events and closing.
  Maps to 02 §4, §5.6/§5.9; 03 §§6–7, 9–11. No NimBLE API or host callback
  appears in this module.
- `firmware/hal/fm1_ble.h`: radio-owned frame fields, byte-sized slot controls,
  DMA publication, CONNECT_IND hardware commit, RX-first event interrupt,
  TX refill, nominal receive windows/correction/instant programming, bounded
  stop and quarantine. Maps to 02 §§2–7, 9; 04 §0. Existing S1 bring-up and
  unit calibration remain in the radio HAL and ble_logic.c.
- `firmware/ble/ble_controller.c`: copied four-command H4 queue, command
  Complete/Status, masks, capability bitmap, main-context event/ACL delivery,
  trace and Reset. Maps to 03 §§1–5, 9–11; 04 §10.
- `firmware/ble/ble_npl.c`, `ble_heap.c`, `port/`: bare-metal NPL events,
  event queues, millisecond callouts, single-main-context mutexes and critical
  bookkeeping, command-ACK semaphore pump, bounded allocation and the needed
  freestanding C declarations/functions. Maps to 04 §§0.11, 4–6, 10–11.
- `firmware/ble/ble_host.c`: actual NimBLE initialization, H4 transport pool
  ownership, zero-IRK callback, advertising, GAP/connection parameter request,
  GATT MIDI service, subscription state and MIDI queues. Maps to 03 §1.3;
  04 §10; 05 §3. `ble_midi.c` is the independent streaming MIDI codec.
- `firmware/src/ble_platform.c`, LCD/main/setup/editor/console yield points,
  USB output hook and storage/flash guards: Felucca integration. Maps to
  04 §0.1, §0.6–§0.9. `ble_console.c` exposes both controller and host counters.

The controller exposes handle 0x0050, LE 1M, CSA#1, 27-byte ACL payloads and five
host TX credits. Hardware performs CRC, hopping, IFS, SN/NESN, automatic empty
PDUs and retransmission. Software preserves the hardware SN when publishing an
empty TX slot, sets MD from the queued ACL data, and never rewrites a loaded
slot. A TX control byte changing to empty releases that exact software owner
once. Reception of a code-9 repetition neither releases that owner nor forwards
a second ACL packet; it does establish/refresh timing. Codes 5/13 may establish,
but never refresh supervision. Unknown/overlong clear packets are counted and
dropped conservatively.

Both hardware RX buffers are 256 bytes, with F_RX_MAXLEN 255. Their headers,
status and acceptable payload are snapshotted before the slot returns to DMA.
There are 19 controller backlog entries, following floor(8500/468)+1, and a
second 19-entry main-loop handoff queue. If the backlog fills, a full hardware
slot is untouched. The worker retries after draining entries. Deferred slots use
lower-bound receive timestamps and do not pretend to have per-slot event/anchor
metadata. The connection has five ACL TX owners and eight control owners; one
control owner is reserved for local termination. Advertising uses its two
40-byte fixed payload areas; connected TX copies into those two DMA areas.

Connection updates and channel maps share one pending instant. Registration
uses closed_n under exclusion, and a 1 ms anchor guard. Hardware programming
occurs at close I−1; the current software parameters change at the instant.
Supervision resets at the old-grid instant and again at the new window start.
There is no software event-count shortcut for establishment: the six-event
E_6 deadline and independent seventh-window cutoff use the r3 timestamps.

Termination marks the link, excludes new loads/intake, stops future events,
settles acknowledged slots while the current event finishes, waits boundedly
for idle, masks/acknowledges, reclaims, then reports Disconnection Complete in
main context. Peer termination waits for the next event close. Local termination
waits for acknowledgement of its own buffer or its supervision-length terminate
timer. NCP precedes Disconnection Complete; unsent/flushed packets receive no
NCP because NimBLE refunds their outstanding credits. Epochs and queue clearing
prevent an old completion from crediting a reused 0x0050. Reset stops/reclaims
before its CC, clears connection/procedure state, emits no link event, and keeps
identity/calibration/power. Transport trace history and startup timestamps remain
available across Reset.

The LE event storage has 16 entries with reserved creation/security capacity;
creation, NCP and disconnection also have dedicated state. Queued link events
are discarded on close. Optional HCI remote-version and channel-map reads are
unclaimed; LL version exchange still replies once and caches the first tuple.
Feature exchange caches peer octets 1–7 and masks octet 0 with local S2 zero.
2016 returns CS followed by cached features, or waits up to 800 ms and returns
0x1A with zero features. Repeats, late arrivals, cancellation and masks follow r3.

## Worker and main-context host

The measured S1 main-loop gap was 116,087 us under load and about 598 ms at boot.
S2 therefore reserves TIMER0 (IRQ 4), crystal/4, 1 ms, at priority 4. Both radio
IRQs retain priority 5; the worker preempts audio at 3 and shares priority 4 with
TIMER5. Vectors are attached masked before the top-of-RAM lock. The timer runs
only bounded controller work: queued control parsing, handoff, backpressure
recovery, deadlines and stop progress. Its operations use short nesting-safe
exclusion; each packet is a separate bounded work item. At most 19 are processed
per pass. No allocator, NPL callout, NimBLE event or host callback runs there.
This is the explicitly authorized deferred-IRQ alternative to the cooperative
worker described in parts of r3. S3 receive cryptography needs a further context
review before extending this worker.

NimBLE runs in the existing main loop and its 15 ms wait. Additional main-only
yields serve it during LCD traffic, the boot panel wait, setup, editor waits,
console backpressure and OTA waits. A yield guard prevents host re-entry. The
synchronous NimBLE command semaphore pumps only the real HCI/controller and ACK
path; it never invents a Reset/features reply or recurses into host events.
Host callbacks copy events and ACL into NimBLE's pools. The audio ISR only
appends USB-form MIDI output packets to a bounded ring; it calls no NimBLE API.
Worker/radio preemption time is subtracted from render CPU figures exactly once;
a radio ISR nested in the worker is already included in the worker interval.
The audio overload decision continues to count total elapsed time.

Raw flash reads switch to the already-plain XIP mapping while a link exists.
Every settings/program/erase/update write path refuses a connected operation,
including a second check inside the interrupt exclusion before disabling XIP.
Disconnect the central before saving persistent changes or updating firmware.
Advertising-only flash blackouts retain S1's policy. Calibration/item/seed
sectors are not written by this BLE implementation.

The architectural worker gap is independent of long main-loop/UI calls. Actual
TIMER0 operation, worst-case dispatch/pass latency, wrapper cost and cumulative
interrupt masking still need the physical PB tests. Passing simulation does not
establish the 7.5 ms or 380/150/390 us hardware bounds.

## Host profile and MIDI behavior

NimBLE is peripheral-only, one connection, public identity, no privacy,
extended/periodic advertising, ISO, PHY updates, filter list or host receive
flow control. ATT preferred MTU is 200. Host receive transport has 24 ACL mbufs;
20 non-discardable 70-byte event buffers and 12 general 256-byte mbufs are
preallocated. HCI advertises 27 bytes × five controller credits. Host software
security is disabled in S2. S3 will add its persistent-seed/random service and
software SC Just Works with legacy fallback; no encryption is requested now.

All 18 mandatory opcode envelopes are recognized, plus 2005 and 201C. The
supported-command nonzero bytes are 0:20, 5:C0, 14:28, 15:02, 25:B7, 26:03,
27:A0, 28:0E. Local HCI features are `00 00 00 00 60 00 00 00`; LE features
are zero; states are `85 00 00 00 00 00 00 00`. In this security-disabled stage,
2018 returns Hardware Failure 0x03 with no random bytes; 201A/B return 0x02 for
an unknown link or 0x0C for a stray reply on the live link, with the handle field.
Encryption procedures cannot succeed until S3. This stage does not claim full
Core conformance or secure randomness.

Advertising is connectable ADV_IND, 100 ms plus hardware advertising delay,
with flags and the complete 128-bit MIDI service UUID. Scan response contains
`Felucca BLE`. GATT exposes service
`03B80E5A-EDE8-4B33-A751-6CE34EC4C700` and characteristic
`7772E5DB-3868-4112-A1A9-F2669D106BF3`: empty read, write without response,
notify, with an automatically managed CCCD. A one-time L2CAP request asks for
7.5–15 ms, latency 0, timeout 4 s, about one second after connection.

The codec handles 13-bit millisecond timestamp wrap, packet-local running
status, system/realtime MIDI and SysEx spanning packets. It validates an entire
packet before emitting bytes. Incoming bytes become USB-form MIDI packets and
enter the common USB/UART MIDI input queue, retaining decoded messages while
that queue is full and reserving eight slots for the wired inputs; SysEx uses the existing USB editor
collector. Playback occurs at the next audio half, as with those inputs; BLE
musical timestamps are parsed rather than used as a delayed playback scheduler.
Local key/chord note output and other events already emitted through Felucca's
USB MIDI output hook also produce notifications while subscribed. `ble echo on`
enables the short-message echo acceptance fixture; it is off by default.

## Memory reduction

Under FELUCCA_BLE=1 only, each track's SLICER STUT recording buffer has 2048
samples at 22.05 kHz: 92.9 ms, saving 16,384 bytes across four tracks. Long live
STUT steps fold/repeat sooner. The standard build retains 4096 samples/185.8 ms.
The host arena is fixed at 4096 bytes; the 64-bit simulation's high water is
784 bytes. NimBLE static pools are placed in the ordinary SRAM .pool section
and zeroed by the existing startup. The build preserves at least 8192 bytes of
pool headroom. The DMA block stays inside the r3 verified SRAM class; the
512-record trace remains in the unused RAMTEXT tail. See the exact final size
report in out/BLE-S2-BUILD.txt.

## Diagnostics and local tests

`ble` prints link state/epoch, interval/latency/timeout/event count, establishment,
class A/B/C/D counts, repetitions, delayed stamps/backpressure/occupancy,
updates/maps/late instants, termination/supervision/establishment failures, last
disconnect reason, accepted/rejected/flushed/acknowledged ACL and NCP credits,
worker calls/max gap/max pass/over-7.5-ms count, NimBLE sync/reset/error/MTU/CCCD,
pool/arena/drop counters and MIDI packets in/out. S1's calibration, canary,
IRQ/critical timing, power, PMU and trace commands remain.

`worker_calls`, `worker_max_gap_us` and `worker_over_7500us` measure live
connections; zero while advertising is expected. The independent
`worker_timer_calls`, `worker_timer_max_gap_us`, `worker_timer_over_7500us`
and `worker_timer_last_ticks` cover every TIMER0 interrupt, including advertising.
`worker_timer_con/cnt/prd/irqcfg` read back the timer and its IRQ nibble.
Use two status reads about one second apart: calls should advance about 1000,
PRD should be 6000, CON's low bits 0x19, and IRQCFG 0x9. All-state gap maxima
can include advertising-only flash blackouts; the connection counters retain
their separate PB4.1 meaning. HCI Reset clears both sets of timing counters;
the timer heartbeat remains live even when the transport trace is frozen.

`ble_breadcrumb` packs the last stage/state/detail in bits 31:24/23:16/15:0;
`ble_breadcrumb_ticks` is its approximate TIMER4 timestamp. On a CPU fault,
the crash handler captures `ble_fault_breadcrumb`, `ble_fault_ticks` and
`ble_fault_crash_count` before drawing the crash screen. The 24-byte .noinit
record survives reboot, new traffic, HCI Reset and counter resets. Its magic
rejects uninitialized RAM; firmware updates/power removal need not preserve it.
Stage values and details are listed in BLE-S2-BENCH-R1.md. Host yields are
disabled during the crash screen, so fault reporting cannot enter NimBLE.

`ble echo on|off` selects the test echo. `ble adv`, `ble nonconn`, `ble stop`,
`ble reset`, and raw `ble hci HEX` enter manual HCI diagnostics and suspend
NimBLE processing; use `ble host` to resynchronize the actual host afterward.
`ble host` queues a NimBLE reset/resynchronisation in main context. It is a
recovery command, not a status query or a chip reboot; it may close a live
connection and restart advertising. Repeated startup allocation is not performed.
Power changes, status and trace reads do not enter manual mode. `ble freeze`
freezes the transport trace/S1 snapshot; new S2 link/host counters remain live.
The 512-record trace holds 32 bytes per packet, enough for each 27-byte ACL.
Pause/drain traffic between freeze/page/thaw captures for long credit tests;
it is not a lossless recorder of an unbounded stream.

The ASan/UBSan runner tests CSA#1, maps, validation, wrap arithmetic, all r3
establishment timelines, new timeout origins, status 9/13, queue pressure,
acknowledgement/credits/epochs, control/terminate behavior, HCI flooding and MIDI
wrap/running status/SysEx. The integration test links the actual pinned NimBLE
and controller against simulated RF slots: startup, UUID/property discovery,
empty read, MTU 200, CCCD, bidirectional MIDI, diagnostic echo, fragmented ACL,
central and L2CAP-initiated updates, 40,000 events at 15 ms (ten simulated
minutes), ten reconnects and an event-independent timeout. It checks zero host
resets/drops and five settled credits. The S1 runner retains advertising,
calibration/identity/trace/malformed-command coverage in both power variants.

## Bench procedure for the parent session

Use the delivered SHA and the existing owner-operated installation procedure.
The following was not executed by the implementer. Save a recovery/stock image,
record the unit address, central/adapter/app versions and uncertainty of timing
measurements, and keep `btmon` plus the FM-1 CDC console transcript. Linux should
have BlueZ 5.x; `tools/ble_midi_bench.py` uses Python 3 and dbus-next on the system
bus. The script is never invoked by builds/tests. Do not pair during S2.
Remove an old bond/cache for this address before testing (for example,
`bluetoothctl remove 62:40:8B:67:65:58`, then rediscover); a cached vendor bond
can otherwise make a central request encryption immediately.

1. **Startup, PB2/PB3/PB4, F-01/F-03/H-01–H-14/K-07.** Power on, scan in
   bluetoothctl with LE transport, and inspect `ble`. Require bring-up error 0,
   calibration path 1/CRC 63B7 for the known unit, its public address
   62:40:8B:67:65:58, connectable `Felucca BLE` with the MIDI service UUID,
   nimble_synced=1 and no assertion, canary error or stop timeout. Run:

   Before connecting, compare two `ble` reads one second apart. Require
   worker_timer_calls to advance about 1000 with PRD 6000, CON low bits 0x19
   and IRQCFG 0x9. The connection-only worker_calls may still be zero. Record
   ble_fault_crash_count before the attempt so a new crash can be identified.

   Run on the desktop:

   ```sh
   sudo btmon -w felucca-s2.btsnoop
   # In another desktop terminal:
   python3 tools/ble_midi_bench.py --duration 60 --send-notes
   ```

   It must connect, discover both UUIDs, verify the empty read and enable CCCD
   notifications. The FM-1 trace must show role 1/handle 0050 and matching
   CONNECT_IND parameters; exchanging data establishes within 2 s and the link
   stays up for the full minute. Require worker_calls advancing near 1000/s,
   worker_max_gap_us ≤7500, worker_max_us ≤1000, worker_over_7500us=0,
   rx_over_380us=event_work_over_150us=ble_critical_over_390us=0, zero NimBLE
   resets/drops and no audio late buffer or shed voice. Include wrappers and
   cumulative 390 us/920 us instrumentation for a timing pass; maxima alone
   prove neither. The all-state timer heartbeat is the TIMER0 operation check.

2. **MIDI/CCCD/echo, F-07/F-08/F-14.** While subscribed, press/release FM-1
   keys: the desktop prints MIDI notification payloads with timestamp/status/
   note/velocity. With `--send-notes`, note 60 must sound and release on the
   selected/appropriate MIDI part. Verify Note Off and repeat at least 100
   times without missing/stuck notes. Toggle CCCD off/on with the central and
   confirm notifications stop/resume. Enable `ble echo on`; write
   `80 80 90 45 55` without response ten times and require that payload in a
   notification each time, ≤100 ms at interval ≤30 ms. Turn echo off afterward.
   Running status and multi-packet SysEx get a separate codec/central test;
   persistent editor writes are refused until disconnect.

3. **Updates, F-11/F-12/H-14/R-07/S-08.** Observe the FM-1's one-time L2CAP
   request for 6..12 units/latency 0/timeout 400 and its accepted central LL
   update. Verify 2016 cannot cause peripheral HCI 2013. For repeatable
   desktop-initiated changes, read the desktop connection handle from btmon,
   then send standard LE Connection Update to that desktop adapter:

   ```sh
   sudo python3 tools/ble_midi_bench.py --conn-update 0xHANDLE 6 6 0 500
   sudo python3 tools/ble_midi_bench.py --conn-update 0xHANDLE 24 24 0 500
   ```

   Replace 0xHANDLE with the actual numeric desktop handle (it is not necessarily
   0050). Use `--index N` for hciN. Require Command Status 0 and successful LE
   Connection Update Complete on both sides, no disconnect and uninterrupted
   MIDI; the DUT completion follows the air PDU within 1 s. A scriptable central
   and sniffer must additionally prove the exact instant/window and map change,
   including old 30 ms →15 ms, offset 3, size 2, instant across wrap.

4. **Reconnect and range, F-13/F-19/R-20/R-26.** Run
   `python3 tools/ble_midi_bench.py --cycles 10 --duration 3 --send-notes`.
   Require ten successful reconnects, both directions of MIDI each cycle,
   stopped/quiesced hardware before each Disconnection Complete and fresh
   advertising within 1 s of re-enable. NCP must never follow that connection's
   Disconnection Complete or credit a new epoch. Then run a ten-minute link:
   `python3 tools/ble_midi_bench.py --duration 600 --send-notes`. Require zero
   disconnect/reset/drop/canary/timeout errors and all sent/played MIDI events.
   Separately walk the FM-1/central out of range without pressing Disconnect.
   Require reason 08 after the negotiated timeout, never early, and delivery by
   timeout+one interval+10 ms after the last CRC-valid frame. Return within
   range and reconnect. For >500 ACL accounting, use four controlled batches
   of ≤125 packets, pause/drain between trace captures, and verify accepted =
   acknowledged = NCP for drained traffic; queued/flushed data get no NCP.

5. **Android interoperability.** With the desktop disconnected, select
   `Felucca BLE` in an Android BLE-MIDI app that allows an unencrypted connection
   and exposes the MIDI endpoint. Subscribe, play FM-1 keys into its monitor,
   and send notes back to hear them. Repeat disconnect/reconnect and the
   ten-minute run. Record an app-imposed pairing requirement as an S3 dependency,
   not a successful S2 pairing test.

6. **Scriptable-central/sniffer acceptance.** Ordinary BlueZ/app success does
   not settle the hardware gaps. Run S-03–S-08/S-10 and R-16–R-23/R-25/R-26
   with the r3 vectors: invalid CONNECT_IND continuation, first windows at both
   edges, unknown/role-invalid/malformed control PDUs, repeated version and
   feature exchanges, CRC failures and repetitions, missed events, both slots
   full, queue holds 8.5/50/500 ms, late/wrapped instants, terminate acknowledgement
   loss, Reset with queued ACL and stop/reopen. For K-08/R-20 require E_6 153.5 ms
   and F 181.25 ms for the nominal vector; a bad-CRC frame at 1.9 ms establishes
   without moving the initial 5 s supervision origin; the update-silence vector
   times out at 1243.75 ms, with the specified delivery tolerance. F-16 needs
   full bidirectional legal packet bursts, worst-phase IRQ/masking measurements
   and audio/USB/UI load. F-21 requires plain single-bit BB IRQ acknowledgement,
   no unhandled pending RX and stable traffic. S3 encryption tests remain out
   of scope. Record every gap retune and repeat its affected tests.

7. **HCI negative tests, H-17–H-21/R-01–R-11/R-22.** Use manual HCI mode
   or a local controller harness for malformed command lengths, unsupported
   opcodes, default/cleared event masks, a 100-command flood, live-link Reset,
   pending feature cancellation and local Disconnect. Every accepted command
   gets exactly one CC/CS and one command credit; masked link events are absent,
   NCP/CC/CS remain unmasked, overflow commands receive 0x0C, and Reset restores
   defaults without a chip restart or stale event. 0406 must acknowledge the
   terminate buffer before reporting local reason 0x16; a peer withholding that
   acknowledgement times out at the negotiated supervision length with 0x08.
   Use `ble host` afterward. H-18's encryption events and H-15/H-16 successful
   security procedures are S3 cases; S2 verifies their declared stage errors.

See SPEC-GAPS-S2.md for each nominal/conservative hardware choice and
BLE-S2-AUDIT.md for the external clean-room inputs.
