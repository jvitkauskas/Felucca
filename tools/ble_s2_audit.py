#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Hash the explicit S2 external input list; no external discovery/search."""
from pathlib import Path
import hashlib,subprocess,sys,shlex
sys.path.insert(0,str(Path(__file__).resolve().parent))
import nimble_build as nb
root=Path(__file__).resolve().parents[1]
inputs=[]
for name in ['RULES.md','00-overview.md','01-radio-bringup.md','02-baseband.md','03-link-layer-hci.md','04-platform.md','05-acceptance-tests.md']:
    inputs.append(Path('/root/wl82-ble-spec-snapshots/r3')/name)
for name in ['IMPLEMENTABILITY-gpt-6.1-sol.md','IMPLEMENTABILITY-r2-gpt-6.1-sol.md','RESPONSES-r1.md','RESPONSES-r2.md','PLATFORM-felucca.md']:
    inputs.append(Path('/root/wl82-ble-spec-review')/name)
inputs += [Path('/root/fm1-app-re/nimble/hci-usage-device.txt')]
# Bench round 1: public driver headers were searched for CPU exception bits
# and timer/IRQ definitions. Record every header searched, not just matches.
inputs += [Path(p) for p in subprocess.check_output(
    ['rg','--files','/root/sdk-blemidi/include_lib/driver','-g','*.h'],text=True).splitlines()]
for name in ['nimble/host/syscfg.yml','porting/nimble/Makefile.defs','porting/npl/linux/include/nimble/nimble_npl_os.h','porting/nimble/src/nimble_port.c']:
    inputs.append(Path('/root/nimble-src')/name)
for line in (root/'out/BLE-S2-NIMBLE-INPUT-SHA256SUMS').read_text().splitlines():
    inputs.append(Path(line.split('  ',1)[1]))
for name in ['uboot.boot','cfg_tool.bin','cfg/eq_cfg_hw.bin']:
    inputs.append(Path('/root/felucca-ble-env/sdk-min/cpu/wl82/tools')/name)
compiler=Path('/root/felucca-ble-env/jieli/toolchain/pi32v2/bin/clang')
headers=set()
for source in [root/'firmware/src/felucca.c',*nb.sources(),root/'firmware/ble/ble_npl.c',root/'firmware/ble/ble_host.c']:
    flags=nb.flags(target=True) if source.name!='felucca.c' else ['-DFELUCCA_BLE=1','-Ifirmware/hal','-Ifirmware/src','-Ibuild/gen']
    r=subprocess.run([str(compiler),'-target','pi32v2','-M','-std=c11',*flags,str(source)],cwd=root,check=True,capture_output=True,text=True)
    for word in shlex.split(r.stdout.replace('\\\n',' '))[1:]:
        p=Path(word)
        if p.is_absolute() and str(p).startswith('/root/felucca-ble-env/'):
            headers.add(p.resolve())
inputs += sorted(headers)
inputs=sorted(set(inputs))
manifest=''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+str(p)+'\n' for p in inputs)
(root/'out/BLE-S2-INPUT-SHA256SUMS').write_text(manifest)
print(f'{len(inputs)} explicit external input files; {len(headers)} compiler headers')
for p in sorted(headers): print(p)
