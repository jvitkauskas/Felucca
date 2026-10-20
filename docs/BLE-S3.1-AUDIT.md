<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S3.1 clean-room input audit

2026-10-20. Worktree `/root/fm1/Felucca-ble`, branch `ble-controller`,
uncommitted above 85cd513. No subagents were spawned. All shell commands used
this explicit workdir; the supplied `/root/fm1-toolchain` cwd was never
inspected or used. No device, USB, desktop, device-network, flashing, commit or
push operation occurred.

The three owner bench records were read first: BLE-S2.1-BENCH-R1.txt,
BLE-S3-BENCH-R1.txt and BLE-S3-ANDROID-R1.txt under out/. They are supplied
observations, not independently remeasured hardware evidence.

## Inputs and hashes

`tools/ble_s31_audit.py` creates `out/BLE-S3.1-INPUT-SHA256SUMS` from an explicit
r5 list, the retained public Core download, the three bench records and a
conservative inventory of repository files. Repository files include build
assets, vendored sources, original docs/review decisions and this work's new
source/tests/docs. The inventory is broader than files used as implementation
references; hashing a local file is not a claim to have reviewed all its prose.
Generated build/out results are excluded except the explicitly named inputs.

- All seven frozen r5 files are hashed. The live spec, trace, prior snapshots,
  specification-review tree and reconstruction sources were never opened.
- The implementation reference was the permitted repository, including its
  own prior S1/S2/S2.1/S3 code/docs/logs and vendored NimBLE 1.9.0 / TinyCrypt.
  The two retained original import manifests verify 152 local upstream files
  against their original SHA256s; no external nimble-src file is opened and
  no vendored code is modified.
- Public Bluetooth Core 5.4, Vol 4 Part E: §§6.27, 7.7.65.1, 7.7.65.10,
  7.8.38–41, 7.8.44, 7.8.77 were read against r5 and the pinned host. Source:
  https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host-controller-interface/host-controller-interface-functional-specification.html
  The downloaded HTML and parsed text are retained in out/s3.1-core54-hci.* and
  hashed. Initial direct requests failed (tool size limit / HTTP 403); a normal
  public HTML request with a cache query succeeded. Initial web search also
  returned incidental non-Core snippets; no such page was opened or used as
  an implementation reference. No implementation uses those snippets.
- No new SDK header or app-re input was required. The authorized single
  hci-usage-device.txt file was not reopened; no other app-re material was read.
- Build execution uses the same previously pinned S3 compiler/linker/objcopy/
  objdump under `/root/felucca-ble-env/jieli/toolchain`, and the existing S3
  sdk-min packaging assets. This is execution/packaging under the user's
  explicit 'same tooling as S3' build request, not a controller reference.
  Build code checks the three packaging hashes itself. No vendor Bluetooth
  firmware binary or IR was opened, linked, disassembled or inspected.
  `out/BLE-S3.1-TOOLING-PROVENANCE-SHA256SUMS` copies only the env-tooling entries
  of the old audit as historical provenance: those hashes are not claimed
  independently revalidated. No old audit script or inherited external read
  list was executed. Compiler-generated IR/disassembly are solely this open
  firmware and its open dependencies.

Forbidden paths named by input documentation were not followed. In particular,
no access to wl82-ble-spec-trace, live wl82-ble-spec, fm1-toolchain,
wl82-patch-work, other SDK directories or vendor controller artifacts occurred.
The r5 snapshot is unchanged. No permission to reopen historical inputs is
inferred from old manifests or docs.

## Results retained

- `out/ble-s3.1-s1-tests.log`, `out/ble-s3.1-s2-tests.log`,
  `out/ble-s3.1-s3-tests.log`, `out/ble-s3.1-tests.log`: complete native BLE
  suites and new privacy/store/timing/SMP integration tests (ASan/UBSan).
- `out/ble-s3.1-build.log`, `out/BLE-S3.1-BUILD.txt`,
  `out/ble-s3.1-target-budget.log`: S3.1 target build/layout/MMIO/static DSP
  checks; the package SHA is in out/felucca-ble-s3.1.sha256.
- `out/ble-s3.1-s2-build.log`, `out/ble-s3.1-s3-build.log`: historical target
  option configurations still build with this tree. Their generated shared
  build outputs are subsequently replaced by a final S3.1 build.
- `docs/SPEC-GAPS-S3.1.md`: review decisions and the additional pinned-host
  Reset cache defect demonstrated by the new integration test before its fix.
  `docs/BLE-S3.1.md`: verification boundaries and owner bench checklist.

No S3.1 hardware result, physical timing guarantee, random-source entropy
qualification or resolution of A9/B23/B24 is asserted.
