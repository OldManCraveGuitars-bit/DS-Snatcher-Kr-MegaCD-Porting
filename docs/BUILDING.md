# Source and developer rebuild

Normal users should follow PATCHING_KO.md. All original artwork, audio, BIOS and two original lookup tables are supplied from the user's disc/BIOS at patch time. The distributed runner and host libraries have those original lookup bytes zeroed; patch metadata records offsets and hashes only.

## Host conversion helpers

The Windows x64 libraries were compiled with GCC 16.1.0 (MinGW-w64/UCRT). Sources are included in `player/source`, `patcher/source_score_driver.c`, `tools/native_sega_z80_r22.c`, `tools/native_fm_synth_r22.c`, and the corresponding `third_party` directories. No compiler is bundled. Keep all license notices when modifying or distributing them.

```powershell
py -3 -X utf8 -B patcher/rebuild_helpers.py --source "D:/Original/Snatcher (USA).cue" --bios "D:/BIOS/bios_CD_U.bin" --output "D:/Helper-Build" --gcc "C:/mingw64/bin/gcc.exe"
```

This builds boot.dll, room.dll, the original-driver converter.dll, score.dll and startup.dll in their respective work directories. It reads original inputs locally. The produced binaries contain locally restored source tables; the distributed binaries are masked versions. `host/windows-x64/manifest.json` lists both identities. This command forces source compilation and does not reuse bundled libraries.

## NDS player

The runner is the v41 port program with game-resource files removed. `player/source`, `player/arm7`, `player/include`, `player/generated`, and the two Makefiles contain its build sources. Unused legacy bitmap arrays were omitted from office_images.h; none is linked into the runner. Other generated headers contain Korean translation/font data, port UI, and source-command/placement metadata, not original game bitmap or sound samples.

Install the Wonderful/BlocksDS ARM toolchain and its libnds/Maxmod dependencies. The storage wrapper is pinned to libnds commit `4674cdfed005eb9a3699f4896e5c587ebc8b6a09` and wf-fatfs commit `2b5c26e2a1eb4cdbbcb57070d49a6ebf7b60c7f9`; `player/include/storage_compat/sdk.json` records the checked library hash. The build intentionally rejects an unaudited incompatible storage ABI.

After running the patcher, copy the 203 resource files listed by `engine/resources.json` from the output resources directory into `player/nitrofiles/` (exclude conversion logs and work subdirectories). Copy the two generated headers from output `build-inputs/` into `player/generated/`.

From the BlocksDS shell inside `player`, run:

```sh
make -f Makefile.arm7 -j2
make -j2
```

The resource manifest is validated before packaging. The included prebuilt runner is checked by hash during normal patching; rebuilding with a different SDK/compiler may change program bytes. A source rebuild is a developer operation, separate from the verified end-user patch command.

## Engine packing

`patcher/assemble_demo.py` assembles the audited ARM9/ARM7 prefix, banner, a flat NitroFS name/allocation table and validated generated resource files. It recalculates the header CRC and ROM length/capacity. `engine/runner.json` describes masked SUBCODE tables restored at patch time. No original disc sectors, converted game audio or game pictures are supplied inside this engine template. Introductory screenshots are separate documentation assets.
