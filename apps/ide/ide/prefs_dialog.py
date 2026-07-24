"""Preferences dialog — IDE-wide settings (font, appearance, output panels)."""

from __future__ import annotations

import dataclasses
import os

from PyQt6.QtCore import pyqtSignal
from PyQt6.QtGui import QFont
from PyQt6.QtWidgets import (
    QCheckBox, QComboBox, QDialog, QDialogButtonBox, QFileDialog, QFormLayout,
    QGroupBox, QHBoxLayout, QLabel, QLineEdit, QPushButton, QSpinBox,
    QVBoxLayout, QWidget,
)

from .prefs import IDEPrefs

# Monospace fonts offered in the family picker (user can also type a custom name).
_MONO_FONTS = [
    "Cascadia Code",
    "Cascadia Mono",
    "Fira Code",
    "JetBrains Mono",
    "Source Code Pro",
    "Consolas",
    "Courier New",
    "DejaVu Sans Mono",
    "Lucida Console",
    "Monospace",
]


class PrefsDialog(QDialog):
    """Modal dialog for IDE-wide preferences.

    Emits ``prefs_applied(IDEPrefs)`` when Apply or OK is clicked so the
    main window can apply the new settings immediately.
    """

    prefs_applied = pyqtSignal(IDEPrefs)

    def __init__(self, prefs: IDEPrefs, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._original = prefs
        self.setWindowTitle("Preferences")
        self.setMinimumWidth(440)
        self._build_ui()
        self._load(prefs)

    # ── UI construction ───────────────────────────────────────────────────────

    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setSpacing(14)

        # ── Projects ────────────────────────────────────────────────────────
        grp_projects = QGroupBox("Projects")
        form_projects = QFormLayout(grp_projects)
        form_projects.setVerticalSpacing(8)

        projects_row = QWidget()
        pr = QHBoxLayout(projects_row)
        pr.setContentsMargins(0, 0, 0, 0)
        self._projects_dir = QLineEdit()
        self._projects_dir.setToolTip(
            "Where New Project's folder picker starts, and the root the\n"
            "File > Projects Folder menu lists for one-click access.\n"
            "You can still create or open a project anywhere else — this\n"
            "only sets the default/shortcut location."
        )
        btn_browse_projects = QPushButton("Browse…")
        btn_browse_projects.clicked.connect(self._on_browse_projects_dir)
        pr.addWidget(self._projects_dir, stretch=1)
        pr.addWidget(btn_browse_projects)
        form_projects.addRow("Projects folder:", projects_row)
        layout.addWidget(grp_projects)

        # ── Editor ──────────────────────────────────────────────────────────
        grp_editor = QGroupBox("Editor")
        form_editor = QFormLayout(grp_editor)
        form_editor.setVerticalSpacing(8)

        self._font_family = QComboBox()
        self._font_family.setEditable(True)
        self._font_family.addItems(_MONO_FONTS)
        self._font_family.setToolTip("Monospace font used in the code editor")

        font_size_row, self._editor_font_size = _spin_row(
            6, 36, " pt", "Editor font size in points"
        )
        tab_size_row, self._tab_size = _spin_row(
            1, 16, " spaces", "Number of spaces per tab stop"
        )

        self._word_wrap = QCheckBox("Enable word wrap")
        self._word_wrap.setToolTip(
            "Wrap long lines instead of showing a horizontal scrollbar"
        )

        self._preview = QLabel("AaBbCc  0123  {}();")
        self._preview.setStyleSheet(
            "background: #1e1e2e; color: #cdd6f4; "
            "padding: 6px 10px; border-radius: 3px;"
        )
        self._font_family.currentTextChanged.connect(self._refresh_preview)
        self._editor_font_size.valueChanged.connect(self._refresh_preview)

        form_editor.addRow("Font family:", self._font_family)
        form_editor.addRow("Font size:",   font_size_row)
        form_editor.addRow("Tab size:",    tab_size_row)
        form_editor.addRow("",             self._word_wrap)
        form_editor.addRow("Preview:",     self._preview)
        layout.addWidget(grp_editor)

        # ── Appearance ───────────────────────────────────────────────────────
        grp_appearance = QGroupBox("Appearance")
        form_appearance = QFormLayout(grp_appearance)
        form_appearance.setVerticalSpacing(8)

        ui_size_row, self._ui_font_size = _spin_row(
            9, 24, " pt",
            "Font size for menus, labels, buttons, and other UI elements.\n"
            "Takes effect immediately on Apply."
        )
        form_appearance.addRow("UI font size:", ui_size_row)

        hint = QLabel("Restart not required — changes apply immediately.")
        hint.setStyleSheet("color: #7f849c; font-size: 11px;")
        form_appearance.addRow("", hint)
        layout.addWidget(grp_appearance)

        # ── Output Panels ────────────────────────────────────────────────────
        grp_output = QGroupBox("Output Panels")
        form_output = QFormLayout(grp_output)
        form_output.setVerticalSpacing(8)

        out_size_row, self._output_font_size = _spin_row(
            6, 24, " pt",
            "Font size used in the Build Output and Serial Monitor panels"
        )
        form_output.addRow("Font size:", out_size_row)
        layout.addWidget(grp_output)

        # ── Buttons ──────────────────────────────────────────────────────────
        bb = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok     |
            QDialogButtonBox.StandardButton.Apply  |
            QDialogButtonBox.StandardButton.Cancel
        )
        bb.accepted.connect(self._on_ok)
        bb.rejected.connect(self.reject)
        bb.button(QDialogButtonBox.StandardButton.Apply).clicked.connect(
            self._on_apply
        )
        layout.addWidget(bb)

    # ── Data ──────────────────────────────────────────────────────────────────

    def _load(self, p: IDEPrefs) -> None:
        self._projects_dir.setText(p.projects_dir)

        # Font family — pick from list or add custom
        idx = self._font_family.findText(p.editor_font_family)
        if idx >= 0:
            self._font_family.setCurrentIndex(idx)
        else:
            self._font_family.setCurrentText(p.editor_font_family)

        self._editor_font_size.setValue(p.editor_font_size)
        self._tab_size.setValue(p.editor_tab_size)
        self._word_wrap.setChecked(p.editor_word_wrap)
        self._ui_font_size.setValue(p.ui_font_size)
        self._output_font_size.setValue(p.output_font_size)
        self._refresh_preview()

    def _collect(self) -> IDEPrefs:
        # dataclasses.replace() (not IDEPrefs(...)) so fields this dialog
        # doesn't manage — e.g. last_project_path, set elsewhere by the main
        # window — carry over from self._original instead of silently
        # resetting to their dataclass default on every Apply/OK.
        return dataclasses.replace(
            self._original,
            projects_dir       = self._projects_dir.text().strip() or self._original.projects_dir,
            editor_font_family = self._font_family.currentText().strip() or "Monospace",
            editor_font_size   = self._editor_font_size.value(),
            editor_tab_size    = self._tab_size.value(),
            editor_word_wrap   = self._word_wrap.isChecked(),
            ui_font_size       = self._ui_font_size.value(),
            output_font_size   = self._output_font_size.value(),
        )

    # ── Slots ─────────────────────────────────────────────────────────────────

    def _on_browse_projects_dir(self) -> None:
        start = self._projects_dir.text().strip() or os.path.expanduser("~")
        path = QFileDialog.getExistingDirectory(self, "Select Projects Folder", start)
        if path:
            self._projects_dir.setText(path)

    def _on_apply(self) -> None:
        p = self._collect()
        p.save()
        self.prefs_applied.emit(p)

    def _on_ok(self) -> None:
        self._on_apply()
        self.accept()

    def _refresh_preview(self) -> None:
        family = self._font_family.currentText().strip() or "Monospace"
        size   = self._editor_font_size.value()
        self._preview.setFont(QFont(family, size))


# ── Helpers ───────────────────────────────────────────────────────────────────

def _spin_row(min_val: int, max_val: int, suffix: str, tooltip: str
              ) -> tuple[QWidget, QSpinBox]:
    """Return a (container_widget, spinbox) pair for use in a QFormLayout row."""
    spin = QSpinBox()
    spin.setRange(min_val, max_val)
    spin.setSuffix(suffix)
    spin.setToolTip(tooltip)
    spin.setFixedWidth(110)

    container = QWidget()
    from PyQt6.QtWidgets import QHBoxLayout
    row = QHBoxLayout(container)
    row.setContentsMargins(0, 0, 0, 0)
    row.addWidget(spin)
    row.addStretch()
    return container, spin
