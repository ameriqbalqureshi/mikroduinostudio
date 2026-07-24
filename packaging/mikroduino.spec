# PyInstaller spec for MikroDuino Studio — bundles the PyQt6 app together with
# the SDK headers and the vendored avr-gcc / avrdude toolchains so the
# resulting build needs nothing else installed on the target machine.
#
# Build from the repo root:
#   pyinstaller packaging/mikroduino.spec --distpath dist --workpath build --noconfirm
#
# Produces an ONEDIR build (dist/MikroDuinoStudio/) rather than onefile: the
# bundled toolchain alone is >400 MB, and onefile mode would re-extract that
# to a temp directory on every launch. onedir mirrors how the Arduino IDE
# itself ships.

import os

REPO_ROOT = os.path.dirname(os.path.abspath(SPECPATH))

block_cipher = None

a = Analysis(
    [os.path.join(REPO_ROOT, "apps", "ide", "main.py")],
    pathex=[os.path.join(REPO_ROOT, "apps", "ide")],
    binaries=[],
    datas=[
        (os.path.join(REPO_ROOT, "sdk"), "sdk"),
        (os.path.join(REPO_ROOT, "docs"), "docs"),
        (os.path.join(REPO_ROOT, "Examples", "core"), os.path.join("Examples", "core")),
        (os.path.join(REPO_ROOT, "Examples", "Modules"), os.path.join("Examples", "Modules")),
        (os.path.join(REPO_ROOT, "apps", "ide", "ide", "assets"), os.path.join("ide", "assets")),
        (
            os.path.join(REPO_ROOT, "tools", "toolchain", "avr8-gnu-toolchain-win32_x86_64"),
            os.path.join("toolchain", "avr-gcc"),
        ),
        (
            os.path.join(REPO_ROOT, "tools", "toolchain", "avrdude-6.3-mingw32"),
            os.path.join("toolchain", "avrdude"),
        ),
    ],
    hiddenimports=["serial.tools.list_ports"],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    cipher=block_cipher,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="MikroDuinoStudio",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=False,
)

coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=False,
    name="MikroDuinoStudio",
)
