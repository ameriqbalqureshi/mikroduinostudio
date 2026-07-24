import json
import os
from dataclasses import dataclass, field
from typing import List, Optional


@dataclass
class TargetConfig:
    mcu: str = "ATmega328P"
    clock: int = 16_000_000


@dataclass
class ProgrammerConfig:
    type: str = "USBASP"
    port: str = ""
    baud_rate: int = 115200


@dataclass
class BuildConfig:
    optimization: str = "O2"
    warnings: str = "all"
    cpp_standard: str = "c++17"
    include_dirs: List[str] = field(default_factory=list)
    extra_sources: List[str] = field(default_factory=list)


@dataclass
class Project:
    path: str = ""
    name: str = "NewProject"
    version: str = "1.0.0"
    target: TargetConfig = field(default_factory=TargetConfig)
    programmer: ProgrammerConfig = field(default_factory=ProgrammerConfig)
    build: BuildConfig = field(default_factory=BuildConfig)
    source_files: List[str] = field(default_factory=list)

    # ── Serialisation ─────────────────────────────────────────────────────────

    def to_dict(self) -> dict:
        return {
            "projectName": self.name,
            "version": self.version,
            "target": {
                "mcu": self.target.mcu,
                "clock": self.target.clock,
            },
            "programmer": {
                "type": self.programmer.type,
                "port": self.programmer.port,
                "baudRate": self.programmer.baud_rate,
            },
            "build": {
                "optimization": self.build.optimization,
                "warnings": self.build.warnings,
                "cppStandard": self.build.cpp_standard,
                "includeDirs": self.build.include_dirs,
                "extraSources": self.build.extra_sources,
            },
            "sourceFiles": self.source_files,
        }

    @classmethod
    def from_dict(cls, data: dict, project_path: str) -> "Project":
        p = cls(path=project_path)
        p.name = data.get("projectName", "NewProject")
        p.version = data.get("version", "1.0.0")

        t = data.get("target", {})
        p.target = TargetConfig(
            mcu=t.get("mcu", "ATmega328P"),
            clock=t.get("clock", 16_000_000),
        )

        pr = data.get("programmer", {})
        p.programmer = ProgrammerConfig(
            type=pr.get("type", "USBASP"),
            port=pr.get("port", ""),
            baud_rate=pr.get("baudRate", 115200),
        )

        b = data.get("build", {})
        p.build = BuildConfig(
            optimization=b.get("optimization", "O2"),
            warnings=b.get("warnings", "all"),
            cpp_standard=b.get("cppStandard", "c++17"),
            include_dirs=b.get("includeDirs", []),
            extra_sources=b.get("extraSources", []),
        )

        p.source_files = data.get("sourceFiles", [])

        # Legacy / hand-crafted .mdp files may omit sourceFiles entirely.
        # Auto-discover all C/C++ files in the project directory so the build
        # system always has something to compile.
        if not p.source_files:
            p.source_files = _discover_sources(project_path)

        return p

    # ── Persistence ───────────────────────────────────────────────────────────

    @property
    def mdp_path(self) -> str:
        return os.path.join(self.path, f"{self.name}.mdp")

    def save(self) -> None:
        with open(self.mdp_path, "w", encoding="utf-8") as f:
            json.dump(self.to_dict(), f, indent=2)

    @classmethod
    def open(cls, mdp_file: str) -> "Project":
        with open(mdp_file, "r", encoding="utf-8") as f:
            data = json.load(f)
        return cls.from_dict(data, os.path.dirname(mdp_file))

    # ── Factory ───────────────────────────────────────────────────────────────

    @classmethod
    def create(cls, parent_dir: str, project_name: str) -> "Project":
        project_dir = os.path.join(parent_dir, project_name)
        src_dir = os.path.join(project_dir, "src")
        os.makedirs(src_dir, exist_ok=True)

        main_cpp = os.path.join(src_dir, "main.cpp")
        with open(main_cpp, "w", encoding="utf-8") as f:
            f.write(
                "#include <avr/io.h>\n"
                "#include <util/delay.h>\n"
                "\n"
                "int main() {\n"
                "    // Set PB5 (built-in LED) as output\n"
                "    DDRB |= (1 << DDB5);\n"
                "\n"
                "    while (1) {\n"
                "        PORTB |= (1 << PORTB5);   // LED on\n"
                "        _delay_ms(500);\n"
                "        PORTB &= ~(1 << PORTB5);  // LED off\n"
                "        _delay_ms(500);\n"
                "    }\n"
                "    return 0;\n"
                "}\n"
            )

        p = cls(path=project_dir, name=project_name)
        p.source_files = ["src/main.cpp"]
        p.save()
        return p

    # ── File tree ─────────────────────────────────────────────────────────────

    def list_files(self) -> List[dict]:
        """Return a flat list of {path, name, rel} dicts for all files in the project."""
        results = []
        for root, dirs, files in os.walk(self.path):
            # Skip hidden dirs and build output
            dirs[:] = [d for d in dirs if not d.startswith(".") and d != "build"]
            for fname in sorted(files):
                if fname.endswith((".cpp", ".c", ".hpp", ".h", ".mdp", ".md", ".txt")):
                    full = os.path.join(root, fname)
                    rel = os.path.relpath(full, self.path)
                    results.append({"path": full, "name": fname, "rel": rel,
                                    "dir": os.path.relpath(root, self.path)})
        return results


# ── Module helpers ────────────────────────────────────────────────────────────

def _discover_sources(project_path: str) -> list[str]:
    """Walk the project directory and return relative paths of all C/C++ files,
    skipping the build output directory and hidden folders."""
    found: list[str] = []
    for root, dirs, files in os.walk(project_path):
        dirs[:] = [d for d in dirs if d != "build" and not d.startswith(".")]
        for fname in sorted(files):
            if fname.endswith((".cpp", ".c", ".cc", ".cxx")):
                abs_path = os.path.join(root, fname)
                rel = os.path.relpath(abs_path, project_path)
                found.append(rel)
    return found
