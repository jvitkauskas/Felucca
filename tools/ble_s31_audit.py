#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""S3.1 audit: r6 and local inputs only. Never inherit old external read lists."""
from pathlib import Path
import hashlib, subprocess
ROOT=Path(__file__).resolve().parents[1]
SPEC=Path('/root/wl82-ble-spec-snapshots/r6')
NAMES=('RULES.md','00-overview.md','01-radio-bringup.md','02-baseband.md',
       '03-link-layer-hci.md','04-platform.md','05-acceptance-tests.md')
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def manifest(paths,out):
    out.write_text(''.join(f'{sha(p)}  {p}\n' for p in sorted(set(paths))))
# Explicit list: no live-spec/review/trace/older-snapshot traversal.
external=[SPEC/n for n in NAMES]
public=sorted((ROOT/'out').glob('s3.1-core54-*.html'))+sorted((ROOT/'out').glob('s3.1-core54-*.txt'))
bench=[ROOT/'out'/n for n in ('BLE-S2.1-BENCH-R1.txt','BLE-S3-BENCH-R1.txt','BLE-S3-ANDROID-R1.txt','BLE-S3.1-BENCH-R1.txt')]
raw=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z'],cwd=ROOT)
local=[]
for b in raw.split(b'\0'):
    if not b: continue
    p=ROOT/b.decode()
    if p.parts[len(ROOT.parts)] in ('out','build'): continue
    if not p.is_file(): continue
    assert p.resolve().is_relative_to(ROOT),p
    local.append(p)
# Hash local copies against the retained import manifests, without opening their
# historical /root/nimble-src paths. This preserves Apache/TinyCrypt provenance.
count=0
for name in ('BLE-S2-NIMBLE-INPUT-SHA256SUMS','BLE-S3-TINYCRYPT-INPUT-SHA256SUMS'):
    for line in (ROOT/'out'/name).read_text().splitlines():
        expected,old=line.split('  ',1)
        rel=Path(old).relative_to('/root/nimble-src')
        p=ROOT/'firmware/third_party/nimble'/rel
        assert sha(p)==expected,p
        count+=1
manifest([*external,*public,*bench,*local],ROOT/'out/BLE-S3.1b-INPUT-SHA256SUMS')
# Existing tooling's historical fingerprints are provenance only. They are not
# permission to read any other external tree and are not claimed revalidated.
lines=[s for s in (ROOT/'out/BLE-S3-INPUT-SHA256SUMS').read_text().splitlines()
       if s.split('  ',1)[1].startswith('/root/felucca-ble-env/')]
(ROOT/'out/BLE-S3.1b-TOOLING-PROVENANCE-SHA256SUMS').write_text('\n'.join(lines)+'\n')
print(f'{len(external)} r6 files; {len(public)} public Core artifacts; {len(bench)} bench records; {len(local)} repository inputs hashed')
print(f'{count} imported NimBLE/TinyCrypt files match retained upstream SHA256s; no upstream source edits')
print(f'{len(lines)} historical tooling fingerprints retained (not independently revalidated)')
