<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S2 specification gaps and implementation decisions

The input is the frozen r3 snapshot, with the permitted implementability reviews
and responses. The live specification and reconstruction were not consulted.
S1 passed on the owner's device. The parent's first S2 bench passed boot,
advertising and audio, then crashed accepting CONNECT_IND; the implementation
bug and focused fix are recorded in BLE-S2-BENCH-R1.md. Local tests pass, but
the correction has not yet been tested on the device by the parent.
“Verify” below means work for the parent/device owner. S1 calibration, PMU,
identity and constant-table provenance decisions remain in SPEC-GAPS-S1.md.

1. **Receive-window geometry — 02 §§5.4–5.5; A1; S-03/S-04.**
   The implementation writes the specified nominal values: initial window
   WinSize×1250+1250 us, steady 2W+50 us, fixed steady-field 50 us, discontinuous
   WinOffset encoding, and W from the SCA formula. The first A/B/C frame switches
   the window in the RX path. The hardware's edges and combination of the two
   window fields remain unknown. `BLE_FIRST_WINDOW_EXTRA_US`,
   `BLE_WINDOW_PAD_US`, `BLE_LOCAL_SCA_PPM` and `BLE_RX_IRQ_LAG_US` are explicit
   retuning points in ble_link.h. A received anchor is estimated from the first
   timely valid frame's RX-service timestamp minus its 1M airtime and the lag
   parameter. This estimate includes unmeasured IRQ-entry latency; it is not a
   measured hardware timestamp. Verify all WinOffset 0/1/5 × WinSize 1/4 cases,
   packets at both ends of the window, and bad-CRC first packets with a sniffer.

2. **Missed-event correction — 02 §5.8; A3; S-06.**
   Use the nominal signed-12-bit A_FINE/A_DRIFT recipe, live timing-register
   phase, raw slot counter, strict abandonment condition, and sign-extended
   negative writes. Any serviced RX class suppresses correction; only A/C sets
   the separate valid-timing indication. The accumulator resets on creation and
   recovery. The step and limit are `BLE_MISS_STEP_US` and
   `BLE_MISS_LIMIT_US`. Hardware accumulation and recovery of A_FINE/A_DRIFT
   remain unknown. Verify offsets ±500 ppm and two omitted events at 7.5, 15
   and 30 ms; retune only from the resulting capture.

3. **Ignored CONNECT_IND — 02 §5.4; A6; R-18/S-12.**
   Validate the full request before changing frame/anchor/buffer state. The
   default ignores an invalid or non-connectable request and assumes normal
   advertising continues. The RX interrupt acknowledgement itself still occurs.
   `BLE_INVALID_CONNECT_RESTART=1` selects the specified stop/quiesce/reopen
   fallback if A6 demonstrates a stopped advertiser. This is a measurement
   switch, disabled in the delivered build. Verify continued packets and a
   subsequent valid connection for every invalid-field vector. Ordinary
   desktop connection success cannot settle this question.

4. **Instant latching and anchor origin — 02 §5.6; A7; R-19/S-08.**
   Keep one pending update and the event-handler-owned closed_n phase marker.
   Register atomically; equality with the close of I−1 is late. Apply in the
   specified store order at that close, with the 1 ms nominal-anchor guard
   checked at registration and again at programming. Long-future instants are
   tracked by modular event distance; they are not turned into overflowing
   absolute 24 MHz deadlines. Rebase the nominal anchor each event. Supervision
   resets use T_inst and T_win; channel-map updates reset neither timer.
   `BLE_INSTANT_GUARD_US`, `BLE_UPDATE_WINDOW_EXTRA_US`,
   `BLE_UPDATE_WIDEN_US`, and the RX lag are retuning points. The extra guard
   conservatively disconnects with 0x28 if there is insufficient programming
   time. Verify the actual latches, wrap, offset and supervision vector with a
   scriptable central/sniffer; the hardware has no specified group latch.

5. **Synthetic advertising-event close — 02 §5.4; A8; K-08/R-20.**
   A possible close after CONNECT_IND must not consume one of the six events or
   move an anchor early. Event housekeeping and missed-event correction ignore
   closes before the nominal first window. Establishment uses timestamps:
   E_6 = first+5×interval+WinSize×1250+1000 us, with the independent RX cutoff
   F = first+6×interval. Classes A/B/C establish as in r3, including CRC-failed class B;
   only A/C refresh supervision. Verify silent first windows and the late-frame
   vectors. No decision counts event IRQ entries to infer six events.

6. **Unavailable delayed RX metadata — 02 §4.2; R-23/R-25.**
   A full slot is untouched when the 19-entry backlog is full. The worker retries
   it after releasing space. Headers/status and up to 27 bytes are copied before
   returning a fixed DMA buffer; slot release precedes decode/probes. Both-full
   or previously blocked slots use a conservative lower-bound timestamp, and
   never use the current F_EVT_COUNT as that packet's anchor. The per-slot event
   and RSSI cannot be recovered from the specified hardware. Diagnostics count
   delayed timestamps. The second, 19-entry main-loop handoff queue adds slack
   for a slow host; sustained host stalls still engage hardware backpressure.
   Verify both-full recovery, exactly-once delivery, and partial/overlong frames
   on the device. These tests are distinct from the passing software queue tests.

7. **Worker scheduling and interrupt budget — 04 §0; PB3.1/PB4.1.**
   The owner's 116,087 us main-loop gap exceeds 7.5 ms. TIMER0/IRQ 4 is therefore
   reserved for a 1 ms controller-only deferred worker at priority 4; radio RX
   and event IRQs remain equal at 5, audio at 3, TIMER5 at 4. This follows the
   owner's explicit authorization of a software/timer-IRQ LL worker. It is a
   deliberate integration interpretation of older prose that describes the
   worker/control parsing as task-only; NimBLE, HCI callbacks, receive delivery,
   host allocation, NPL events and callouts all remain in main context. TIMER0's
   source/prescaler uses the specified TIMER5 layout and is untested on this BSP.
   The original worker_calls/gap counters only cover live connections, so their
   zero advertising values in bench round 1 did not establish timer failure.
   New worker_timer_calls and all-state gap/readback diagnostics check TIMER0
   before connection; expect about 1000/s, PRD 6000, CON low bits 0x19,
   IRQCFG 0x9. During a connection check worker_calls near 1000/s,
   gap ≤7500 us and pass ≤1000 us
   under full RF/audio/USB/UI load. The 380/150/390 us diagnostics exclude
   wrappers and do not prove the cumulative 390 us per 920 us requirement.
   The final bench must measure it. Main-context LCD and boot-wait yield points
   also serve NimBLE; their improved host latency needs measurement.

8. **Stage capability/security boundary — 00 §1.1 versus 03 §1.3.**
   All 18 mandatory opcodes are dispatched and answered. S2 advertises zero LE
   encryption features, accepts no encryption procedure, and keeps SM off.
   Consequently a valid-handle LTK reply is stray (0x0C), an unknown handle is
   0x02, and LE Rand returns 0x03 without random data: its entropy service is
   unavailable in this stage. No predictable random stream is called secure.
   The successful random/key/encryption behavior is S3 work, with its persistent
   seed and host software ECC; it is not claimed here. The remote-feature event
   masks octet 0 with the S2 local zero instead of the full S3 profile's 01;
   peer octets 1–7 are retained. This keeps NimBLE's L2CAP update path. Verify
   S2 capability tests against these stage values; do not use pairing/S3 tests
   as S2 pass claims. S2 does not claim full Bluetooth Core conformance.

9. **Application fixture versus product — 05 §3/F-08/F-11.**
   Default incoming MIDI plays through the USB/UART input queue; local MIDI
   output uses the existing USB-output hook. `ble echo on` enables the reference
   short-message echo fixture, preserving its 13-bit timestamp; `ble echo off`
   restores ordinary operation. A 7.5–15 ms/latency 0/4 s L2CAP request is made
   once, about one second after connection. Whole incoming packets are validated
   before any MIDI bytes are emitted. Running status is local to a BLE packet;
   SysEx spans packets. USB editor SysEx follows the existing collector, but
   flash writes are refused while connected. Some Android BLE-MIDI clients may
   require pairing; use a client that permits an unencrypted S2 link. No public
   specification or website was fetched because this task forbids network access.

10. **Version-cache wording — 03 §7 versus 05 K-09.**
    The LL reply is queued at most once per connection. Cache the first received
    version tuple, following K-09's explicit first-value policy; subsequent
    version PDUs receive no second reply. The optional HCI 041D is unimplemented
    and unclaimed, so no HCI consumer can observe the differing cache wording in
    03 §7. A future 041D implementation should retain the tested K-09 policy or
    obtain a clarification.

11. **Memory and physical qualification — 04 §0.2/§6; PB2.2/PB2.3.**
    NimBLE pools live in the normal-SRAM .pool section; the DMA block remains in
    the verified SRAM class. The BLE build reduces STUT capture from 4096 to
    2048 samples per track (185.8 to 92.9 ms), saving 16,384 bytes. Its audible
    effect is an earlier fold/repeat for long STUT steps. The stock build keeps
    its original allocation. A fixed 4096-byte host arena bounds startup
    allocation (784-byte peak in the 64-bit simulation; target high water must
    be read on the bench). Remaining pool headroom is checked by the build.
    Stack use under the additional interrupt level and actual TX/DMA quiescence
    still require the physical checks; the simulated ten-minute link is not RF
    qualification. S1's successful bring-up and PMU observations are retained
    without regulator changes.

12. **CPU exception bit-0 label — platform diagnostics.**
    fm1_irq.h/fm1_guard.h identify divide-by-zero bit 2 and stack-limit bit 3.
    The allowed public driver headers expose EMU_MSG/EMU_CON but do not define
    bit 0's mnemonic or the fault-report pipeline delay. Do not borrow a decode
    from forbidden SDK debug sources or claim that label is verified. Bench
    round 1 is consistent with the concrete unaligned word store found in the
    generated CONNECT_IND path: r3 02 §3.3 already requires separate 16-bit
    window fields. That store is fixed; this was an implementation defect,
    not missing BLE register information. Parent retesting must confirm that
    the connection crash is gone. No trap or hardware guard was disabled.
