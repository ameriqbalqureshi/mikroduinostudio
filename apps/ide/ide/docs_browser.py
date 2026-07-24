"""In-IDE documentation browser.

Renders the project's Markdown docs (guide docs under ``docs/``, SDK library
reference under ``sdk/docs/``, and one README per module under
``sdk/modules/<Name>/``) in a searchable tree + viewer, opened from
Help > Documentation.

This module only *indexes and displays* Markdown files that already exist on
disk — it defines no content of its own. New guides, library docs, or module
READMEs picked up by :func:`build_doc_index` show up automatically the next
time the dialog is opened; nothing here needs editing to add content.
"""

from __future__ import annotations

import os
import webbrowser
from dataclasses import dataclass

from PyQt6.QtCore import Qt, QUrl
from PyQt6.QtGui import QColor
from PyQt6.QtWidgets import (
    QDialog, QHBoxLayout, QLabel, QLineEdit, QSplitter, QTextBrowser,
    QTreeWidget, QTreeWidgetItem, QVBoxLayout, QWidget,
)

from . import respaths

# ── Doc index ────────────────────────────────────────────────────────────────


@dataclass(frozen=True)
class DocEntry:
    title: str
    path: str  # absolute path on disk


# Hand-titled guide docs under docs/, in display order. Entries whose file
# doesn't exist (yet) are skipped rather than shown broken.
_GUIDES = [
    ("Getting Started", "getting-started.md"),
    ("Arduino Mode — Tutorial", "arduino-mode.md"),
    ("Arduino Compatibility — Reference", "Arduino.md"),
]

_CORE_PERIPHERALS = [
    ("Registers & Atomic Macros", "registers.md"),
    ("GPIO", "gpio.md"),
    ("USART — Serial", "uart-reference.md"),
    ("ADC", "ADC.md"),
    ("PWM", "pwm.md"),
    ("Timers", "timer.md"),
    ("SPI", "spi.md"),
    ("I2C", "i2c.md"),
    ("External Interrupts", "interrupts.md"),
    ("EEPROM", "eeprom.md"),
]

_MODULE_LIBRARY_DOCS = [
    ("Module Drivers — Full Reference", "core-libraries.md"),
]


def _manifest_entries(base_dir: str, items: list[tuple[str, str]]) -> list[DocEntry]:
    out = []
    for title, relpath in items:
        p = os.path.normpath(os.path.join(base_dir, relpath))
        if os.path.isfile(p):
            out.append(DocEntry(title, p))
    return out


def _scan_md_dir(base_dir: str) -> list[DocEntry]:
    """Every *.md file directly inside base_dir, alphabetically."""
    if not os.path.isdir(base_dir):
        return []
    out = []
    for name in sorted(os.listdir(base_dir)):
        if name.lower().endswith(".md"):
            title = os.path.splitext(name)[0].replace("-", " ").replace("_", " ")
            out.append(DocEntry(title, os.path.normpath(os.path.join(base_dir, name))))
    return out


def _scan_module_readmes() -> list[DocEntry]:
    """One entry per sdk/modules/<Name>/README.md, so new modules appear
    automatically without touching this file."""
    root = os.path.join(respaths.SDK_ROOT, "modules")
    if not os.path.isdir(root):
        return []
    out = []
    for name in sorted(os.listdir(root)):
        readme = os.path.join(root, name, "README.md")
        if os.path.isfile(readme):
            out.append(DocEntry(name, os.path.normpath(readme)))
    return out


def build_doc_index() -> list[tuple[str, list[DocEntry]]]:
    """Returns [(category title, [DocEntry, ...]), ...]. Empty categories are
    kept (not skipped) so the structure itself is visible before content for
    that category has been written."""
    docs = respaths.DOCS_ROOT
    sdk_docs = os.path.join(respaths.SDK_ROOT, "docs")

    return [
        (
            "Getting Started & Guides",
            _manifest_entries(docs, _GUIDES) + _scan_md_dir(os.path.join(docs, "guides")),
        ),
        ("Core Peripherals", _manifest_entries(docs, _CORE_PERIPHERALS)),
        ("Module Drivers", _manifest_entries(sdk_docs, _MODULE_LIBRARY_DOCS) + _scan_module_readmes()),
        ("API Reference", _scan_md_dir(os.path.join(docs, "api"))),
    ]


# ── Dialog ───────────────────────────────────────────────────────────────────

_PLACEHOLDER_COLOR = QColor("#6c7086")


class DocsBrowserDialog(QDialog):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setWindowTitle("MikroDuino Documentation")
        self.resize(1000, 700)
        self.setModal(False)

        self._current_path: str | None = None
        self._path_to_item: dict[str, QTreeWidgetItem] = {}
        self._index = build_doc_index()

        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        search_bar = QWidget()
        sb = QHBoxLayout(search_bar)
        sb.setContentsMargins(8, 8, 8, 4)
        self._search = QLineEdit()
        self._search.setPlaceholderText("Filter documents…")
        self._search.textChanged.connect(self._on_filter)
        sb.addWidget(self._search)
        root.addWidget(search_bar)

        splitter = QSplitter(Qt.Orientation.Horizontal)
        root.addWidget(splitter, 1)

        self._tree = QTreeWidget()
        self._tree.setHeaderHidden(True)
        self._tree.setMinimumWidth(260)
        self._tree.itemClicked.connect(self._on_item_clicked)
        splitter.addWidget(self._tree)

        right = QWidget()
        rl = QVBoxLayout(right)
        rl.setContentsMargins(0, 0, 0, 0)
        rl.setSpacing(0)
        self._browser = QTextBrowser()
        self._browser.setOpenLinks(False)
        self._browser.anchorClicked.connect(self._on_anchor_clicked)
        rl.addWidget(self._browser, 1)
        self._path_label = QLabel("")
        self._path_label.setStyleSheet("color: #7f849c; font-size: 11px; padding: 4px 8px;")
        rl.addWidget(self._path_label)
        splitter.addWidget(right)
        splitter.setSizes([280, 720])

        self._populate_tree()
        self._select_first_available()

    # ── Tree population ──────────────────────────────────────────────────

    def _populate_tree(self) -> None:
        self._tree.clear()
        self._path_to_item.clear()

        for category, entries in self._index:
            cat_item = QTreeWidgetItem([category])
            cat_item.setFlags(cat_item.flags() & ~Qt.ItemFlag.ItemIsSelectable)
            bold = cat_item.font(0)
            bold.setBold(True)
            cat_item.setFont(0, bold)
            self._tree.addTopLevelItem(cat_item)

            if entries:
                for entry in entries:
                    child = QTreeWidgetItem([entry.title])
                    child.setData(0, Qt.ItemDataRole.UserRole, entry.path)
                    cat_item.addChild(child)
                    self._path_to_item[os.path.normpath(entry.path)] = child
            else:
                placeholder = QTreeWidgetItem(["(no documents yet)"])
                placeholder.setFlags(placeholder.flags() & ~Qt.ItemFlag.ItemIsSelectable)
                placeholder.setForeground(0, _PLACEHOLDER_COLOR)
                cat_item.addChild(placeholder)

            cat_item.setExpanded(True)

    def _select_first_available(self) -> None:
        for _category, entries in self._index:
            if entries:
                self._select_path(entries[0].path)
                return
        self._browser.setPlainText(
            "No documentation files found yet.\n\n"
            f"Guides go under: {respaths.DOCS_ROOT}\n"
            f"Module docs go under: {os.path.join(respaths.SDK_ROOT, 'modules')}/<Name>/README.md"
        )

    # ── Navigation ────────────────────────────────────────────────────────

    def _on_item_clicked(self, item: QTreeWidgetItem, _col: int) -> None:
        path = item.data(0, Qt.ItemDataRole.UserRole)
        if path:
            self._load(path)

    def _select_path(self, path: str) -> None:
        item = self._path_to_item.get(os.path.normpath(path))
        if item is not None:
            self._tree.setCurrentItem(item)
        self._load(path)

    def _load(self, path: str) -> None:
        try:
            with open(path, "r", encoding="utf-8") as f:
                text = f.read()
        except OSError as exc:
            self._browser.setPlainText(f"Could not open {path}:\n{exc}")
            return

        self._current_path = path
        self._browser.setSearchPaths([os.path.dirname(path)])
        self._browser.setMarkdown(text)

        try:
            rel = os.path.relpath(path, respaths.BASE_DIR)
        except ValueError:
            rel = path
        self._path_label.setText(rel.replace("\\", "/"))

    def _on_anchor_clicked(self, url: QUrl) -> None:
        if url.scheme() in ("http", "https", "mailto"):
            webbrowser.open(url.toString())
            return

        fragment = url.fragment()
        if not url.path():
            # In-page anchor (e.g. a table-of-contents link).
            if fragment:
                self._browser.scrollToAnchor(fragment)
            return

        base_dir = os.path.dirname(self._current_path or respaths.DOCS_ROOT)
        target = os.path.normpath(os.path.join(base_dir, url.path()))
        if os.path.isfile(target):
            self._select_path(target)
            if fragment:
                self._browser.scrollToAnchor(fragment)
        else:
            webbrowser.open(url.toString())

    # ── Search filter ─────────────────────────────────────────────────────

    def _on_filter(self, text: str) -> None:
        text = text.strip().lower()
        for i in range(self._tree.topLevelItemCount()):
            cat_item = self._tree.topLevelItem(i)
            visible_count = 0
            for j in range(cat_item.childCount()):
                child = cat_item.child(j)
                has_path = bool(child.data(0, Qt.ItemDataRole.UserRole))
                if not text:
                    child.setHidden(False)
                    visible_count += 1 if has_path else 0
                    continue
                match = has_path and text in child.text(0).lower()
                child.setHidden(not match)
                if match:
                    visible_count += 1
            cat_item.setHidden(bool(text) and visible_count == 0)
