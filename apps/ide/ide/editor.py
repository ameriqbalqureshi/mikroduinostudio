"""CodeEditor: QPlainTextEdit with a line-number gutter and current-line highlight."""

from __future__ import annotations

from __future__ import annotations
from typing import TYPE_CHECKING

from PyQt6.QtCore import QRect, QRectF, QSize, Qt
from PyQt6.QtGui import QColor, QKeyEvent, QPainter, QPalette, QTextFormat
from PyQt6.QtWidgets import QPlainTextEdit, QTextEdit, QWidget

if TYPE_CHECKING:
    from .autocomplete import CppCompleter

_GUTTER_BG      = QColor("#181825")  # slightly darker than editor
_GUTTER_FG      = QColor("#45475a")  # dim line numbers
_GUTTER_FG_CUR  = QColor("#cdd6f4")  # current-line number (bright)
_GUTTER_CUR_BG  = QColor("#313244")  # pill/rect behind current line number
_GUTTER_BORDER  = QColor("#313244")  # right-edge separator (VSCode style)
_CURLINE_BG     = QColor("#26263a")  # subtle current-line tint in editor
_ACCENT_BAR     = QColor("#89b4fa")  # blue left-edge bar on current gutter row


class _LineNumberArea(QWidget):
    """Gutter widget — paints line numbers by delegating to CodeEditor."""

    def __init__(self, editor: "CodeEditor") -> None:
        super().__init__(editor)
        self._editor = editor

    def sizeHint(self) -> QSize:
        return QSize(self._editor.line_number_area_width(), 0)

    def paintEvent(self, event) -> None:  # type: ignore[override]
        self._editor._paint_line_numbers(event)


class CodeEditor(QPlainTextEdit):
    """QPlainTextEdit with a line-number gutter on the left side."""

    def __init__(self, parent=None) -> None:
        super().__init__(parent)

        # Set colors via palette so QSyntaxHighlighter is not overridden.
        # (Qt6: stylesheet `color:` on QPlainTextEdit injects into defaultCharFormat
        # and silently overwrites syntax-highlighter token colors after each rehighlight.)
        pal = self.palette()
        pal.setColor(QPalette.ColorRole.Base,            QColor("#1e1e2e"))
        pal.setColor(QPalette.ColorRole.Text,            QColor("#cdd6f4"))
        pal.setColor(QPalette.ColorRole.Highlight,       QColor("#3d3f52"))
        pal.setColor(QPalette.ColorRole.HighlightedText, QColor("#cdd6f4"))
        self.setPalette(pal)

        self._completer: CppCompleter | None = None

        self._gutter = _LineNumberArea(self)

        self.blockCountChanged.connect(self._update_gutter_width)
        self.updateRequest.connect(self._on_update_request)
        self.cursorPositionChanged.connect(self._highlight_current_line)

        self._update_gutter_width(0)
        self._highlight_current_line()

    # ── Public ────────────────────────────────────────────────────────────────

    def line_number_area_width(self) -> int:
        """Width needed to display the highest line number plus padding."""
        digits = max(3, len(str(self.blockCount())))
        return 10 + self.fontMetrics().horizontalAdvance("9") * digits + 8

    # ── Gutter geometry ───────────────────────────────────────────────────────

    def _update_gutter_width(self, _: int) -> None:
        self.setViewportMargins(self.line_number_area_width(), 0, 0, 0)

    def _on_update_request(self, rect: QRect, dy: int) -> None:
        if dy:
            self._gutter.scroll(0, dy)
        else:
            self._gutter.update(0, rect.y(), self._gutter.width(), rect.height())
        if rect.contains(self.viewport().rect()):
            self._update_gutter_width(0)

    def resizeEvent(self, event) -> None:  # type: ignore[override]
        super().resizeEvent(event)
        cr = self.contentsRect()
        self._gutter.setGeometry(
            QRect(cr.left(), cr.top(), self.line_number_area_width(), cr.height())
        )

    # ── Completion ────────────────────────────────────────────────────────────

    def keyPressEvent(self, event: QKeyEvent) -> None:
        if self._completer and self._completer.key_press(event):
            return                          # consumed by popup navigation

        if event.key() in (Qt.Key.Key_Return, Qt.Key.Key_Enter):
            cursor = self.textCursor()
            line = cursor.block().text()
            indent = len(line) - len(line.lstrip())
            super().keyPressEvent(event)
            if indent:
                self.textCursor().insertText(line[:indent])
            if self._completer:
                self._completer.key_release(event)
            return

        if event.text() == "{":
            cursor = self.textCursor()
            cursor.insertText("{}")
            cursor.movePosition(cursor.MoveOperation.Left)
            self.setTextCursor(cursor)
            if self._completer:
                self._completer.key_release(event)
            return

        if event.text() == "}" :
            cursor = self.textCursor()
            doc = self.document()
            pos = cursor.position()
            if pos < doc.characterCount() and doc.characterAt(pos) == "}":
                cursor.movePosition(cursor.MoveOperation.Right)
                self.setTextCursor(cursor)
                if self._completer:
                    self._completer.key_release(event)
                return

        super().keyPressEvent(event)
        if self._completer:
            self._completer.key_release(event)

    def _apply_completion(self, text: str) -> None:
        if self._completer:
            self._completer._apply(text)

    # ── Painting ──────────────────────────────────────────────────────────────

    def _paint_line_numbers(self, event) -> None:
        painter = QPainter(self._gutter)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.fillRect(event.rect(), _GUTTER_BG)

        gutter_w  = self._gutter.width()
        line_h    = self.fontMetrics().height()
        cur_block = self.textCursor().blockNumber()
        block     = self.firstVisibleBlock()
        num       = block.blockNumber()
        top       = round(
            self.blockBoundingGeometry(block).translated(self.contentOffset()).top()
        )
        bottom    = top + round(self.blockBoundingRect(block).height())

        while block.isValid() and top <= event.rect().bottom():
            if block.isVisible() and bottom >= event.rect().top():
                is_cur = (num == cur_block)

                if is_cur:
                    # Rounded-rect highlight behind the current line number
                    pill_margin = 3
                    pill = QRectF(
                        pill_margin, top + 1,
                        gutter_w - pill_margin * 2 - 2, line_h - 2,
                    )
                    painter.setBrush(_GUTTER_CUR_BG)
                    painter.setPen(Qt.PenStyle.NoPen)
                    painter.drawRoundedRect(pill, 3, 3)

                    # Thin accent bar on the left edge
                    bar_w = 3
                    painter.setBrush(_ACCENT_BAR)
                    bar = QRectF(0, top + 2, bar_w, line_h - 4)
                    painter.drawRoundedRect(bar, 1, 1)

                painter.setPen(_GUTTER_FG_CUR if is_cur else _GUTTER_FG)
                painter.setFont(self.font())
                painter.drawText(
                    0, top,
                    gutter_w - 8, line_h,
                    Qt.AlignmentFlag.AlignRight,
                    str(num + 1),
                )

            block  = block.next()
            top    = bottom
            bottom = top + round(self.blockBoundingRect(block).height())
            num   += 1

        # Right-edge separator line (VSCode style)
        painter.setPen(_GUTTER_BORDER)
        painter.drawLine(gutter_w - 1, event.rect().top(), gutter_w - 1, event.rect().bottom())

    # ── Current-line highlight ────────────────────────────────────────────────

    def _highlight_current_line(self) -> None:
        extra: list[QTextEdit.ExtraSelection] = []
        if not self.isReadOnly():
            sel = QTextEdit.ExtraSelection()
            sel.format.setBackground(_CURLINE_BG)
            sel.format.setProperty(QTextFormat.Property.FullWidthSelection, True)
            sel.cursor = self.textCursor()
            sel.cursor.clearSelection()
            extra.append(sel)
        self.setExtraSelections(extra)
