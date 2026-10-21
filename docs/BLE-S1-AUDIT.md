<!-- SPDX-License-Identifier: MIT -->
# BLE S1 clean-room access audit

Implementation session: 2026-10-19, working directory always
`/root/fm1/Felucca-ble`, branch `ble-controller`. Source changes, tests, generated
firmware and documentation are confined to that worktree; no git commit was
made. The read-only Felucca reference source was not opened or modified. Git
itself used the worktree's normal shared repository metadata.

The complete list of external **task input files opened/read**, including text
searched with `rg`, is below. Some review files were read in excerpts; no claim
is made that their older/live-spec references were followed. Directory filename
inventories were confined to the allowed snapshot, review directory and this
worktree. No live specification, trace folder, reconstruction, forbidden FM-1
source, Bluetooth SDK source/library, vendor object/IR/disassembly, device, or
network host was accessed. No web browser/search or network tool was used.

Frozen specification:

- `/root/wl82-ble-spec-snapshots/r2/RULES.md`
- `/root/wl82-ble-spec-snapshots/r2/00-overview.md`
- `/root/wl82-ble-spec-snapshots/r2/01-radio-bringup.md`
- `/root/wl82-ble-spec-snapshots/r2/02-baseband.md`
- `/root/wl82-ble-spec-snapshots/r2/03-link-layer-hci.md`
- `/root/wl82-ble-spec-snapshots/r2/04-platform.md`
- `/root/wl82-ble-spec-snapshots/r2/05-acceptance-tests.md`

Additional frozen specification, authorized by the S1 update:

- `/root/wl82-ble-spec-snapshots/r2b/RULES.md`
- `/root/wl82-ble-spec-snapshots/r2b/00-overview.md`
- `/root/wl82-ble-spec-snapshots/r2b/01-radio-bringup.md`
- `/root/wl82-ble-spec-snapshots/r2b/02-baseband.md`
- `/root/wl82-ble-spec-snapshots/r2b/03-link-layer-hci.md`
- `/root/wl82-ble-spec-snapshots/r2b/04-platform.md`
- `/root/wl82-ble-spec-snapshots/r2b/05-acceptance-tests.md`

Permitted review context:

- `/root/wl82-ble-spec-review/IMPLEMENTABILITY-gpt-6.1-sol.md`
- `/root/wl82-ble-spec-review/IMPLEMENTABILITY-r2-gpt-6.1-sol.md`
- `/root/wl82-ble-spec-review/RESPONSES-r1.md`
- `/root/wl82-ble-spec-review/RESPONSES-r2.md`
- `/root/wl82-ble-spec-review/PLATFORM-felucca.md`

Permitted host behavior/reference material:

- `/root/nimble-src/nimble/host/src/ble_hs_startup.c`
- `/root/nimble-src/nimble/host/src/ble_gap.c`
- `/root/fm1-app-re/nimble/hci-usage-device.txt`

Public SDK register header, definitions only:

- `/root/sdk-blemidi/include_lib/driver/cpu/wl82/asm/WL82.h`

Build inputs read by the existing packer, not inspected as implementation material:

- `/root/felucca-ble-env/sdk-min/cpu/wl82/tools/uboot.boot`
- `/root/felucca-ble-env/sdk-min/cpu/wl82/tools/cfg_tool.bin`
- `/root/felucca-ble-env/sdk-min/cpu/wl82/tools/cfg/eq_cfg_hw.bin`

Compiler builtin headers (reported by the target compiler's dependency output;
`toolchain` resolves within the permitted build environment):

- `/root/felucca-ble-env/jieli/jieli-linux-toolchains-20260730.1/common/lib/clang/4.0.1/include/stdint.h`
- `/root/felucca-ble-env/jieli/jieli-linux-toolchains-20260730.1/common/lib/clang/4.0.1/include/stddef.h`
- `/root/felucca-ble-env/jieli/jieli-linux-toolchains-20260730.1/common/lib/clang/4.0.1/include/__stddef_max_align_t.h`

These 29 files are hashed in `out/BLE-S1-INPUT-SHA256SUMS`. Standard host
executables, system/standard-library/compiler runtime dependencies, ordinary Git
metadata and ephemeral files created by the repository's existing host tests are
tooling rather than clean-room task inputs. The compiler/linker/objcopy/objdump
executables ran only from the permitted toolchain. Every ELF/object/IR/disassembly
read for implementation validation was generated from this worktree's own source;
no vendor ELF/object/IR/disassembly was read. The new host tests use no radio or
external host. The regression suite used its simulated devices; the optional
DaisySP reference was explicitly pointed at a nonexistent path within this
worktree and skipped.

The frozen part-01 numerical tables were parsed and checked for counts
64/256/117/27/73/83/18/57/128/128/42 before writing the source header. The test-unit
68-byte calibration example appears only in host tests, never as a firmware
fallback. No Apache NimBLE code was copied or linked; its two source files were
read to establish command order/behavior. Specification table provenance and
its unresolved redistribution decision are separately recorded in `LICENSING.md`
and `SPEC-GAPS-S1.md`; this access audit makes no additional rights claim.
