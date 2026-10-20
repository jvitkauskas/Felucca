<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S3.1b round-2 clean-room audit — frozen r6

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
