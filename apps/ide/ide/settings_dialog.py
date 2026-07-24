from __future__ import annotations

import os

from PyQt6.QtWidgets import (
    QDialog, QDialogButtonBox, QComboBox, QFileDialog, QFormLayout,
    QGroupBox, QHBoxLayout, QLabel, QListWidget, QPushButton,
    QSpinBox, QVBoxLayout, QWidget,
)

from .project import BuildConfig, ProgrammerConfig, Project, TargetConfig
from .serial_conn import list_ports

MCU_LIST        = ["ATmega328P", "ATmega32", "ATmega16", "ATmega64", "ATmega128"]
PROGRAMMER_LIST = ["USBASP", "AVRISP", "STK500", "ARDUINO", "DRAGON", "JTAG"]
OPT_LIST        = [
    ("O0", "O0 — no optimisation (debug)"),
    ("O1", "O1 — basic"),
    ("O2", "O2 — recommended (default)"),
    ("O3", "O3 — aggressive"),
    ("Os", "Os — optimise for size"),
    ("Og", "Og — debug-friendly"),
]
WARN_LIST = [
    ("none",    "None"),
    ("default", "Default"),
    ("all",     "All  (-Wall)"),
    ("extra",   "Extra  (-Wall -Wextra)"),
]
CPP_STD_LIST = ["c++11", "c++14", "c++17", "c++20"]


class SettingsDialog(QDialog):
    def __init__(self, project: Project, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.project = project
        self.setWindowTitle("Project Properties")
        self.setMinimumWidth(420)
        self._build_ui()
        self._load()

    # ── UI construction ───────────────────────────────────────────────────────

    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setSpacing(12)

        # ── Target ──────────────────────────────────────────────────────────
        grp_target = QGroupBox("Target")
        form_target = QFormLayout(grp_target)
        self.cb_mcu   = QComboBox(); self.cb_mcu.addItems(MCU_LIST)
        self.sb_clock = QSpinBox()
        self.sb_clock.setRange(1, 25); self.sb_clock.setSuffix(" MHz")
        form_target.addRow("MCU:",         self.cb_mcu)
        form_target.addRow("Clock:",       self.sb_clock)
        layout.addWidget(grp_target)

        # ── Programmer ──────────────────────────────────────────────────────
        grp_prog = QGroupBox("Programmer")
        form_prog = QFormLayout(grp_prog)
        self.cb_prog = QComboBox(); self.cb_prog.addItems(PROGRAMMER_LIST)

        port_row = QHBoxLayout()
        self.cb_port = QComboBox()
        self.cb_port.setEditable(True)
        self.cb_port.setInsertPolicy(QComboBox.InsertPolicy.NoInsert)
        self.cb_port.lineEdit().setPlaceholderText("e.g. COM3 or /dev/ttyUSB0")
        self._populate_ports(current="")
        btn_refresh = QPushButton("↺"); btn_refresh.setFixedWidth(32)
        btn_refresh.setToolTip("Refresh port list")
        btn_refresh.clicked.connect(self._refresh_ports)
        port_row.addWidget(self.cb_port, 1); port_row.addWidget(btn_refresh)

        self.sb_baud = QSpinBox()
        self.sb_baud.setRange(1200, 1_000_000); self.sb_baud.setSingleStep(9600)
        form_prog.addRow("Type:",      self.cb_prog)
        form_prog.addRow("Port:",      port_row)  # type: ignore[arg-type]
        form_prog.addRow("Baud rate:", self.sb_baud)
        layout.addWidget(grp_prog)

        # ── Build ────────────────────────────────────────────────────────────
        grp_build = QGroupBox("Build")
        form_build = QFormLayout(grp_build)
        self.cb_opt  = QComboBox()
        for val, label in OPT_LIST:
            self.cb_opt.addItem(label, val)
        self.cb_warn = QComboBox()
        for val, label in WARN_LIST:
            self.cb_warn.addItem(label, val)
        self.cb_std  = QComboBox(); self.cb_std.addItems(CPP_STD_LIST)
        form_build.addRow("Optimisation:", self.cb_opt)
        form_build.addRow("Warnings:",     self.cb_warn)
        form_build.addRow("C++ standard:", self.cb_std)
        layout.addWidget(grp_build)

        # ── Include Directories ──────────────────────────────────────────────
        grp_inc = QGroupBox("Include Directories")
        inc_layout = QVBoxLayout(grp_inc)
        inc_layout.setSpacing(4)

        self.lw_inc = QListWidget()
        self.lw_inc.setFixedHeight(90)
        inc_layout.addWidget(self.lw_inc)

        inc_btn_row = QHBoxLayout()
        btn_add_inc = QPushButton("Add…")
        btn_add_inc.clicked.connect(self._add_include_dir)
        self._btn_remove_inc = QPushButton("Remove")
        self._btn_remove_inc.clicked.connect(self._remove_include_dir)
        inc_btn_row.addWidget(btn_add_inc)
        inc_btn_row.addWidget(self._btn_remove_inc)
        inc_btn_row.addStretch()
        inc_layout.addLayout(inc_btn_row)

        hint = QLabel("Paths relative to the project root (e.g. <i>include</i>)")
        hint.setStyleSheet("color: #7f849c; font-size: 11px;")
        inc_layout.addWidget(hint)

        layout.addWidget(grp_inc)

        # ── Buttons ──────────────────────────────────────────────────────────
        bb = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok |
                              QDialogButtonBox.StandardButton.Cancel)
        bb.accepted.connect(self._save); bb.rejected.connect(self.reject)
        layout.addWidget(bb)

    # ── Data ──────────────────────────────────────────────────────────────────

    def _load(self) -> None:
        p = self.project
        _set_combo(self.cb_mcu,  p.target.mcu)
        self.sb_clock.setValue(p.target.clock // 1_000_000)

        _set_combo(self.cb_prog, p.programmer.type)
        self._populate_ports(current=p.programmer.port)
        self.sb_baud.setValue(p.programmer.baud_rate)

        _set_combo_data(self.cb_opt,  p.build.optimization)
        _set_combo_data(self.cb_warn, p.build.warnings)
        _set_combo(self.cb_std, p.build.cpp_standard)

        self.lw_inc.clear()
        for inc in p.build.include_dirs:
            self.lw_inc.addItem(inc)

    def _save(self) -> None:
        p = self.project
        p.target.mcu   = self.cb_mcu.currentText()
        p.target.clock = self.sb_clock.value() * 1_000_000

        p.programmer.type      = self.cb_prog.currentText()
        p.programmer.port      = self.cb_port.currentText().strip()
        p.programmer.baud_rate = self.sb_baud.value()

        p.build.optimization  = self.cb_opt.currentData()
        p.build.warnings      = self.cb_warn.currentData()
        p.build.cpp_standard  = self.cb_std.currentText()
        p.build.include_dirs  = [
            self.lw_inc.item(i).text()
            for i in range(self.lw_inc.count())
        ]

        p.save()
        self.accept()

    def _add_include_dir(self) -> None:
        chosen = QFileDialog.getExistingDirectory(
            self, "Select Include Directory", self.project.path,
        )
        if not chosen:
            return
        # Store as a path relative to the project root when possible
        try:
            rel = os.path.relpath(chosen, self.project.path)
        except ValueError:
            rel = chosen   # different drive on Windows — keep absolute
        # Avoid duplicates
        existing = [self.lw_inc.item(i).text() for i in range(self.lw_inc.count())]
        if rel not in existing:
            self.lw_inc.addItem(rel)

    def _remove_include_dir(self) -> None:
        row = self.lw_inc.currentRow()
        if row >= 0:
            self.lw_inc.takeItem(row)

    def _populate_ports(self, current: str) -> None:
        """Rebuild the port combo with live system ports, preserving *current*."""
        ports = list_ports()
        self.cb_port.blockSignals(True)
        self.cb_port.clear()
        self.cb_port.addItems(ports)
        # If the saved port is not in the detected list, prepend it so it is
        # still selectable (e.g. device temporarily unplugged).
        if current and current not in ports:
            self.cb_port.insertItem(0, current)
        if current:
            idx = self.cb_port.findText(current)
            if idx >= 0:
                self.cb_port.setCurrentIndex(idx)
            else:
                self.cb_port.setCurrentIndex(0)
        elif ports:
            self.cb_port.setCurrentIndex(0)
        self.cb_port.blockSignals(False)

    def _refresh_ports(self) -> None:
        self._populate_ports(current=self.cb_port.currentText())


# ── Helpers ───────────────────────────────────────────────────────────────────

def _set_combo(cb: QComboBox, value: str) -> None:
    idx = cb.findText(value)
    if idx >= 0:
        cb.setCurrentIndex(idx)


def _set_combo_data(cb: QComboBox, value: str) -> None:
    for i in range(cb.count()):
        if cb.itemData(i) == value:
            cb.setCurrentIndex(i)
            return
