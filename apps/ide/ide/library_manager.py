"""MikroDuino IDE — Library Manager dialog.

Supports installing Arduino-compatible libraries from:
  - Local .zip file
  - GitHub repository URL  (downloads archive/refs/heads/main.zip or master.zip)
  - Any direct .zip download URL
"""

from __future__ import annotations

import json
import os
import re
import shutil
import ssl
import tempfile
import urllib.request
import zipfile

from PyQt6.QtCore import Qt, QThread, pyqtSignal
from PyQt6.QtWidgets import (
    QDialog, QFileDialog, QGroupBox, QHBoxLayout, QLabel,
    QLineEdit, QListWidget, QListWidgetItem, QMessageBox,
    QPlainTextEdit, QPushButton, QSplitter, QVBoxLayout, QWidget,
)

from . import respaths


# ── Library directory ─────────────────────────────────────────────────────────

def lib_dir() -> str:
    """Return (and create if needed) the user's global library directory."""
    d = os.path.join(os.path.expanduser("~"), ".mikroduino", "libraries")
    os.makedirs(d, exist_ok=True)
    return d


def _parse_properties(path: str) -> dict:
    """Parse a library.properties file into a dict."""
    props: dict = {}
    try:
        with open(path, encoding="utf-8", errors="ignore") as f:
            for line in f:
                line = line.strip()
                if "=" in line and not line.startswith("#"):
                    k, _, v = line.partition("=")
                    props[k.strip()] = v.strip()
    except OSError:
        pass
    return props


_EXAMPLE_EXTS = frozenset({".ino", ".cpp", ".c", ".hpp", ".h"})


def list_examples(library_dir_path: str) -> list[dict]:
    """Return [{name, path, files}] for each example in library_dir_path/examples/."""
    result: list[dict] = []
    ex_dir = os.path.join(library_dir_path, "examples")
    if not os.path.isdir(ex_dir):
        return result
    try:
        for entry in sorted(os.scandir(ex_dir), key=lambda e: e.name.lower()):
            if not entry.is_dir():
                continue
            files = sorted(
                f.path for f in os.scandir(entry.path)
                if f.is_file() and os.path.splitext(f.name)[1].lower() in _EXAMPLE_EXTS
            )
            if files:
                result.append({"name": entry.name, "path": entry.path, "files": files})
    except OSError:
        pass
    return result


def arduino_primary_header(lib_dir_path: str) -> str | None:
    """Return the basename of the header a project should #include for the
    Arduino-compatible library at lib_dir_path — the one the builder's own
    _resolve_libraries() would actually pick up (see builder.py): headers
    live under src/ for the modern (1.5+) library layout, or the library
    root for the legacy flat layout. Prefers a header whose stem matches
    the library's folder name; falls back to the first header found."""
    src_sub = os.path.join(lib_dir_path, "src")
    inc_root = src_sub if os.path.isdir(src_sub) else lib_dir_path
    try:
        headers = sorted(
            f.name for f in os.scandir(inc_root) if f.name.endswith((".h", ".hpp"))
        )
    except OSError:
        return None
    if not headers:
        return None
    folder = os.path.basename(lib_dir_path.rstrip(os.sep))
    return next((h for h in headers if os.path.splitext(h)[0] == folder), headers[0])


def list_sdk_modules() -> list[dict]:
    """Return info dicts for every driver module under sdk/modules/ — the
    MikroDuino-native equivalent of list_installed() for Arduino libraries.

    Unlike Arduino libraries (auto-detected by builder.py from #include
    statements alone — see its _resolve_libraries()), an SDK module needs
    its include dir — and, for modules with one, its .cpp — explicitly
    added to the project's build config; there is no scanning for these.
    That's why each entry carries include_dir/header_path/cpp_path: enough
    for a caller to both insert the #include and wire up the project file.
    """
    base = os.path.join(respaths.SDK_ROOT, "modules")
    mods: list[dict] = []
    try:
        entries = sorted(os.scandir(base), key=lambda e: e.name.lower())
    except OSError:
        return mods

    for entry in entries:
        if not entry.is_dir():
            continue
        info: dict = {}
        pkg_path = os.path.join(entry.path, "package.json")
        if os.path.isfile(pkg_path):
            try:
                with open(pkg_path, encoding="utf-8") as f:
                    info = json.load(f)
            except (OSError, ValueError):
                info = {}

        name = info.get("name", entry.name)
        include_dir = os.path.join(entry.path, "include")

        # Convention every module in this SDK follows: include/<Name>.hpp.
        # Fall back to whatever single header actually exists, in case a
        # module ever doesn't (there is no compile-time guarantee here).
        header_path = os.path.join(include_dir, f"{name}.hpp")
        if not os.path.isfile(header_path):
            try:
                headers = sorted(
                    f for f in os.listdir(include_dir) if f.endswith((".hpp", ".h"))
                ) if os.path.isdir(include_dir) else []
            except OSError:
                headers = []
            header_path = os.path.join(include_dir, headers[0]) if headers else ""

        cpp_path = os.path.join(entry.path, "src", f"{name}.cpp")

        mods.append({
            "dir":         entry.path,
            "name":        name,
            "version":     info.get("version", "—"),
            "description": info.get("description", ""),
            "include_dir": include_dir,
            "header_name": os.path.basename(header_path) if header_path else f"{name}.hpp",
            "cpp_path":    cpp_path if os.path.isfile(cpp_path) else "",
        })
    return mods


# ── Core SDK (sdk/core/avr/include/mikroduino/) ─────────────────────────────
#
# Two layers, per CLAUDE.md:
#   Layer 1 — hardware peripherals: always compiled in (builder.py passes
#     -I on both the core include root and .../mikroduino/ unconditionally),
#     no opt-in needed — just #include <mikroduino/X.hpp> directly. These
#     headers predate the "Name — description." doc-comment convention the
#     rest of the SDK uses, so their descriptions are hand-written below
#     (matching CLAUDE.md's own table) rather than parsed from the file.
#   Layer 2 — opt-in utilities, grouped by #define MD_INCLUDE_<GUARD>
#     (ds/, sched/, string/, codec/, power/, math/, proto/, crypto/, util/).
#     Auto-discovered by directory so a future phase's new subdirectory
#     shows up without this file needing an update.

_CORE_PERIPHERALS: list[tuple[str, str, str]] = [
    # (header filename, display name, description)
    ("gpio.hpp",      "GPIO",      "Pin I/O; encoding: bits[5:3]=port, bits[2:0]=bit"),
    ("usart.hpp",     "USART",     "UART with baud/parity/stopbits/interrupts/PROGMEM write"),
    ("spi.hpp",       "SPI",       "Master/slave, 4 modes, clock dividers"),
    ("i2c.hpp",       "I2C",       "Master/slave, bus scan, repeated start, status codes"),
    ("adc.hpp",       "ADC",       "10-bit, reference select, free-running, prescalers"),
    ("timer.hpp",     "Timer",     "Normal/CTC/PWM modes, prescalers, compare/capture"),
    ("pwm.hpp",       "PWM",       "16-bit PWM via Timer1, frequency + duty control"),
    ("interrupt.hpp", "Interrupt", "External INT0-INT7, sense modes, user callbacks"),
    ("eeprom.hpp",    "EEPROM",    "Byte/block/typed read-write-update, smart update"),
    ("registers.hpp", "Registers", "BITSET/CLEAR/READ, ATOMIC_BLOCK_START/END, MD_INLINE"),
    ("platform.hpp",  "Platform",  "MCU detection, capability flags, F_CPU guard"),
]

_UTIL_GROUP_LABELS: dict[str, str] = {
    "ds":     "Data Structures",
    "sched":  "Scheduling",
    "string": "String Utilities",
    "codec":  "Encoding / Checksums",
    "power":  "Power Management",
    "math":   "Math",
    "proto":  "Protocols",
    "crypto": "Cryptography",
    "util":   "Utilities",
}


def _core_include_root() -> str:
    return os.path.join(respaths.SDK_ROOT, "core", "avr", "include", "mikroduino")


def _first_comment_line(path: str) -> str:
    """Return the first content line of a header's opening /* ... */ block —
    every header in this SDK opens with a one-line "Name — description."
    (or, for the older Layer-1 headers, just a short title) on its own
    line right after '/*'."""
    try:
        with open(path, encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()[:12]
    except OSError:
        return ""
    started = False
    for raw in lines:
        s = raw.strip()
        if not started:
            if s == "/*":
                started = True
            continue
        if not s or s == "*/":
            break
        if s.startswith("*"):
            s = s[1:].strip()
        if s:
            return s
    return ""


def list_core_peripherals() -> list[dict]:
    """Layer-1 core hardware headers — always available, no #define guard,
    just #include <mikroduino/X.hpp>. See _CORE_PERIPHERALS above."""
    base = _core_include_root()
    mods: list[dict] = []
    for filename, label, desc in _CORE_PERIPHERALS:
        if not os.path.isfile(os.path.join(base, filename)):
            continue
        mods.append({
            "name": label,
            "include": f"mikroduino/{filename}",
            "description": desc,
        })
    return mods


def list_core_utility_groups() -> list[dict]:
    """Layer-2 opt-in utility headers, grouped by their MD_INCLUDE_<GUARD>.

    Selecting one needs '#define MD_INCLUDE_<GUARD>' placed BEFORE the
    project's #include <mikroduino/mikroduino.hpp> — that header is
    #pragma once, so the guard has no effect if defined after it's already
    been processed once. main_window.py's insertion logic finds and
    respects that existing #include rather than just inserting at the
    cursor, which is why 'guard' is returned separately from 'include'.
    """
    base = _core_include_root()
    groups: list[dict] = []
    try:
        entries = sorted(os.scandir(base), key=lambda e: e.name.lower())
    except OSError:
        return groups

    for entry in entries:
        if not entry.is_dir() or entry.name not in _UTIL_GROUP_LABELS:
            continue
        try:
            header_files = sorted(f for f in os.listdir(entry.path) if f.endswith(".hpp"))
        except OSError:
            header_files = []
        headers = [
            {
                "name": os.path.splitext(h)[0],
                "include": f"mikroduino/{entry.name}/{h}",
                "description": _first_comment_line(os.path.join(entry.path, h)),
            }
            for h in header_files
        ]
        if headers:
            groups.append({
                "group":   entry.name,
                "label":   _UTIL_GROUP_LABELS[entry.name],
                "guard":   f"MD_INCLUDE_{entry.name.upper()}",
                "headers": headers,
            })
    return groups


def _describe_mdp_dir(dir_path: str) -> dict | None:
    """Describe a single directory as one node: a 'project' node if it has
    an .mdp of its own, a 'group' node if it has example subdirectories
    (recursively), or None if it has neither.

    A directory with a .mdp file directly inside it is always treated as a
    leaf, even though it typically also has a src/ subfolder alongside that
    .mdp — every generated example project looks like that, so subfolders
    are only recursed into when the directory has no .mdp of its own.
    """
    try:
        entries = sorted(os.scandir(dir_path), key=lambda e: e.name.lower())
    except OSError:
        return None

    mdp_files = sorted(e.name for e in entries if e.is_file() and e.name.lower().endswith(".mdp"))
    if mdp_files:
        if len(mdp_files) == 1:
            return {
                "kind": "project",
                "name": os.path.basename(dir_path),
                "mdp_path": os.path.join(dir_path, mdp_files[0]),
            }
        # More than one .mdp in the same folder (unusual) — list each by its
        # own filename rather than guessing which one is "the" project.
        return {
            "kind": "group",
            "name": os.path.basename(dir_path),
            "children": [
                {"kind": "project", "name": os.path.splitext(f)[0], "mdp_path": os.path.join(dir_path, f)}
                for f in mdp_files
            ],
        }

    children: list[dict] = []
    for e in entries:
        if not e.is_dir() or e.name.startswith(".") or e.name == "build":
            continue
        node = _describe_mdp_dir(e.path)
        if node:
            children.append(node)
    if not children:
        return None
    return {"kind": "group", "name": os.path.basename(dir_path), "children": children}


def list_mdp_tree(dir_path: str) -> list[dict]:
    """Recursively describe the native .mdp example projects under dir_path.

    Returns a list of nodes, each either:
      {"kind": "group",   "name": <dir name>, "children": [...]}  — a folder
        of further examples, no .mdp of its own (e.g. "Modules", "ST7735")
      {"kind": "project", "name": <label>, "mdp_path": <path>}    — a leaf
        project folder containing an .mdp file

    dir_path itself (e.g. the examples/ or Samples/ root) is never shown as
    its own named group in the result — only its children are listed, ready
    to drop straight into a menu.
    """
    if not os.path.isdir(dir_path):
        return []
    node = _describe_mdp_dir(dir_path)
    if node is None:
        return []
    return node["children"] if node["kind"] == "group" else [node]


def list_installed() -> list[dict]:
    """Return info dicts for every library in the user library directory."""
    base = lib_dir()
    libs: list[dict] = []
    try:
        for entry in sorted(os.scandir(base), key=lambda e: e.name.lower()):
            if not entry.is_dir():
                continue
            props = _parse_properties(os.path.join(entry.path, "library.properties"))
            libs.append({
                "dir":      entry.path,
                "name":     props.get("name", entry.name),
                "version":  props.get("version", "—"),
                "author":   props.get("author", "—"),
                "sentence": props.get("sentence", ""),
            })
    except OSError:
        pass
    return libs


def _find_lib_root(extracted_dir: str) -> str:
    """Return the actual library root inside an extracted zip.
    Many zips have a single top-level folder; some are flat."""
    entries = [e for e in os.scandir(extracted_dir) if not e.name.startswith(".")]
    if len(entries) == 1 and entries[0].is_dir():
        return entries[0].path
    return extracted_dir


def install_zip(zip_path: str, log=None) -> str:
    """Extract *zip_path* and install into the user library directory.
    Returns the installed library name.  Raises on error."""

    def _log(msg: str) -> None:
        if log:
            log(msg)

    base = lib_dir()

    with zipfile.ZipFile(zip_path, "r") as zf:
        _log("Reading archive…")
        with tempfile.TemporaryDirectory() as tmp:
            _log("Extracting…")
            zf.extractall(tmp)

            lib_root = _find_lib_root(tmp)

            props = _parse_properties(os.path.join(lib_root, "library.properties"))
            raw_name = (
                props.get("name")
                or os.path.splitext(os.path.basename(zip_path))[0]
            )
            # Strip common suffixes: "-main", "-master", "-1.0.0"
            lib_name = re.sub(r"[-_](main|master|\d+[\.\d]*)$", "", raw_name).strip()
            if not lib_name:
                lib_name = raw_name.strip()

            dest = os.path.join(base, lib_name)
            if os.path.exists(dest):
                shutil.rmtree(dest)
            _log(f"Installing '{lib_name}'…")
            shutil.copytree(lib_root, dest)
            _log(f"Installed '{lib_name}'  v{props.get('version', '?')}")
            return lib_name


# ── Background download thread ────────────────────────────────────────────────

class _DownloadThread(QThread):
    log   = pyqtSignal(str)
    done  = pyqtSignal(str)   # installed library name
    error = pyqtSignal(str)   # error message

    def __init__(self, url: str, parent=None):
        super().__init__(parent)
        self._url = url.strip().rstrip("/")

    def run(self) -> None:
        try:
            tmp_zip = self._fetch()
            try:
                name = install_zip(tmp_zip, lambda m: self.log.emit(m))
                self.done.emit(name)
            finally:
                try:
                    os.unlink(tmp_zip)
                except OSError:
                    pass
        except Exception as exc:  # noqa: BLE001
            self.error.emit(str(exc))

    def _fetch(self) -> str:
        """Download the library and return a path to a temporary .zip file."""
        url = self._url

        if url.lower().endswith(".zip"):
            return self._download(url)

        m = re.match(r"https?://github\.com/([^/?#]+)/([^/?#]+)", url)
        if m:
            user, repo = m.group(1), m.group(2).removesuffix(".git")
            last_err: Exception | None = None
            for branch in ("main", "master"):
                candidate = (
                    f"https://github.com/{user}/{repo}"
                    f"/archive/refs/heads/{branch}.zip"
                )
                self.log.emit(f"Trying branch '{branch}'…")
                try:
                    return self._download(candidate)
                except Exception as e:  # noqa: BLE001
                    last_err = e
                    self.log.emit(f"  ✗ {e}")
            raise ValueError(
                f"Could not download from GitHub.\n"
                f"Last error: {last_err}\n\n"
                f"Tip: paste a direct .zip link from the GitHub Releases page."
            )

        # Unknown URL format — try as a direct download
        return self._download(url)

    def _download(self, url: str) -> str:
        ctx = ssl.create_default_context()
        req = urllib.request.Request(
            url, headers={"User-Agent": "MikroDuino-IDE/0.2"}
        )
        fd, tmp_path = tempfile.mkstemp(suffix=".zip")
        os.close(fd)
        try:
            with urllib.request.urlopen(req, context=ctx, timeout=30) as resp:
                total = int(resp.headers.get("Content-Length", 0) or 0)
                received = 0
                with open(tmp_path, "wb") as out:
                    while True:
                        chunk = resp.read(65536)
                        if not chunk:
                            break
                        out.write(chunk)
                        received += len(chunk)
                        if total:
                            pct = received * 100 // total
                            self.log.emit(f"Downloading… {pct}%")
                        else:
                            self.log.emit(f"Downloading… {received // 1024} KB")
        except Exception:
            try:
                os.unlink(tmp_path)
            except OSError:
                pass
            raise
        return tmp_path


# ── Library Manager dialog ────────────────────────────────────────────────────

class LibraryManagerDialog(QDialog):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setWindowTitle("Library Manager")
        self.resize(760, 500)
        self._thread: _DownloadThread | None = None
        self._build_ui()
        self._refresh()

    # ── UI construction ───────────────────────────────────────────────────────

    def _build_ui(self) -> None:
        root = QVBoxLayout(self)

        splitter = QSplitter(Qt.Orientation.Horizontal)

        # ── Left: installed library list ──────────────────────────────────────
        left = QWidget()
        ll = QVBoxLayout(left)
        ll.setContentsMargins(0, 0, 0, 0)
        ll.addWidget(QLabel("Installed Libraries"))

        self._lib_list = QListWidget()
        self._lib_list.currentRowChanged.connect(self._on_row_changed)
        ll.addWidget(self._lib_list)

        self._btn_remove = QPushButton("Remove Selected")
        self._btn_remove.setEnabled(False)
        self._btn_remove.clicked.connect(self._on_remove)
        ll.addWidget(self._btn_remove)

        splitter.addWidget(left)

        # ── Right: install controls ───────────────────────────────────────────
        right = QWidget()
        rl = QVBoxLayout(right)

        zip_grp = QGroupBox("Install from .ZIP file")
        zl = QHBoxLayout(zip_grp)
        self._zip_path = QLineEdit()
        self._zip_path.setPlaceholderText("Path to .zip file…")
        btn_browse = QPushButton("Browse…")
        btn_browse.clicked.connect(self._on_browse)
        btn_install_zip = QPushButton("Install ZIP")
        btn_install_zip.clicked.connect(self._on_install_zip)
        zl.addWidget(self._zip_path, stretch=1)
        zl.addWidget(btn_browse)
        zl.addWidget(btn_install_zip)
        rl.addWidget(zip_grp)

        gh_grp = QGroupBox("Install from GitHub / URL")
        ghl = QVBoxLayout(gh_grp)
        ghl.addWidget(QLabel(
            "Paste a GitHub repo URL  (e.g. https://github.com/user/repo)\n"
            "or a direct link to a .zip file from the Releases page:"
        ))
        gh_row = QHBoxLayout()
        self._gh_url = QLineEdit()
        self._gh_url.setPlaceholderText("https://github.com/user/repo")
        self._btn_gh = QPushButton("Install from GitHub")
        self._btn_gh.clicked.connect(self._on_install_github)
        gh_row.addWidget(self._gh_url, stretch=1)
        gh_row.addWidget(self._btn_gh)
        ghl.addLayout(gh_row)
        rl.addWidget(gh_grp)

        rl.addWidget(QLabel("Status / Log:"))
        self._log = QPlainTextEdit()
        self._log.setReadOnly(True)
        self._log.setMaximumHeight(160)
        rl.addWidget(self._log)
        rl.addStretch()

        splitter.addWidget(right)
        splitter.setSizes([260, 480])
        root.addWidget(splitter)

        close_row = QHBoxLayout()
        close_row.addStretch()
        close_row.addWidget(QPushButton("Close", clicked=self.accept))
        root.addLayout(close_row)

    # ── List management ───────────────────────────────────────────────────────

    def _refresh(self) -> None:
        self._lib_list.clear()
        for lib in list_installed():
            item = QListWidgetItem(f"{lib['name']}   v{lib['version']}")
            item.setToolTip(
                f"Author: {lib['author']}\n"
                f"{lib['sentence']}\n"
                f"Dir: {lib['dir']}"
            )
            item.setData(Qt.ItemDataRole.UserRole, lib["dir"])
            self._lib_list.addItem(item)
        self._btn_remove.setEnabled(False)

    def _on_row_changed(self, row: int) -> None:
        self._btn_remove.setEnabled(row >= 0)

    # ── Actions ───────────────────────────────────────────────────────────────

    def _on_remove(self) -> None:
        item = self._lib_list.currentItem()
        if not item:
            return
        name = item.text().split("   ")[0]
        if (
            QMessageBox.question(
                self, "Remove Library", f"Remove '{name}'?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            )
            != QMessageBox.StandardButton.Yes
        ):
            return
        lib_dir_path = item.data(Qt.ItemDataRole.UserRole)
        try:
            shutil.rmtree(lib_dir_path)
            self._log_msg(f"Removed: {name}")
        except Exception as exc:  # noqa: BLE001
            QMessageBox.critical(self, "Error", f"Could not remove library:\n{exc}")
        self._refresh()

    def _on_browse(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self, "Select Library ZIP", "",
            "ZIP files (*.zip);;All Files (*)"
        )
        if path:
            self._zip_path.setText(path)

    def _on_install_zip(self) -> None:
        path = self._zip_path.text().strip()
        if not path:
            QMessageBox.warning(self, "No File", "Select a .zip file first.")
            return
        if not os.path.isfile(path):
            QMessageBox.warning(self, "Not Found", f"File not found:\n{path}")
            return
        try:
            self._log_msg("Installing from ZIP…")
            name = install_zip(path, self._log_msg)
            self._log_msg(f"Done! '{name}' is ready to use.")
            self._zip_path.clear()
        except Exception as exc:  # noqa: BLE001
            QMessageBox.critical(self, "Install Failed", str(exc))
            self._log_msg(f"Error: {exc}")
        self._refresh()

    def _on_install_github(self) -> None:
        url = self._gh_url.text().strip()
        if not url:
            QMessageBox.warning(self, "No URL", "Enter a URL first.")
            return
        if self._thread and self._thread.isRunning():
            return
        self._btn_gh.setEnabled(False)
        self._log_msg(f"Downloading: {url}")
        self._thread = _DownloadThread(url, self)
        self._thread.log.connect(self._log_msg)
        self._thread.done.connect(self._on_download_done)
        self._thread.error.connect(self._on_download_error)
        self._thread.start()

    def _on_download_done(self, name: str) -> None:
        self._btn_gh.setEnabled(True)
        self._gh_url.clear()
        self._log_msg(f"'{name}' installed successfully!")
        self._refresh()

    def _on_download_error(self, msg: str) -> None:
        self._btn_gh.setEnabled(True)
        self._log_msg(f"Error: {msg}")
        QMessageBox.critical(self, "Download Failed", msg)

    # ── Helpers ───────────────────────────────────────────────────────────────

    def _log_msg(self, msg: str) -> None:
        self._log.appendPlainText(msg)
        self._log.moveCursor(self._log.textCursor().MoveOperation.End)
