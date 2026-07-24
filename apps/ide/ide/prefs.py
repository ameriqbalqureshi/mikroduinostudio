"""IDE-wide user preferences — persisted to ~/.mikroduino/prefs.json."""

from __future__ import annotations

import json
import os
from dataclasses import asdict, dataclass, fields
from pathlib import Path

_PREFS_PATH = Path.home() / ".mikroduino" / "prefs.json"
_DEFAULT_PROJECTS_DIR = os.path.join(os.path.expanduser("~"), "MikroDuinoProjects")


@dataclass
class IDEPrefs:
    # Editor
    editor_font_family: str = "Cascadia Code"
    editor_font_size: int   = 13
    editor_tab_size: int    = 4
    editor_word_wrap: bool  = False

    # UI chrome
    ui_font_size: int = 13

    # Output panels (build log + serial monitor)
    output_font_size: int = 12

    # Session — path to the .mdp file of the last opened/created project,
    # reopened automatically on startup if it still exists.
    last_project_path: str = ""

    # Projects folder ("sketchbook"): where New Project's folder picker
    # starts, and the root the File > Projects Folder menu lists for
    # one-click access. The user is never forced to create projects here.
    projects_dir: str = _DEFAULT_PROJECTS_DIR

    # ── Persistence ────────────────────────────────────────────────────────

    @classmethod
    def load(cls) -> "IDEPrefs":
        """Return saved prefs, or defaults if the file is missing/corrupt."""
        try:
            raw = json.loads(_PREFS_PATH.read_text(encoding="utf-8"))
            valid = {f.name for f in fields(cls)}
            return cls(**{k: v for k, v in raw.items() if k in valid})
        except Exception:
            return cls()

    def save(self) -> None:
        _PREFS_PATH.parent.mkdir(parents=True, exist_ok=True)
        _PREFS_PATH.write_text(
            json.dumps(asdict(self), indent=2), encoding="utf-8"
        )
