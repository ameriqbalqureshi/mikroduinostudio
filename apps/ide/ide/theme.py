import os as _os
from . import respaths as _respaths
_ARROW_DOWN = _os.path.join(_respaths.ASSETS_DIR, "arrow_down.svg").replace("\\", "/")


def make_dark(ui_font_size: int = 13) -> str:
    """Return the dark stylesheet with the given UI font size (in pt)."""
    return _DARK_TEMPLATE.replace("__UI_FONT_SIZE__", str(ui_font_size)) \
                         .replace("__ARROW_DOWN__", _ARROW_DOWN)


_DARK_TEMPLATE = """
QMainWindow, QDialog, QWidget {
    background-color: #1e1e2e ;   
    color: #cdd6f4;
    font-family: 'Segoe UI', Tahoma, sans-serif;
    font-size: __UI_FONT_SIZE__px;
}

QMenuBar {
    background-color: #181825;
    color: #cdd6f4;
    padding: 2px 0;
}
QMenuBar::item { padding: 4px 10px; }
QMenuBar::item:selected { background-color: #313244; }

QMenu {
    background-color: #252535;
    border: 1px solid #45475a;
    padding: 4px 0;
}
QMenu::item { padding: 4px 24px; }
QMenu::item:selected { background-color: #45475a; }
QMenu::separator { height: 1px; background: #45475a; margin: 4px 8px; }

QToolBar {
    background-color: #181825;
    border-bottom: 1px solid #313244;
    spacing: 4px;
    padding: 3px 6px;
}
QToolBar::separator { width: 1px; background: #45475a; margin: 2px 4px; }

QToolButton {
    background-color: transparent;
    color: #cdd6f4;
    border: none;
    padding: 4px 10px;
    border-radius: 4px;
    font-size: 12px;
}
QToolButton:hover { background-color: #313244; }
QToolButton:pressed { background-color: #45475a; }
QToolButton[objectName="btn_build"] {
    background-color: #7c5af6;
    color: #ffffff;
    padding: 4px 14px;
}
QToolButton[objectName="btn_build"]:hover { background-color: #9171f8; }
QToolButton[objectName="btn_flash"] {
    background-color: #313244;
    color: #cdd6f4;
}

QPushButton {
    background-color: #313244;
    color: #cdd6f4;
    border: none;
    padding: 5px 14px;
    border-radius: 4px;
}
QPushButton:hover { background-color: #45475a; }
QPushButton:pressed { background-color: #585b70; }
QPushButton[primary="true"] {
    background-color: #7c5af6;
    color: #ffffff;
}
QPushButton[primary="true"]:hover { background-color: #9171f8; }
QPushButton:disabled { color: #585b70; }

QTabWidget::pane { border: none; border-top: 1px solid #313244; }
QTabBar { background-color: #181825; }
QTabBar::tab {
    background-color: #252535;
    color: #7f849c;
    padding: 6px 16px;
    border-right: 1px solid #313244;
    min-width: 80px;
}
QTabBar::tab:selected {
    background-color: #1e1e2e;
    color: #cdd6f4;
    border-top: 2px solid #7c5af6;
}
QTabBar::tab:hover:!selected { background-color: #2a2a3e; }
QTabBar::close-button { image: none; }

QTreeWidget {
    background-color: #252535;
    border: none;
    color: #cdd6f4;
    outline: none;
}
QTreeWidget::item {
    padding: 3px 4px;
    border-left: 2px solid transparent;
}
QTreeWidget::item:selected {
    background-color: #3a3a5c;
    color: #ffffff;
    border-left: 2px solid #7c5af6;
}
QTreeWidget::item:hover:!selected {
    background-color: #2d2d48;
    color: #e4e4f8;
    border-left: 2px solid transparent;
}
QTreeWidget::branch { background: transparent; }

QPlainTextEdit {
    background-color: #1e1e2e;
    border: none;
    selection-background-color: #3d3f52;
}

QTextEdit {
    background-color: #1e1e2e;
    color: #cdd6f4;
    border: none;
    selection-background-color: #3d3f52;
}

QStatusBar {
    background-color: #7c5af6;
    color: #ffffff;
    font-size: 12px;
    padding: 0 8px;
}
QStatusBar::item { border: none; }

QLabel { color: #cdd6f4; }

QLineEdit, QSpinBox, QDoubleSpinBox {
    background-color: #313244;
    color: #cdd6f4;
    border: 1px solid #45475a;
    padding: 4px 8px;
    border-radius: 3px;
}
QLineEdit:focus, QSpinBox:focus { border-color: #7c5af6; }

QComboBox {
    background-color: #313244;
    color: #cdd6f4;
    border: 1px solid #45475a;
    padding: 4px 8px;
    border-radius: 3px;
    min-height: 20px;
}
QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 22px;
    border: none;
    border-left: 1px solid #45475a;
    border-radius: 0 3px 3px 0;
    background-color: #3a3a52;
}
QComboBox::drop-down:hover { background-color: #45475a; }
QComboBox::down-arrow { image: url(__ARROW_DOWN__); width: 10px; height: 6px; }
QComboBox QAbstractItemView {
    background-color: #252535;
    color: #cdd6f4;
    selection-background-color: #45475a;
    border: 1px solid #45475a;
    outline: none;
}

QScrollBar:vertical {
    background: transparent;
    width: 8px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: #45475a;
    border-radius: 4px;
    min-height: 20px;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal {
    background: transparent;
    height: 8px;
}
QScrollBar::handle:horizontal {
    background: #45475a;
    border-radius: 4px;
    min-width: 20px;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

QSplitter::handle {
    background-color: #313244;
}

QDockWidget {
    titlebar-close-icon: url(none);
    titlebar-normal-icon: url(none);
    color: #cdd6f4;
}
QDockWidget::title {
    background-color: #252535;
    padding: 6px 8px;
    font-size: 11px;
    font-weight: bold;
    letter-spacing: 0.5px;
    border-bottom: 1px solid #313244;
}

QGroupBox {
    border: 1px solid #45475a;
    border-radius: 4px;
    margin-top: 8px;
    padding-top: 8px;
    font-weight: bold;
    font-size: 12px;
    color: #7f849c;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 8px;
    padding: 0 4px;
}

QDialogButtonBox QPushButton { min-width: 80px; }

QHeaderView::section {
    background-color: #252535;
    color: #7f849c;
    padding: 4px 8px;
    border: none;
    border-right: 1px solid #313244;
    font-size: 11px;
}
"""

# Backward-compatible constant (used by any import that predates make_dark).
DARK = make_dark()
