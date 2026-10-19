<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S1 specification gaps and implementation decisions

Inputs are the frozen snapshots at `/root/wl82-ble-spec-snapshots/r2/` and
`/root/wl82-ble-spec-snapshots/r2b/`, plus the permitted review responses. The live specification was not consulted.
These are S1 issues; the unimplemented S2/S3 procedures are outside this report.
No hardware test was performed. “Verify” below means work for the device owner.

1. **VM log selection and preservation — 04 §7, §0.9; PB2.1.**
   The active-half magic/selection and compaction format are omitted. Felucca also
   owns much of the nominal VM range after installation. The reader therefore
   accepts only the preserved first sector, `0x93000..0x93fff`, with the printed
   magic, and BTIF `0xe9000..0xe9fff`. It never hunts through Felucca's store for
   apparent calibration records. It keeps the last record with a valid outer
   CRC, skips a bad payload CRC when the length is bounded, and stops on a
   malformed/torn length. Missing records use RAM defaults/self-calibration.
   Verify the first-sector records and `vm_status`, `btif_status`,
   `records_present` on the unit. A compacted/moved log needs a new specification
   of active-half selection; this implementation does not claim to parse it.

2. **Identity fallback provisioning — 04 §7.1 and §8.1.**
   No stable unique fallback identity source is supplied for Felucca when item
   102 is absent. S1 implements 2005 and accepts a user-provisioned unique static
   random address from the build option or console. It does not invent a public
   address or use the predictable hardware LFSR as secure randomness. With no
   usable identity the internal driver fails with 0x12 and leaves advertising
   off. Verify public derivation with K-05, stability over power cycles, or assign
   a unique static random address and verify its address-type bit on air.

3. **Stage-specific capability reports — 00 §1.1 versus 03 §§1,3.4 and 05 H-08/K-07.**
   The HCI tables describe the full connection/encryption profile, even for tests
   listed under S1. S1 reports zero LE feature octets, supported states `05 00…`,
   and only its implemented commands. Read Buffer Size retains the specified
   27/5 shape; ACL intake is rejected because S1 creates no link. H-08's `01…`
   and the full-profile mandatory bitmap are deliberately not claimed. Verify
   H-02..H-09/K-07 against this stage's documented bitmap; add the S2/S3 bits
   only with the corresponding implementation.

4. **Clock and interrupt platform facts — r2b 04 §0.1/§0.4/§0.6;
   02 §7.3a; PB1.1/PB3.2; R2-N02/R2-B09.**
   The old eight-cycle extrapolation from this compiler's loop was removed.
   A 100,000-iteration sample now reports only iterations/us ×1000. The cycle
   cost of a RAM loop is not established, so true core Hz remains unmeasured;
   `ble rate` repeats throughput sampling under audio/USB load. All radio waits
   use TIMER4 microseconds independently, with fixed 10 us for loop10/100/240.
   The latter exceeds the measured 5.3 us loop240 wait; verify full bring-up.
   Startup now follows P1/P2/P3/P4/P5/P6 call positions, with P4 immediately
   after P5 and instance opening only in P6. The nested 250 us TIMER5 probe
   refuses RF if the mask fails. Both masked-attached BLE IRQs use level 5.
   RX body/inline RX, event work after step 1a, and outer controller critical
   sections have separate 380/150/390 us diagnostics. They exclude assembly
   entry/exit and some accounting overhead; the BSP/cumulative 390 us limit in
   every 920 us window is not instrumented or claimed proven. Verify those
   bounds, same-level arbitration, wrapper overhead, and audio statistics on
   hardware; F-16 padding/full bursts belong to S2.
   Residual wording conflict: 04 §0.1 says every other line stays masked until
   P5, but the actual `audio_init()` attaches/enables line 11 before P3, and
   `timer5_start()` attaches/enables 63 before P5; P1 can have executed `sti`.
   The implementation assumes no global state and protects its own attachment;
   it does not change the BSP's existing line-enabling policy. Log actual ICFG
   fields at these phases if stronger whole-BSP boot ordering is required.

5. **PMU entry state — 04 §0.5; 01 §2; PB2.1.**
   The radio supply values under the Felucca boot chain are unknown. S1 records
   all 38 requested indices before and after startup; it makes no regulator
   writes. The reference values are 01:20, 74:C0, 75:07; index 17 differs because
   Felucca deliberately uses a plain-reset watchdog. A difference alone does not
   establish that a regulator needs changing. Verify the snapshots and RF
   operation before proposing the three narrowly permitted regulator writes.

6. **DMA directions/placement — 02 §3.6; 04 §0.2/§0.3; P3-2/PB2.3.**
   Isolated transmit visibility, adjacent control-byte writes and the modem table
   at this Felucca address were not measured. S1 uses one dedicated aligned
   `.ble_dma` reservation in the verified SRAM class, exact access widths,
   `csync` publication and `ssync` reads. It keeps fixed RX buffers and copies
   only the scanner address in the ISR. Trailing DMA canaries detect overruns,
   not stale reads or all ownership races. Verify actual advertising and scan
   response payloads, channels, canaries and repeated enable/disable on hardware.

7. **Calibration completion and numeric edge cases — 01 §3.4/§4.6/§4.7.**
   Self-calibration has no physical success/range criterion. Its DC-measurement
   busy timeout is unspecified; S1 chooses 1 ms per measurement, matching the
   generous radio-port bound, and fails closed on timeout. A VCO scan with no
   middle-bank transition is rejected instead of using the described degenerate
   table. TX gain search says the best correction is stored as signed eight
   bits while a separate sign flag is saved; S1 saves that flag from the winning
   original candidate and reapplies the truncated correction. Other signed phase
   values retain their specified 10-bit patterns. Results are read back into a
   RAM record with CRC, never written to flash. On failed bring-up, before any
   BB instance opens, S1 makes a best-effort request for the specified manual
   all-off mode (WLA_CON20=0, mode 5). Physical RF silence after a failed port
   is not established; power-cycle the unit on an error. Verify K-05's quick path first;
   on a unit genuinely missing/holding an invalid trim, compare the RAM result,
   bring-up duration and RF function. The printed records are test vectors only.

8. **Nonconnectable autonomous behavior and immediate reopen — 02 §5.3.4/§5.9;
   A6/P3-6; 05 S-12.**
   F_MODE bit 8 scan-response suppression and hardware behavior after an ignored
   CONNECT_IND are inferred. Immediate reopen after bounded stop is unmeasured.
   S1 always keeps advDelay enabled and restarts either advertising profile after
   any CONNECT_IND report: stop new anchors, poll busy in the worker, mask/ACK,
   mark closing under the refill lock, clear anchors/frame only after quiescence,
   then reopen. Disable/Reset CC follows reclamation; handlers discard closing
   packets without publishing. It emits no Connection Complete. A busy timeout
   quarantines the complete DMA reservation until power cycle. Verify S-12,
   targeted connection attempts, and repeated Reset/disable/re-enable. There is
   no automatic reduced recipe, delay omission or undocumented abort-bit use.

9. **Filter policy — 02 §3.4.3 and 03 §3.5.**
   Only policy 0 has the required implementation/profile. The error table lists
   policies 0..3 as syntactically valid without supplying a list-management
   contract. S1 returns 0x11 for 1..3 and 0x12 for reserved values. Verify H-12
   and that policy 0 remains unrestricted for addressed SCAN_REQ packets.

10. **TX power in dBm — 01 §8.1, 03 §1.1/§3.2.**
    No absolute calibration is supplied. By default 2007 is unknown (0x01) and
    its support bit is clear. An integrator can supply a measured dBm value for
    level 6; that optional build returns Hardware Failure if another level is
    selected, rather than returning the level-6 measurement as its power. The
    0..11 bench control is a hardware index, not dBm. Verify S-11 using a fixed
    placement and calibrated measurement; do not infer absolute output power
    from scanner RSSI. Item 615's specified default 6 is used; S1 has no parser
    for unspecified default-configuration storage.

11. **Reset versus accepted command replies — 03 §2.2 versus §2.4.**
    “Every accepted command gets one reply” conflicts with discarding commands
    queued behind Reset. S1 cancels their effects but subsequently answers each
    with Command Disallowed. Reset CC follows DMA quiescence. The trace and boot
    platform measurements survive Reset; LL/IRQ event counters reset. The
   transport's <255-octet limit takes precedence over the queue paragraph's
   unreachable 258-octet payload wording: each slot accepts at most 254 octets.
   Host/trace
    diagnostic totals are lifetime observations. Verify the flood/Reset fixture
    in the host test and repeat it through the actual H4 entry on a bench build.

12. **First-air-event marker — 02 §5.3.1, 05 §2.4/S-01.**
    The test's CC marker can be delayed by a pending audio ISR after the anchor
    was armed; the 1 ms transport trace cannot resolve a 625 us first anchor.
    S1 records raw TIMER4 `enable_ticks` immediately before the arming port write
    and the first *event-close* timestamp separately. Neither event close nor CC
    is mislabeled as first RF transmission. Verify the first 625 us with a
    common DUT/sniffer timing reference or a separately specified GPIO marker;
    no unassigned pin is guessed here. Period and channel-spacing tests need
    only RF timestamps and can run independently.

13. **Worker/flash blackout bounds — 04 §0.8/§0.9; PB2.4/PB4.1.**
    Felucca's UI, storage and CDC output can hold the worker beyond 7.5 ms;
    flash blackouts can also lose advertising events. The specification permits
    that in advertising. S1 keeps ISR buffers static and the worker nonblocking,
    reports its largest gap, and primes the finite startup command sequence
    before the first long UI redraw. It does not claim the S2 worker/link bound.
    Verify audio/USB coexistence, display activity, settings saves and OTA on the
    bench; reassess flash exclusion and worker scheduling before enabling links.

14. **Table redistribution decision — 01 §0/appendix A.0; 04 §0.12; review N21.**
    The specification gives the origin and numeric values but explicitly does
    not decide redistribution rights for these datasets. The requested full
    recipe uses those facts transcribed from the permitted snapshot; no vendor
    program/library/binary was consulted or incorporated into the application.
    New implementation source carries GPL-3.0-only. The project's unresolved
    table redistribution decision is recorded in `LICENSING.md`; compilation and
    clean-room access compliance do not decide it. This is a project/provenance
    question, not a test that device measurements can resolve.

15. **Radio-error Reset retry — r2b 01 §3.4 versus its stated editorial-only scope;
    02 §5.9.**
    The r2b diff additionally says HCI Reset repeats the whole bring-up after
    an aborted radio port operation, while explicitly saying safe repetition is
    unknown. This is a functional addition to part 01, although the update was
    described as editorial-only. S1 implements the closing lifecycle for Reset
    but retains the latched radio failure; it does not retry physical bring-up
    after an abort or touch quarantined DMA. Subsequent enables report 0x03.
    Recovery is a power cycle. A mid-recipe retry needs an independently
    established reset/re-entry recipe and device test before it can be claimed.

r2b 04 §6.6's 80-byte advertising TX allocation matches S1's existing two
40-byte buffers. Its 416-byte connection allocation and persistent entropy seed
services apply to later stages; no seed sectors or flash-write services were
added in S1. The frozen r2b snapshot resolves the old global-disable, masked-attach
and event-budget ambiguities; their remaining physical checks are item 4 above.
