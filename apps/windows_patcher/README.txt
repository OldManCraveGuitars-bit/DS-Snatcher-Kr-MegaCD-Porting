Windows EXE patcher v0.811 (Mega-CD Korean patch 0.81).

The source-kit/ directory at the checkout root contains the manifest-verified
converter. v0.811 adds the NitroFS cartridge marker after FAT. Player code and
all original-derived resources are unchanged from v0.81.

Build: see docs/BUILDING.md (or source/BUILDING.md in the release ZIP).
Use build_exe.py with --pyinstaller-source pointing to the official source-built
PyInstaller 6.22.3 directory. The MinGW x64 launcher is built with --gcc --no-cfg
for Unicorn longjmp compatibility. No OS security settings are modified.

Run test_nitrofs_marker.py for padding/boundary regression coverage. Run
verify_exe.py after setting SOURCE and BIOS to the user's original files.
It checks the actual frozen EXE conversion with a System32-only PATH, Unicode
paths, GUI callbacks, invalid inputs, cancellation and final NDS identity.
This is not a manual desktop click test. Output: work/exe-patcher-v0811/.

Use package_v0811.py for this release. package_exe.py is the old internal v0.81
packager and is retained only as historical source.

Tcl 8.6.15 license provenance:
https://raw.githubusercontent.com/tcltk/tcl/core-8-6-15/license.terms
GUI wrapper: MIT License. Engine, converters and dependencies keep their own licenses.
