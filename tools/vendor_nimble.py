#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Offline import, from the explicitly permitted Apache NimBLE checkout only."""
from pathlib import Path
import hashlib, shutil, subprocess
src=Path('/root/nimble-src'); dst=Path('firmware/third_party/nimble')
assert subprocess.check_output(['git','-C',str(src),'rev-parse','HEAD'],text=True).strip()=='da7e3256da3ba80b232df880f40d8359311cc62e'
roots=['nimble/include','nimble/host/include','nimble/host/src','nimble/host/services/gap','nimble/host/services/gatt','nimble/transport/include','porting/nimble/include','nimble/host/util/include']
files=[]
for root in roots:
    files += [p for p in (src/root).rglob('*') if p.is_file() and p.suffix in ('.c','.h')]
files += [src/'porting/nimble/src'/n for n in ['os_mbuf.c','os_mempool.c','os_msys.c','mem.c','endian.c']]
files += [src/'nimble/transport/src/transport.c',src/'nimble/transport/src/monitor.c']
files += [src/n for n in ['LICENSE','NOTICE'] if (src/n).is_file()]
manifest=[]
for p in sorted(set(files)):
    out=dst/p.relative_to(src); out.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(p,out); manifest.append(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+str(p))
Path('out/BLE-S2-NIMBLE-INPUT-SHA256SUMS').write_text('\n'.join(manifest)+'\n')
(dst/'PINNED.md').write_text('Apache NimBLE 1.9.0, tag nimble_1_9_0_tag\nCommit da7e3256da3ba80b232df880f40d8359311cc62e\nOffline source subset; original Apache-2.0 notices retained.\nNo NimBLE controller is compiled. See firmware/ble/port for the Felucca NPL.\n')
