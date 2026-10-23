# R6 investigation and diagnostics

The MIC failure with central LE Ping is **not reproduced or fixed**. Do not
promote the zero-failure simulation into a claim about silicon reliability.
The fm1-base deliverable `out/open-ble/RESULTS-R6.md` describes the path audit,
model assumptions, evidence gaps and proposed r9 probes in full.

Both implementations retain the existing advertised LE Encryption feature
only. Unknown ping requests still receive LL_UNKNOWN_RSP. R5's resolving-list
capacity remains ten; this Felucca host's bond journal remains at two peers.

Diagnostics add a 16-active-event, 6-row-per-event ring in Felucca (fm1-base
uses 32 events and 64 rows). RX includes empties,
duplicates, CRC outcomes, slot/header/status and the counter actually used by
CCM. TX rows are prepare/load/ACK and descriptor snapshots, with logical buffer
id, assigned counter and raw slot control/toggle. The first local 0x3D or 0x08
freezes it through reconnect and HCI Reset until power-off. Silent closes do
not erase the exchanges before a supervision timeout. The CDC BLE status
formatter prints it, with no payload or key material.

The documented hardware interface cannot supply a per-air-TX stream, actual TX
NESN or a reliable new/repeat classification. These fields are explicitly
unknown; descriptor headers in empty slots can be stale. Full-slot refused
receptions have no descriptor. Per-event overflow is counted. This partially
addresses the requested diagnostics; it does not claim unobservable packets.

The independent central now uses its own ARQ and counters, a separate TinyCrypt
AES-based CCM oracle, an authenticated-payload ping timer, ATT writes, CRC errors,
packet/ACK loss and delayed slot service. It checks both directions and exact
ATT delivery. `tests/run_ble_s3_tests.py --mic-stress` runs 10 M ping-model events
(5 M at 15 ms, 5 M at 30 ms), then the inherited 10 M-event stress and directed
corruption, DMA, encryption transition, ring-freeze and formatter tests. Logs:
`out/ble-r6-regression-final-tests.log` and
`out/ble-r6-mic-small-final-tests.log`. The final S3.1 target build also passes
(`out/ble-r6-build-final.log`): 8,332 bytes of pool headroom remain; the 8,192-byte
gate was retained. CDC readout uses a 160-byte stack event snapshot. No physical device was accessed and no
commit was created. Hardware acceptance still starts with the owner ping test.
