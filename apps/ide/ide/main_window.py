"""MikroDuino Studio — Main Window."""

from __future__ import annotations

import json
import os
import re
import shutil
from typing import Optional

from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QAction, QFont, QKeySequence, QColor, QTextCursor
from PyQt6.QtWidgets import (
    QApplication, QDockWidget, QFileDialog, QHBoxLayout,
    QLabel, QMainWindow, QMenu, QMessageBox, QPlainTextEdit,
    QSplitter, QStatusBar, QStyle, QTabWidget, QToolBar,
    QTreeWidget, QTreeWidgetItem, QVBoxLayout, QWidget,
    QTextEdit, QLineEdit, QPushButton, QComboBox,
)

from .autocomplete import CppCompleter
from .builder import BuildThread
from .docs_browser import DocsBrowserDialog
from .editor import CodeEditor
from .ino_importer import import_ino
from .library_manager import (
    LibraryManagerDialog, list_installed, list_examples, list_mdp_tree,
    list_sdk_modules, arduino_primary_header,
    list_core_peripherals, list_core_utility_groups,
)
from .prefs import IDEPrefs
from .prefs_dialog import PrefsDialog
from .project import Project
from . import respaths
from .serial_conn import SerialThread, list_ports, SERIAL_OK
from .settings_dialog import SettingsDialog
from .syntax import CppHighlighter
from .theme import make_dark


class MainWindow(QMainWindow):
    def __init__(self, prefs: IDEPrefs | None = None) -> None:
        super().__init__()
        self._prefs:        IDEPrefs              = prefs or IDEPrefs.load()
        self._project:      Optional[Project]     = None
        self._build_thread: Optional[BuildThread] = None
        self._serial:       SerialThread          = SerialThread()
        self._editors:      dict[str, CodeEditor] = {}        # path → editor
        self._dirty:        set[str]              = set()
        self._docs_dialog:  Optional[DocsBrowserDialog] = None

        # Ensure the configured Projects Folder exists so a fresh install
        # has somewhere sensible for New Project / Projects Folder to point
        # at, same as Arduino IDE creating its sketchbook folder on first run.
        try:
            os.makedirs(self._prefs.projects_dir, exist_ok=True)
        except OSError:
            pass

        self.setWindowTitle("MikroDuino Studio")
        self.resize(1400, 900)
        self.setMinimumSize(900, 600)

        self._make_menu()
        self._make_toolbar()
        self._make_central()
        self._make_explorer_dock()
        self._make_bottom_dock()
        self._make_libraries_dock()
        self._make_statusbar()

        self._serial.data_received.connect(self._on_serial_data)
        self._serial.status.connect(self._on_serial_status)

        self._update_actions()
        self._restore_last_project()

    # ═══════════════════════════════════════════════════════════════════════════
    # UI Construction
    # ═══════════════════════════════════════════════════════════════════════════

    def _make_menu(self) -> None:
        mb = self.menuBar()

        # ── File ──────────────────────────────────────────────────────────────
        file_m = mb.addMenu("&File")
        self._act_new   = file_m.addAction("New Project…",   self._on_new_project)
        self._act_open  = file_m.addAction("Open Project…",  self._on_open_project)
        self._projects_menu = file_m.addMenu("Projects Folder")
        self._projects_menu.aboutToShow.connect(self._populate_projects_folder_menu)
        self._examples_menu = file_m.addMenu("Examples")
        self._examples_menu.aboutToShow.connect(self._populate_examples_menu)
        file_m.addAction("Import Sketch (.ino)…", self._on_import_ino)
        file_m.addSeparator()
        self._act_save  = file_m.addAction("Save",           self._on_save)
        self._act_save.setShortcut(QKeySequence.StandardKey.Save)
        self._act_save_all = file_m.addAction("Save All",    self._on_save_all)
        self._act_save_all.setShortcut(QKeySequence("Ctrl+Shift+S"))
        file_m.addSeparator()
        self._act_props = file_m.addAction("Project Properties…", self._on_properties)
        file_m.addSeparator()
        file_m.addAction("Exit", self.close)

        # ── Edit ──────────────────────────────────────────────────────────────
        edit_m = mb.addMenu("&Edit")
        _add_action(edit_m, "Undo",       QKeySequence.StandardKey.Undo,      lambda: self._current_editor() and self._current_editor().undo())
        _add_action(edit_m, "Redo",       QKeySequence.StandardKey.Redo,      lambda: self._current_editor() and self._current_editor().redo())
        edit_m.addSeparator()
        _add_action(edit_m, "Cut",        QKeySequence.StandardKey.Cut,       lambda: self._current_editor() and self._current_editor().cut())
        _add_action(edit_m, "Copy",       QKeySequence.StandardKey.Copy,      lambda: self._current_editor() and self._current_editor().copy())
        _add_action(edit_m, "Paste",      QKeySequence.StandardKey.Paste,     lambda: self._current_editor() and self._current_editor().paste())
        edit_m.addSeparator()
        _add_action(edit_m, "Select All", QKeySequence.StandardKey.SelectAll, lambda: self._current_editor() and self._current_editor().selectAll())
        edit_m.addSeparator()
        act_prefs = edit_m.addAction("Preferences…", self._on_preferences)
        act_prefs.setShortcut(QKeySequence("Ctrl+,"))

        # ── View ──────────────────────────────────────────────────────────────
        view_m = mb.addMenu("&View")
        self._act_toggle_exp    = view_m.addAction("Explorer",     lambda: self._explorer_dock.setVisible(not self._explorer_dock.isVisible()))
        self._act_toggle_bottom = view_m.addAction("Bottom Panel", lambda: self._bottom_dock.setVisible(not self._bottom_dock.isVisible()))
        self._act_toggle_libs   = view_m.addAction("Libraries",    lambda: self._libraries_dock.setVisible(not self._libraries_dock.isVisible()))

        # ── Build ─────────────────────────────────────────────────────────────
        build_m = mb.addMenu("&Build")
        self._act_build = build_m.addAction("Build Project", self._on_build)
        self._act_build.setShortcut(QKeySequence("F7"))
        self._act_rebuild = build_m.addAction("Rebuild All", lambda: self._on_build(clean_first=True))
        self._act_rebuild.setShortcut(QKeySequence("Ctrl+F7"))
        self._act_clean = build_m.addAction("Clean",         self._on_clean)
        build_m.addSeparator()
        self._act_flash = build_m.addAction("Flash to Device", self._on_flash)
        self._act_flash.setShortcut(QKeySequence("F8"))
        build_m.addSeparator()
        build_m.addAction("Project Properties…", self._on_properties)

        # ── Tools ─────────────────────────────────────────────────────────────
        tools_m = mb.addMenu("&Tools")
        act_libs = tools_m.addAction("Library Manager…", self._on_library_manager)
        act_libs.setShortcut(QKeySequence("Ctrl+L"))
        tools_m.addSeparator()
        tools_m.addAction("Project Properties…", self._on_properties)

        # ── Help ──────────────────────────────────────────────────────────────
        help_m = mb.addMenu("&Help")
        act_docs = help_m.addAction("Documentation…", self._on_docs)
        act_docs.setShortcut(QKeySequence("F1"))
        help_m.addSeparator()
        help_m.addAction("About MikroDuino Studio", self._on_about)

    def _make_toolbar(self) -> None:
        tb = QToolBar("Main Toolbar", self)
        tb.setMovable(False)
        tb.setIconSize(tb.iconSize())
        self.addToolBar(tb)

        def _btn(label: str, slot, obj_name: str = "") -> QAction:
            a = tb.addAction(label, slot)
            if obj_name:
                w = tb.widgetForAction(a)
                if w:
                    w.setObjectName(obj_name)
            return a

        _btn("✦ New",    self._on_new_project)
        _btn("⏏ Open",   self._on_open_project)
        _btn("⬇ Save",   self._on_save)
        tb.addSeparator()
        self._tb_build = _btn("▶ Build",      self._on_build,  "btn_build")
        self._tb_flash = _btn("⬆ Flash",      self._on_flash,  "btn_flash")
        tb.addSeparator()
        _btn("⚙ Properties", self._on_properties)

        # Project name label on right
        spacer = QWidget(); spacer.setSizePolicy(
            spacer.sizePolicy().horizontalPolicy(),
            spacer.sizePolicy().verticalPolicy(),
        )
        from PyQt6.QtWidgets import QSizePolicy
        spacer.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
        tb.addWidget(spacer)

        self._lbl_project = QLabel("")
        self._lbl_project.setStyleSheet("color: #7f849c; margin-right: 12px; font-size: 12px;")
        tb.addWidget(self._lbl_project)

    def _make_central(self) -> None:
        """Tabbed editor as the central widget."""
        self._editor_tabs = QTabWidget()
        self._editor_tabs.setTabsClosable(True)
        self._editor_tabs.setMovable(True)
        self._editor_tabs.tabCloseRequested.connect(self._on_close_tab)
        self._editor_tabs.currentChanged.connect(self._on_tab_changed)

        # Welcome placeholder
        welcome = QWidget()
        wl = QVBoxLayout(welcome)
        wl.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title = QLabel("MikroDuino Studio")
        title.setStyleSheet("font-size: 28px; font-weight: bold; color: #5bcefa;")
        sub   = QLabel("Open a project or create a new one to start coding.")
        sub.setStyleSheet("color: #7f849c; font-size: 14px;")
        wl.addWidget(title, alignment=Qt.AlignmentFlag.AlignHCenter)
        wl.addWidget(sub,   alignment=Qt.AlignmentFlag.AlignHCenter)
        self._welcome = welcome

        container = QWidget()
        cl = QVBoxLayout(container)
        cl.setContentsMargins(0, 0, 0, 0)
        cl.addWidget(self._editor_tabs)
        cl.addWidget(self._welcome)
        self._editor_tabs.hide()

        self.setCentralWidget(container)

    def _make_explorer_dock(self) -> None:
        self._explorer_dock = QDockWidget("EXPLORER", self)
        self._explorer_dock.setAllowedAreas(Qt.DockWidgetArea.LeftDockWidgetArea)
        self._explorer_dock.setFeatures(
            QDockWidget.DockWidgetFeature.DockWidgetClosable |
            QDockWidget.DockWidgetFeature.DockWidgetMovable
        )

        self._file_tree = QTreeWidget()
        self._file_tree.setHeaderHidden(True)
        self._file_tree.itemDoubleClicked.connect(self._on_file_double_clicked)
        self._file_tree.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self._file_tree.customContextMenuRequested.connect(self._on_tree_context_menu)

        self._explorer_dock.setWidget(self._file_tree)
        self.addDockWidget(Qt.DockWidgetArea.LeftDockWidgetArea, self._explorer_dock)
        self._explorer_dock.setMinimumWidth(220)

    def _make_bottom_dock(self) -> None:
        self._bottom_dock = QDockWidget("OUTPUT", self)
        self._bottom_dock.setAllowedAreas(Qt.DockWidgetArea.BottomDockWidgetArea)
        self._bottom_dock.setFeatures(
            QDockWidget.DockWidgetFeature.DockWidgetClosable |
            QDockWidget.DockWidgetFeature.DockWidgetMovable
        )

        tabs = QTabWidget()

        # ── Build output tab ─────────────────────────────────────────────────
        self._build_output = QPlainTextEdit()
        self._build_output.setReadOnly(True)
        self._build_output.setFont(QFont(self._prefs.editor_font_family, self._prefs.output_font_size))
        tabs.addTab(self._build_output, "BUILD OUTPUT")

        # ── Serial monitor tab ───────────────────────────────────────────────
        serial_widget = self._make_serial_panel()
        tabs.addTab(serial_widget, "SERIAL MONITOR")

        # Auto-refresh port list whenever Serial Monitor tab is activated
        tabs.currentChanged.connect(self._on_bottom_tab_changed)

        self._bottom_tabs = tabs
        self._bottom_dock.setWidget(tabs)
        self.addDockWidget(Qt.DockWidgetArea.BottomDockWidgetArea, self._bottom_dock)
        self._bottom_dock.setMinimumHeight(160)
        self.resizeDocks([self._bottom_dock], [200], Qt.Orientation.Vertical)

    def _make_libraries_dock(self) -> None:
        self._libraries_dock = QDockWidget("LIBRARIES", self)
        self._libraries_dock.setAllowedAreas(
            Qt.DockWidgetArea.LeftDockWidgetArea | Qt.DockWidgetArea.RightDockWidgetArea
        )
        self._libraries_dock.setFeatures(
            QDockWidget.DockWidgetFeature.DockWidgetClosable |
            QDockWidget.DockWidgetFeature.DockWidgetMovable
        )

        container = QWidget()
        cl = QVBoxLayout(container)
        cl.setContentsMargins(4, 4, 4, 4)
        cl.setSpacing(4)

        filter_row = QHBoxLayout()
        self._lib_filter = QLineEdit()
        self._lib_filter.setPlaceholderText("Filter libraries…")
        self._lib_filter.textChanged.connect(self._apply_lib_filter)
        filter_row.addWidget(self._lib_filter)
        btn_refresh = QPushButton()
        btn_refresh.setIcon(self.style().standardIcon(QStyle.StandardPixmap.SP_BrowserReload))
        btn_refresh.setFixedWidth(28)
        btn_refresh.setToolTip("Refresh library list")
        btn_refresh.clicked.connect(self._refresh_libraries_pane)
        filter_row.addWidget(btn_refresh)
        cl.addLayout(filter_row)

        self._lib_tree = QTreeWidget()
        self._lib_tree.setHeaderHidden(True)
        # itemActivated alone covers double-click AND Enter/Return — also
        # wiring itemDoubleClicked here fired the same slot twice per click.
        self._lib_tree.itemActivated.connect(self._on_library_item_activated)
        cl.addWidget(self._lib_tree)

        hint = QLabel("Double-click a library to #include it in the current file.")
        hint.setWordWrap(True)
        hint.setStyleSheet("color: #7f849c; font-size: 11px;")
        cl.addWidget(hint)

        btn_manage = QPushButton("Manage Arduino Libraries…")
        btn_manage.clicked.connect(self._on_library_manager)
        cl.addWidget(btn_manage)

        self._libraries_dock.setWidget(container)
        self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, self._libraries_dock)
        self._libraries_dock.setMinimumWidth(220)

        self._refresh_libraries_pane()

    def _make_serial_panel(self) -> QWidget:
        w = QWidget()
        layout = QVBoxLayout(w)
        layout.setSpacing(4)
        layout.setContentsMargins(4, 4, 4, 4)

        # Controls row
        ctrl = QHBoxLayout()
        self._serial_port  = QComboBox()
        self._serial_port.setEditable(True)
        self._serial_port.setMinimumWidth(120)
        self._serial_baud  = QComboBox()
        self._serial_baud.addItems(["9600","19200","38400","57600","115200","230400"])
        self._serial_baud.setCurrentText("115200")
        self._btn_serial   = QPushButton("Connect")
        self._btn_serial.clicked.connect(self._on_serial_toggle)
        self._btn_serial_refresh = QPushButton("↺")
        self._btn_serial_refresh.setFixedWidth(28)
        self._btn_serial_refresh.clicked.connect(self._refresh_serial_ports)
        btn_clr = QPushButton("Clear")
        btn_clr.clicked.connect(lambda: self._serial_output.clear())
        ctrl.addWidget(QLabel("Port:"))
        ctrl.addWidget(self._serial_port)
        ctrl.addWidget(self._btn_serial_refresh)
        ctrl.addWidget(QLabel("Baud:"))
        ctrl.addWidget(self._serial_baud)
        ctrl.addWidget(self._btn_serial)
        ctrl.addWidget(btn_clr)
        ctrl.addStretch()
        layout.addLayout(ctrl)

        # Output
        self._serial_output = QPlainTextEdit()
        self._serial_output.setReadOnly(True)
        self._serial_output.setFont(QFont(self._prefs.editor_font_family, self._prefs.output_font_size))
        layout.addWidget(self._serial_output)

        # Input row
        inp = QHBoxLayout()
        self._serial_input = QLineEdit()
        self._serial_input.setPlaceholderText("Type and press Enter to send…")
        self._serial_input.returnPressed.connect(self._on_serial_send)
        btn_send = QPushButton("Send")
        btn_send.clicked.connect(self._on_serial_send)
        inp.addWidget(self._serial_input); inp.addWidget(btn_send)
        layout.addLayout(inp)

        self._refresh_serial_ports()
        return w

    def _make_statusbar(self) -> None:
        sb = self.statusBar()
        self._sb_mcu     = QLabel("—")
        self._sb_clock   = QLabel("—")
        self._sb_prog    = QLabel("—")
        self._sb_build   = QLabel("Ready")
        self._sb_serial  = QLabel("Serial: disconnected")

        for w in (self._sb_build, self._sb_mcu, self._sb_clock,
                  self._sb_prog, self._sb_serial):
            sb.addPermanentWidget(w)
            sb.addPermanentWidget(_sep())

    # ═══════════════════════════════════════════════════════════════════════════
    # Slots — File / Project
    # ═══════════════════════════════════════════════════════════════════════════

    def _restore_last_project(self) -> None:
        """Reopen the last project from a previous session, if it still exists.
        Silent no-op on any failure — this is a startup convenience, not a
        user-initiated action, so it should never pop up an error dialog."""
        path = self._prefs.last_project_path
        if not path or not os.path.isfile(path):
            return
        try:
            project = Project.open(path)
        except Exception:
            return
        self._load_project(project, mdp_path=path)

    def _on_new_project(self) -> None:
        # Starts in the configured Projects Folder as a convenience — the
        # user can still navigate anywhere else in the same dialog.
        start_dir = self._prefs.projects_dir if os.path.isdir(self._prefs.projects_dir) else ""
        parent_dir = QFileDialog.getExistingDirectory(self, "Select Parent Folder", start_dir)
        if not parent_dir:
            return
        from PyQt6.QtWidgets import QInputDialog
        name, ok = QInputDialog.getText(self, "Project Name", "Project name:", text="MyProject")
        if not ok or not name.strip():
            return
        name = name.strip()
        try:
            project = Project.create(parent_dir, name)
        except Exception as exc:
            QMessageBox.critical(self, "Error", f"Could not create project:\n{exc}")
            return
        self._load_project(project)

    def _on_open_project(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self, "Open MikroDuino Project", "",
            "MikroDuino Project (*.mdp);;All Files (*)"
        )
        if path:
            self._open_project_path(path)

    def _open_project_path(self, mdp_path: str) -> None:
        """Open an existing .mdp project in place — shared by Open Project…
        and the Projects Folder menu (list_mdp_tree() over prefs.projects_dir)."""
        try:
            project = Project.open(mdp_path)
        except Exception as exc:
            QMessageBox.critical(self, "Error", f"Could not open project:\n{exc}")
            return
        self._load_project(project, mdp_path=mdp_path)

    def _load_project(self, project: Project, mdp_path: Optional[str] = None) -> None:
        self._close_all_tabs()
        self._project = project
        self._refresh_explorer()
        self._update_statusbar()
        self._update_actions()
        self._lbl_project.setText(project.name)
        self.setWindowTitle(f"MikroDuino Studio — {project.name}")
        self._build_output.clear()
        self._append_build(f"Project loaded: {project.path}\n")

        # mdp_path defaults to the computed path (correct for newly created/
        # imported projects, whose .mdp filename always matches project.name);
        # callers that opened an arbitrary .mdp file via a dialog pass the
        # actual selected path instead, since a project's internal name can
        # differ from the filename it was saved under.
        self._prefs.last_project_path = mdp_path or project.mdp_path
        self._prefs.save()

    def _on_save(self) -> None:
        path = self._current_tab_path()
        if path:
            self._save_file(path)

    def _on_save_all(self) -> None:
        for path in list(self._dirty):
            self._save_file(path)

    def _save_file(self, path: str) -> None:
        editor = self._editors.get(path)
        if not editor:
            return
        try:
            with open(path, "w", encoding="utf-8") as f:
                f.write(editor.toPlainText())
            self._dirty.discard(path)
            self._update_tab_title(path)
        except Exception as exc:
            QMessageBox.critical(self, "Save Failed", str(exc))

    def _on_properties(self) -> None:
        if not self._project:
            QMessageBox.information(self, "No Project", "Open or create a project first.")
            return
        dlg = SettingsDialog(self._project, self)
        if dlg.exec():
            self._update_statusbar()

    # ═══════════════════════════════════════════════════════════════════════════
    # Slots — Explorer
    # ═══════════════════════════════════════════════════════════════════════════

    _EXPLORER_EXTS = frozenset({".cpp", ".c", ".hpp", ".h", ".mdp", ".md", ".txt"})

    def _refresh_explorer(self) -> None:
        self._file_tree.clear()
        if not self._project:
            return

        root_item = QTreeWidgetItem([self._project.name])
        root_item.setData(0, Qt.ItemDataRole.UserRole, self._project.path)
        self._file_tree.addTopLevelItem(root_item)
        self._populate_dir_item(root_item, self._project.path)
        root_item.setExpanded(True)

    def _populate_dir_item(self, parent_item: QTreeWidgetItem, dir_path: str) -> None:
        try:
            entries = sorted(os.scandir(dir_path), key=lambda e: (not e.is_dir(), e.name.lower()))
        except PermissionError:
            return
        for entry in entries:
            if entry.name.startswith(".") or entry.name == "build":
                continue
            if entry.is_dir(follow_symlinks=False):
                folder_item = QTreeWidgetItem([entry.name])
                folder_item.setData(0, Qt.ItemDataRole.UserRole, entry.path)
                parent_item.addChild(folder_item)
                self._populate_dir_item(folder_item, entry.path)
                folder_item.setExpanded(True)
            elif entry.is_file(follow_symlinks=False):
                if os.path.splitext(entry.name)[1].lower() in self._EXPLORER_EXTS:
                    file_item = QTreeWidgetItem([entry.name])
                    file_item.setData(0, Qt.ItemDataRole.UserRole, entry.path)
                    parent_item.addChild(file_item)

    def _on_file_double_clicked(self, item: QTreeWidgetItem, _col: int) -> None:
        path = item.data(0, Qt.ItemDataRole.UserRole)
        if path and os.path.isfile(path):
            self._open_file(path)

    # ── Context menu ─────────────────────────────────────────────────────────

    def _on_tree_context_menu(self, pos) -> None:
        item = self._file_tree.itemAt(pos)
        if not self._project or item is None:
            return
        path = item.data(0, Qt.ItemDataRole.UserRole)
        if not path:
            return

        is_dir  = os.path.isdir(path)
        is_root = (os.path.normcase(path) == os.path.normcase(self._project.path))

        menu = QMenu(self)
        if is_dir:
            menu.addAction("New File…",   lambda: self._fm_new_file(path))
            menu.addAction("New Folder…", lambda: self._fm_new_folder(path))
            if not is_root:
                menu.addSeparator()
                menu.addAction("Rename…", lambda: self._fm_rename(path))
                menu.addAction("Delete",  lambda: self._fm_delete_dir(path))
        else:
            menu.addAction("Open",           lambda: self._open_file(path))
            menu.addSeparator()
            menu.addAction("Rename…",        lambda: self._fm_rename(path))
            menu.addAction("Move to Folder…",lambda: self._fm_move_file(path))
            menu.addSeparator()
            menu.addAction("Delete",         lambda: self._fm_delete_file(path))

        menu.exec(self._file_tree.viewport().mapToGlobal(pos))

    # ── File-management actions ───────────────────────────────────────────────

    def _fm_new_file(self, folder: str) -> None:
        from PyQt6.QtWidgets import QInputDialog
        name, ok = QInputDialog.getText(self, "New File", "File name:", text="newfile.cpp")
        if not ok or not name.strip():
            return
        name = name.strip()
        new_path = os.path.join(folder, name)
        if os.path.exists(new_path):
            QMessageBox.warning(self, "Already Exists", f"'{name}' already exists.")
            return
        try:
            open(new_path, "w", encoding="utf-8").close()
        except Exception as exc:
            QMessageBox.critical(self, "Error", str(exc))
            return
        if name.endswith((".cpp", ".c", ".cc", ".cxx")) and self._project:
            rel = os.path.relpath(new_path, self._project.path)
            if rel not in self._project.source_files:
                self._project.source_files.append(rel)
                self._project.save()
        self._refresh_explorer()
        self._open_file(new_path)

    def _fm_new_folder(self, parent_dir: str) -> None:
        from PyQt6.QtWidgets import QInputDialog
        name, ok = QInputDialog.getText(self, "New Folder", "Folder name:", text="include")
        if not ok or not name.strip():
            return
        name = name.strip()
        new_path = os.path.join(parent_dir, name)
        try:
            os.makedirs(new_path, exist_ok=False)
        except FileExistsError:
            QMessageBox.warning(self, "Already Exists", f"'{name}' already exists.")
            return
        except Exception as exc:
            QMessageBox.critical(self, "Error", str(exc))
            return
        self._refresh_explorer()

    def _fm_rename(self, path: str) -> None:
        from PyQt6.QtWidgets import QInputDialog
        old_name = os.path.basename(path)
        new_name, ok = QInputDialog.getText(self, "Rename", "New name:", text=old_name)
        if not ok or not new_name.strip() or new_name.strip() == old_name:
            return
        new_name = new_name.strip()
        new_path = os.path.join(os.path.dirname(path), new_name)
        if os.path.exists(new_path):
            QMessageBox.warning(self, "Already Exists", f"'{new_name}' already exists.")
            return

        is_file = os.path.isfile(path)
        if is_file:
            if not self._ensure_closed(path):
                return
        else:
            # folder — close all open files inside it
            for p in list(self._editors):
                if p.startswith(path + os.sep):
                    self._close_tab_for_path(p)

        try:
            os.rename(path, new_path)
        except Exception as exc:
            QMessageBox.critical(self, "Rename Failed", str(exc))
            return

        if self._project:
            if is_file:
                rel_old = os.path.relpath(path,     self._project.path)
                rel_new = os.path.relpath(new_path, self._project.path)
                if rel_old in self._project.source_files:
                    i = self._project.source_files.index(rel_old)
                    self._project.source_files[i] = rel_new
                    self._project.save()
            else:
                old_prefix = os.path.relpath(path,     self._project.path)
                new_prefix = os.path.relpath(new_path, self._project.path)
                changed = False
                for i, sf in enumerate(self._project.source_files):
                    norm = sf.replace("/", os.sep)
                    if norm.startswith(old_prefix + os.sep):
                        self._project.source_files[i] = new_prefix + norm[len(old_prefix):]
                        changed = True
                if changed:
                    self._project.save()

        self._refresh_explorer()
        if is_file:
            self._open_file(new_path)

    def _fm_delete_file(self, path: str) -> None:
        name = os.path.basename(path)
        ans = QMessageBox.question(
            self, "Delete File", f"Delete '{name}'?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
        )
        if ans != QMessageBox.StandardButton.Yes:
            return
        self._close_tab_for_path(path)
        try:
            os.remove(path)
        except Exception as exc:
            QMessageBox.critical(self, "Delete Failed", str(exc))
            return
        if self._project:
            rel = os.path.relpath(path, self._project.path)
            if rel in self._project.source_files:
                self._project.source_files.remove(rel)
                self._project.save()
        self._refresh_explorer()

    def _fm_delete_dir(self, path: str) -> None:
        name = os.path.basename(path)
        ans = QMessageBox.question(
            self, "Delete Folder",
            f"Delete folder '{name}' and all its contents?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
        )
        if ans != QMessageBox.StandardButton.Yes:
            return
        for p in list(self._editors):
            if p.startswith(path + os.sep):
                self._close_tab_for_path(p)
        try:
            shutil.rmtree(path)
        except Exception as exc:
            QMessageBox.critical(self, "Delete Failed", str(exc))
            return
        if self._project:
            prefix = os.path.relpath(path, self._project.path)
            self._project.source_files = [
                sf for sf in self._project.source_files
                if not sf.replace("/", os.sep).startswith(prefix + os.sep)
            ]
            self._project.save()
        self._refresh_explorer()

    def _fm_move_file(self, path: str) -> None:
        dest_dir = QFileDialog.getExistingDirectory(
            self, "Move to Folder", self._project.path if self._project else "",
        )
        if not dest_dir:
            return
        name     = os.path.basename(path)
        new_path = os.path.join(dest_dir, name)
        if os.path.exists(new_path):
            QMessageBox.warning(self, "Already Exists", f"'{name}' already exists in the destination.")
            return
        if not self._ensure_closed(path):
            return
        try:
            shutil.move(path, new_path)
        except Exception as exc:
            QMessageBox.critical(self, "Move Failed", str(exc))
            return
        if self._project:
            rel_old = os.path.relpath(path,     self._project.path)
            rel_new = os.path.relpath(new_path, self._project.path)
            if rel_old in self._project.source_files:
                i = self._project.source_files.index(rel_old)
                self._project.source_files[i] = rel_new
                self._project.save()
        self._refresh_explorer()
        self._open_file(new_path)

    # ── Tab/editor helpers ────────────────────────────────────────────────────

    def _ensure_closed(self, path: str) -> bool:
        """Close the editor tab for path, prompting to save if dirty.
        Returns False if the user cancelled."""
        if path not in self._editors:
            return True
        if path in self._dirty:
            ans = QMessageBox.question(
                self, "Unsaved Changes",
                f"'{os.path.basename(path)}' has unsaved changes. Save before continuing?",
                QMessageBox.StandardButton.Save |
                QMessageBox.StandardButton.Discard |
                QMessageBox.StandardButton.Cancel,
            )
            if ans == QMessageBox.StandardButton.Cancel:
                return False
            if ans == QMessageBox.StandardButton.Save:
                self._save_file(path)
        self._close_tab_for_path(path)
        return True

    def _close_tab_for_path(self, path: str) -> None:
        """Remove the editor tab for path without prompting."""
        editor = self._editors.pop(path, None)
        self._dirty.discard(path)
        if editor is None:
            return
        for i in range(self._editor_tabs.count()):
            if self._editor_tabs.widget(i) is editor:
                self._editor_tabs.removeTab(i)
                break
        if self._editor_tabs.count() == 0:
            self._show_welcome()

    def _open_file(self, path: str) -> None:
        if path in self._editors:
            self._switch_to_tab(path)
            return

        try:
            with open(path, "r", encoding="utf-8") as f:
                content = f.read()
        except Exception as exc:
            QMessageBox.critical(self, "Open Failed", str(exc))
            return

        editor = CodeEditor()
        self._apply_prefs_to_editor(editor)
        editor.setPlainText(content)

        ext = os.path.splitext(path)[1].lower()
        if ext in (".cpp", ".c", ".hpp", ".h"):
            # Store reference: PyQt6 does not protect QSyntaxHighlighter from
            # Python GC even when Qt holds the C++ object via document parent.
            editor._highlighter = CppHighlighter(editor.document())
            editor._completer   = CppCompleter(editor)

        editor.document().contentsChanged.connect(lambda p=path: self._on_content_changed(p))

        self._editors[path] = editor
        tab_title = os.path.basename(path)
        idx = self._editor_tabs.addTab(editor, tab_title)
        self._editor_tabs.setCurrentIndex(idx)
        self._show_editor_tabs()

    def _switch_to_tab(self, path: str) -> None:
        for i in range(self._editor_tabs.count()):
            w = self._editor_tabs.widget(i)
            if w is self._editors.get(path):
                self._editor_tabs.setCurrentIndex(i)
                return

    def _on_content_changed(self, path: str) -> None:
        if path not in self._dirty:
            self._dirty.add(path)
            self._update_tab_title(path)

    def _update_tab_title(self, path: str) -> None:
        editor = self._editors.get(path)
        if editor is None:
            return
        base = os.path.basename(path)
        title = f"● {base}" if path in self._dirty else base
        for i in range(self._editor_tabs.count()):
            if self._editor_tabs.widget(i) is editor:
                self._editor_tabs.setTabText(i, title)
                break

    def _on_close_tab(self, index: int) -> None:
        editor = self._editor_tabs.widget(index)
        path   = next((p for p, e in self._editors.items() if e is editor), None)
        if path and path in self._dirty:
            ans = QMessageBox.question(
                self, "Unsaved Changes",
                f"{os.path.basename(path)} has unsaved changes.\nSave before closing?",
                QMessageBox.StandardButton.Save |
                QMessageBox.StandardButton.Discard |
                QMessageBox.StandardButton.Cancel,
            )
            if ans == QMessageBox.StandardButton.Cancel:
                return
            if ans == QMessageBox.StandardButton.Save:
                self._save_file(path)
        if path:
            del self._editors[path]
            self._dirty.discard(path)
        self._editor_tabs.removeTab(index)
        if self._editor_tabs.count() == 0:
            self._show_welcome()

    def _on_tab_changed(self, _idx: int) -> None:
        self._update_actions()

    def _close_all_tabs(self) -> None:
        self._editor_tabs.clear()
        self._editors.clear()
        self._dirty.clear()
        self._show_welcome()

    def _show_editor_tabs(self) -> None:
        self._welcome.hide()
        self._editor_tabs.show()

    def _show_welcome(self) -> None:
        self._editor_tabs.hide()
        self._welcome.show()

    # ═══════════════════════════════════════════════════════════════════════════
    # Slots — Build
    # ═══════════════════════════════════════════════════════════════════════════

    def _on_build(self, clean_first: bool = False) -> None:
        if not self._project:
            QMessageBox.information(self, "No Project", "Open or create a project first.")
            return
        if self._build_thread and self._build_thread.isRunning():
            return
        self._on_save_all()
        self._build_output.clear()
        self._show_bottom_tab(0)
        self._sb_build.setText("⚙ Building…")

        if clean_first:
            clean = BuildThread(self._project, "clean")
            clean.output.connect(self._append_build)
            clean.finished.connect(lambda ok, _: self._start_build_after_clean())
            self._build_thread = clean
            clean.start()
        else:
            self._start_build()

    def _start_build_after_clean(self) -> None:
        self._start_build()

    def _start_build(self) -> None:
        if not self._project:
            return
        thread = BuildThread(self._project, "build")
        thread.output.connect(self._append_build)
        thread.finished.connect(self._on_build_finished)
        self._build_thread = thread
        thread.start()

    def _on_clean(self) -> None:
        if not self._project:
            return
        self._build_output.clear()
        self._show_bottom_tab(0)
        thread = BuildThread(self._project, "clean")
        thread.output.connect(self._append_build)
        thread.finished.connect(lambda ok, ms: self._sb_build.setText("Clean done"))
        thread.start()

    def _on_flash(self) -> None:
        if not self._project:
            QMessageBox.information(self, "No Project", "Open or create a project first.")
            return
        if self._build_thread and self._build_thread.isRunning():
            return
        self._build_output.clear()
        self._show_bottom_tab(0)
        self._sb_build.setText("⬆ Flashing…")
        thread = BuildThread(self._project, "flash")
        thread.output.connect(self._append_build)
        thread.finished.connect(self._on_build_finished)
        self._build_thread = thread
        thread.start()

    def _on_build_finished(self, success: bool, duration_ms: float) -> None:
        if success:
            self._sb_build.setText(f"✓ Done  ({duration_ms/1000:.1f}s)")
        else:
            self._sb_build.setText("✗ Failed")
        self._update_actions()

    def _append_build(self, text: str) -> None:
        self._build_output.moveCursor(self._build_output.textCursor().MoveOperation.End)
        self._build_output.insertPlainText(text)
        self._build_output.moveCursor(self._build_output.textCursor().MoveOperation.End)

    # ═══════════════════════════════════════════════════════════════════════════
    # Slots — Serial Monitor
    # ═══════════════════════════════════════════════════════════════════════════

    def _on_bottom_tab_changed(self, index: int) -> None:
        if index == 1:  # Serial Monitor tab
            self._refresh_serial_ports()

    def _refresh_serial_ports(self) -> None:
        current = self._serial_port.currentText()
        self._serial_port.clear()
        ports = list_ports()
        self._serial_port.addItems(ports)
        if current in ports:
            self._serial_port.setCurrentText(current)
        elif ports:
            self._serial_port.setCurrentText(ports[0])

    def _on_serial_toggle(self) -> None:
        if self._serial.isRunning():
            self._serial.close()
        else:
            port = self._serial_port.currentText().strip()
            baud = int(self._serial_baud.currentText())
            if not port:
                QMessageBox.warning(self, "Serial", "Select a port first.")
                return
            self._serial.open(port, baud)

    def _on_serial_data(self, text: str) -> None:
        self._serial_output.moveCursor(
            self._serial_output.textCursor().MoveOperation.End
        )
        self._serial_output.insertPlainText(text)
        self._serial_output.moveCursor(
            self._serial_output.textCursor().MoveOperation.End
        )

    def _on_serial_status(self, connected: bool, port: str) -> None:
        if connected:
            self._btn_serial.setText("Disconnect")
            self._sb_serial.setText(f"Serial: {port}")
        else:
            self._btn_serial.setText("Connect")
            self._sb_serial.setText("Serial: disconnected")

    def _on_serial_send(self) -> None:
        text = self._serial_input.text()
        if text:
            self._serial.send(text + "\n")
            self._serial_input.clear()

    # ═══════════════════════════════════════════════════════════════════════════
    # Slots — Preferences
    # ═══════════════════════════════════════════════════════════════════════════

    def _on_preferences(self) -> None:
        dlg = PrefsDialog(self._prefs, self)
        dlg.prefs_applied.connect(self._apply_prefs)
        dlg.exec()

    def _apply_prefs(self, prefs: IDEPrefs) -> None:
        """Apply *prefs* to all live widgets and re-emit the stylesheet."""
        self._prefs = prefs

        # Editor tabs
        for editor in self._editors.values():
            self._apply_prefs_to_editor(editor)

        # Build output & serial monitor panels
        mono = QFont(prefs.editor_font_family, prefs.output_font_size)
        self._build_output.setFont(mono)
        self._serial_output.setFont(mono)

        # Application-wide stylesheet (UI font size)
        app = QApplication.instance()
        if app:
            app.setStyleSheet(make_dark(prefs.ui_font_size))

    def _apply_prefs_to_editor(self, editor: "CodeEditor") -> None:
        p = self._prefs
        editor.setFont(QFont(p.editor_font_family, p.editor_font_size))
        wrap = (QPlainTextEdit.LineWrapMode.WidgetWidth
                if p.editor_word_wrap
                else QPlainTextEdit.LineWrapMode.NoWrap)
        editor.setLineWrapMode(wrap)
        space_w = editor.fontMetrics().horizontalAdvance(' ')
        editor.setTabStopDistance(p.editor_tab_size * space_w)

    # ═══════════════════════════════════════════════════════════════════════════
    # Slots — Tools
    # ═══════════════════════════════════════════════════════════════════════════

    def _on_library_manager(self) -> None:
        dlg = LibraryManagerDialog(self)
        dlg.exec()
        self._refresh_libraries_pane()   # installs/removals should show up immediately

    # ── Libraries dock ───────────────────────────────────────────────────────

    def _refresh_libraries_pane(self) -> None:
        self._lib_tree.clear()

        sdk_root = QTreeWidgetItem(["MikroDuino SDK"])
        self._lib_tree.addTopLevelItem(sdk_root)
        self._style_category_item(sdk_root)
        for mod in list_sdk_modules():
            item = QTreeWidgetItem([mod["name"]])
            item.setToolTip(0, f"{mod['description']}\nv{mod['version']}")
            item.setData(0, Qt.ItemDataRole.UserRole, {"kind": "sdk", "info": mod})
            sdk_root.addChild(item)

        core_root = QTreeWidgetItem(["Core Peripherals"])
        self._lib_tree.addTopLevelItem(core_root)
        self._style_category_item(core_root)
        for periph in list_core_peripherals():
            item = QTreeWidgetItem([periph["name"]])
            item.setToolTip(0, periph["description"])
            item.setData(0, Qt.ItemDataRole.UserRole, {"kind": "core_peripheral", "info": periph})
            core_root.addChild(item)

        util_root = QTreeWidgetItem(["Core Utilities"])
        self._lib_tree.addTopLevelItem(util_root)
        self._style_category_item(util_root)
        util_groups = list_core_utility_groups()
        for group in util_groups:
            group_item = QTreeWidgetItem([group["label"]])
            self._style_category_item(group_item)
            util_root.addChild(group_item)
            for header in group["headers"]:
                item = QTreeWidgetItem([header["name"]])
                item.setToolTip(0, f"{header['description']}\n#define {group['guard']}")
                item.setData(0, Qt.ItemDataRole.UserRole, {
                    "kind": "core_utility", "info": header, "guard": group["guard"],
                })
                group_item.addChild(item)
        if not util_groups:
            placeholder = QTreeWidgetItem(["(none in this build)"])
            placeholder.setFlags(placeholder.flags() & ~Qt.ItemFlag.ItemIsSelectable)
            util_root.addChild(placeholder)

        arduino_root = QTreeWidgetItem(["Arduino Libraries"])
        self._lib_tree.addTopLevelItem(arduino_root)
        self._style_category_item(arduino_root)
        installed = list_installed()
        if installed:
            for lib in installed:
                item = QTreeWidgetItem([lib["name"]])
                item.setToolTip(0, f"{lib['sentence']}\nv{lib['version']}  by {lib['author']}")
                item.setData(0, Qt.ItemDataRole.UserRole, {"kind": "arduino", "info": lib})
                arduino_root.addChild(item)
        else:
            placeholder = QTreeWidgetItem(["(none installed)"])
            placeholder.setFlags(placeholder.flags() & ~Qt.ItemFlag.ItemIsSelectable)
            arduino_root.addChild(placeholder)

        self._lib_tree.expandAll()
        self._apply_lib_filter(self._lib_filter.text())

    @staticmethod
    def _style_category_item(item: QTreeWidgetItem) -> None:
        item.setFlags(item.flags() & ~Qt.ItemFlag.ItemIsSelectable)
        f = item.font(0)
        f.setBold(True)
        item.setFont(0, f)

    def _apply_lib_filter(self, text: str) -> None:
        needle = text.strip().lower()
        for i in range(self._lib_tree.topLevelItemCount()):
            self._apply_lib_filter_recursive(self._lib_tree.topLevelItem(i), needle)

    def _apply_lib_filter_recursive(self, item: QTreeWidgetItem, needle: str) -> bool:
        """Hide leaf items that don't match; a group (a category or a Core
        Utilities sub-group, e.g. "Cryptography") stays visible as long as
        at least one descendant matches. Returns whether item is visible."""
        if item.childCount() == 0:
            visible = (not needle) or (needle in item.text(0).lower())
            item.setHidden(not visible)
            return visible

        any_visible = False
        for j in range(item.childCount()):
            if self._apply_lib_filter_recursive(item.child(j), needle):
                any_visible = True
        item.setHidden(not any_visible)
        return any_visible

    def _on_library_item_activated(self, item: QTreeWidgetItem, _column: int = 0) -> None:
        data = item.data(0, Qt.ItemDataRole.UserRole)
        if not data:
            return   # category/group header or "(none installed)" placeholder
        kind = data["kind"]
        if kind == "sdk":
            self._add_sdk_module_to_project(data["info"])
        elif kind == "arduino":
            self._add_arduino_library_to_project(data["info"])
        elif kind == "core_peripheral":
            self._add_core_peripheral(data["info"])
        elif kind == "core_utility":
            self._add_core_utility(data["info"], data["guard"])

    def _add_sdk_module_to_project(self, mod: dict) -> None:
        """Wire an SDK module into the current project's build config (its
        include dir, and its .cpp if it has one — see list_sdk_modules()'s
        docstring for why this needs explicit wiring, unlike Arduino
        libraries below) and insert its #include at the cursor."""
        if not self._project:
            QMessageBox.information(self, "No Project", "Open or create a project first.")
            return

        changed = False
        if mod["include_dir"] not in self._project.build.include_dirs:
            self._project.build.include_dirs.append(mod["include_dir"])
            changed = True
        if mod["cpp_path"] and mod["cpp_path"] not in self._project.build.extra_sources:
            self._project.build.extra_sources.append(mod["cpp_path"])
            changed = True
        if changed:
            self._project.save()
            self._append_build(f"Added SDK module '{mod['name']}' to the project build config.\n")

        self._insert_include_or_notify(mod["name"], mod["header_name"])

    def _add_arduino_library_to_project(self, lib: dict) -> None:
        """Insert the library's primary header's #include at the cursor.
        No project-file wiring needed: builder.py's _resolve_libraries()
        already auto-detects any installed library from that #include alone."""
        if not self._project:
            QMessageBox.information(self, "No Project", "Open or create a project first.")
            return

        header = arduino_primary_header(lib["dir"])
        if not header:
            QMessageBox.warning(self, "No Header Found",
                                 f"Could not find a header file in '{lib['name']}'.")
            return

        self._insert_include_or_notify(lib["name"], header)

    def _add_core_peripheral(self, periph: dict) -> None:
        """Layer-1 core headers need no project-file wiring at all — the
        core include root is always on -I (see builder.py's _SDK_INCLUDE),
        and no project needs to be open either, just an editor tab."""
        self._insert_include_or_notify(periph["name"], periph["include"])

    def _add_core_utility(self, header: dict, guard: str) -> None:
        """Layer-2 utility headers need '#define MD_INCLUDE_<GUARD>' placed
        BEFORE the project's #include <mikroduino/mikroduino.hpp> — that
        header is #pragma once, so the guard has no effect if defined
        after it's already been processed once in this file. If that
        #include already exists in the current editor, the #define is
        inserted right above it (wherever it is) rather than at the
        cursor; only when there's no existing #include yet does this fall
        back to inserting both lines at the cursor."""
        editor = self._current_editor()
        if editor is None:
            QMessageBox.information(
                self, "Open a File First",
                f"Open a source file, then add:\n\n"
                f"#define {guard}\n#include <mikroduino/mikroduino.hpp>\n\n"
                f"(the #define must come before that #include)"
            )
            return

        umbrella_block = self._find_umbrella_include_block(editor)
        if umbrella_block is not None:
            insert_cursor = QTextCursor(umbrella_block)
            insert_cursor.movePosition(QTextCursor.MoveOperation.StartOfBlock)
            insert_cursor.insertText(f"#define {guard}\n")
        else:
            cursor = editor.textCursor()
            prefix = "" if cursor.atBlockStart() else "\n"
            cursor.insertText(f"{prefix}#define {guard}\n#include <mikroduino/mikroduino.hpp>\n")
            editor.setTextCursor(cursor)
        editor.setFocus()

    _UMBRELLA_INCLUDE_RE = re.compile(r'^\s*#\s*include\s+[<"]mikroduino/mikroduino\.hpp[>"]')

    @classmethod
    def _find_umbrella_include_block(cls, editor: CodeEditor):
        """Return the QTextBlock containing the first
        '#include <mikroduino/mikroduino.hpp>' line in editor, or None."""
        block = editor.document().begin()
        while block.isValid():
            if cls._UMBRELLA_INCLUDE_RE.match(block.text()):
                return block
            block = block.next()
        return None

    def _insert_include_or_notify(self, lib_name: str, header_name: str) -> None:
        if not self._insert_include_at_cursor(f"<{header_name}>"):
            QMessageBox.information(
                self, "Library Added",
                f"'{lib_name}' is ready to use.\n"
                f"Open a source file and add:\n\n#include <{header_name}>"
            )

    def _insert_include_at_cursor(self, header_line: str) -> bool:
        """Insert '#include header_line' on its own line at the current
        cursor position of the active editor tab. Returns False if there
        is no open editor tab to insert into."""
        editor = self._current_editor()
        if editor is None:
            return False
        cursor = editor.textCursor()
        prefix = "" if cursor.atBlockStart() else "\n"
        cursor.insertText(f"{prefix}#include {header_line}\n")
        editor.setTextCursor(cursor)
        editor.setFocus()
        return True

    # ── Projects Folder menu ─────────────────────────────────────────────────

    def _populate_projects_folder_menu(self) -> None:
        self._projects_menu.clear()
        self._projects_menu.addAction("Set Projects Folder…", self._on_set_projects_folder)
        self._projects_menu.addSeparator()

        root = self._prefs.projects_dir
        nodes = list_mdp_tree(root) if root else []
        if not nodes:
            msg = "(no projects found here)" if root and os.path.isdir(root) else "(no Projects Folder set)"
            act = self._projects_menu.addAction(msg)
            act.setEnabled(False)
            return
        self._add_mdp_tree_nodes(self._projects_menu, nodes, self._open_project_path)

    def _on_set_projects_folder(self) -> None:
        start = self._prefs.projects_dir if os.path.isdir(self._prefs.projects_dir) else os.path.expanduser("~")
        path = QFileDialog.getExistingDirectory(self, "Select Projects Folder", start)
        if not path:
            return
        self._prefs.projects_dir = path
        self._prefs.save()

    def _populate_examples_menu(self) -> None:
        self._examples_menu.clear()

        arduino_sub = self._examples_menu.addMenu("Arduino Library Examples")
        self._populate_arduino_examples_menu(arduino_sub)

        ide_sub = self._examples_menu.addMenu("Examples with IDE")
        self._populate_mdp_examples_menu(ide_sub, respaths.EXAMPLES_ROOT)

        samples_sub = self._examples_menu.addMenu("Samples")
        self._populate_mdp_examples_menu(samples_sub, respaths.SAMPLES_ROOT)

    def _populate_arduino_examples_menu(self, menu: QMenu) -> None:
        """Examples bundled inside installed Arduino-compatible libraries
        (~/.mikroduino/libraries/<lib>/examples/*.ino) — imported (converted
        and copied) into a new project on click, same as File > Import Sketch."""
        has_any = False
        for lib in list_installed():
            exs = list_examples(lib["dir"])
            if not exs:
                continue
            has_any = True
            lib_sub = menu.addMenu(lib["name"])
            for ex in exs:
                ino_files = [f for f in ex["files"] if f.endswith(".ino")]
                if not ino_files:
                    continue
                # Prefer the file whose stem matches the example folder name
                folder = ex["name"]
                primary = next(
                    (f for f in ino_files
                     if os.path.splitext(os.path.basename(f))[0] == folder),
                    ino_files[0],
                )
                lib_sub.addAction(
                    ex["name"], lambda p=primary: self._do_import_ino(p)
                )
        if not has_any:
            act = menu.addAction("(no examples installed)")
            act.setEnabled(False)

    def _populate_mdp_examples_menu(self, menu: QMenu, root_dir: str) -> None:
        """Native MikroDuino .mdp example projects shipped with the IDE
        itself — examples/ ("Examples with IDE") or Samples/ ("Samples").
        Copied into a new project on click, same reasoning as the Arduino
        examples above: the shipped copy must never be edited in place."""
        nodes = list_mdp_tree(root_dir)
        if not nodes:
            act = menu.addAction("(none found)")
            act.setEnabled(False)
            return
        self._add_mdp_tree_nodes(menu, nodes, self._do_import_mdp_example)

    def _add_mdp_tree_nodes(self, menu: QMenu, nodes: list[dict], on_select) -> None:
        """Recursively mirror an list_mdp_tree() tree into menu, calling
        on_select(mdp_path) when a leaf project is clicked. Shared between
        the Examples submenus (which copy-then-open, see
        _do_import_mdp_example) and the Projects Folder menu (which opens
        the project directly — it's the user's own, not a shipped copy)."""
        for node in nodes:
            if node["kind"] == "project":
                menu.addAction(
                    node["name"].replace("_", " "),
                    lambda p=node["mdp_path"]: on_select(p),
                )
            else:
                sub = menu.addMenu(node["name"])
                self._add_mdp_tree_nodes(sub, node["children"], on_select)

    def _on_import_ino(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self, "Import Arduino Sketch", "",
            "Arduino Sketch (*.ino);;All Files (*)"
        )
        if path:
            self._do_import_ino(path)

    def _do_import_ino(self, ino_path: str) -> None:
        # Starts in the configured Projects Folder as a convenience — the
        # user can still navigate anywhere else in the same dialog.
        start_dir = self._prefs.projects_dir if os.path.isdir(self._prefs.projects_dir) else ""
        parent_dir = QFileDialog.getExistingDirectory(
            self, "Select Folder for New Project", start_dir
        )
        if not parent_dir:
            return

        sketch_name = os.path.splitext(os.path.basename(ino_path))[0]
        project_dir = os.path.join(parent_dir, sketch_name)
        src_dir     = os.path.join(project_dir, "src")

        if os.path.exists(project_dir):
            ans = QMessageBox.question(
                self, "Folder Already Exists",
                f"'{sketch_name}' already exists in that location.\n"
                "Files inside will be overwritten. Continue?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            )
            if ans != QMessageBox.StandardButton.Yes:
                return

        try:
            os.makedirs(src_dir, exist_ok=True)
            created = import_ino(ino_path, src_dir)
        except Exception as exc:  # noqa: BLE001
            QMessageBox.critical(self, "Import Failed", str(exc))
            return

        source_files = [
            os.path.relpath(p, project_dir).replace("\\", "/")
            for p in created
            if p.endswith((".cpp", ".c"))
        ]
        project = Project(path=project_dir, name=sketch_name)
        project.source_files = source_files
        project.save()

        self._load_project(project)
        for path in created:
            self._open_file(path)

    def _do_import_mdp_example(self, mdp_path: str) -> None:
        """Copy a native .mdp example project (from Examples with IDE / Samples)
        into a user-chosen location and open the copy — never edit the
        shipped original in place, same reasoning as _do_import_ino above."""
        src_dir = os.path.dirname(mdp_path)
        folder_name = os.path.basename(src_dir)

        # Starts in the configured Projects Folder as a convenience — the
        # user can still navigate anywhere else in the same dialog.
        start_dir = self._prefs.projects_dir if os.path.isdir(self._prefs.projects_dir) else ""
        parent_dir = QFileDialog.getExistingDirectory(
            self, "Select Folder for New Project", start_dir
        )
        if not parent_dir:
            return

        dest_dir = os.path.join(parent_dir, folder_name)
        if os.path.exists(dest_dir):
            ans = QMessageBox.question(
                self, "Folder Already Exists",
                f"'{folder_name}' already exists in that location.\n"
                "Files inside will be overwritten. Continue?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            )
            if ans != QMessageBox.StandardButton.Yes:
                return
            try:
                shutil.rmtree(dest_dir)
            except Exception as exc:  # noqa: BLE001
                QMessageBox.critical(self, "Import Failed", str(exc))
                return

        try:
            # Never copy stale build output along with the example.
            shutil.copytree(src_dir, dest_dir, ignore=shutil.ignore_patterns("build"))
        except Exception as exc:  # noqa: BLE001
            QMessageBox.critical(self, "Import Failed", str(exc))
            return

        dest_mdp = os.path.join(dest_dir, os.path.basename(mdp_path))
        try:
            project = Project.open(dest_mdp)
        except Exception as exc:  # noqa: BLE001
            QMessageBox.critical(
                self, "Import Failed",
                f"Copied to '{dest_dir}', but could not open the project:\n{exc}"
            )
            return

        # The shipped example's includeDirs/extraSources are relative paths
        # pointing back into the SDK (e.g. "../../../../sdk/modules/X/include"),
        # valid only at the example's original depth under examples/ or
        # Samples/. Now that the project lives at dest_dir, resolve any
        # relative entries against the ORIGINAL location (src_dir) and
        # rewrite them as absolute paths, so the copy still finds the SDK
        # no matter where the user put it.
        project.build.include_dirs = [
            p if os.path.isabs(p) else os.path.normpath(os.path.join(src_dir, p))
            for p in project.build.include_dirs
        ]
        project.build.extra_sources = [
            p if os.path.isabs(p) else os.path.normpath(os.path.join(src_dir, p))
            for p in project.build.extra_sources
        ]
        # Project.save() writes to "{name}.mdp", but the on-disk filename
        # (dest_mdp) is a separate short slug that never matches the human-
        # readable projectName (e.g. "hello_shift_basics.mdp" vs. "SevenSegShift
        # Hello Shift Basics") for every shipped example — write back to the
        # exact file we just opened instead of letting save() create a
        # differently-named duplicate.
        with open(dest_mdp, "w", encoding="utf-8") as f:
            json.dump(project.to_dict(), f, indent=2)

        self._load_project(project, mdp_path=dest_mdp)

    # ═══════════════════════════════════════════════════════════════════════════
    # Slots — Help
    # ═══════════════════════════════════════════════════════════════════════════

    def _on_docs(self) -> None:
        if self._docs_dialog is None:
            self._docs_dialog = DocsBrowserDialog(self)
        self._docs_dialog.show()
        self._docs_dialog.raise_()
        self._docs_dialog.activateWindow()

    def _on_about(self) -> None:
        version = QApplication.applicationVersion() or "1.0"
        QMessageBox.about(
            self, "About MikroDuino Studio",
            "<b>MikroDuino Studio</b><br>"
            f"Version {version} (Python / PyQt6)<br><br>"
            "Embedded development IDE for AVR microcontrollers.<br>"
            "© 2026 Mikrotronics Pakistan"
        )

    # ═══════════════════════════════════════════════════════════════════════════
    # Helpers
    # ═══════════════════════════════════════════════════════════════════════════

    def _current_editor(self) -> Optional[CodeEditor]:
        w = self._editor_tabs.currentWidget()
        if isinstance(w, CodeEditor):
            return w
        return None

    def _current_tab_path(self) -> Optional[str]:
        editor = self._current_editor()
        if editor is None:
            return None
        for path, e in self._editors.items():
            if e is editor:
                return path
        return None

    def _update_actions(self) -> None:
        has_project = self._project is not None
        busy        = bool(self._build_thread and self._build_thread.isRunning())
        self._act_save.setEnabled(bool(self._current_tab_path()))
        self._act_save_all.setEnabled(len(self._dirty) > 0)
        self._act_props.setEnabled(has_project)
        self._act_build.setEnabled(has_project and not busy)
        self._act_rebuild.setEnabled(has_project and not busy)
        self._act_clean.setEnabled(has_project)
        self._act_flash.setEnabled(has_project and not busy)

    def _update_statusbar(self) -> None:
        p = self._project
        if p:
            self._sb_mcu.setText(f"⚡ {p.target.mcu}")
            self._sb_clock.setText(f"🕐 {p.target.clock // 1_000_000} MHz")
            self._sb_prog.setText(f"⬆ {p.programmer.type}")
        else:
            self._sb_mcu.setText("—")
            self._sb_clock.setText("—")
            self._sb_prog.setText("—")

    def _show_bottom_tab(self, index: int) -> None:
        self._bottom_dock.show()
        self._bottom_tabs.setCurrentIndex(index)

    # ── Window close ─────────────────────────────────────────────────────────

    def closeEvent(self, event) -> None:   # type: ignore[override]
        if self._dirty:
            ans = QMessageBox.question(
                self, "Unsaved Changes",
                "You have unsaved changes. Save all before exit?",
                QMessageBox.StandardButton.Save |
                QMessageBox.StandardButton.Discard |
                QMessageBox.StandardButton.Cancel,
            )
            if ans == QMessageBox.StandardButton.Cancel:
                event.ignore(); return
            if ans == QMessageBox.StandardButton.Save:
                self._on_save_all()
        self._serial.close()
        event.accept()


# ── Module-level helpers ──────────────────────────────────────────────────────

def _add_action(menu, text: str, shortcut, slot) -> QAction:
    """Create a QAction with shortcut+slot and add it to menu."""
    act = QAction(text, menu)
    act.setShortcut(QKeySequence(shortcut))
    act.triggered.connect(slot)
    menu.addAction(act)
    return act


def _sep() -> QWidget:
    w = QWidget(); w.setFixedWidth(1)
    w.setStyleSheet("background: #7f849c;")
    return w
