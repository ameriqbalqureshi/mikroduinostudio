"""
CppCompleter — lightweight IntelliSense for CodeEditor.

Trigger rules
─────────────
  Typing ≥ 2 chars   → keyword / type / stdlib / document-identifier completion
  '.' after a name   → member completion (type is inferred from declarations)
  '->' after a name  → same as '.'
  Ctrl+Space         → force-show global completion at any time

Popup navigation
────────────────
  ↑ / ↓            → move selection
  Tab / Enter       → accept selected item
  Escape            → dismiss
  Mouse click       → accept
"""

from __future__ import annotations

import re
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QColor, QKeyEvent
from PyQt6.QtWidgets import QApplication, QListWidget, QListWidgetItem

# ── Completion database ────────────────────────────────────────────────────────

_KEYWORDS: list[str] = [
    "auto", "break", "case", "catch", "char", "class", "const",
    "constexpr", "continue", "default", "delete", "do", "double",
    "else", "enum", "explicit", "extern", "false", "final", "float",
    "for", "friend", "goto", "if", "inline", "int", "long",
    "mutable", "namespace", "new", "noexcept", "nullptr", "operator",
    "override", "private", "protected", "public", "register", "return",
    "short", "signed", "sizeof", "static", "static_assert", "struct",
    "switch", "template", "this", "throw", "true", "try", "typedef",
    "typename", "union", "unsigned", "using", "virtual", "void",
    "volatile", "while",
    "ISR", "SIGNAL", "sei", "cli",
]

_TYPES: list[str] = [
    "bool", "byte",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "int8_t",  "int16_t",  "int32_t",  "int64_t",
    "size_t", "ptrdiff_t", "FILE",
]

_FUNCTIONS: list[str] = [
    # avr-libc
    "_delay_ms()", "_delay_us()", "_BV()",
    "bit_is_set()", "bit_is_clear()",
    "loop_until_bit_is_set()", "loop_until_bit_is_clear()",
    # stdio
    "printf()", "sprintf()", "snprintf()", "puts()", "putchar()", "getchar()",
    "scanf()", "sscanf()",
    # string.h
    "strlen()", "strcpy()", "strncpy()", "strcat()", "strcmp()", "strncmp()",
    "strchr()", "strrchr()", "strstr()",
    "memcpy()", "memset()", "memcmp()", "memmove()",
    # stdlib.h
    "malloc()", "free()", "calloc()", "realloc()",
    "atoi()", "atof()", "atol()",
    "abs()", "labs()", "rand()", "srand()",
    # math.h
    "sqrt()", "pow()", "sin()", "cos()", "fabs()", "floor()", "ceil()",
]

# MikroDuino SDK type → member methods
_MEMBERS: dict[str, list[str]] = {
    "GPIO": [
        "init()", "set()", "clear()", "toggle()", "read()",
        "write()", "setMode()", "setDirection()", "setPullup()",
    ],
    "USART": [
        "init()", "enable()", "disable()",
        "send()", "receive()", "sendByte()", "receiveByte()",
        "print()", "println()", "available()", "flush()",
        "setBaudRate()",
    ],
    "SPI": [
        "init()", "enable()", "disable()",
        "transfer()", "select()", "deselect()",
        "setMode()", "setBitOrder()", "setClockDiv()",
    ],
    "I2C": [
        "init()", "start()", "stop()",
        "write()", "read()", "available()",
        "beginTransmission()", "endTransmission()", "requestFrom()",
    ],
    "ADC": [
        "init()", "enable()", "disable()",
        "read()", "readVoltage()",
        "setChannel()", "setReference()", "setPrescaler()",
        "startConversion()", "waitForResult()",
    ],
    "Timer": [
        "init()", "start()", "stop()", "reset()",
        "setMode()", "setPrescaler()", "setCompare()", "getCount()",
        "enableInterrupt()", "disableInterrupt()",
    ],
    "PWM": [
        "init()", "enable()", "disable()",
        "setDuty()", "setFrequency()", "setMode()", "setPrescaler()",
    ],
    "Interrupt": [
        "attach()", "detach()", "enable()", "disable()",
        "setMode()", "setCallback()",
    ],
    "Serial": [
        "begin()", "end()",
        "print()", "println()", "write()",
        "read()", "available()", "flush()",
        "parseInt()", "parseFloat()", "readString()",
    ],
    # Standard containers
    "string": [
        "length()", "size()", "empty()", "clear()",
        "append()", "c_str()", "substr()",
        "find()", "rfind()", "replace()",
        "at()", "front()", "back()", "push_back()", "pop_back()",
    ],
    "vector": [
        "push_back()", "pop_back()", "size()", "empty()", "clear()",
        "front()", "back()", "begin()", "end()", "at()",
        "resize()", "reserve()", "capacity()", "insert()", "erase()",
    ],
    "map": [
        "insert()", "erase()", "find()", "count()", "contains()",
        "size()", "empty()", "clear()",
        "begin()", "end()", "at()",
    ],
}

# Completion item kinds (used for colour coding in the popup)
_K_KEYWORD  = "keyword"
_K_TYPE     = "type"
_K_FUNCTION = "function"
_K_MEMBER   = "member"
_K_IDENT    = "identifier"

_KIND_COLOR: dict[str, str] = {
    _K_KEYWORD:  "#60b8ff",   # blue
    _K_TYPE:     "#3fffd2",   # teal
    _K_FUNCTION: "#ffe580",   # yellow
    _K_MEMBER:   "#ffe580",   # yellow
    _K_IDENT:    "#c0ecff",   # light blue
}

# Static global item list built once at import time
_GLOBAL_ITEMS: list[tuple[str, str]] = (
    [(k, _K_KEYWORD)  for k in _KEYWORDS]  +
    [(t, _K_TYPE)     for t in _TYPES]     +
    [(f, _K_FUNCTION) for f in _FUNCTIONS]
)


# ── Popup widget ───────────────────────────────────────────────────────────────

class CompletionPopup(QListWidget):
    """Floating, focus-stealing-free suggestion list."""

    _MAX_VISIBLE = 10

    def __init__(self, editor) -> None:
        super().__init__()
        self._editor = editor

        self.setWindowFlags(
            Qt.WindowType.Tool |
            Qt.WindowType.FramelessWindowHint |
            Qt.WindowType.WindowStaysOnTopHint
        )
        self.setAttribute(Qt.WidgetAttribute.WA_ShowWithoutActivating)
        self.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.setFont(editor.font())
        self.setMouseTracking(True)
        self.setStyleSheet("""
            QListWidget {
                background-color: #181825;
                border: 1px solid #7c5af6;
                border-radius: 4px;
                padding: 2px 0;
                outline: none;
            }
            QListWidget::item {
                padding: 3px 12px;
                border-radius: 0px;
                color: #cdd6f4;
            }
            QListWidget::item:selected {
                background-color: #3d3068;
                border-left: 2px solid #7c5af6;
                padding-left: 10px;
            }
            QListWidget::item:hover:!selected {
                background-color: #282840;
            }
            QScrollBar:vertical {
                width: 6px;
                background: transparent;
            }
            QScrollBar::handle:vertical {
                background: #45475a;
                border-radius: 3px;
                min-height: 20px;
            }
            QScrollBar::add-line:vertical,
            QScrollBar::sub-line:vertical { height: 0; }
        """)
        self.itemClicked.connect(self._on_item_clicked)

    # ── Public API ─────────────────────────────────────────────────────────────

    def populate(self, items: list[tuple[str, str]]) -> None:
        self.clear()
        for text, kind in items:
            li = QListWidgetItem(text)
            li.setForeground(QColor(_KIND_COLOR.get(kind, "#cdd6f4")))
            self.addItem(li)

    def show_at_cursor(self) -> None:
        if self.count() == 0:
            self.hide()
            return
        self.setCurrentRow(0)
        self._resize_to_content()
        self._reposition()
        self.show()
        self.raise_()

    def step(self, delta: int) -> None:
        row = max(0, min(self.count() - 1, self.currentRow() + delta))
        self.setCurrentRow(row)

    def accept_current(self) -> str | None:
        item = self.currentItem()
        self.hide()
        return item.text() if item else None

    # ── Internal ───────────────────────────────────────────────────────────────

    def _resize_to_content(self) -> None:
        fm    = self.fontMetrics()
        rows  = min(self.count(), self._MAX_VISIBLE)
        row_h = fm.height() + 8
        width = max(
            (fm.horizontalAdvance(self.item(i).text())
             for i in range(min(self.count(), 30))),
            default=160,
        ) + 48
        self.setFixedSize(max(200, width), rows * row_h + 6)

    def _reposition(self) -> None:
        cr  = self._editor.cursorRect()
        vp  = self._editor.viewport()
        pos = vp.mapToGlobal(cr.bottomLeft())
        pos.setX(pos.x() - 4)
        pos.setY(pos.y() + 2)

        # Flip above cursor if too close to screen bottom
        screen = QApplication.primaryScreen().availableGeometry()
        if pos.y() + self.height() > screen.bottom() - 20:
            pos.setY(vp.mapToGlobal(cr.topLeft()).y() - self.height() - 2)

        self.move(pos)

    def _on_item_clicked(self, item: QListWidgetItem) -> None:
        self.hide()
        self._editor._apply_completion(item.text())


# ── Completer logic ────────────────────────────────────────────────────────────

class CppCompleter:
    """
    Attaches to a CodeEditor instance.  CodeEditor must call
    key_press() before forwarding the event and key_release() after.
    """

    def __init__(self, editor) -> None:
        self._editor      = editor
        self._popup       = CompletionPopup(editor)
        self._member_mode = False
        self._member_pool: list[tuple[str, str]] = []

    # ── Key hooks (called from CodeEditor.keyPressEvent) ──────────────────────

    def key_press(self, event: QKeyEvent) -> bool:
        """Return True to consume the event (don't pass to QPlainTextEdit)."""
        if not self._popup.isVisible():
            return False

        key = event.key()
        if key == Qt.Key.Key_Down:
            self._popup.step(+1); return True
        if key == Qt.Key.Key_Up:
            self._popup.step(-1); return True
        if key in (Qt.Key.Key_Tab, Qt.Key.Key_Return, Qt.Key.Key_Enter):
            text = self._popup.accept_current()
            if text:
                self._apply(text)
            return True
        if key == Qt.Key.Key_Escape:
            self._popup.hide(); return True
        return False

    def key_release(self, event: QKeyEvent) -> None:
        """Called after QPlainTextEdit has processed the keystroke."""
        key = event.key()
        ch  = event.text()
        mod = event.modifiers()

        # Ctrl+Space → force global completion
        if (key == Qt.Key.Key_Space
                and mod & Qt.KeyboardModifier.ControlModifier):
            self._show_global(min_prefix=0)
            return

        # Cursor movement / structural keys → dismiss
        if key in (Qt.Key.Key_Escape, Qt.Key.Key_Return, Qt.Key.Key_Enter,
                   Qt.Key.Key_Tab, Qt.Key.Key_Left, Qt.Key.Key_Right,
                   Qt.Key.Key_Home, Qt.Key.Key_End,
                   Qt.Key.Key_PageUp, Qt.Key.Key_PageDown,
                   Qt.Key.Key_Up, Qt.Key.Key_Down):
            self._popup.hide()
            self._member_mode = False
            return

        # '.' → member completion
        if ch == '.':
            self._show_members()
            return

        # '->' → member completion (triggered on '>')
        if ch == '>':
            line = self._editor.textCursor().block().text()
            col  = self._editor.textCursor().positionInBlock()
            if col >= 2 and line[col - 2] == '-':
                self._show_members(arrow=True)
            return

        # Word character → global or re-filter member pool
        if ch and (ch.isalnum() or ch == '_'):
            if self._member_mode and self._popup.isVisible():
                self._refilter_members()
            else:
                self._show_global()
            return

        # Backspace — update whatever mode is active
        if key == Qt.Key.Key_Backspace:
            if self._member_mode:
                prefix = self._word_before_cursor()
                if not prefix:
                    # Deleted the '.' or '->' → leave member mode
                    self._popup.hide()
                    self._member_mode = False
                else:
                    self._refilter_members()
            else:
                self._show_global()
            return

        # Anything else → close
        self._popup.hide()
        self._member_mode = False

    # ── Trigger helpers ────────────────────────────────────────────────────────

    def _show_members(self, arrow: bool = False) -> None:
        var = self._word_before_accessor(arrow=arrow)
        if not var:
            self._popup.hide()
            return
        typ = self._infer_type(var)
        if not typ or typ not in _MEMBERS:
            self._popup.hide()
            return
        items = [(m, _K_MEMBER) for m in _MEMBERS[typ]]
        self._member_mode = True
        self._member_pool = items
        self._popup.populate(items)
        self._popup.show_at_cursor()

    def _refilter_members(self) -> None:
        prefix   = self._word_before_cursor()
        filtered = [
            (t, k) for t, k in self._member_pool
            if t.lower().startswith(prefix.lower())
        ] if prefix else self._member_pool
        if not filtered:
            self._popup.hide()
            return
        self._popup.populate(filtered)
        self._popup.show_at_cursor()

    def _show_global(self, min_prefix: int = 2) -> None:
        prefix = self._word_before_cursor()
        if len(prefix) < min_prefix:
            self._popup.hide()
            return
        doc_idents = self._document_identifiers()
        all_items  = _GLOBAL_ITEMS + [(i, _K_IDENT) for i in doc_idents]

        seen: set[str] = set()
        result: list[tuple[str, str]] = []
        for text, kind in all_items:
            bare = text.rstrip("()")       # strip parens for matching
            if bare == prefix:             # skip exact match — already typed
                continue
            if bare.lower().startswith(prefix.lower()) and bare not in seen:
                seen.add(bare)
                result.append((text, kind))
            if len(result) >= 60:
                break

        if not result:
            self._popup.hide()
            return
        self._member_mode = False
        self._popup.populate(result)
        self._popup.show_at_cursor()

    # ── Apply completion ───────────────────────────────────────────────────────

    def _apply(self, text: str) -> None:
        editor = self._editor
        cursor = editor.textCursor()

        # Replace whatever prefix the user has already typed
        prefix = self._word_before_cursor()
        if prefix:
            cursor.movePosition(
                cursor.MoveOperation.Left,
                cursor.MoveMode.KeepAnchor,
                len(prefix),
            )

        cursor.insertText(text)

        # Move cursor inside parentheses for function completions
        if text.endswith("()"):
            cursor.movePosition(cursor.MoveOperation.Left)

        editor.setTextCursor(cursor)
        self._popup.hide()
        self._member_mode = False

    # ── Text analysis helpers ──────────────────────────────────────────────────

    def _word_before_cursor(self) -> str:
        """Partial identifier the user is currently typing."""
        cursor = self._editor.textCursor()
        line   = cursor.block().text()
        col    = cursor.positionInBlock()
        start  = col
        while start > 0 and (line[start - 1].isalnum() or line[start - 1] == '_'):
            start -= 1
        return line[start:col]

    def _word_before_accessor(self, arrow: bool = False) -> str | None:
        """
        Return the identifier immediately before '.' or '->'.
        Works after the accessor character has already been inserted.
        """
        cursor = self._editor.textCursor()
        line   = cursor.block().text()
        col    = cursor.positionInBlock()

        if arrow:
            # Cursor is after '>', check for '->'
            if col < 2 or line[col - 2 : col] != "->":
                return None
            end = col - 2
        else:
            if col < 1 or line[col - 1] != '.':
                return None
            end = col - 1

        start = end
        while start > 0 and (line[start - 1].isalnum() or line[start - 1] == '_'):
            start -= 1
        return line[start:end] or None

    def _infer_type(self, var_name: str) -> str | None:
        """
        Naive backward scan for declarations like:
            TypeName var;  /  TypeName<N> var;  /  TypeName var =  /  TypeName *var;
        """
        doc = self._editor.toPlainText()
        pat = re.compile(
            r'\b([A-Za-z_]\w*(?:<[^>]*>)?)\s*\*?\s+' + re.escape(var_name) + r'\b'
        )
        matches = list(pat.finditer(doc))
        if not matches:
            return None
        raw = matches[-1].group(1)       # last declaration wins
        return raw.split('<')[0]         # strip template params: USART<0> → USART

    def _document_identifiers(self) -> list[str]:
        """All multi-char identifiers in the document, minus known keywords/types."""
        text  = self._editor.toPlainText()
        words = set(re.findall(r'\b[A-Za-z_]\w{2,}\b', text))
        words -= set(_KEYWORDS)
        words -= set(_TYPES)
        return sorted(words)
