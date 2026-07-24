#!/usr/bin/env python3
"""MikroDuino Studio — entry point."""

import sys
import os

# Ensure the apps/ide directory is on the path
sys.path.insert(0, os.path.dirname(__file__))

from PyQt6.QtWidgets import QApplication
from PyQt6.QtCore import Qt
from ide.main_window import MainWindow
from ide.prefs import IDEPrefs
from ide.theme import make_dark


def main() -> None:
    prefs = IDEPrefs.load()

    app = QApplication(sys.argv)
    app.setApplicationName("MikroDuino Studio")
    app.setApplicationVersion("1.0")
    app.setOrganizationName("MikroDuino")
    app.setStyle("Fusion")
    app.setStyleSheet(make_dark(prefs.ui_font_size))

    window = MainWindow(prefs)
    window.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
