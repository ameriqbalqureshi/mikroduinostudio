"""Resolve bundled-resource paths, whether running from source or a PyInstaller build.

When frozen by PyInstaller, ``sys._MEIPASS`` points at the onedir/onefile
extraction root where ``packaging/mikroduino.spec`` places ``sdk/``,
``toolchain/`` and ``ide/assets`` (see the spec's ``datas``). When running
from source, the same relative layout is derived from this file's location.
"""

from __future__ import annotations

import os
import sys


def _base_dir() -> str:
    if getattr(sys, "frozen", False):
        return getattr(sys, "_MEIPASS", os.path.dirname(sys.executable))
    return os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))


BASE_DIR = _base_dir()

SDK_ROOT = os.path.join(_base_dir(), "sdk")
ASSETS_DIR = os.path.join(os.path.dirname(__file__), "assets")

# Markdown documentation — guides live under DOCS_ROOT, SDK library/module
# reference lives under SDK_ROOT (sdk/docs, sdk/modules/*/README.md). See
# packaging/mikroduino.spec's `datas` for how DOCS_ROOT gets bundled.
DOCS_ROOT = os.path.join(_base_dir(), "docs")

# Native .mdp example projects shipped with the IDE, split into the two
# submenus under File > Examples: core-peripheral walkthroughs and
# module-driver demos. Both live under the single Examples/ tree in the
# repo (Examples/core, Examples/Modules) — see mikroduino.spec's `datas`
# for how they get bundled into a frozen build. Casing matches the on-disk
# folder names exactly (matters on case-sensitive filesystems).
EXAMPLES_ROOT = os.path.join(_base_dir(), "Examples", "core")
SAMPLES_ROOT  = os.path.join(_base_dir(), "Examples", "Modules")

if getattr(sys, "frozen", False):
    # Layout produced by packaging/mikroduino.spec's `datas`.
    TOOLCHAIN_ROOT = os.path.join(_base_dir(), "toolchain")
    AVR_GCC_BIN    = os.path.join(TOOLCHAIN_ROOT, "avr-gcc", "bin")
    AVRDUDE_DIR    = os.path.join(TOOLCHAIN_ROOT, "avrdude")
else:
    # Vendored toolchain layout under tools/toolchain/ in the source tree.
    TOOLCHAIN_ROOT = os.path.join(_base_dir(), "tools", "toolchain")
    AVR_GCC_BIN    = os.path.join(TOOLCHAIN_ROOT, "avr8-gnu-toolchain-win32_x86_64", "bin")
    AVRDUDE_DIR    = os.path.join(TOOLCHAIN_ROOT, "avrdude-6.3-mingw32")
