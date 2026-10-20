<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S3.1d round-4 clean-room audit — frozen r7

Worktree /root/fm1/Felucca-ble, branch ble-controller, uncommitted above
0fb0475. No child agents, devices, flashing, commit or push. Every shell
command used the permitted repository workdir; the supplied toolchain-workspace
cwd was not inspected. Only frozen r7 and permitted repository sources/records
were consulted, plus public Bluetooth Core 5.4 Link Layer HTML/text. The four
supplied failure records were read before implementation. No other snapshot,
live spec, trace, reconstruction, patch work, SDK tree or vendor controller
binary/object/IR was opened. No spec or vendored NimBLE/TinyCrypt source changed.

Public Core source:
https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/low-energy-controller/link-layer-specification.html
The retained out/s3.1-core54-ll.html and .txt are new allowed inputs. Window
validation was checked against §5.1.1. No SDK header or app-re file was needed.
The local btsnoop reader decoded supplied HCI records, not radio LL packets.

Existing pinned /root/felucca-ble-env compiler/linker/objcopy/objdump and sdk-min
package assets were used only as opaque build/packaging dependencies as in
round 3. No tooling workspace was traversed. The builder checks the three
retained package-asset hashes. Historical tooling fingerprints are carried
forward unchanged, not claimed independently revalidated. Native executables
and inspected target ELF/disassembly contain this open firmware/dependencies.

The r7-only audit script emits BLE-S3.1d-INPUT-SHA256SUMS and checks 152 local
NimBLE/TinyCrypt hashes against retained import manifests without visiting
historical source roots. Four failure records and public Core artifacts are
explicit out/ inputs; local inventory excludes build/out otherwise. Inventory
hashing is not a claim that every source was read. Old manifests are preserved.
BLE-S3.1d-TOOLING-PROVENANCE-SHA256SUMS, CHANGED-FILES.txt and
ARTIFACT-SHA256SUMS retain provenance and outputs.

All S1/S2/S3/S3.1 sanitizer suites and stage-2/stage-3/S3.1 target builds pass.
The inherited TinyCrypt AES shift-base exception remains. The original LL
source reproduces the first-window gate regression; all new matrix, encrypted,
instant/reconnect and 600-second phase tests pass on the existing virtual radio.
Owner bench script checks are offline only. Target guards and DSP comparison
pass; build layout/hash are in out/BLE-S3.1D-BUILD.txt. No silicon timing or
hardware anchor-latch qualification is claimed. Exact historical PDU/reconnect
causality remains qualified in out/BLE-S3.1d-CONNUPDATE-ANALYSIS.md.

<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Archived S3.1c round-3 clean-room audit — frozen r7

2026-10-20. Worktree `/root/fm1/Felucca-ble`, branch `ble-controller`,
uncommitted above c5973a1; round-2 implementation b242adc. No agents were
spawned. Commands used the explicit permitted repository or r7 snapshot
workdir. The supplied toolchain-workspace cwd was not inspected or used.
No devices, flashing, device-network access, commit or push operation occurred.

The required `out/BLE-S3.1b-BENCH-R1.txt` was read before implementation. It
supplies the Reset/control timing failures and F-24/F-28 baseline, not new
S3.1c measurements. Controller references this round were r7 and the permitted
repository, including its open vendored NimBLE/TinyCrypt and retained reports.
No live/older specification, trace/reconstruction/patch tree, vendor controller
binary/library/object/IR, or additional SDK/app-re input was opened. Paths
mentioned in historical documents/manifests were not followed.

## Round-3 inputs and outputs

`tools/ble_s31_audit.py` now names **r7 only**. It hashes seven snapshot files,
local source/docs/tests/tools, retained public Core HCI text/HTML, and the
round-2 bench record. It checks the local 152 NimBLE/TinyCrypt import hashes
without visiting their historical external origins. Public Core artifacts are
retained inputs; no new external document or internet download was needed.
No upstream source, spec file, version/feature advertisement or licence changed.

`out/BLE-S3.1c-INPUT-SHA256SUMS`,
`out/BLE-S3.1c-TOOLING-PROVENANCE-SHA256SUMS`,
`out/BLE-S3.1c-CHANGED-FILES.txt`,
`out/BLE-S3.1c-ARTIFACT-SHA256SUMS` and `out/ble-s3.1c-audit.log` retain the
current evidence. Historical output manifests remain intact. Inventory paths
must resolve within the repository; build/out are excluded except the named
input records. Hashing an inventory does not claim every file was consulted.

Builds use the same existing pinned compiler/linker/objcopy/objdump and
sdk-min packaging dependencies as round 2; the three Apache-licensed package
asset hashes are checked by the existing builder. These are opaque execution/
packaging dependencies, not controller implementation references. No vendor
controller code is linked. Historical tool fingerprints are carried forward
from the round-2 manifest, not claimed independently revalidated. Inspected
ELF/disassembly output is this open firmware and its open dependencies only.

Full S1 (both options), S2, S3 and S3.1 suites pass with ASan/UBSan and warnings
as errors; logs use `out/ble-s3.1c-{s1,s2,s3}-tests.log` and
`out/ble-s3.1c-tests.log`. The inherited TinyCrypt AES shift-base-only exception
remains. Test-only SHA/memory wrappers and simulated AES costs do not enter
target builds. Reset interruption, busy-stop ownership, stale events, seed/
identity/power persistence, opcode/MIC attribution and stream/barrier/counter
ordering are exercised along with real pinned-host F-24/F-28 regressions.

Target options stage 2, stage 3 and stage 3/S3.1 pass, including capacity,
DMA, RAM-code and HAL-only MMIO guards. The DSP budget comparison passes.
Image `out/felucca-ble-s3.1c.fwsc` has SHA256
`c287bfe52f2ba203ddc1cc89cebc30777ffd5e9d860d61dcd77c84c2f2d2d800`;
layout is in `out/BLE-S3.1C-BUILD.txt`. No image has been flashed this round.

The 2525-tick historical control opcode cannot be recovered. A reproducible
0x06 path has three CCM calls / 17 AES blocks before the split. New injected
models bound Reset at 48 us and control/refill at 60.167 us; these do not
establish physical target timing. The per-kind/opcode bench checklist and
remaining F-27/F-24/F-28 qualification are in `docs/BLE-S3.1.md`; r7 choices
are appended to `docs/SPEC-GAPS-S3.1.md`.

## Archived round-2 / r6 audit

The following describes round 2 and its historical read set and artifacts.

2026-10-20. Worktree `/root/fm1/Felucca-ble`, branch `ble-controller`,
uncommitted above ee63586; round-1 code 94eb8a8. No agents were spawned. All
shell commands used this explicit permitted repository workdir or the r6
snapshot workdir. The supplied `/root/fm1-toolchain` cwd was never inspected
or used. No devices, USB/desktop sessions, device-network access, flashing,
commit or push operations occurred.

`out/BLE-S3.1-BENCH-R1.txt` was read first, as instructed. It is supplied
hardware evidence for the old image, not independently measured S3.1b evidence.
The permitted repository (source, vendored NimBLE 1.9.0/TinyCrypt, docs and out
records) and frozen r6 are the only implementation references read this round.
No new internet download, SDK header or app-re input was needed.

## Input inventory and provenance

Run `python3 tools/ble_s31_audit.py`; its explicit spec root is now **r6 only**.
The script never executes an inherited external read list. It emits
`out/BLE-S3.1b-INPUT-SHA256SUMS` and
`out/BLE-S3.1b-TOOLING-PROVENANCE-SHA256SUMS`, preserving the round-1 manifests.

- Exactly seven frozen r6 files are hashed. The live spec, r5/older snapshots,
  trace, patch work, toolchain workspace and reconstruction trees were never
  opened. No specification file was edited. Forbidden references in permitted
  documents/manifests were not followed.
- Repository inputs are conservatively inventoried with git's local file
  list. Symlinks must resolve inside this repository before hashing. Generated
  build/out files are excluded except explicit permitted input records.
  Hashing the inventory does not claim every file was an implementation source.
- The retained public Core 5.4 HCI text/HTML `out/s3.1-core54-hci.*` was reviewed,
  especially Vol 4 Part E §§7.7.65.1/10, against r6 and pinned host source.
  Original public source:
  https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host-controller-interface/host-controller-interface-functional-specification.html
  No non-Core material was used as a new external implementation source.
- Local vendored NimBLE/TinyCrypt copies are checked against the two retained
  import manifests (152 files), without opening the historical external paths.
  No vendored source was modified. Test-only link wrappers count SHA blocks;
  they neither patch TinyCrypt nor enter target builds. Existing SPDX/licence
  notices and combined GPL-3.0-only licensing remain.
- Four supplied bench records (S2.1, S3, S3 Android, S3.1 round 1) are hashed as
  allowed historical inputs. Reading old reports does not authorize reopening
  their original external sources. `/root/fm1-app-re/nimble/hci-usage-device.txt`
  and all SDK headers were unnecessary this round and were not opened.
- Builds execute the existing compiler/linker/objcopy/objdump at the pinned
  `/root/felucca-ble-env/jieli/toolchain` and existing `sdk-min` packaging assets,
  as round 1 did. These are execution/packaging dependencies needed to produce
  the requested image, not new controller references. The build checks the
  three retained packaging hashes. No vendor controller binary, object,
  library, disassembly or IR was read or linked. Inspected build outputs, where
  applicable, are solely this open firmware and its open dependencies.
  Historical tooling hashes are retained from the existing repository audit;
  they are provenance, not claims of independent revalidation of external files.

## Verification and retained outputs

Complete S1/S2/S3/S3.1 suites pass under ASan/UBSan; the inherited unmodified
TinyCrypt AES source has its existing shift-base-only UBSan exception. Logs:
`out/ble-s3.1b-{s1,s2,s3}-tests.log`, `out/ble-s3.1b-tests.log`.
Real pinned-host F-28 runs legacy and SC, three repeat/replacement cases each,
three fresh-RPA encrypted reconnects per case, console and normal host resets,
2028/2027 ordering, capacity replacement and live explicit/absent unpair.
Corrected H-25 and per-kind timing/one-SHA-step arithmetic tests also pass.

Target option build logs: `out/ble-s3.1b-{s2,s3}-build.log` and
`out/ble-s3.1b-build.log`. Final image:
`out/felucca-ble-s3.1b.fwsc`; hash `out/felucca-ble-s3.1b.sha256`;
layout `out/BLE-S3.1B-BUILD.txt`; static DSP check
`out/ble-s3.1b-target-budget.log`. Build checks cover capacities, DMA,
call-free flash routines in RAM and HAL-only register access.

`docs/SPEC-GAPS-S3.1.md` appends round-2 review decisions. `docs/BLE-S3.1.md`
describes the implementation, evidence and exact bench checklist. Changed
paths and generated artifact hashes are saved separately under out/.

The historical 164/226-us samples lack attribution and cannot be assigned
retrospectively. Call-graph exclusion and actual SHA-operation counts locate
candidate executor paths; a 60-us/compression injected scheduling model is
explicitly not target timing. No physical <=100-us claim, stack high-water,
entropy qualification, full Core conformance or resolution of A9/B23/B24 is
asserted. No new image has been flashed.
