<!-- SPDX-License-Identifier: GPL-3.0-only -->
# BLE S2 clean-room access audit

Implementation date: 2026-10-19. All commands used the worktree
`/root/fm1/Felucca-ble` or the expressly permitted `/root/nimble-src` checkout.
The working branch is `ble-controller`; HEAD remains `48ee2a0`. S2 source,
tests, documentation and firmware are uncommitted. No subagents were spawned.

The explicit external-input hash list is
`out/BLE-S2-INPUT-SHA256SUMS` (283 files). It includes files whose contents were
read, searched, copied, counted/indexed or used by the target compiler/packer.
`out/BLE-S2-NIMBLE-INPUT-SHA256SUMS` lists the 129 original files imported into
the worktree. Some specification/review files were read in selected sections;
references inside them to live/reconstruction material were never followed.
Allowed directory filename inventories are not claims of reading each file.

## Frozen specification

All seven files under `/root/wl82-ble-spec-snapshots/r3/`:

- `RULES.md`
- `00-overview.md`
- `01-radio-bringup.md`
- `02-baseband.md`
- `03-link-layer-hci.md`
- `04-platform.md`
- `05-acceptance-tests.md`

S2 used the r3 connection/timing/ownership contracts. S1's earlier frozen-input
provenance remains documented separately in BLE-S1-AUDIT.md; those older
snapshots were not reopened during this S2 task.

## Permitted review and usage inputs

- `/root/wl82-ble-spec-review/IMPLEMENTABILITY-gpt-6.1-sol.md`
- `/root/wl82-ble-spec-review/IMPLEMENTABILITY-r2-gpt-6.1-sol.md`
- `/root/wl82-ble-spec-review/RESPONSES-r1.md`
- `/root/wl82-ble-spec-review/RESPONSES-r2.md`
- `/root/wl82-ble-spec-review/PLATFORM-felucca.md`
- `/root/fm1-app-re/nimble/hci-usage-device.txt`

The S1 bench findings in the user's brief were accepted as owner-supplied
observations. They were not independently remeasured. The read-only Felucca
reference `/root/fm1/Felucca` was not opened as an implementation input or
modified; the permitted S1 worktree supplied the platform source. Normal Git
worktree/shared-repository metadata was used for status and identity checks.

The parent's S2 round-1 crash addresses and bench observations were likewise
accepted as supplied. The saved S2 ELF/disassembly/package were copied inside
the worktree to build/felucca-ble-s2-bench-r1.* before rebuilding. No parent
device or desktop endpoint was accessed.

## Public register headers (bench round 1)

All 129 .h files selected by `rg --files` under the explicitly allowed
`/root/sdk-blemidi/include_lib/driver/` were text-searched for EMU exception
definitions. Every searched path is hashed in the explicit input manifest,
including headers with no match. Selected definitions were read in:

- `cpu/wl82/asm/WL82.h`: TIMER0..5 layout/base definitions.
- `cpu/wl82/asm/csfr.h`: EMU_CON/EMU_MSG and CPU register layout.
- `cpu/wl82/asm/hwi.h`: exception, TIMER0 and BLE IRQ indices.

The search also displayed a startup.h exception-comment match and a gpcnt.h
comment match; those files are included in the same manifest. No header in
this permitted set defined the bit-0 fault mnemonic. No SDK implementation,
object or debug.c source was opened; the worktree's existing guard/IRQ code
and its own generated instructions supplied the other fault evidence.

## Apache NimBLE

`/root/nimble-src` HEAD was checked against
`da7e3256da3ba80b232df880f40d8359311cc62e` (nimble_1_9_0_tag).
`tools/vendor_nimble.py` copied only .c/.h source and license notices from these
explicit permitted roots/files, preserving their original contents:

- `nimble/include/`
- `nimble/host/include/`, `nimble/host/src/`
- `nimble/host/services/gap/`, `nimble/host/services/gatt/`
- `nimble/host/util/include/`
- `nimble/transport/include/`, `nimble/transport/src/transport.c`,
  `nimble/transport/src/monitor.c`
- `porting/nimble/include/`
- `porting/nimble/src/os_mbuf.c`, `os_mempool.c`, `os_msys.c`, `mem.c`, `endian.c`
- `LICENSE`, `NOTICE`

Every one of those 129 original input paths and hashes is in the NimBLE
manifest. Vendored source files remain byte-identical to those inputs. The
controller is Felucca's clean-room implementation; no NimBLE controller source
is linked. Host files disabled by the pinned configuration or unused APIs can
remain in the source subset while linker GC omits them from the image.

Additional permitted original NimBLE files read for the integration:

- `/root/nimble-src/nimble/host/syscfg.yml`
- `/root/nimble-src/porting/nimble/Makefile.defs`
- `/root/nimble-src/porting/npl/linux/include/nimble/nimble_npl_os.h`
- `/root/nimble-src/porting/nimble/src/nimble_port.c`

The local NPL, C shims, bounded arena, MIDI service and transport glue were
written in the worktree. Existing upstream host diagnostics for unused
parameters, sign comparisons and old-clang aggregate-zero initializers are
suppressed only while compiling the imported files. New code is compiled with
-Wall/-Wextra/-Werror. Apache copyright notices, LICENSE and NOTICE are retained;
LICENSING.md describes the GPL-3.0-only combined firmware.

## Build environment

The existing packer read these three permitted build-environment data files;
they were not examined as Bluetooth implementation material:

- `/root/felucca-ble-env/sdk-min/cpu/wl82/tools/uboot.boot`
- `/root/felucca-ble-env/sdk-min/cpu/wl82/tools/cfg_tool.bin`
- `/root/felucca-ble-env/sdk-min/cpu/wl82/tools/cfg/eq_cfg_hw.bin`

Target preprocessor dependency output identified five compiler builtin headers
under `/root/felucca-ble-env/jieli/jieli-linux-toolchains-20260730.1/common/lib/clang/4.0.1/include/`:

- `stdint.h`
- `stddef.h`
- `__stddef_max_align_t.h`
- `stdbool.h`
- `limits.h`

Compiler/linker/objcopy/objdump executables ran from the permitted build
environment. All objects, IR, ELF and disassembly inspected for validation were
generated from this worktree's own source. No vendor object, archive, IR or
Bluetooth library was read. Standard system executables, Python/Pillow/Node,
host compiler/runtime/standard-library dependencies, normal Git metadata and
regression-suite temporary/generated files are tooling, not independent
clean-room implementation inputs. The optional external DaisySP comparison
was explicitly pointed at a nonexistent path inside this worktree and skipped.
The installer/USB/editor tests use simulated objects; no real device endpoint
was opened.

## Access boundaries

No live `/root/wl82-ble-spec/`, trace directory, reconstruction or patch-work
source was accessed. No other `/root/fm1-app-re/` file was read. The starting
`/root/fm1-toolchain/` directory was not searched or read. No other FM-1 worktree,
SDK Bluetooth source, SDK library/object, vendor disassembly or IR was used.
The original S2 implementation used r3 and the permitted S1 HAL. Bench round 1
additionally used only the public driver headers listed above; other SDK paths
were never opened.

No browser, web search, network connection, USB/device operation, flashing or
on-device testing occurred. Public Core/BLE-MIDI documents were not fetched
because the user explicitly prohibited network access; the implementation used
r3's supplied protocol contracts/examples and public protocol knowledge. The
Linux D-Bus/HCI bench client was written and syntax-checked only. It is an
owner-operated procedure artifact and is never run by a build or local test.

No permission, approval, message-sending, deployment, PR or commit operation was
performed. No specification, snapshot, review file, calibration/item store or
reference firmware was modified. The unresolved numerical-table redistribution
question inherited from S1 is unchanged; this audit makes no additional rights
claim.
