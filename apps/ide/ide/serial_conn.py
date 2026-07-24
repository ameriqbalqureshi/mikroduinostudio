"""Serial monitor — wraps pyserial in a background QThread."""

from __future__ import annotations

from PyQt6.QtCore import QThread, pyqtSignal

try:
    import serial
    import serial.tools.list_ports
    SERIAL_OK = True
except ImportError:
    SERIAL_OK = False


def list_ports() -> list[str]:
    """Return all available serial ports sorted numerically (COM1 < COM2 < COM10)."""
    if not SERIAL_OK:
        return []
    import re
    ports = [p.device for p in serial.tools.list_ports.comports()]

    def _key(name: str):
        m = re.match(r'([A-Za-z/]+)(\d+)', name)
        return (m.group(1), int(m.group(2))) if m else (name, 0)

    return sorted(ports, key=_key)


class SerialThread(QThread):
    data_received = pyqtSignal(str)
    status        = pyqtSignal(bool, str)   # (connected, port)

    def __init__(self) -> None:
        super().__init__()
        self._port: serial.Serial | None = None
        self._running = False

    # ── Public API (call from main thread) ────────────────────────────────────

    def open(self, port: str, baud: int) -> bool:
        if not SERIAL_OK:
            self.status.emit(False, "pyserial not installed")
            return False
        try:
            self._port = serial.Serial(port, baud, timeout=0.1)
            self._running = True
            self.start()
            self.status.emit(True, port)
            return True
        except Exception as exc:
            self.status.emit(False, str(exc))
            return False

    def close(self) -> None:
        self._running = False
        if self._port and self._port.is_open:
            self._port.close()
        self.status.emit(False, "")

    def send(self, text: str) -> None:
        if self._port and self._port.is_open:
            self._port.write(text.encode("utf-8"))

    # ── QThread.run ───────────────────────────────────────────────────────────

    def run(self) -> None:
        while self._running and self._port and self._port.is_open:
            try:
                raw = self._port.read(256)
                if raw:
                    self.data_received.emit(raw.decode("utf-8", errors="replace"))
            except Exception:
                self._running = False
        self.status.emit(False, "")
