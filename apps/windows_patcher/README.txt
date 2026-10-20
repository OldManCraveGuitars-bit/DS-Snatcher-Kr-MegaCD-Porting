Windows EXE wrapper for the immutable Snatcher DS source kit v26.

Build on Windows x64 with Python 3.13, PyInstaller 6.22.3, numpy 2.5.1,
Pillow 12.3.0, unicorn 2.1.4 and the bundled Python tkinter runtime.

From the project root:
  python -X utf8 -B apps/windows_patcher/build_exe.py

Before that build, extract the official pyinstaller-6.22.3.tar.gz source to
work/exe-patcher-bootloader/pyinstaller-6.22.3 and build its launcher:
  cd work/exe-patcher-bootloader/pyinstaller-6.22.3/bootloader
  python -X utf8 -B waf all --gcc --no-cfg
Use a Windows x64 MinGW compiler on the build process PATH. The verified build
used winlibs GCC 16.1.0 UCRT r3. Return to the project root before build_exe.py.
The official PyInstaller launcher is rebuilt for Unicorn's longjmp compatibility.
No Windows security setting or global Python installation is modified.
The builder rejects a CFG-enabled launcher. Official background:
  https://github.com/pyinstaller/pyinstaller/issues/9352
  https://github.com/pyinstaller/pyinstaller/pull/9390

The builder checks the sealed package manifest and every original-free input
before invoking PyInstaller. Output: work/exe-patcher-v1/dist/Snatcher-DS-Patcher.exe.
The executable embeds the converter source package and its source/masked engine,
not an already-converted NDS or any original game/BIOS/media payload.

The build uses the package at:
  work/r22-resources/release-owned-final-source-candidate-v26/패처 폴더
Unpack Snatcher-DS-source-kit-v26-ko081.zip at that location when reconstructing
the wrapper from the distributed source bundle. No ROM or BIOS is needed to
build the EXE. User-owned originals are required to generate an NDS afterward.

Verification:
  python -X utf8 -B apps/windows_patcher/verify_exe.py
This checks actual EXE conversion, error paths and cancellation using the same
GUI callbacks, Unicode/space paths, and a PATH containing only Windows System32.
Adjust SOURCE and BIOS constants to your owned originals before running it.
The verification directory is new-only and cannot overwrite an earlier run.
Desktop automation is separate; callback tests are not a manual UI inspection.
The actual frozen EXE also constructs the full Tcl/Tk window in a hidden check.
Use --work exe-patcher-v1-r4 for the final verified build and its tests.
The optional release packager package_exe.py also needs tzdata on Windows for
its Asia/Seoul timestamp. This is a packaging-only dependency, not an EXE requirement.

Tcl 8.6.15 license provenance:
  https://raw.githubusercontent.com/tcltk/tcl/core-8-6-15/license.terms
The 8.6.15 and 8.6.16 license files were checked byte-identical.

GUI wrapper source in this directory: MIT License (see LICENSE.txt).
Engine, converter and bundled dependencies retain their respective licenses.
