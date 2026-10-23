<!-- SPDX-License-Identifier: MIT -->
# Round 5: r8 resolving-list capacity

Uncommitted above `88d426f` on `ble-controller`. The shared controller now has
**N_RL=10**. It keeps R4's resumable one-entry / at most one AES block per
resolution item, before connection-event formatting. Reset clears the larger
list in 128-byte executor slices; a full synchronous clear would exceed the
existing pessimistic Reset test's 70-us target. Other controller timing,
encryption, MIC journal and host security policy are unchanged.

This deployment keeps **N_BOND=2** in its original sector journal, explicitly
allowed by r8 04 section 7 item 5. It does not receive fm1-base's compact A/B
store, persistent eight-peer LRU, or R3/R4 Apple host changes.

ASan/UBSan: S1, S2, S3, S3.1 and the 10-million-event MIC suite. Logs are
`out/ble-r5-{s1,s2,s3,s31,mic}-tests.log`. H-22/H-23 now fill all ten entries,
then test overflow, duplicate identity/IRK and absent entries. Full resolution
checks miss and last-entry match: 9 blocks with a local zero-IRK entry,
10 without it, across 10 items, with no item
performing more than one block, event deferred, modeled maximum 1260 ticks
(52.5 us) with a deliberately pessimistic 52-us/block cost. Actual HAL's whole
AES transaction has a shared 26-us deadline; the model is not a device timing
measurement. The R4 355-tick measured resolution maximum already applied to
one-entry work: nine such items batched could take 3195 ticks (133.1 us).

The patch is exported by the fm1-base deliverable at
`out/open-ble/R5-felucca.patch`; no devices or bench hosts were contacted, and
no flashing, commits or pushes were performed.
