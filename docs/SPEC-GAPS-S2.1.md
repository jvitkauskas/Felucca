<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S2.1 decisions and remaining r4 qualification

The frozen r4 snapshot supersedes the r3 software contracts used in S2.
Historical S1/S2 reports are retained as historical reports.

1. **Executor trigger — 04 §0.8 / PB4.2.** Select candidate (ii), dedicated
   TIMER0/IRQ4, with software-requested 1-us expiry and a 1-ms autonomous
   backstop. S2 proved this register/IRQ path at level 4 (max gap 1046 us,
   pass 59 us). The ILAT mapping remains unknown, so implementing either
   hypothesis without PB4.2 would add an unnecessary hardware dependency.
   The new build uses normative level 2; retain the S2 level-4 result as the
   documented fallback only if the parent's level-2 dispatch test fails.
   Exactly-once startup self-test, pass/dispatch/refill counters, and the
   continuous-request guard make the choice reviewable. Physical G, entry
   cost, stack margin and worst-phase cumulative masking remain unmeasured
   for the new level. A failed dispatch self-test disables radio operation;
   if all executor mechanisms fail, asynchronous command delivery cannot be
   guaranteed either. That startup failure-response case needs qualification;
   the implementation does not run NimBLE or controller pumps in an ISR wait.

2. **Host transport ring — 04 §10.5 alternative.** A 32-entry ring keeps all
   NimBLE pool operations in main. The semaphore wait drains only this ring
   and feeds the watchdog. Delivery deadlines are measured to the ring.
   Overflow is counted; this is not an unlimited queue for a central flooding
   a stalled host. S3's pairing/ECC fixture must exercise independent executor
   progress and the actual host buffering limits.

3. **Delayed RX — 02 §4.2(4).** Per-slot t_free is the proved lower bound,
   with max against the older packet of the service. No previous-worker or
   previous-event interval is invented. The earlier-supervision price of
   delayed metadata and P3-4/P3-7 hardware attribution questions remain.
   r4's armed reset/expiry precedence replaces the earlier future assignment;
   delayed evaluation, wrap and tie vectors are software-tested.

4. **Unchanged physical questions.** A1, A3, A6–A8 and P3-3–P3-7 remain the
   r4 nominal/conservative choices. S2's desktop success does not settle
   adversarial coalescing, encryption boundaries, instant latching or sniffer
   geometry. The parent's central update and Android tests remain outstanding.
   No r3 snapshot or forbidden reconstruction input was reopened for this task.
