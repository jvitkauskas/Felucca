<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S3.1 round 4 — r7 connection-update analysis

Base 0fb0475 on `ble-controller`; no commit, push, device access or flashing.
The four committed failure records were read first. Reference requirements:
r7 02 §§5.4–5.6/5.8/5.9, 03 §§6.2/6.3/7/7.0, 04 §0.8. The supplied bench
image is S3.1c, not a measurement of this S3.1d implementation.

## Evidence and conclusions

The supplied Linux-monitor btsnoop (datalink 2001) has 195 records. A local
reader decoded the big-endian record headers and HCI payloads; these times
are relative to its first record, which is already on an existing link:

- 87.091662 s: command 2013, handle 0800, interval range 6..6, latency 0,
  timeout 500. 87.206029 s: LE Update Complete, status 00, interval 6.
- 246.259902 s: command 2013, 24..24/0/500. 246.657091 s: Complete 00/24.
- 604.117583 s: host Disconnect 0406, reason 13. 605.187598 s: Disconnect
  Complete 16. This is the desktop's eventual local termination, not proof
  that the FM-1 stayed synchronized until then.
- 609.709549 s: Enhanced Connection Complete 00, interval 12, latency 0,
  timeout 400. 610.723298 s: command 2013, 6..12/0/400; 610.841290 s: Complete
  00/12 (BlueZ's own new-link update).

No LL PDU, WinSize, WinOffset, Instant or original CONNECT_IND is present.
Central HCI success proves its own change; it does not prove peripheral apply.

**Proved code bug: the original first-window gate expires.** Both
`fm1_ble_event_body` and `ble_ll_event` used a signed comparison of `now` with
`time.first` for the entire link. `first` is the original first-window time,
not refreshed by normal packets or updates. At 24 MHz the signed horizon is
2^31 / 24,000,000 = **89.478485333 s**; full wrap is 178.956970667 s. After
89.478 s the gate suppresses LL closes and missed-event correction, even
though the hardware and interrupt counter continue. Processing resumes in
the next positive half. A signed tick comparison is valid for nearby deadlines,
not an unbounded connection age.

The stuck dump is quantitatively consistent with this bug. Its last event
29825 at 15 ms is near the fifth half-wrap boundary (447.392427 s, within one
event/close phase), and three serviced halves predict 17895.697 closes,
matching **conn_events 17896**. The original HEAD `ble_link.c` fails the native
600-second event-grid test at the first half-wrap; the fixed file serves all
40000 closes. See `ble-s3.1d-baseline-repro.log`.

**Proved second code bug: exact-close-only instant firing.** The old handler
programs only when event + 1 == Instant. If I-1 was suppressed/skipped,
`instant_kind` stays nonzero indefinitely; later closes never treat it as
missed. No worker deadline detects that loss. Consequently a received valid
update can remain pending without `late_instants` increasing, and the next
instant-bearing control PDU is ignored as busy. A stale closed counter can
also pass registration's modular test; a large distance can skip its absolute
guard as “far”. This explains a concrete software path for the first two
observations, conditional on the actual PDUs having been accepted.

**Historical PDU attribution remains a hypothesis.** The first update may
have been registered before a suppressed I-1 close, or an earlier channel-map
procedure may already have occupied the slot. An invalid new window would
also have been ignored before the instant test, without counting late. The
ignored-count changes alone identify none of these cases. The second +1 is
consistent with busy, but its opcode/reason were not retained. S3.1d records
those distinctions instead of asserting an unknown LL encoding.

**The validation interval is already correct.** Core 5.4 Vol 6 Part B §5.1.1
bounds the window by the new interval. `ble_conn_params_valid` checks WinSize
1..min(8, newInterval-1) and WinOffset <= newInterval. Keep that behavior:
new 7.5 ms permits size at most 5/offset 6; new 30 ms permits size 8/offset 24,
including offsets larger than the old 15 ms interval. r7 specifies ignoring
invalid parameters without a reject or parameter change. Legal updates are
mandatory; late valid instants close with 28. Source:
https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/low-energy-controller/link-layer-specification.html
(retained locally as `s3.1-core54-ll.html` and `.txt`).

**“Worker spinning” is not proved to be a pending-work busy loop.** The adapter
runs an autonomous 1-ms worker backstop. Each call on a live link increments
worker_calls. The dump's TIMER0 period is 6000 (the normal backstop, versus 6
for immediate pending work); 522624 worker calls / 527737 timer calls at
uptime 531078 ms is consistent with this normal tick. A failed LL-close gate
does not prevent housekeeping from running. These snapshots cannot assign a
historical jump from 7350 to a particular trigger. `enabled 0` is the HCI
advertising-enable diagnostic; a connected link normally disables advertising.

**Incomplete reset was real; the reported cross-link causal chain is unproved.**
Old close explicitly cleared `instant_kind`; old creation reset the timeline,
anchor, accumulator and `instant_programmed`. Thus “instant_kind survives close”
is contradicted by the source. Close nevertheless retained pending parameters,
programmed/time/instant fields and much runtime state; creation initialized
only selected members. Delayed-slot flags in the HAL also lacked an explicit
connection reset. These omissions are removed. Importantly, console conn_event
is copied from hardware F_EVT_COUNT; software anchor projection cannot itself
make that hardware counter run at 1.89 ms/event. The supplied single next-link
dump and HCI capture cannot establish whether anchor-latch behavior, stale
hardware state, measurement provenance or another cause produced that rate.
The existing stop already clears all 17 anchor fields and the frame after
quiescence, and CONNECT_IND writes event counters and A_INSTANT to zero.
New exhaustive software resets and reconnect tests remove software carryover,
but RF verification of the physical rate remains an owner bench requirement.

## Implementation

- Gate pre-first-anchor synthetic closes only while no real close has been
  served, in both HAL and LL. Ignore duplicate/stale close notifications so
  they cannot project a future update anchor backwards.
- Register one procedure atomically using the current closed-event phase and
  r7's 1-ms guard. Preserve receipt event through authentication/deferred
  dispatch. Hold latency disabled through an instant procedure.
- Keep far instants as event numbers. Rebase a bounded absolute deadline when
  within the signed tick horizon. Event skips beyond I-1 and an unprogrammed
  deadline crossing the guard close with 28. RX counter progress strictly beyond
  I is a fallback for far instants with lost close IRQs and delayed anchors;
  equality is left to the phase/guard test. No silent pending state remains.
- At I-1 preserve r7's hardware write order and encodings. The nominal instant
  is anchor(I-1) + oldInterval; the new window starts there + WinOffset. Keep
  old software parameters until the instant; then publish the new ones and
  Update Complete, retire the complete pending record and restore priority.
  Supervision resets remain armed at the nominal instant and window start;
  expiry wins ties. A delayed old-event packet cannot consume the new window
  switch or move its future anchor backwards.
- Every close uses the existing stop/settle/quiesce/reclaim/report lifecycle,
  cancelling pending/timing state under refill exclusion. New creation clears
  all connection-local runtime, deferred queue indexes and HAL delayed-slot
  flags. Lifetime diagnostics and epoch are separate; Reset still clears them
  using bounded slices, after stop, with no old completion afterward.
- Console pending_proc reports kind (0 none/1 update/2 map), instant, receipt
  event, programmed flag, interval/latency/timeout/window and map. Lifetime
  ignore counters and four update attempt records survive ordinary reconnect
  and clear on HCI Reset. Records carry serial and epoch; ring slots are not
  chronological—sort serials. A flood can evict an active attempt from the
  four-entry history; its independent pending record remains authoritative.

Ignore reason IDs: 1 length, 2 invalid parameters, 3 invalid map, 4 pending
instant, 5 wrong role, 6 unexpected/log-only, 7 repeated version, 8 reserved
LLID/empty control (opcode 255 unavailable), 9 encryption already active.
`control_last_ignored.event` is the event at receipt, not worker processing.
Update outcomes: 0 empty, 1 pending, 2 programmed, 3 applied, 4 invalid ignored,
5 busy ignored, 6 instant passed (28), 7 cancelled by close (reason recorded).
Units are 1.25 ms for interval/window/offset and 10 ms for timeout.

## Verification and its limits

Full S1 (both options), S2, S3 and S3.1 suites run with ASan/UBSan and warnings
as errors. The inherited TinyCrypt AES shift-base exception remains; upstream
files are unchanged. Added tests run on the existing virtual radio in S2,
S3 and S3.1:

- 48 combinations: 15->7.5, 15->30, 7.5->15, 30->7.5 ms, offset 0/mid/max,
  size 1/legal max, near/far instant, tick wrap. Each is followed immediately
  by another update: 96 successful updates per suite, plus 12 real-CCM
  encrypted matrix cases in S3/S3.1.
- Observe port programming at precisely I-1, retain old parameters until the
  instant, check positive offset geometry, restore the steady window, and
  verify one HCI completion. Far cases span 123–492 seconds.
- New-interval invalid limits, all basic parameter bounds, length/busy reasons,
  skipped I-1, missing-IRQ worker guard, delayed-RX far-instant progress, late update/map and last-legal I-1
  receipt, duplicate closes, and the existing event-counter-wrap map test.
- Pending map/update and programmed update closed for seven reasons, including
  supervision loss; reconnect with clean epoch/queues/timing and 100 events
  at the fresh 15-ms grid. A programmed case injects an old deadline just
  before the reset to test expiry ordering. Native simulation does not create
  that deadline by an RF loss. A 600-second session serves every event across
  several tick wraps and an update after the first 89.478-second boundary.
- Existing real NimBLE legacy/SC/RPA/repeat-pairing, Reset interruptions,
  encryption/barrier/MIC, bond/privacy/timing and ten-minute suites remain.
  Two command-test helpers now allow 64 passes for the larger Reset clear,
  retaining the 128-byte production slice and original timing thresholds.

The port model records requested parameters/time/window. It does NOT model
A_INTERVAL/A_WIN_OFFSET/A_INSTANT latch timing, actual window edges, RF anchor
synchronization, F_EVT_COUNT interpretation on silicon, or real executor/IRQ
latency. r7 A1/A7/B22 uncertainty is unchanged. No invented register rewrite
is presented as a proven hardware cure. Run the owner script and retain the
new diagnostics; a common-clock LL sniffer remains necessary for those fields.
