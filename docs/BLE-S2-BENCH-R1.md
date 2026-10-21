<!-- SPDX-License-Identifier: MIT -->
# S2 bench round 1 correction

The parent tested package SHA256 684ea866bf5d1f23fa8356266b9e04671ab5dbb88877260839a12b357bb1bc05.
Boot, calibration, NimBLE sync, advertising/scan response, MIDI UUID and audio
passed. Both connection attempts crashed. This implementer did not access the
device, USB, a network host, or any forbidden clean-room input. The replacement
package hash and exact memory sizes are in out/BLE-S2-BUILD.txt and
out/felucca-ble-s2.sha256. Changes remain uncommitted above 48ee2a0.

## Connection crash

The firmware defect is an unaligned 32-bit store to F_WIN_LEN. Frozen r3
02 §3.3 defines its low and high halves as two 16-bit fields at frame offsets
0x22 and 0x24. The frame begins at DMA block +16, with the block 4-byte aligned.
The old `BB_F32(BF_WIN)` wrote a word at block+50, address 0x01c70c92 in the
bench build. That address is 2 modulo 4.

The saved target disassembly establishes the failing path:

- At 0x02008552, `[r1+0] = r0` writes the first receive window, with
  r1 = ble_dma+50. This occurs only after a valid CONNECT_IND.
- At 0x02008560 the next anchor-programming call enters fm1_ble_irq_save;
  its return address is 0x02008564, exactly the supplied crash RETS.
- The supplied exception PC is 0x020004be, that helper's first instruction.
  The instruction at that PC reads ICFG; the preceding unaligned data access
  is a concrete C alignment violation emitted in the connection path.

The PC at the following call is consistent with a deferred fault report;
the pipeline delay and the bit-0 mnemonic are not defined by the permitted
public headers. EMU_MSG=1 must therefore not be presented as a verified named
decode. Stack-limit bit 3 and DBG_MSG are clear, and the supplied SP is inside
the reserved supervisor stack. The corrected build still needs the parent's
physical retest to establish that this fixes the observed exception.

All four window-write sites now use the shared volatile halfword helper:
connection acceptance, first/steady/update window changes, missed-event
widening, and recovery. The target emits `h[...+50]` and `h[...+52]` stores.
The nominal timing values, anchor order, radio IRQ priorities, XIP placement,
stack reservations and hardware guards are unchanged.

ASan/UBSan tests call this actual helper at an address 2 modulo 4, cover low/high
halves through UINT32_MAX, and verify adjacent fields are untouched. A local
reproducer of the original word store fails UBSan alignment as expected
(out/ble-s2-alignment-regression.log). Existing controller/MIDI tests and actual
NimBLE simulated-peer integration pass. Both target configurations build cleanly;
the stock app/package remain byte-identical to the S1 stock baseline.

## TIMER0 and fault diagnostics

`worker_calls` and `worker_max_gap_us` only measure live connections. Their
advertising values of zero did not establish whether TIMER0 was dispatching.
The new all-state worker_timer counters advance at every timer IRQ, including
advertising; live-link counters keep their original interpretation. CON, CNT,
PRD and the IRQ enable/priority nibble are read back by `ble`.

The 24-byte `fm1_ble_breadcrumb` object is in .noinit. Its aligned stage word
packs stage in bits 31:24, hardware link state in 23:16 (0 off, 1 advertising,
2 stopping, 3 quarantined, 4 connected), and detail in 15:0. Tick timestamps
are approximate across nested interrupts. It records the latest operation,
not a call stack; e.g. an anchor stage can remain after its function returns.
The fault handler copies the word/timestamp and crash count before the crash
screen and disables main-context host yields during that screen. Subsequent
boot, traffic, HCI Reset and diagnostic counter resets retain the fault snapshot. Cold or
incompatible RAM is rejected by its versioned magic. Power removal or firmware
update does not guarantee retention.

| Stage (hex) | Operation | Detail |
| --- | --- | --- |
| 01 / 02 | prepare / ready | zero |
| 10 | RX entry | frame event counter |
| 11 | CONNECT_IND programming | interval in 1.25 ms units |
| 12 | indirect anchor write | instance ID << 8, OR register index |
| 13 | receive-window halfword stores | low 16 bits of window microseconds |
| 14 / 15 | LL create / connected | zero |
| 20 | event entry | frame event counter |
| 30 | TIMER0 worker entry | frame event counter |
| 40 | main-context host/controller pass | frame event counter |

`ble_fault_breadcrumb`, `ble_fault_ticks`, and `ble_fault_crash_count` are the
reboot-persistent snapshot. Match the count with `crash`. A zero fault count
means no captured fault with this breadcrumb format, not proof of no earlier
crash. `ble_breadcrumb` / `ble_breadcrumb_ticks` show current-boot activity.

`ble host` intentionally queues a real NimBLE reset/resynchronisation in main
context; it is a recovery command and may end a connection/restart advertising.
It does not reboot the chip or allocate another host instance. `ble` alone is
the status command.

## Focused parent retest

1. Before connecting, take two `ble` reads about one second apart. Expect
   worker_timer_calls to advance about 1000, PRD=6000, CON low bits=0x19,
   IRQCFG=0x9, and CNT changing. Connection-only worker_calls can be zero.
   Record the crash and BLE fault counts.
2. Use the existing desktop D-Bus bench client or bluetoothctl to connect,
   discover the MIDI service, subscribe, and exchange notes in both directions.
   Require no crash/reboot and no new BLE fault count. During the connection,
   require worker_calls advancing, worker_max_gap_us ≤7500,
   worker_over_7500us=0 and the existing IRQ/critical timing pass criteria.
3. If stable, run ten reconnects and the ten-minute link, central parameter
   update, Android MIDI and out-of-range supervision tests in BLE-S2.md. If it
   crashes, collect `crash`, `ble`, and the exact matching saved ELF/disassembly;
   preserve ble_fault_breadcrumb/ticks/count before another fault overwrites it.

No additional memory reduction was made in this fix. Pool/DMA placement and
the earlier BLE-only STUT reduction are unchanged. Unmeasured window geometry,
missed-event correction, instant latching and actual worker/IRQ latency remain
the existing r3 bench risks. This correction includes no r4 changes.
