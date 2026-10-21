<!-- SPDX-License-Identifier: MIT -->
# Felucca BLE S3 — frozen specification r4

Uncommitted implementation above the owner's device-passed S2 commit 3581aba.
S2.1 is supplied separately for regression before encryption. No flashing,
device, USB or network access was performed. The owner's S2 measurements remain
historical evidence; none of the new S3 hardware timing is claimed measured.

## Implementation and module map

| Module | Contract / resulting behavior |
| --- | --- |
| `ble_link.c`, `ble_security.inc`, `ble_security.h` | r4 02 §8.5 and 03 §8: peripheral encryption start, pause, restart and negative LTK reply, full phase table, reserved procedure/terminate slots, ACL/control barriers, separate Encryption Change and Key Refresh pending bits |
| `hal/fm1_ble.h` | 02 §4.2/§4.3/§8.5.1: snapshot two owned RX slots, boundary ACK observed before snapshot, ordered markers using the coalesced length rule; boundary completions excluded from generic refill; ciphertext retained until ACK |
| `ble_crypto.c`, `hal/fm1_ble_crypto.h` | 02 §8.2–§8.6, 04 §8.2–§8.3: hardware AES, Core byte orders, CCM with 13-byte nonce, LLID AAD, four-byte MIC and 39-bit counters |
| `ble_controller.c` | 03 §1.3/§3.4/§8: truthful S3 encryption feature and Encrypt/Rand/LTK command bitmap, CC before key installation/procedure start; masked LTK request gets an internal negative reply |
| `ble_seed.c`, `ble_seed.h` | 04 §8.1: boot seed journal, SHA-256 conditioning / counter generator, readiness classes, LE Rand and SKD/IV service, zero-credit event/RSSI input |
| `hal/fm1_flash.h`, `src/ble_platform.c` | 04 §0.9/§8.1: reserved seed/bond ranges, main-only idle flash services, bounded RAM-only ready polling and reset while still busy; no erase/program while connected |
| `ble_host.c`, `ble_npl.c`, `port/felucca_nimble_config.h` | 03 §1.3 and 04 §10.5: NimBLE 1.9.0 host, SC Just Works plus legacy fallback, main-only software ECC/SMP and NPL; in-memory H4 rings |
| `ble_bonds.c` | Integration extension: two peers' security material and CCCDs in RAM, deferred journal flush after disconnect |
| `ble_console.c`, `console.c` | Encryption phase/counters, pairing method/status/key size, MIC failures, Rand and seed state, bond state; console greeting records boot seed class/origin/persistence/sequence/credit |

The LL executor remains dedicated TIMER0/IRQ4 at priority 2, as in S2.1.
Software requests a 1-us expiry; the timer has a 1-ms autonomous backstop,
plus the platform tick's 10-ms request. All NimBLE code runs in main; ISR
`to_host` only copies into the controller-owned ring. See BLE-S2.1.md for the
exactly-once self-test, 250-us pass budget and starvation guard. S2's measured
priority-4 gap of 1046 us does not qualify priority 2 or encrypted operation.

Receive crypto state changes only when the executor reaches a FIFO marker.
The marker and PDUs retain a link epoch. A class-C repetition never consumes
another counter. Two coalesced packets use r4's length rule; no invented SN/NESN
metadata is used. TX CCM runs once when a queued PDU enters a DMA slot, so a
retransmission has identical ciphertext/MIC. The last TX nonce is reserved for
terminate; RX exhaustion and every invalid MIC close with reason 0x3D.

MIC validation and the closing flag share refill exclusion, including the AES
unlock, so nothing new can load after detection. This implements D-1: the
hardware may already have acknowledged a bad frame and can finish the current
event. No upper bound on that RF tail is asserted. The asynchronous stop either
reaches quiescence before reporting Disconnection Complete, or quarantines the
DMA allocation and latches failure at its existing bound. It never reuses a
busy frame. AES transactions have a 26-us deadline and nested IRQ save/restore;
a port deadline latches hardware error 11. Refill's required bound remains 60 us.

## Pairing and MIDI permissions

NimBLE is pinned to da7e3256da3ba80b232df880f40d8359311cc62e (1.9.0).
SC and legacy are enabled, SM level 2, runtime SC/bonding on, MITM off,
NoInputNoOutput, no debug keys/TRNG, peripheral only. Its generated-system-init
ECC RNG hook is installed explicitly by the bare-metal port. ECC, AES-CMAC and
SMP AES run in host software. The controller supplies LE Rand; no HCI P-256 or
DHKey command is advertised. Privacy remains off through the zero local IRK
callback; ENC and identity key distribution are permitted, signing is off.

The application sends one Security Request 100 ms after connection. A central
can pair/encrypt, or continue using open MIDI when it ignores that request.
The MIDI characteristic stays read (empty), write without response and notify,
with no encryption permission flags. This preserves S2 interoperability and
allows apps which expect encryption to negotiate it. An encryption attempt
with an invalid key still terminates as the phase table requires. Automatic
repair of a stale bond permits a new Just Works pairing; this has no MITM
protection by design. Peer address resolution/RPA reconnect remains a phone
qualification case under this privacy-disabled profile.

HCI ACL payload size and credits remain 27 / 5. In S3 only, the *physical*
NimBLE transport mbufs have 72-byte payload capacity, with the same 24 LL
receive buffers. NimBLE's SMP code pulls the complete 64-byte public key into
one mbuf after its 27/27/15 LL fragments arrive; the S2 27-byte physical mbufs
cannot satisfy that pullup. This is internal storage, not a larger HCI or radio
PDU. No upstream NimBLE or TinyCrypt source is patched.

## Entropy and reserved flash

Seed A/B are 0x94000/0x95000. Each has sixty 64-byte WLSD v1 records and an
untouched final 256 bytes. Validation covers magic/version, CRC, flags, reserved
bytes and credit semantics. Selection uses modular sequence arithmetic with
deterministic ties/half-range conflicts. Append skips every nonblank slot;
when full it erases the other sector, retaining the previous valid record.
One program runs per boot, one erase per sixty boots, each sector once per
120 boots. Actual part endurance is unknown; no endurance guarantee is made.

The seed step runs at P2, after plain flash reads and before radio/host start.
The pool hashes labelled, length-prefixed old seed/sequence, public address,
reset reason, an actual boot timer reading and one R64 pair. No SRAM window has
been qualified, so power-on RAM is omitted. R64 is a deterministic diversifier,
never entropy. Returned flash completion timing is mixed into the pool after
the write. Event timestamp differences and RX RSSI words enter a bounded sample
ring and are conditioned in the executor; none earns credit.

The generator uses SHA-256 over domain-labelled `(key,counter)` for each output
block, then hashes `(old key,block)` into a new key, discarding unused bytes.
The counter is 32 bits and wraps to 1 only after rekeying. The stored successor
seed and the in-use generator key have separate domains. SHA-256, generator
and hardware AES known-answer tests run at boot. A reseed mixes the pool into
the key on the first request after connect and after 65536 output bytes.
HCI Reset retains the state, class and flash; operational Rand/output/reseed
statistics reset, while boot parsing/persistence results remain visible.

Readiness numbers are FAILED=0, FIRST_BOOT=1, CHAINED=2, PROVISIONED=3,
UNPERSISTED=4. Persistence is STORED=0, NOT_STORED=1, UNVERIFIED=2; origin is
reported separately. Output is available in every non-FAILED class. FIRST_BOOT
and an unprovisioned CHAINED state have no proved unpredictability. Only a
qualified provisioned seed carries credit, and it is never increased here.
No installer/provisioning source is supplied or silently substituted.

Flash services reject a live link, bad address/alignment or mismatched flash
identity. Seed failure is attempted once per boot and classified without a
retry. Erase/program return late-ready after 500/10 ms if ready before the
extended 2 s/50 ms limits. A part still busy causes a reset from RAM without
restoring XIP or interrupts. The RAM section passes the no-calls check. Behavior
of ROM startup while NOR remains busy is still unknown. All flash writes are
outside links; idle flash operations can therefore enlarge the *all-state*
timer gap diagnostic. `worker_max_gap_us` is measured while a link is live.

## Persistent bonds

The extension reserves sector 0x96000, outside Felucca's existing store. A
30-slot, 128-byte FLBD v1 append journal stores canonical security/CCCD values,
index, active/tombstone, sequence and CRC. It never stores raw compiler structs
or writes the last 256 bytes. RAM holds eight entries, with at most two of each
security type. Updates only mark RAM dirty during a link. After Disconnection
Complete, main flushes once while both LL and radio are idle, before restarting
advertising. A failed flush is counted and left pending until a later disconnect
or reboot; there is no busy retry loop. Identical values cause no write.

Normal reboots reload verified records and retain the phone's bond. A power
cut before deferred flush loses that session's changes. Compaction uses one
sector, so a cut during erase/rewrite can lose bonds; seed sectors are separate
and unaffected. The recovery is to remove the FM-1 bond on the central and pair
again. Physical flash readers can obtain the keys; no at-rest protection is
claimed. The additional reservation/compaction policy is explicitly a spec gap.

## Memory, builds and validation

The machine-readable size/hash reports are `out/BLE-S2.1-BUILD.txt` and
`out/BLE-S3-BUILD.txt`. Both packages are 609849 bytes including the fixed
application slot, loader and packaging assets. S2.1 app is 488956 bytes;
S3 app is 519952 bytes, leaving 61612 bytes in the app slot. Its final hash is
in the report. S3 keeps 1996 bytes of static
RAM headroom and approximately 14 KiB of pool headroom; stacks are 24320/7936
bytes and their physical high water is unmeasured.

S3 additionally reduces each track's slicer STUT history from S2's 2048 samples
(92.9 ms at 22.05 kHz) to 1024 (46.4 ms). Long stutters repeat a shorter phrase;
gate mode and other engines are unchanged. It saves 8192 pool bytes. The inherited
S2 reduction is the slicer history from stock 4096 samples (185.8 ms) to 2048;
S3 reduces it further. No factory sample sets or delay settings are removed.
All reductions apply only with FELUCCA_BLE=1. Stock binary and package
hashes exactly match the original baseline (see build logs/audit).

Build without any device operation:

```
FELUCCA_BLE=1 FELUCCA_BLE_STAGE=2 JIELI_TOOLCHAIN=/root/felucca-ble-env/jieli/toolchain AC79_SDK=/root/felucca-ble-env/sdk-min ./build.sh
python3 tools/ble_report.py s2.1
FELUCCA_BLE=1 FELUCCA_BLE_STAGE=3 JIELI_TOOLCHAIN=/root/felucca-ble-env/jieli/toolchain AC79_SDK=/root/felucca-ble-env/sdk-min ./build.sh
python3 tools/ble_report.py s3
```

FELUCCA_BLE=1 defaults to stage 3; explicit stage 2 disables all S3 functionality.
FELUCCA_BLE=0 builds stock. Run `tests/run_ble_s1_tests.sh`,
`python3 tests/run_ble_s2_tests.py`, `python3 tests/run_ble_s3_tests.py`,
`python3 tests/target_budget.py`, and `python3 tools/ble_s3_audit.py`.

Native ASan/UBSan tests cover Core AES/four CCM vectors and negative nonce/AAD/
direction checks, counter exhaustion, real HCI Encrypt/Rand/Reset and failure,
seed CRC/semantics/wrap/ties/torn prefixes/wear/failure classes, deferred bond
compaction/reload/deletion, phase barriers, positive key refresh, negative
restart, repetition and RX marker ordering. The actual NimBLE host runs SC and
legacy Just Works against a simulated central, including public-key fragments,
CCM, encrypted MIDI both directions, MTU 200, stored-key reconnect and MIC loss.
The first SC pairing uses exactly 10 LE Rand calls; the legacy fixture with key
distribution uses 6. The S2 ten-minute/reconnect/update integration also runs
against S3 with an unpaired central. No host-side test proves physical IRQ
preemption or ECC duration on the chip.

Warnings are errors for our code. Unchanged upstream TinyCrypt gets GCC's
old-style-declaration warning suppressed in native tests, and only its
`aes_encrypt.c` disables UBSan shift-base checking (signed-int byte promotion
before a 24-bit shift). All other ASan/UBSan checks remain enabled. Target
compilation is warning-clean; upstream licenses and hashes are retained.

## Parent bench and pass criteria

1. **S2.1 first / PB4.2, PB3.1, F-07, F-21.** Confirm its self-test, advancing
   timer calls, stage-2 reports, open MIDI, central update, ten reconnects,
   ten-minute link and range timeout. `ble host` queues NimBLE resynchronisation;
   it is not a status command. Use plain `ble` for status.
2. **S3 boot / F-20, F-23, H-15.** Parent installs the S3 package. Save `ble`
   and console greeting. Require error 0, seed state FIRST_BOOT or CHAINED
   (PROVISIONED only with a qualified installed seed), STORED persistence,
   self-test 1, NimBLE synced. Reboot twice: sequence advances once per boot,
   normally CHAINED, no malformed/flash failure. No live-sector fault injection.
3. **Desktop / F-04, F-05, H-15.** On Linux BlueZ 5.x use `btmon` and
   `bluetoothctl`: `power on`, `agent NoInputNoOutput`, `default-agent`,
   `scan on`, `pair 62:40:8B:67:65:58`, `trust 62:40:8B:67:65:58`,
   `connect 62:40:8B:67:65:58`. Remove an old incompatible bond on the desktop
   before the first test. Require encryption enabled, key size 16, method SC=2
   when both peers support SC, no key/MIC/phase errors and no host reset. Record
   LE Rand count/build choices; other host consumers may add calls. Legacy
   fallback needs a legacy-configured central (the native fixture covers it).
4. **MIDI / F-05–F-10.** Discover service
   03B80E5A-EDE8-4B33-A751-6CE34EC4C700 and characteristic
   7772E5DB-3868-4112-A1A9-F2669D106BF3, empty-read, subscribe. Run
   `python3 tools/ble_midi_bench.py --address 62:40:8B:67:65:58 --duration 600`.
   Play FM-1 notes and observe desktop MIDI; find BlueZ's ALSA port using
   `aplaymidi -l`, then send a MIDI file via `aplaymidi -p CLIENT:PORT FILE.mid`,
   or use script `--send-notes --alsa-port CLIENT:PORT`. Require heard notes,
   valid timestamp framing, zero MIDI/host drops, balanced accepted/ACKed/NCP
   after draining. BlueZ's MIDI plugin owns D-Bus WriteValue and returns
   NotAuthorized; use ALSA. Script Connect retries up to three times for local
   BlueZ aborts that never reach the DUT.
5. **Persistence / F-04 extension.** Disconnect; wait until `bond_pending 0`
   and verified writes are counted. Parent reboots FM-1 without deleting the
   desktop bond. Reconnect and require encryption using the retained key, no
   new pairing, MIDI both ways. Repeat on Android. Do not reboot while still
   connected when testing deferred persistence.
6. **Load / PB4.2, PB5, F-07, F-16, F-21, R-28.** At 7.5, 15 and 30 ms under
   audio/UI load, pair with fresh SC keys while MIDI traffic is active; run
   ten minutes encrypted. Require live worker/dispatch gap ≤7500 us, executor
   pass ≤250 us (record preemption), refill ≤60 us, no corresponding overruns,
   no audio late buffers/shed voices, no assertions or canary/stop errors.
   Record ECC wall time and host/RF queue peaks. Qualify the supervisor-stack
   fill/margin and cumulative masking separately. Timer max gaps across idle
   bond erases are reported separately from the live-link bound.
7. **Update / reconnect / range, F-08–F-10.** Use the bench script's
   `--conn-update HANDLE 6 6 0 500` with sudo and the desktop handle from btmon;
   repeat at 15 and 30 ms and inspect Update Complete. Do ten reconnects;
   all must work (up to three local Connect retries). Walk out of range:
   one supervision failure, reason 0x08 retained during the next connection,
   no crash, successful reconnect.
8. **Android / F-04, F-05.** A BLE-MIDI app connects/subscribes and accepts the
   Android pairing prompt if presented. Confirm encryption/key size on `ble`,
   play/receive notes, disconnect/flush/reboot FM-1, reconnect with the existing
   phone bond, then repeat ten cycles and a ten-minute link. Record Android/app
   versions and address behavior. Cached GATT state may need refresh after a
   firmware upgrade; this is distinct from bond persistence.
9. **Scriptable peer/sniffer / K-04/K-10, R-12–R-16, R-24/R-26.** Exercise
   pause/restart/refresh, missing LTK, corrupt MIC, counter limits and coalesced
   epoch boundaries. Require phase-table behavior, 0x3D on invalid encrypted
   traffic, no TX refill after detection, stop then report, no stale event or
   DMA reuse in a new epoch. Measure D-1 detection latency and RF tail
   separately. These adversarial cases are not reachable through bluetoothctl.

No on-device S2.1 or S3 pass is asserted by this implementation report.
