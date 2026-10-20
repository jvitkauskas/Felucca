<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S3 specification gaps and implementation decisions

Only frozen r4, the permitted reviews and open NimBLE/platform sources were
used. The r3 and earlier snapshots were not reopened. Decisions below are
reviewable choices or unmeasured qualifications; they are not new hardware facts.

1. **Persistent bonds — 03 §1.3, 04 §0.9; integration extension.** r4's
   profile says bonds in RAM and specifies no persistent format, reservation,
   replacement policy or crash consistency. The owner expressly authorized a
   persistent store if it fits. S3 reserves 0x96000 for a CRC append journal,
   with two bond peers and eight canonical security/CCCD RAM records. Dirty
   state flushes only after disconnect, before advertising, with an idle radio
   and atomic no-link guard. Repeated values cause no write. One-sector
   compaction can lose bonds on a power cut, and an immediate reboot before
   deferred flush loses the latest session. Recovery requires forgetting the
   bond on the central and pairing again. The two seed sectors remain
   independent. The 0x96000 sector is outside all existing Felucca allocations,
   but its vendor-store contents and compatibility with restoring vendor
   firmware have not been inspected. It becomes reserved under S3; its final
   256 bytes remain unused to avoid update-loader records. This extension
   needs owner review alongside the parent bench, not a claim that r4 defines it.

2. **Entropy qualification — 04 §8.1, F-20/F-23/H-15.** The record format,
   append/wear/selection rules and readiness classes are implemented exactly.
   No physical source is credited. No Felucca SRAM window is qualified, so
   optional power-on RAM is omitted. The chosen generator is a documented
   SHA-256 counter/rekey construction, with independent successor-seed and
   generator-key domains and startup known-answer tests. FIRST_BOOT and an
   unprovisioned CHAINED state are functional readiness, not proof of secret
   randomness. Provisioning is outside this task. Public address, R64,
   completion times and event/RSSI inputs cannot justify a stronger class.
   Statistical/health qualification on the 24-MHz target time base remains
   R3-C1/C2; no credited-source health tests are claimed applicable here.

3. **Flash readiness and physical timing — 04 §8.1, PB2.4/F-23(e).** Both
   seed services and the underlying S3 RAM driver keep XIP/interrupts disabled
   until ready; after 2 s erase / 50 ms program without ready, they reset from
   RAM rather than return into busy NOR. Actual latency/endurance and whether
   ROM/loader can start while NOR remains busy are unknown. The software NOR
   model exercises guard/refusal/not-issued/late/mismatch outcomes, torn
   records and wear rotation. It does not execute the target RAM assembly or
   prove the stuck-busy reset electrically. That BSP fault-injection check
   remains outstanding; never test it on calibration or user sectors. Idle
   bond flushes silence audio while XIP is unavailable and can make the
   all-state TIMER0 gap large; the link-only worker diagnostic is the G metric.

4. **Executor and budgets — 04 §0.8/§10.5, PB4.2/PB3.1/PB5/F-21.** Retain
   the timer alternative and normative priority 2 from S2.1, with priority 4
   only the documented measured fallback. Native pairing really executes
   NimBLE software ECC and synchronous LE Rand through HCI. Its virtual-time
   radio model is single-threaded: it proves protocol/credit behavior, not
   real IRQ preemption during ECC. Target dispatch/pass/refill maxima, stack
   margin, ECC wall time and cumulative masking need the parent bench. The
   26-us hardware-AES deadline and 60-us refill bound are instrumented/bounded
   choices; successful software AES vectors do not measure the physical port.
   Fresh-boot AES clock/reset readiness at P2 is unmeasured; the boot KAT uses
   the stated r4 register sequence and no guessed extra clock gate.
   Finite H4/host queues count exhaustion; a central flooding an indefinitely
   stalled main loop is not claimed lossless. Existing native R-22 command
   stress uses the controller's direct test receiver, not a stalled NimBLE
   consumer of the 32-entry transport ring.

5. **Epoch attribution — 02 §8.5.1, P3-7/R-14/R-24.** Boundary ACK visibility
   in its own RX step and the ability of a boundary to fall between coalesced
   slots remain hardware assumptions. The exact r4 conservative length rule
   is used, older first; reserved marker capacity prevents partial handover.
   FIFO tests prove on/off/reject ordering, a delayed allowed S1 control before
   RX_ON, positive refresh and an encrypted terminate before RX_OFF. They
   cannot prove the frame/ACK metadata. A scripted central and sniffer must
   resolve P3-7 before adversarial coalescing is called qualified.

6. **MIC loss — D-1, 02 §5.9 / R-13/R-26.** Every nonempty encrypted frame,
   including a clear-looking terminate, authenticates or closes with 0x3D.
   The authentication decision and closing flag share refill exclusion.
   Hardware acknowledgement before software validation and the unbounded
   tail of the current event remain D-1, not silent-drop compatibility.
   Measure reception-to-detection and detection-to-last-air-packet separately;
   do not turn the three historical stop observations into a 3-ms guarantee.

7. **Host buffers and phone policy — 03 §1.3.** The profile fixes HCI 27/5
   but not the physical transport mbuf capacity needed by NimBLE's 64-byte
   SMP public-key pullup. S3 uses 72-byte physical buffers, retaining 24 LL
   buffers, 27-byte HCI fragments and five controller credits. Just Works,
   bonding and legacy fallback are enabled; MIDI permissions remain open.
   A Security Request after 100 ms asks cooperating centrals to encrypt.
   Address privacy/resolution remains disabled as the pinned profile requires;
   a phone reconnecting from a different RPA may need fresh pairing despite
   persistent key storage. Android cache/address/encryption behavior and the
   desktop plugin's bond recovery must be measured. Two peers' normal
   same-identity reconnects are the implemented persistence contract.

8. **Unchanged radio unknowns.** A1/A3/A6–A8, P3-3/P3-4/P3-6, receive-window
   geometry, missed-event corrections and instant latching keep the r4
   nominal/conservative tunables in ble_link.h. S2's excellent desktop
   results do not settle their sniffer cases. No new measurement is invented.
