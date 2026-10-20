<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S3.1 r5 review and implementation choices

Reviewed before implementation, 2026-10-20. Normative input is exclusively frozen
r5. Core references below are Bluetooth Core 5.4, Vol 4 Part E unless stated.
Core text: https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host-controller-interface/host-controller-interface-functional-specification.html
The downloaded public text and HTML are retained in out/s3.1-core54-hci.*.

1. **Legacy event types (03 §3.6, H-25).** §7.7.65.1 reserves all types except
   00/01; resolved types 02/03 belong to §7.7.65.10. Use r5's explicit fallback:
   legacy event reports the resolved identity as 00/01, Enhanced reports 02/03
   with peer RPA and zero local RPA. NimBLE ble_hs_conn.c accepts either identity
   representation. Legacy loses OTA address provenance; it still finds the LTK.
2. **Duplicate add (03 §3.6, H-23).** §7.8.38 allows success or rejection for
   identical four-parameter entries, recommends 12 for a duplicate identity OR
   duplicate non-zero peer IRK. Reject all these cases with 12, before capacity
   checking (including a full list). All-zero IRKs may repeat, never resolve.
   Validate random identities as static, with valid random portion. r5 omits the
   duplicate-IRK case; it is implemented and tested.
3. **Privacy mode errors and races.** §7.8.77 confirms absent entry = 02,
   reserved mode = 12, default network mode. It also has the same busy-state
   restriction as Add/Remove/Clear; r5's command row omits it. Apply it to 204E
   too. Mutation with resolution disabled is allowed while advertising, as Core
   permits. The blanket ban in r5 item 4 is read with its command table.
4. **Connection creation (03 §3.6).** Core's pending create-connection rule is
   expressed in central HCI terms; this peripheral has no such command. To
   preserve r5's acceptance-to-event consistency, treat created_pending as busy
   for 202D, and for 2027/28/29/204E when resolution is enabled. Guard state check
   and mutation together against the acceptance ISR. Established links do not
   freeze the list; NimBLE adds the bond during its current connection.
5. **Network mode and disabled resolution.** Enforce identity rejection only
   while resolution is enabled and the peer IRK is non-zero. The interrupt does
   bounded address comparisons only. AES resolution belongs to the executor.
   Filter policy zero still accepts unmatched RPAs. Local privacy remains D-6;
   do not advertise the full LL Privacy feature or a full Core conformance claim.
6. **N_RL work bound.** Four entries can all have non-zero peer IRKs; the
   claimed N_RL-1 AES maximum assumes NimBLE's zero-IRK local entry. Implement a
   safe N_RL bound for arbitrary HCI clients, one entry per executor work item,
   preserving list order. AES failure ends resolution as unresolved and counts
   a failure. Startup K-11 failure fails the existing crypto/entropy readiness.
7. **Host identity and request policy.** The local IRK is domain-separated
   SHA-256 of the public identity, truncated to 128 bits and forced non-zero.
   This is stable and deliberately public under r5's own-public-address profile;
   it is not a secret IRK for future local privacy. Existing bonds need no new
   LTK. Query the pinned host's ble_sm_num_procs (one-link profile) plus LL phase
   and live encrypted state before Security Request. EALREADY is a benign race.
   No vendor host code is changed. r5's 'every synchronisation' clear sequence
   is conditional on a changed IRK in NimBLE; restore_irks still re-adds peers.
8. **Bond keys and migration.** Canonical keys accept public or valid random
   static identities, including normalized NimBLE identity types 02/03. Refuse
   all private random addresses, not only RPAs: non-resolvable random addresses
   are also not identities. Count refusal; old private-address records are
   excluded at load and removed by later compaction. CCCD may temporarily exist
   without security during pairing; compaction drops an orphan. Net-state
   equality is against the durable per-slot baseline, including delete/recreate
   and slot reuse; failed writes never advance that baseline.
9. **Measurement.** Dispatch stamp and entry read are atomic and signed;
   negative samples count an error and never update maximum. A shared nesting
   depth accounts higher-priority handlers once. Fatal exceptions never return,
   so no executor pass completes after one. Own time governs yield decisions;
   individual RX/control/command/delivery/service items report W_item, including
   failed items. 250 us is the stop-before-next-item threshold; the pass ceiling
   is 250 us + W_item, W_item <=100 us. Wall maxima remain informative. The
   old F-16/K-07/H-18 wording in part 05 is stale where it conflicts with explicit
   r5 F-27, supported bitmap and conditional Enhanced event requirements.
10. **Refill and build limits.** Bound is now 80 us, summed CCM and remainder
    measured per refill. These native tests prove arithmetic, not physical
    deadlines. Four-entry privacy, baseline bond cache and diagnostics must fit
    the existing memory budget. S3.1 uses FELUCCA_BLE_STAGE=3 plus
    FELUCCA_BLE_S31=1; historical stage flags remain supported.
11. **Still open.** A9 disconnect provenance, B23 Android legacy selection,
    B24 slow consent timeout require phone/sniffer evidence. This work does not
    claim to fix them. D-1/D-6, preemption instrumentation overhead, IRQ/RF
    timings, stack fill, flash crash/endurance and prior radio unknowns remain
    hardware qualifications. F-25's 40 unchanged sessions cannot force wear:
    use retained S3 journal history or intentional net changes to force the
    first compaction; unchanged subscribe/unsubscribe sessions must not write.

12. **Confirmed NimBLE Reset cache gap (F-24).** The actual host-backed RPA
    simulation initially failed after HCI Reset: one restored peer entry,
    resolution disabled. `ble_hs_pvcy_set_our_irk` does nothing for an unchanged
    IRK; its reset function is called by host stop, not every HCI error reset.
    The application's startup IRK callback now calls the pinned host's
    `ble_hs_pvcy_reset()` before returning the same derived IRK. Startup then
    emits disable/clear/enable/local entry and restores peer entries normally.
    The vendor source stays byte-identical. This is required beyond r5's stable
    non-zero IRK alone, and the real NimBLE Reset/RPA reconnect regression passes.

13. **Existing 2006 discrepancy.** 03 §3.6 says own types 02/03 return 12,
    whereas §3.5 and Core classify valid unsupported values as 11. Retain the
    existing 11 behavior and regression tests. D-6 still excludes local RPAs.
    The historical journal uses 30 records plus an untouched 256-byte tail;
    05's references to 32 slots are descriptive bench shorthand, not a format
    change. No on-device data was read during this task.
