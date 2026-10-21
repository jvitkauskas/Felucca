# Felucca licensing

Felucca is free software, licensed under the GNU General Public License, version 3 only
(`GPL-3.0-only`, full text in `LICENSE`). That covers the code and its own assets. A few
bundled or ported parts keep their own licences; they are listed below, and their licence
texts are in `LICENSES/`.

Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments

## What is GPL-3.0-only

Every file in this tree that carries an `SPDX-License-Identifier: GPL-3.0-only` header:

- the firmware: `firmware/` (app, HAL, update loader)
- the build script and tools: `build.sh`, `tools/`
- the web pages (installer, editor) and their tests: `web/` (not the Fukiai font, below)
- the host tests: `tests/`

Three source files are ports and keep the licence of their originals:
`firmware/src/phys_dsp.c` (DaisySP, MIT), `firmware/src/phys_symp.c` (Rings, MIT) and
`firmware/src/fm6_core.c` (msfa, Apache-2.0). The firmware built with them is GPL-3.0-only as
a whole.

You may use, study, change and share Felucca under the GPL. If you distribute Felucca, or
firmware derived from it, you must also give your recipients its complete corresponding
source under the same licence. That includes devices that ship with modified Felucca inside.

## Hügelton Instruments' own work

All by Hügelton Instruments (Leo Kuroshita), in this tree:

| What | Licence | Where |
| --- | --- | --- |
| Felucca: firmware, tools, web pages, tests | GPL-3.0-only | `LICENSE` |
| The PHASE engine's waveforms: a C port of the oscillator of CrispyZebra (<https://github.com/hugelton/CrispyZebra>) | GPL-3.0 | `firmware/src/eng_phase.c` |
| The DRUM voices and kit | GPL-3.0-only | `firmware/src/drum_voice.c`, `firmware/src/eng_drum.c` |
| The Hügelton Sample Pack: Felucca's drum sounds, made by `tools/gen_waves.py`, from which the SLICE engine's BREAK is built (not CC0) | GPL-3.0-only | `tools/gen_waves.py` |
| The controls picture | GPL-3.0-only | `docs/controls.jpg` |
| The Fukiai icon font (<https://github.com/hugelton/Fukiai>): the firmware's icons (rasterised at build time by `tools/gen_aa_icons.py`) and the web editor's | MIT | `web/fukiai.ttf`, `LICENSES/MIT-Fukiai.txt`, `web/FUKIAI-LICENSE.txt` |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Inter Tight font by The Inter Project Authors: the UI text, rasterised into the firmware at build time (`tools/gen_aa_font.py`; the generated tables are not offered as a font, and the font declares no Reserved Font Name) | SIL OFL 1.1 | `assets/fonts/InterTight[wght].ttf`, `LICENSES/OFL-InterTight.txt` (also `assets/fonts/OFL.txt`) |
| Instrument samples (Versilian Studios VSCO-2 Community Edition, VCSL) | CC0 1.0 | `assets/samples-cc0/`, provenance in `ATTRIBUTION.txt` there |
| DaisySP by Electrosmith, Corp and Emilie Gillet (<https://github.com/electro-smith/DaisySP>): the PHYS engine's modal and string models and the resonator, ported to fixed point | MIT | `firmware/src/phys_dsp.c`, `LICENSES/MIT-DaisySP.txt` |
| Rings by Emilie Gillet (<https://github.com/pichenettes/eurorack>): the PHYS engine's sympathetic strings, ported to fixed point | MIT | `firmware/src/phys_symp.c`, `LICENSES/MIT-Rings.txt` |
| msfa by Google Inc. and Pascal Gauthier, from Dexed (<https://github.com/asb2m10/dexed>): the FM6 engine's synthesis, ported to integer C (Dexed itself is GPL-3.0; only msfa is used; the FM6 factory patches are Felucca's own) | Apache-2.0 | `firmware/src/fm6_core.c`, `LICENSES/Apache-2.0-msfa.txt` |
| klattsch by Tony Gies (<https://github.com/tgies/klattsch>): design reference for the VOICE engine; no code copied. Formant data from Klatt (1980) / Hillenbrand et al. (1995) | MIT (klattsch) | credit only |
| JieLi AC79 SDK by JieLi Technology: three of its files go into every `.fwsc` package (below); none are in this tree | Apache-2.0 | `LICENSES/Apache-2.0.txt` |

On the device, HOME held > ABOUT opens the information screen; turning PRESETS scrolls on into
CREDITS, a short list of these authors, licences and source URLs.

## JieLi SDK files in the packages (Apache-2.0)

A `.fwsc` package made by `tools/build.py` (with `tools/fm1pkg_make.py`; this is the package the
web installer installs) holds three unmodified files from the JieLi AC79 SDK
(<https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK>, `cpu/wl82/tools/`). They are read from your SDK
checkout at build time (see BUILDING.md); no SDK files are in this tree.

| File in the package | What it is |
| --- | --- |
| `uboot.boot` | the first-stage boot loader (SPL) |
| `cfg_tool.bin` | the chip configuration block |
| `eq_cfg_hw.bin` | the default hardware EQ table |

They are licensed under the Apache License, Version 2.0 (`LICENSES/Apache-2.0.txt`), not under
the GPL, and their copyright stays with JieLi Technology. The SDK has no NOTICE file. Apache-2.0
files may be distributed together with GPL-3.0 code; Felucca itself stays GPL-3.0-only.

Every distribution of a package carries the licence texts with it: the release folder
(`tools/build.py --release`) and the site (`web/make_site.py`, next to the package in
`firmware/` and linked from the installer page).

## No vendor material

The firmware links no vendor object code or Bluetooth library. With the default build, the
packages hold Felucca and the three SDK files above. The optional BLE build also contains
numerical radio configuration tables transcribed from the frozen clean-room specification;
see `docs/BLE-S1.md` for the precise provenance and its unresolved redistribution policy. The installer's
"Return to official V15" holds only the official file's size and SHA-256: you select the
official firmware file you downloaded yourself, and it is checked and installed in your browser,
never uploaded or redistributed.

## Contributions

Contributions are welcome under GPL-3.0-only.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments.

"M-VAVE" and "FM-1" are trademarks of their respective owners. Felucca is independent
firmware that runs on FM-1 hardware. It is not affiliated with, endorsed by or supported
by those owners.

## Radio

The default build leaves the radio disabled. `FELUCCA_BLE=1` enables the experimental
clean-room BLE S2 controller, Apache NimBLE host and BLE-MIDI; see
`docs/BLE-S2.md`. Pairing and encryption belong to S3.

## Apache NimBLE host in BLE builds

`firmware/third_party/nimble/` contains an offline source subset of Apache
NimBLE 1.9.0, commit `da7e3256da3ba80b232df880f40d8359311cc62e`.
Its original Apache-2.0 copyright notices, LICENSE and NOTICE are retained.
Only its host, GAP/GATT services, transport memory pools and porting utilities
are linked. The Felucca NPL, H4 glue, controller and MIDI application are
MIT-licensed new files (next section). See `docs/BLE-S2-AUDIT.md` and the import hash manifest.

S3 also links the pinned NimBLE checkout's bundled TinyCrypt AES, CMAC, SHA-256
and P-256 ECC sources under `firmware/third_party/nimble/ext/tinycrypt/`.
The original BSD-3-Clause / Kenneth MacKay notices, LICENSE and AUTHORS are
retained unchanged. The combined firmware remains GPL-3.0-only. See
`out/BLE-S3-TINYCRYPT-INPUT-SHA256SUMS` and `docs/BLE-S3-AUDIT.md`.

## BLE controller files (MIT)

The files added by the BLE work are MIT-licensed (`SPDX-License-Identifier: MIT`,
text in `LICENSES/MIT-BLE.txt`, Copyright (c) 2026 Julius Vitkauskas). These are the
new files under `firmware/ble/` (including `port/`), `firmware/hal/fm1_ble.h`,
`fm1_ble_crypto.h` and `fm1_ble_radio.h`, `firmware/src/ble_console.c` and
`ble_platform.c`, `tests/ble_*` with the BLE test runners, `tests/compile_nimble.py`,
`tools/ble_*.py`, `tools/nimble_build.py`, `tools/vendor_nimble.py`, and the
`docs/BLE-*.md` and `docs/SPEC-GAPS-*.md` documents. MIT is compatible with the GPL, so
firmware built with them remains GPL-3.0-only as a whole, as above.

Not covered by the MIT grant:
- the changes the BLE work made to existing Felucca files (for example `main.c`,
  `console.c`, `app.ld`): those files stay GPL-3.0-only;
- `firmware/hal/fm1_ble_tables.h`: numerical radio configuration tables transcribed from
  the clean-room specification. Their redistribution policy is unresolved (see "No vendor
  material" and `docs/BLE-S1.md`), so the file keeps its GPL-3.0-only header and no
  further rights are granted for it;
- the vendored Apache NimBLE (Apache-2.0) and TinyCrypt (BSD-3-Clause) sources above.
