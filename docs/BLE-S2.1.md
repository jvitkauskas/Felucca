<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Felucca BLE S2.1 — frozen specification r4

Based on the owner's device-passed S2 commit 3581aba. No device/USB/network
access or commit was performed by the implementer. The supplied S2 observations
are accepted as bench evidence: bidirectional MIDI, 10 reconnects, ten minutes,
15 ms interval, exact 850/850/850 ACL accounting and range-timeout recovery.
Central parameter updates, Android and sniffer cases remain untested on hardware.

## Changes

- r4 04 §0.8/§10.5: both controller workers now execute in the dedicated
  TIMER0 interrupt. Software requests a 1-us timer expiry; a 1-ms autonomous
  expiry and TIMER5's independent 10-ms request provide backstops. The timer
  register layout/IRQ mechanism passed S2 at priority 4; r4's normative default
  priority 2 is used here, below audio 3 and tick 4 and below radio 5. No guess
  about ILAT bit-to-line mapping is required. Priority 4 remains the documented
  measured alternative if PB4.2 shows that priority 2 misses G=7.5 ms; it is
  not selected without that measurement.
- Startup, before the first instance, requests one timer entry and checks for
  exactly one dispatch within 1 ms. Failure latches radio error 10. The executor
  checks time between work items against a 250-us entry budget, returns from
  stop polls, re-requests pending work, and defers a continuous one-second
  request chain to the 10-ms backstop. Diagnostics record pass/dispatch maxima,
  over-budget entries and starvation deferrals. Hardware timing remains PB4.2.
- The executor hands H4 packets to a 32-entry controller-owned ring. Only main,
  including the synchronous command semaphore wait, moves that ring into
  NimBLE pools. The wait feeds the watchdog and runs no controller or host
  events. This is the explicit ring alternative of 04 §10.5; delivery bounds
  terminate at the ring. Ring exhaustion is counted; normal command credits
  allow one outstanding command. The queue and NPL critical sections use the
  platform's nesting-safe interrupt save/restore.
- r4 02 §5.6 / 03 §6.2: connection updates arm instant/window resets without
  assigning a future supervision deadline. Due resets apply chronologically;
  expiry wins ties and an old expired link cannot be rescued by a later reset.
- r4 02 §4.2: per-slot t_free is recorded just after releasing the full flag.
  Delayed stamps use max(t_free, older stamp in that service); recovery is
  explicitly delayed. Lower-bound refreshes never lower the deadline.
- The RX FIFO reserves two extra entries for epoch markers in addition to
  19 PDU entries. Markers and PDUs are consumed in order with connection epochs.
  S2.1 has no encryption transitions; S3 uses these markers.
- The entire TX refill remains under interrupt exclusion, including two-slot
  loads; unstable snapshots defer after four attempts. Its separate 60-us
  maximum/overrun diagnostics support the r4 bound.
- Reset cancels the work of commands queued before its intake, replies 0x0C
  to them in order, and replies to Reset last after quiescence. Commands arriving
  afterward follow Reset. The finite flood test retains every response through
  a reserved opcode queue; callbacks are delivered from the executor.
- Stage-specific capability reports omit LE Rand and the LTK commands in S2.1.
  They answer Unknown HCI Command; LE features remain zero and states 0x85.
- last_disconnect_reason retains the previous completed link's reason while
  the next link is up. Connect retries are bounded at three in the desktop tool.
  Notes enter through aplaymidi/BlueZ's ALSA port: the MIDI plugin owns D-Bus
  WriteValue and can return NotAuthorized for it.

The transport ring is explicitly placed in ordinary static RAM; it is cleared
by normal startup. No additional audible feature reduction is made in S2.1.
The S2 BLE-only 2048-sample STUT allocation is retained. Exact figures and SHA
are in out/BLE-S2.1-BUILD.txt and out/felucca-ble-s2.1.sha256.
The final source rebuild is e555e67b93de406f0aef538b3bfa7151459ada14653049610dc43ff62b01d7ad
(app 488956 bytes; static RAM 96292, pool 335268). The initial 80c720dc image
is archived as out/felucca-ble-s2.1-first.fwsc; its behavior and RAM allocation
are unchanged. See BLE-S3-AUDIT.md for the shared r4 input audit.

## Build and local checks

Use FELUCCA_BLE=1 FELUCCA_BLE_STAGE=2, the permitted JIELI_TOOLCHAIN and AC79_SDK,
then ./build.sh. Run python3 tests/run_ble_s2_tests.py and
tests/run_ble_s1_tests.sh. Pure tests add armed-reset expiry/tie/wrap vectors,
monotonic delayed refresh, reserved FIFO markers, prior disconnect reason and
Reset cancellation. Actual NimBLE integration exercises both updates separately,
the MTU/GATT/MIDI path, fragmented traffic, ten simulated minutes and reconnects.
These are software tests, not new hardware measurements.

## Short parent regression bench

1. PB4.2/PB3.1: boot; require error=0 and executor_selftest=1. IRQCFG should be
   0x5 (priority 2 enabled). Compare two ble reads: timer calls advance. Use a
   7.5-ms central link and then a 30-ms link for ten minutes each under heavy
   audio/USB/UI load. Require executor dispatch ≤7500 us, pass ≤250 us,
   refill ≤60 us, no corresponding overrun, no audio late buffers/shed voices.
   Pass time includes preemption; record any overrun and its workload. Perform
   the supervisor-stack-fill/margin and cumulative IRQ-mask probes separately.
2. H-03/K-07/R-22/R-25/R-29: check stage reports and repeat the queued-Reset,
   FIFO/backpressure and stalled-host/ECC-progress fixtures using simulated
   inputs first. A timer self-test failure must leave the radio disabled; its
   failure-response path and physical timing require parent qualification.
3. F-05/F-07/F-08/F-09/F-10: connect, discover, empty-read, subscribe; play
   keys and send a standard MIDI file with aplaymidi -p CLIENT:PORT FILE.mid
   (find the port with aplaymidi -l). The script's --send-notes now requires
   --alsa-port CLIENT:PORT. Require zero drops and balanced ACL/NCP counters.
4. Central-initiated parameter update with sudo and btmon, ten reconnects,
   ten-minute MIDI link and out-of-range supervision as in BLE-S2.md. During
   the next link last_disconnect_reason must still show 0x08. Then test an
   Android BLE-MIDI app. Sniffer timing/epoch/instant cases remain separate.

The hardware timer alternative avoids an unverified software-latch mapping;
the changed priority and request semantics still require fresh PB4.2. S2's
priority-4 measurements are not presented as proof for this new build.
