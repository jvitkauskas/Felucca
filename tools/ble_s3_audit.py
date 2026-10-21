#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Explicit allowed-input audit. Never opens a superseded snapshot."""
from pathlib import Path
import hashlib,os,subprocess,sys,shlex
root=Path(__file__).resolve().parents[1]
os.environ['FELUCCA_BLE_STAGE']='3'
sys.path.insert(0,str(root/'tools'))
import nimble_build as nb
paths=set()
# Historical manifest is provenance, not authority to open old snapshots.
for line in (root/'out/BLE-S2-INPUT-SHA256SUMS').read_text().splitlines():
    p=line.split('  ',1)[1]
    if p.startswith('/root/wl82-ble-spec-snapshots/'): continue
    paths.add(Path(p))
paths.update(Path('/root/wl82-ble-spec-snapshots/r4')/n for n in ('RULES.md','00-overview.md','01-radio-bringup.md','02-baseband.md','03-link-layer-hci.md','04-platform.md','05-acceptance-tests.md'))
paths.update(Path('/root/wl82-ble-spec-review')/n for n in ('RESPONSES-r3.md','IMPLEMENTABILITY-r3-gpt-6.1-sol.md'))
for line in (root/'out/BLE-S3-TINYCRYPT-INPUT-SHA256SUMS').read_text().splitlines(): paths.add(Path(line.split('  ',1)[1]))
paths.update(Path('/root/nimble-src/porting/nimble/src').glob('*.c'))
paths.add(Path('/root/nimble-src/porting/nimble/Makefile.tinycrypt'))
compiler=Path('/root/felucca-ble-env/jieli/toolchain/pi32v2/bin/clang')
for source in [root/'firmware/src/felucca.c',*nb.sources(),*(root/'firmware/ble'/n for n in ('ble_host.c','ble_npl.c','ble_seed.c','ble_bonds.c'))]:
    flags=nb.flags(target=True) if source.name!='felucca.c' else ['-DFELUCCA_BLE=1','-DFELUCCA_BLE_STAGE=3','-Ifirmware/hal','-Ifirmware/src','-Ibuild/gen']
    r=subprocess.run([str(compiler),'-target','pi32v2','-M','-std=c11',*flags,str(source)],cwd=root,check=True,capture_output=True,text=True)
    for word in shlex.split(r.stdout.replace('\\\n',' '))[1:]:
        if word.startswith('/root/felucca-ble-env/'): paths.add(Path(word).resolve())
allowed=('/root/felucca-ble-env/','/root/nimble-src/','/root/sdk-blemidi/include_lib/driver/','/root/wl82-ble-spec-snapshots/r4/','/root/wl82-ble-spec-review/')
for p in paths:
    assert str(p).startswith(allowed) or str(p)=='/root/fm1-app-re/nimble/hci-usage-device.txt',p
manifest=''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+str(p)+'\n' for p in sorted(paths))
(root/'out/BLE-S3-INPUT-SHA256SUMS').write_text(manifest)
# Every upstream source imported into the worktree stays byte-for-byte identical.
for name in ('BLE-S2-NIMBLE-INPUT-SHA256SUMS','BLE-S3-TINYCRYPT-INPUT-SHA256SUMS'):
    for line in (root/'out'/name).read_text().splitlines():
        sha,p=line.split('  ',1); copy=root/'firmware/third_party/nimble'/Path(p).relative_to('/root/nimble-src')
        assert hashlib.sha256(copy.read_bytes()).hexdigest()==sha,copy
print(f'{len(paths)} allowed external inputs hashed; upstream copies unchanged')
