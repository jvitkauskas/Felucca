#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Save the current local build as a named BLE-stage artifact. No device I/O."""
from pathlib import Path
import hashlib, re, shutil, subprocess, sys
root=Path(__file__).resolve().parents[1]
stage=sys.argv[1]
assert re.fullmatch(r's[23](?:\.1[bcd]?)?',stage)
tool=Path('/root/felucca-ble-env/jieli/toolchain/common/bin/objdump')
symbols=subprocess.check_output([str(tool),'-t',str(root/'build/felucca.elf')],text=True)
sym={line.split()[-1]:int(line.split()[0],16) for line in symbols.splitlines() if re.match(r'^[0-9a-f]{8} ',line)}
for suffix in ('elf','dis','bin'):
    shutil.copyfile(root/f'build/felucca.{suffix}',root/f'build/felucca-ble-{stage}.{suffix}')
package=root/f'out/felucca-ble-{stage}.fwsc'
shutil.copyfile(root/'build/felucca.fwsc',package)
sha=hashlib.sha256(package.read_bytes()).hexdigest()
(root/f'out/felucca-ble-{stage}.sha256').write_text(f'{sha}  out/{package.name}\n')
app=root/'build/felucca.bin'
base=subprocess.check_output(['git','rev-parse','--short','HEAD'],cwd=root,text=True).strip()
snapshot='r7' if stage in ('s3.1c','s3.1d') else 'r6' if stage=='s3.1b' else 'r5' if stage=='s3.1' else 'stage regression configuration (current tree)'
ram=sym['_bss_end']-sym['_data_start']; pool=sym['_pool_end']-sym['_pool_start']
report=f'''Felucca BLE {stage} — uncommitted above {base}; {snapshot}
Package: {package.name}; {package.stat().st_size} B
SHA256: {sha}
App: {app.stat().st_size} / 581564 B; free {581564-app.stat().st_size} B
App SHA256: {hashlib.sha256(app.read_bytes()).hexdigest()}
Static RAM allocation incl. alignment: {ram} / 98304 B; free {98304-ram} B
Pool incl. DMA: {pool} / 344064 B; free {344064-pool} B
DMA: {sym['_ble_dma_end']-sym['_ble_dma_start']} B at {sym['_ble_dma_start']:#010x}
Trace: {sym['_ble_trace_end']-sym['_ble_trace_start']} B at {sym['_ble_trace_start']:#010x}
RAM text: {sym['_rt_end']-sym['_rt_start']} B
User / supervisor stacks: 24320 / 7936 B, physical high water unmeasured
No device, USB, device-network, flashing, commit or push operation.
'''
(root/f'out/BLE-{stage.upper()}-BUILD.txt').write_text(report)
print(report)
