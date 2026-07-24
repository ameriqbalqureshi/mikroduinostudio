import os
import re
import shutil
import subprocess
import sys
import time

from PyQt6.QtCore import QThread, pyqtSignal

from . import respaths
from .project import Project

# On Windows, a windowed (console=False) PyInstaller build has no console of
# its own, so each child process (avr-gcc, avrdude, ...) would otherwise
# briefly flash its own new console window. CREATE_NO_WINDOW suppresses that;
# it's a no-op on other platforms.
_NO_WINDOW = subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0


# ── SDK / compat paths ────────────────────────────────────────────────────────

_SDK_ROOT = respaths.SDK_ROOT
_SDK_INCLUDE      = os.path.join(_SDK_ROOT, "core", "avr", "include")
_SDK_INCLUDE_MIKRO = os.path.join(_SDK_INCLUDE, "mikroduino")
_COMPAT_INCLUDE   = os.path.join(_SDK_ROOT, "compat", "include")
_COMPAT_SRC       = os.path.join(_SDK_ROOT, "compat", "src")

# ── Library include scanner ───────────────────────────────────────────────────

_INCLUDE_RE = re.compile(r'^\s*#\s*include\s+[<"]([^>"]+)[>"]', re.MULTILINE)

# Compat headers → which .cpp files they need compiled alongside the project
_COMPAT_TRIGGERS: dict[str, list[str]] = {
    "Arduino.h": ["Arduino.cpp", "WString.cpp", "Print.cpp"],
    "WString.h": ["WString.cpp"],
    "Print.h":   ["Print.cpp"],
    "SPI.h":     ["SPI.cpp"],
    "Wire.h":    ["Wire.cpp"],
}

# User library search paths (populated at import time; project-local path is
# added dynamically in _resolve_libraries because it varies per project).
_GLOBAL_LIB_DIR = os.path.join(os.path.expanduser("~"), ".mikroduino", "libraries")


def _scan_includes(path: str) -> set[str]:
    """Return basenames of all headers #include'd by the file at *path*."""
    try:
        with open(path, encoding="utf-8", errors="ignore") as f:
            return set(_INCLUDE_RE.findall(f.read()))
    except OSError:
        return set()

def _tool(name: str) -> str:
    """Resolve *name* (e.g. "avr-gcc") to the bundled toolchain exe if present,
    else fall back to the bare command name (resolved via system PATH)."""
    bundled = os.path.join(respaths.AVR_GCC_BIN, f"{name}.exe")
    return bundled if os.path.isfile(bundled) else name


def _avrdude_cmd() -> list[str]:
    """Return the avrdude invocation prefix: bundled exe + explicit config
    path when available, else the bare "avrdude" command from PATH."""
    bundled_exe  = os.path.join(respaths.AVRDUDE_DIR, "avrdude.exe")
    bundled_conf = os.path.join(respaths.AVRDUDE_DIR, "avrdude.conf")
    if os.path.isfile(bundled_exe) and os.path.isfile(bundled_conf):
        return [bundled_exe, "-C", bundled_conf]
    return ["avrdude"]


MCU_MAP = {
    "ATmega328P": "atmega328p",
    "ATmega32":   "atmega32",
    "ATmega16":   "atmega16",
    "ATmega64":   "atmega64",
    "ATmega128":  "atmega128",
}


class BuildThread(QThread):
    output   = pyqtSignal(str)
    finished = pyqtSignal(bool, float)   # (success, duration_ms)

    def __init__(self, project: Project, action: str = "build") -> None:
        super().__init__()
        self.project = project
        self.action  = action

    # ── QThread entry ─────────────────────────────────────────────────────────

    def run(self) -> None:
        start = time.time()
        try:
            ok = {"build": self._build, "clean": self._clean,
                  "flash": self._flash}.get(self.action, lambda: False)()
        except Exception as exc:
            self.output.emit(f"[EXCEPTION] {exc}\n")
            ok = False
        self.finished.emit(ok, (time.time() - start) * 1000)

    # ── Build ─────────────────────────────────────────────────────────────────

    def _build(self) -> bool:
        p       = self.project
        avr_mcu = MCU_MAP.get(p.target.mcu, p.target.mcu.lower())
        clock   = p.target.clock
        opt     = f"-{p.build.optimization}"
        std     = p.build.cpp_standard

        # All paths normalised so avr-gcc always receives clean backslash paths on Windows
        build_dir = os.path.normpath(os.path.join(p.path, "build"))
        os.makedirs(build_dir, exist_ok=True)

        self.output.emit(
            f"=== Building {p.name}  [{avr_mcu}  {clock // 1_000_000} MHz] ===\n"
            f"    Build dir: {build_dir}\n"
        )

        source_files = list(p.source_files)   # copy so we don't mutate the project

        # Append extra sources declared in build.extraSources (e.g. module .cpp files).
        # Paths are relative to the project directory; absolute paths are accepted
        # too and kept as-is (not relativized: os.path.relpath raises ValueError on
        # Windows when extra and p.path are on different drives, and the compile
        # loop below already resolves an absolute src_rel correctly via os.path.join's
        # "absolute component discards everything before it" behavior).
        for extra in p.build.extra_sources:
            rel = os.path.normpath(extra) if os.path.isabs(extra) else extra
            if rel not in source_files:
                source_files.append(rel)

        if not source_files:
            # No explicit list — scan the project directory for C/C++ files
            source_files = _discover_sources(p.path)
            if source_files:
                self.output.emit(
                    f"  (auto-discovered {len(source_files)} source file(s))\n"
                )
            else:
                self.output.emit(
                    "[ERROR] No C/C++ source files found in project directory.\n"
                    "Create or add .c / .cpp files to your project folder.\n"
                )
                return False

        warn_flags = {
            "all":   ["-Wall"],
            "extra": ["-Wall", "-Wextra"],
        }.get(p.build.warnings, [])

        # Dead-code elimination: keeps firmware size small when libraries are
        # included; safe on bare-metal AVR where no code runs without explicit call.
        gc_flags = ["-ffunction-sections", "-fdata-sections"]

        # Resolve explicit includeDirs from the project file
        inc_flags: list[str] = [f"-I{_SDK_INCLUDE}", f"-I{_SDK_INCLUDE_MIKRO}"]
        for inc in p.build.include_dirs:
            if os.path.isabs(inc):
                inc_flags.append(f"-I{inc}")
            else:
                inc_flags.append(f"-I{os.path.normpath(os.path.join(p.path, inc))}")

        # ── Library / compat resolution ────────────────────────────────────────
        proj_src_abs = [
            os.path.normpath(os.path.join(p.path, f))
            for f in source_files
        ]
        compat_incs, compat_srcs = self._resolve_compat(proj_src_abs)
        lib_incs,    lib_srcs    = self._resolve_libraries(proj_src_abs, p.path)

        if compat_incs or lib_incs:
            self.output.emit(
                f"  Libraries: {len(lib_srcs)} source file(s) from "
                f"{len(lib_incs)} library dir(s)\n"
            )

        inc_flags += [f"-I{d}" for d in compat_incs + lib_incs]

        # ── Compile each project source file ──────────────────────────────────
        obj_files: list[str] = []

        for src_rel in source_files:
            src_abs = os.path.normpath(os.path.join(p.path, src_rel))
            if not os.path.isfile(src_abs):
                self.output.emit(f"[ERROR] Source file not found:\n  {src_abs}\n")
                return False

            obj_name = os.path.splitext(os.path.basename(src_abs))[0] + ".o"
            obj_abs  = os.path.normpath(os.path.join(build_dir, obj_name))

            compiler = _tool("avr-g++") if src_abs.endswith((".cpp", ".cc", ".cxx")) else _tool("avr-gcc")

            cmd = [
                compiler,
                f"-mmcu={avr_mcu}",
                f"-DF_CPU={clock}UL",
                opt,
                f"-std={std}",
                *warn_flags,
                *gc_flags,
                *inc_flags,
                "-c", src_abs,
                "-o", obj_abs,
            ]

            self.output.emit(f"\n  Compiling  {src_rel}\n")
            self._echo(cmd)
            if not self._run(cmd):
                return False

            if not os.path.isfile(obj_abs):
                self.output.emit(
                    f"[ERROR] Compiler ran but produced no output:\n  {obj_abs}\n"
                    "Check errors above.\n"
                )
                return False

            obj_files.append(obj_abs)

        # ── Compile compat + library sources ──────────────────────────────────
        extra_srcs = compat_srcs + lib_srcs
        if extra_srcs:
            self.output.emit(f"\n  Compiling {len(extra_srcs)} library/compat file(s)…\n")

        for idx, src_abs in enumerate(extra_srcs):
            if not os.path.isfile(src_abs):
                continue
            stem     = os.path.splitext(os.path.basename(src_abs))[0]
            obj_name = f"_lib_{idx:03d}_{stem}.o"
            obj_abs  = os.path.normpath(os.path.join(build_dir, obj_name))
            compiler = _tool("avr-g++") if src_abs.endswith((".cpp", ".cc", ".cxx")) else _tool("avr-gcc")

            cmd = [
                compiler,
                f"-mmcu={avr_mcu}",
                f"-DF_CPU={clock}UL",
                opt,
                f"-std=c++17",      # compat layer uses C++17
                *gc_flags,
                *inc_flags,
                "-c", src_abs,
                "-o", obj_abs,
            ]
            self.output.emit(f"    {os.path.basename(src_abs)}\n")
            if not self._run(cmd):
                return False
            if os.path.isfile(obj_abs):
                obj_files.append(obj_abs)

        # ── Link ──────────────────────────────────────────────────────────────
        elf = os.path.normpath(os.path.join(build_dir, f"{p.name}.elf"))
        cmd = [
            _tool("avr-gcc"), f"-mmcu={avr_mcu}", opt,
            "-Wl,--gc-sections",    # discard unused sections from libraries
            *obj_files, "-o", elf,
        ]
        self.output.emit("\n  Linking…\n")
        self._echo(cmd)
        if not self._run(cmd):
            return False

        # ── ELF → Intel HEX ──────────────────────────────────────────────────
        hex_file = os.path.normpath(os.path.join(build_dir, f"{p.name}.hex"))
        cmd = [_tool("avr-objcopy"), "-O", "ihex", "-R", ".eeprom", elf, hex_file]
        self.output.emit("\n  Generating HEX…\n")
        if not self._run(cmd):
            return False

        # ── Size report ───────────────────────────────────────────────────────
        self.output.emit("\n")
        self._run([_tool("avr-size"), "--format=avr", f"--mcu={avr_mcu}", elf])

        self.output.emit(f"\n=== Build successful ===\n")
        return True

    # ── Clean ─────────────────────────────────────────────────────────────────

    def _clean(self) -> bool:
        build_dir = os.path.normpath(os.path.join(self.project.path, "build"))
        if os.path.exists(build_dir):
            shutil.rmtree(build_dir)
            self.output.emit(f"Cleaned: {build_dir}\n")
        else:
            self.output.emit("Nothing to clean.\n")
        return True

    # ── Flash ─────────────────────────────────────────────────────────────────

    def _flash(self) -> bool:
        p        = self.project
        avr_mcu  = MCU_MAP.get(p.target.mcu, p.target.mcu.lower())
        hex_file = os.path.normpath(
            os.path.join(p.path, "build", f"{p.name}.hex")
        )

        if not os.path.isfile(hex_file):
            self.output.emit(
                f"[ERROR] HEX file not found:\n  {hex_file}\n"
                "Run Build (F7) first.\n"
            )
            return False

        programmer = p.programmer.type.lower()
        port       = p.programmer.port.strip()
        baud       = p.programmer.baud_rate

        cmd = [*_avrdude_cmd(), f"-p{avr_mcu}", f"-c{programmer}"]
        if port:
            cmd += [f"-P{port}"]
        if baud:
            cmd += [f"-b{baud}"]
        cmd += ["-U", f"flash:w:{hex_file}:i"]

        self.output.emit(f"=== Flashing {p.name} via {programmer.upper()} ===\n")
        self._echo(cmd)
        return self._run(cmd)

    # ── Library resolution ────────────────────────────────────────────────────

    def _resolve_compat(
        self, src_abs_list: list[str]
    ) -> tuple[list[str], list[str]]:
        """Return (inc_dirs, src_abs_paths) for the Arduino compat shim.
        Only activates when a project source actually includes a compat header."""
        needed_cpps: set[str] = set()
        for src in src_abs_list:
            for header in _scan_includes(src):
                for cpp in _COMPAT_TRIGGERS.get(header, []):
                    needed_cpps.add(cpp)

        if not needed_cpps:
            return [], []

        srcs = [
            os.path.normpath(os.path.join(_COMPAT_SRC, cpp))
            for cpp in needed_cpps
            if os.path.isfile(os.path.join(_COMPAT_SRC, cpp))
        ]
        return [_COMPAT_INCLUDE], srcs

    def _resolve_libraries(
        self, src_abs_list: list[str], project_path: str
    ) -> tuple[list[str], list[str]]:
        """Return (inc_dirs, src_abs_paths) for all third-party libraries needed
        by the project.  Two scan passes handle one level of transitive deps."""

        search_dirs: list[str] = []
        project_libs = os.path.join(project_path, "libraries")
        if os.path.isdir(project_libs):
            search_dirs.append(project_libs)
        if os.path.isdir(_GLOBAL_LIB_DIR):
            search_dirs.append(_GLOBAL_LIB_DIR)

        if not search_dirs:
            return [], []

        # Start with includes from project sources
        all_includes: set[str] = set()
        for src in src_abs_list:
            all_includes |= _scan_includes(src)

        inc_dirs:  list[str] = []
        src_files: list[str] = []
        seen_libs: set[str]  = set()

        # Two passes: first finds direct deps, second finds transitive deps
        for _pass in range(2):
            new_includes: set[str] = set()

            for search_base in search_dirs:
                try:
                    entries = sorted(os.scandir(search_base), key=lambda e: e.name.lower())
                except OSError:
                    continue

                for entry in entries:
                    if not entry.is_dir() or entry.name in seen_libs:
                        continue

                    src_sub = os.path.join(entry.path, "src")
                    inc_root = src_sub if os.path.isdir(src_sub) else entry.path

                    try:
                        lib_headers = {
                            f.name
                            for f in os.scandir(inc_root)
                            if f.name.endswith((".h", ".hpp"))
                        }
                    except OSError:
                        continue

                    if not (all_includes & lib_headers):
                        continue

                    seen_libs.add(entry.name)
                    inc_dirs.append(os.path.normpath(inc_root))
                    self.output.emit(f"  Library:  {entry.name}  ({inc_root})\n")

                    for root, dirs, files in os.walk(inc_root):
                        dirs[:] = [
                            d for d in dirs
                            if d.lower() not in ("examples", "extras", "test", "tests")
                        ]
                        for fname in sorted(files):
                            if fname.endswith((".cpp", ".c", ".cc", ".cxx")):
                                abs_path = os.path.normpath(os.path.join(root, fname))
                                if abs_path not in src_files:
                                    src_files.append(abs_path)

                    # Collect headers from this library for the next pass
                    for h in lib_headers:
                        new_includes |= _scan_includes(os.path.join(inc_root, h))

            all_includes |= new_includes
            if not new_includes:
                break   # no new transitive deps found — done early

        return inc_dirs, src_files

    # ── Helpers ───────────────────────────────────────────────────────────────

    @staticmethod
    def _discover_sources(project_path: str) -> list[str]:
        return _discover_sources(project_path)

    def _echo(self, cmd: list[str]) -> None:
        """Print the command being run so the user can see it."""
        self.output.emit("  $ " + " ".join(f'"{a}"' if " " in a else a for a in cmd) + "\n")

    def _run(self, cmd: list[str]) -> bool:
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                cwd=self.project.path,
                creationflags=_NO_WINDOW,
            )
            if result.stdout.strip():
                self.output.emit(result.stdout)
            if result.stderr.strip():
                self.output.emit(result.stderr)
            return result.returncode == 0
        except FileNotFoundError:
            self.output.emit(
                f"[ERROR] Not found: {cmd[0]}\n"
                "The bundled avr-gcc/avrdude toolchain is missing and no matching\n"
                "tool was found on your system PATH either.\n"
                "Download: https://www.microchip.com/avr-support/avr-and-arm-toolchains-c-compilers\n"
            )
            return False


# ── Module helpers ────────────────────────────────────────────────────────────

def _discover_sources(project_path: str) -> list[str]:
    """Walk the project directory and return relative paths of all C/C++ files,
    skipping the build output directory and hidden folders."""
    found: list[str] = []
    for root, dirs, files in os.walk(project_path):
        dirs[:] = [d for d in dirs
                   if d != "build" and not d.startswith(".")]
        for fname in sorted(files):
            if fname.endswith((".cpp", ".c", ".cc", ".cxx")):
                abs_path = os.path.join(root, fname)
                rel = os.path.relpath(abs_path, project_path)
                found.append(rel)
    return found
