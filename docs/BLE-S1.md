<!-- SPDX-License-Identifier: MIT -->
# Felucca BLE S1

This is an **unflashed, unqualified bench implementation** of the frozen r2
controller specification with the authorized r2b S1 corrections. Both builds and the host tests pass; scanner visibility,
RF timing, PMU state and DMA behavior remain device acceptance work. No device or
network host was contacted during implementation. Changes are uncommitted on
`ble-controller`.

S1 implements the complete 24 MHz radio recipe, runtime unit calibration reads,
legacy ADV_IND with autonomous SCAN_RSP, ADV_NONCONN_IND, and the advertising HCI
interface. The internal host advertises as **Felucca BLE**, at 100 ms on channels
37/38/39, with hardware advDelay. It creates no connections. CONNECT_IND reports
cause safe advertising restart. There is no NimBLE, BLE MIDI service, pairing,
encryption, scanning, initiating, Wi-Fi stack, or sleep.

## Build and artifacts

From this worktree:

```sh
JIELI_TOOLCHAIN=/root/felucca-ble-env/jieli/toolchain \
AC79_SDK=/root/felucca-ble-env/sdk-min \
FELUCCA_BLE=1 ./build.sh
cp build/felucca.fwsc out/felucca-ble-s1.fwsc
sha256sum out/felucca-ble-s1.fwsc
```

`FELUCCA_BLE` defaults to 0. Use `FELUCCA_BLE=0` or leave it unset for stock.
S1 requires the default `FELUCCA_FLASH=1` and `FELUCCA_CDC=1`; the build rejects
turning either off. All existing engines, samples, effects and USB functions stay
present. No vendor controller library is linked. Only the normal three packaging
files are read from `sdk-min`.

When the unit has item 102, the platform validates it and derives the public BLE
address using 04 §7.1. With the specification's test-unit MAC this gives
`62:40:8B:67:65:58`; no sample address or sample calibration is compiled into the
firmware. If item 102 is unusable, assign a **unique** static random address with
`FELUCCA_BLE_RANDOM_ADDRESS=XX:XX:XX:XX:XX:XX` (display order; leading octet has its
top two bits set), or `ble random …` at the console followed by `ble adv`.
It is retained in RAM for this boot; a build-provisioned address is restored each
boot. Never install one fallback address on multiple units. Missing both kinds of
identity leaves advertising off with HCI status 0x12.

By default LE Read Advertising Channel TX Power (2007) is unsupported, as the
specification requires when no measured dBm value exists. Only if a level-6 value
has actually been measured, pass `FELUCCA_BLE_TX_DBM=<integer -127..20>`. That build
claims 2007 and answers it at level 6; other levels return Hardware Failure.
The shipped S1 artifact uses neither optional provisioning override.

Final image sizes and SHA-256 are in `out/BLE-S1-BUILD.txt` and `out/SHA256SUMS`.
`out/felucca-ble-s1.fwsc` is a copy of the BLE-on `build/felucca.fwsc`.
`out/stock-build.log`, `out/ble-s1-build.log`, `out/ble-s1-host-tests.log` and
`out/felucca-host-regression.log` retain validation output. The stock image and
package hashes match the pre-change build exactly.

## Module map and execution

| File | Responsibility / frozen specification sections |
| --- | --- |
| `firmware/ble/ble_s1.h`, `ble_memory.h` | Stage API, static types, callback and storage bounds; 04 §§0.2,6,10 |
| `firmware/ble/ble_logic.c` | CRC-16/XMODEM, bounded read-only item log, identity, parameter validation, channel table, frame and PDU construction; 01 §§2.1–2.2,4.7; 02 §§3,5.3,6.2; 03 §3.5; 04 §7 |
| `firmware/ble/ble_controller.c` | Copied H4 command FIFO, CC events, masks, Reset, advertising state, finite internal host and 512-record trace; 03 §§1–5; 04 §10; 05 §2.4 |
| `firmware/hal/fm1_ble_tables.h` | Numeric facts from 01 A.1, 24 MHz A.2, A.3.1–5, A.4, A.6; ordered duplicates preserved |
| `firmware/hal/fm1_ble_radio.h` | Full shared radio recipe, quick trim/self-calibration, bounded ports, sequencer, modem/AGC, clock/PMU probes; 01 §§2–8,4.13; 04 §0.4/§0.5 |
| `firmware/hal/fm1_ble.h` | Dedicated DMA memory, width-specific frame access, barriers, anchor port, both IRQ bodies, advertising stop/restart/quarantine; 02 §§2–7,9; 04 §0 |
| `firmware/hal/fm1_isr.S` | Conditional BLE entry/return wrappers; 04 §0.6 |
| `firmware/src/ble_platform.c` | Felucca adapters, runtime configuration, nested-mask probe, ISR/audio accounting, startup script and worker; 04 §§0,11 |
| `firmware/src/ble_console.c`, `console.c` | CDC diagnostics, raw command injection, atomic freeze and paged trace; 05 §2.4; PB checks |
| `firmware/src/felucca.c`, `main.c`, `audio.c` | Default-off option, phased masked vector attachment and controller start, worker in both main-loop paths, subtraction of BLE time nested in audio; 04 §0.6/§0.8 |
| `firmware/app.ld`, `tools/build.py` | Multi-object optional link, DMA/trace regions, size and register-access checks, new-code warnings; 04 §0.2/§0.11 |
| `tests/ble_s1_test.c`, `run_ble_s1_tests.sh` | Pure logic and real H4 state machine tests using a simulated platform; K-05/06/07, H-01..H-13, malformed commands, Reset, finite queues |

The unit's first preserved VM sector and BTIF are read through Felucca's plain
XIP mapping after `persist_boot` has validated the flash and installed that mapping.
No raw SPI read, flash blackout, item write or calibration save is used by BLE.
106/107/108 default in RAM; valid 187 takes the quick path; only an invalid/missing
187 takes self-calibration. A self-calibration result and CRC stay in RAM. Item
110's optional fixed-frequency mode and the optional temperature watchdog are
omitted as allowed by the specification. The reduced radio recipe is absent.

Startup follows r2b 04 §0.1 phases P0–P6. `persist_boot()` is the first main
call (P1); plain item reads and the loop-rate sample run immediately afterwards,
before `audio_init()` (P2). The masked-attach adapter installs 29/45 just before
`timer5_start()` and before `fm1_guard_lock_top()` (P3). Immediately after the
BSP's global enable (P5), radio/baseband startup runs with USB, audio and TIMER5
active (P4); it opens no instance and leaves CPU lines 29/45 masked. HCI runs on
the first main-loop pass (P6), and instance opening enables those lines. In BLE
builds the panel's 30/400 ms boot delays follow this finite HCI sequence so they
do not consume F-18's startup bound. Normal worker calls resume after that
advertising-only pause. The stock boot sequence is unchanged.

The masked adapter and all controller critical sections save `icfg`, clear bit 8,
and restore the exact saved value. No global IRQ state is assumed after flash
startup; no Felucca non-nesting flash helper runs inside a controller critical
section. A nested 250 us probe checks that TIMER5 cannot enter while masked,
before RF begins. Every radio wait uses microseconds on TIMER4; the former
loop-10/100/240 waits use a conservative fixed 10 us. No wait uses CPU Hz.

Both BLE IRQs use priority **5**, above TIMER5 (4) and audio (3). The event body
services pending RX first. ISR work is bounded, uses no allocator or HCI callback,
and does not wait for hardware quiescence. Scan responses are hardware generated.
At event close software refreshes payloads and remaps slots to the current TX
toggle. The worker handles HCI, stop polling, CONNECT_IND recovery and canaries.
A BLE IRQ inside TIMER5 is included in TIMER5's nested time; one directly inside
audio is added once to the same subtraction. Audio overload decisions still see
total elapsed time.

The r2b 02 §5.9 closing lifecycle marks the instance closing under the same
critical section as the stop request. Neither IRQ publishes new PDUs while
closing. Stopping clears A_CNT_HI, then polls busy across worker passes with
interrupts running. The timeout is two configured advertising intervals, clamped to
20..100 ms. Only after busy clears are instance interrupts masked/acknowledged,
anchors cleared and buffers reset; only then is Disable/Reset CC delivered.
No link callbacks or deferred connection actions exist in S1. On timeout the DMA
region is quarantined;
future radio enables fail with 0x03 until reboot/power cycle. No undocumented
baseband abort or sleep handshake is used.

The DMA reservation is 4,340 bytes in `.ble_dma`, included in the pool headroom
check. It contains a 3,352-byte BB window, 972-byte modem table and 16-byte trailing
canary. The BB window holds the eight-entry table, one 324-byte frame, two aligned
256-byte RX buffers and two aligned 40-byte advertising buffers (80 total,
r2b 04 §6.6), plus reserved slack. S1 allocates no connection TX pool; S2's
416-byte requirement is recorded for that stage. A separate `.ble_trace` uses
20,480 otherwise-unused bytes after `.ram_text`
in the 24 KiB RAM-code region. It contains **data only**, outside the executable
PC guard, is NOLOAD, and has a link-time bound. Each valid record is initialized
before exposure; retained contents from an earlier boot are not exposed. There
is no heap or change to a Felucca feature's allocation.

## H4 contract and internal test host

Call `ble_set_host(callback)` before sending commands to replace the internal
host. `ble_hci_send(type, packet, size)` takes the H4 type separately; packet data
starts with opcode, not the type octet. Accepted commands are copied into four
254-byte slots. Return 0 means accepted/answered; a nonzero intake status means
no event follows. The worker handles one command per pass. CC carries one command
credit. A full FIFO answers that command immediately with CC 0x0c using a separate
reply buffer. Other callbacks run from the worker. Callback buffers are valid
only during the call. ACL/SCO/ISO are rejected at intake in S1.

Implemented: 0C03, 1001, 1002, 1003, 1009, 0C01, 2001, 2002, 2003, 2005, 2006,
2008, 2009, 200A, 201C; optional measured-power build adds 2007. Unknown opcodes
always receive CC 0x01. Invalid command parameters give 0x12, unsupported valid
advertising types/address types/filter policies give 0x11, and parameter/address
changes prohibited while advertising give 0x0c. Repeated enable/disable is
idempotent. Reset retains unit identity, power limit and radio calibration, stops
the instance, restores HCI/advertising defaults and unsets the random address.
Queued commands behind Reset are canceled with their own 0x0c replies.

Default supported-command bytes are zero except 5:C0, 14:28, 15:02, 25:B7, 26:03,
28:08. Local features are `00 00 00 00 60 00 00 00`, LE features all zero, LE states
`05 00 00 00 00 00 00 00`. Version is 4.0 (6), experimental manufacturer FFFF,
revision/subversion 1. This is an S1 subset, not a full Core conformance claim.

The internal driver sends the startup order read from permitted NimBLE
`ble_hs_startup.c`, then the legacy advertising path from `ble_gap.c`:
Reset, Version, Commands, Features, Event Mask, LE Event Mask, LE Buffer Size,
LE Features, BD_ADDR, optional Random Address, Advertising Data, Scan Response
Data, Parameters, Enable. It waits for and checks each actual controller CC.
Sixteen bounded worker passes prime cold startup before UI drawing. Normal
workers continue in the main loop and its 15 ms wait. The driver does not locally
fake Reset or bypass the H4 dispatcher.

## Diagnostics without a debugger

Open Felucca's USB CDC serial port, usually `/dev/ttyACM0`, with a serial terminal
that asserts DTR (for example `picocom -b 115200 /dev/ttyACM0`). Baud is ignored.
Type `ble` for counters and timings. This console is separate from the BLE HCI
transport.

| Command | Use |
| --- | --- |
| `ble` | Loop throughput/raw ticks, clock registers, mask probe, calibration path/CRC, stages, HCI status, event/IRQ counts, maxima, DMA addresses and canaries |
| `ble rate` | Repeat the 100,000-iteration throughput sample with audio/USB running; then read `ble` |
| `ble pmu` | Index, before-radio byte, after-radio byte for 00..17, 30..35, 70..77 |
| `ble regs` | Live register readback: indices 0..30 WLA, 31..38 WL0..7, 39..48 WF0..9, 49 BSB |
| `ble adv`, `ble nonconn` | Run Reset and the internal startup script for the chosen profile |
| `ble stop`, `ble reset` | Disable advertising / actual HCI Reset; read later status with `ble` |
| `ble power N` | Set hardware level 0..11; takes effect at event close; not dBm |
| `ble random XX:XX:XX:XX:XX:XX` | Configure RAM fallback identity, display order; use `ble adv` afterwards |
| `ble hci HEX` | Inject command body, no leading 01 H4 octet; disables the internal script |
| `ble freeze`, `ble trace START N`, `ble thaw` | Freeze trace and counter snapshot; read oldest-relative records in pages of at most 32; resume logging |

Trace rows: index, time_ms, dir_type (81 host command, 04 controller event), body
length, intake status, first min(length,32) bytes. The ring retains 512 records;
while frozen new records are counted but not stored. `ble` then shows the frozen
counter snapshot; PMU/regs remain their explicitly documented boot/live views.
Read all pages before thawing. Command replies are inspectable even with event
masks zero. The cold driver produces 26 records with a valid public address
(13 commands + 13 replies), or 28 when it provisions a random address.

Expected bench diagnostics:

- `error=0`, `driver_error=0`, `driver_step=14`, `enabled=1`, last opcode 200A/status 0.
- Valid record: `calibration_1_record_2_self=1`; normally records_present 1F;
  no write is made. Missing/invalid trim: path 2, typically about 0.6 s extra.
- PB1.1: `loop_iters_per_us_x1000 / 1000` gives iterations per microsecond
  for this compiler's loop, memory/cache state and any concurrent interrupt load.
  `loop_ticks` is the raw 24 MHz duration for 100,000 iterations. Repeat with
  `ble rate` while audio/USB run. This is **not Hz**: no known-cycle RAM loop
  was established, so the true core-frequency part of PB1.1 remains open.
- PB2.1: reference P33 01:20, 74:C0, 75:07; timeouts 0. Compare wider snapshots
  rather than blindly replaying them. Index 17's watchdog difference is expected.
- First three `icfg_probe` words have bit 8 clear, fourth restores it;
  `ticks_while_irq_masked=0`. This is an observed-gate check when run on hardware.
- At 100 ms advertising, roughly 9–10 completed events/s; scan requests rise with
  an active scanner; CONNECT_IND attempts increment ignored/restarts, no link.
- IRQ maxima are **raw 24 MHz ticks of C bodies**; divide by 24 for us. Account
  for entry/exit overhead separately. Require `rx_over_380us=0`,
  `event_work_over_150us=0`, `ble_critical_over_390us=0`, `dma_canary_errors=0`
  and `stop_timeouts=0`. `event_work_max_ticks` starts **after** inline RX
  step 1a; `event_max_ticks` includes it. Inline RX is also timed against 380 us.
  `ble_critical_max_ticks` measures only controller outer critical sections;
  BSP sections and their cumulative sum in every 920 us window still need
  external instrumentation. Allocation: 390 + 150 + 380 = 920 us. Worker gaps
  during UI/console/flash are reported, not asserted to satisfy the S2 bound.

Errors: 1 PHY-port timeout; 2 degenerate VCO; 3 DC-measurement timeout;
4 bandwidth timeout; 5 sequencer timeout; 6 unexpectedly busy at open;
7 stop timeout/quarantine; 8 DMA canary damage; 9 IRQ mask probe failed;
10 TIMER4 failed to advance during the clock probe. `failure_stage` records the
radio stage at first error. Stages 1..15: clocks, bridge, digital pass 1, static
analog, PLL/VCO, RF trims, PA, IQ/readback, final analog, digital pass 2, CFR/RAPP,
sequencer, auto-control/MCS, BT enables, modem/AGC/timers.

## Bench procedure and pass criteria

These steps are instructions for the owner; none was executed during this task.
Keep a stock Felucca package and the unit's official firmware available first.
Use ambient temperature, normal supply and about 1 m scanner distance. Record the
artifact SHA, identity, central/adapter, console transcript and RF captures.

1. **PB2.2 / K-05 / F-18:** build, install using the existing Felucca procedure,
   power-cycle, capture `ble` and `ble pmu`. Check the build's RAM/slot limits,
   valid-unit calibration path 1 and its CRC, zero errors, masks, PMU and canaries.
   Repeat 20 boots. With a valid stored trim, radio bring-up must be <=150 ms,
   first HCI command within 50 ms of its end, and Enable within 100 ms of that
   command. Unsigned `first_hci_command_ticks - bringup_end_ticks`, divided by
   24, measures the first-command gap; `enable_ticks - first_hci_command_ticks`
   similarly measures enable latency (both differences within one timer wrap). Do not
   erase calibration to test fallback; use a unit genuinely holding an invalid
   record, or a separately reviewed RAM-only fault-injection build.
2. **F-01 / H-01..H-13 / K-07:** start `btmon` capture on a Linux desktop and an
   active LE scan. A typical BlueZ interactive setup is:

   ```text
   bluetoothctl
   menu scan
   transport le
   duplicate-data on
   back
   scan on
   ```

   Within 3 s expect the unit's address, connectable/scannable legacy ADV_IND,
   advertising bytes `02 01 06`, and SCAN_RSP bytes `0C 09` + ASCII `Felucca BLE`.
   No BLE MIDI service UUID is claimed. Freeze and page through the controller
   trace: all 13 startup commands have real CC status 0, correct response lengths
   and the S1 capability bytes above. The sample application's name/UUID oracle
   in F-01 is replaced by this explicitly requested Felucca payload.
3. **H-12/H-13, Reset and malformed input:** use `ble stop`; wait for stopping=0.
   `ble hci 011000` reads version; `ble hci 072000` returns unknown command in the
   default build. `ble hci 0a200102` must give CC 0x12. `ble adv`, then repeated
   `ble hci 0a200101`, succeeds without a new instance. Advertising-parameter
   changes while enabled return 0x0c. `ble reset` stops RF and returns success
   only after busy clears; `ble adv` works again. For arbitrary 32-byte data
   commands use the expanded console line; embedded data length >31 gives 0x12,
   with no state change. The host tests exercise the full malformed/flood matrix.
4. **F-02 / S-01 / S-02 / K-06:** use a multi-channel BLE sniffer for 60 s.
   Require channels 37/38/39 at 2402/2426/2480 MHz, 1250 us start-to-start spacing
   inside ADV_IND events, periods 100..110.7 ms with the specified 0.7 ms capture
   uncertainty and no >111 ms RF gaps in the controlled run. SCAN_RSP starts
   150 us after SCAN_REQ ends: every deviation <=4 us, mean 150 +/-2 us, with
   uncertainty recorded. Desktop reports alone cannot prove these timings.
   Test min/max 100/200 ms via H4 and require use of the maximum. First-anchor
   625 us acceptance requires a common timing marker as described in gap 12;
   event-close time is not a substitute.
5. **S-12 / A6:** `ble nonconn`; active and passive scanners should both see
   ADV_NONCONN_IND, nonconnectable/nonscannable, with `02 01 06 0C 09` + the name
   in ADV data. The sniffer must see 625 us channel spacing and advertising delay.
   A scriptable central sends addressed SCAN_REQ and valid CONNECT_IND: no
   SCAN_RSP or connection event may result, and advertising must continue/restart.
   `connect_ignored`/`restarts` expose deliberate restart; this run does not claim
   that the hardware itself ignored the request. Then `ble adv` must restore
   ordinary scan responses. Repeat stop/start/Reset and connection attempts.
6. **S-11 / PB3.1 / PB3.2 / PB4.1:** in a fixed RF placement, use `ble power 0`,
   2, 6, 11 and collect >=20 calibrated observations per level. Medians should
   rise from 2 to 6 to 11, with >=30 dB separation between 2 and 11; level 0 may
   disappear. Restore 6. Play dense audio, use USB MIDI/audio and turn controls
   while actively scanning. Read IRQ maxima, canaries, audio late/shed counters
   (`status`, `dbg`) and worker gap. Require no crash, canary damage, mask-probe
   failure, or stop timeout; RX including wrapper overhead must fit 380 us,
   event work after inline RX must fit 150 us, and controller critical sections
   must fit 390 us. The sum of BSP/controller disabled sections in every 920 us
   window must also fit 390 us: these console maxima alone cannot prove it.
   F-16's full padding/burst demonstration belongs to S2 and remains pending;
   S1 exercises the priority and accounting with advertising scan pressure. Repeat ordinary settings
   saves: their advertising gaps are an allowed flash-blackout case, distinct
   from the controlled F-02 run. F-22 is optional and not applicable: this image
   always uses the full recipe.

Host verification, no hardware:

```sh
tests/run_ble_s1_tests.sh
AC79_SDK=/root/felucca-ble-env/sdk-min tests/run_tests.sh
```

The BLE runner builds both default and optional measured-power variants with
`-Wall -Wextra -Werror -pedantic`, AddressSanitizer and UBSan. It tests the actual
controller objects, CRC/sample records, invalid/torn logs, identity, all channel
maps, PDU/frame bounds, H4 envelope/parameter failures, immutable command copies,
four-entry queue and 100-command flood, asynchronous stop/Reset, both internal
advertising profiles, random-address provisioning and frozen/wrapped trace.
The target build compiles both new controller objects with `-Wall -Wextra
-Werror`; new HAL and adapter code promotes those warnings to errors as well.
The normal build retains its no-ROM-reference, no-calls-in-`.ram_text`, HAL-only
register-access and pool-headroom checks.

## Risks and recovery

Radio register accesses, an unproven boot supply state, calibration detectors,
DMA ownership or a baseband that never becomes idle can stop RF or hang the SoC.
The recipe bounds software polls and feeds the watchdog during long startup;
quarantine prevents DMA buffer reuse after a failed stop. Failed bring-up requests
manual RF all-off using the specified enable matrix; that is a best-effort action,
not a verified RF-silence guarantee. Power-cycle after any startup error. A stalled bus or the
kind of chip-wide hang reported in the specification can defeat timer/watchdog
recovery, so a **power cycle may still be necessary**. The radio and IRQ/audio
interaction have not been bench-qualified. Missing identity intentionally causes
silence. An invalid trim invokes an unqualified physical self-calibration; it is
not evidence of output power, sensitivity or temperature performance.

First try `ble stop` if the console responds, then power-cycle. To revert a
running unit, install a stock `FELUCCA_BLE=0` Felucca `.fwsc` with the existing web
installer or `tools/fm1_install.py`. The existing `uboot yes` console command or
OCT- and OCT+ held for 5 s enters UBOOT while Felucca is responsive. Reflash stock
Felucca or use the installer's **Return to official V15** with the owner's official
firmware file, or the vendor updater. No calibration/item writes are added by S1.

If the unit stays black after an interrupted update, follow the repository's
[recovery instructions](../README.md#if-the-fm-1-does-not-start): check for
WL80UBOOT / USB ID 4C4A:8057, retry the documented installer/cable steps, and use
the separately documented FM-1 Transporter recovery if necessary. This task did
not exercise any recovery path or promise that software reset alone repairs RF
state.

See [SPEC-GAPS-S1.md](SPEC-GAPS-S1.md) for every S1 ambiguity/assumption and its
verification plan, and [BLE-S1-AUDIT.md](BLE-S1-AUDIT.md) for the external input
file list. The configuration tables' provenance is the frozen specification;
its unresolved redistribution decision is retained in `LICENSING.md`. This
implementation does not resolve that project decision.
