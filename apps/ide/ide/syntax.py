from PyQt6.QtCore import QRegularExpression
from PyQt6.QtGui import (
    QColor, QFont, QSyntaxHighlighter, QTextCharFormat
)


def _fmt(color: str, bold: bool = False, italic: bool = False) -> QTextCharFormat:
    f = QTextCharFormat()
    f.setForeground(QColor(color))
    if bold:
        f.setFontWeight(QFont.Weight.Bold)
    if italic:
        f.setFontItalic(True)
    return f


# VS Code dark+ colours — rule order matters: later rules WIN (setFormat overwrites)
_KW        = _fmt("#569cd6", bold=True)   # keywords: while, for, if, return …
_TYPE      = _fmt("#4ec9b0", bold=True)   # types: int, uint8_t, bool …
_PP        = _fmt("#c586c0")              # preprocessor keyword: #include, #define
_PREP      = _fmt("#9cdcfe")              # uppercase macro identifiers: DDRB, F_CPU
_INC       = _fmt("#ce9178")              # include paths: <avr/io.h> and "file.h"
_STR       = _fmt("#ce9178")             # string / char literals
_NUM       = _fmt("#b5cea8")              # numeric literals
_FN        = _fmt("#dcdcaa")              # function calls: main(), setup(), loop()
_CMT       = _fmt("#6a9955", italic=True) # comments
_BR_CURLY  = _fmt("#ffd700")              # { }  gold
_BR_SQUARE = _fmt("#da70d6")              # [ ]  orchid
_BR_PAREN  = _fmt("#9cdcfe")              # ( )  sky blue

_KEYWORDS = [
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
    # avr-libc / interrupt helpers
    "ISR", "SIGNAL", "sei", "cli",
]

_TYPES = [
    "bool", "byte", "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "int8_t", "int16_t", "int32_t", "int64_t", "size_t", "ptrdiff_t",
    "string", "wstring", "vector", "map", "set", "list", "array",
    "optional", "variant", "pair", "tuple",
]


class CppHighlighter(QSyntaxHighlighter):
    """C/C++ syntax highlighter (regex-based, VS Code dark+ palette)."""

    def __init__(self, document):
        super().__init__(document)

        self._rules: list[tuple[QRegularExpression, QTextCharFormat]] = []

        # ── Priority 1 (lowest): function calls — overridden by keywords/types ──
        self._rules.append((QRegularExpression(r"\b([A-Za-z_]\w*)(?=\s*\()"), _FN))

        # ── Priority 2: numeric literals ─────────────────────────────────────────
        self._rules.append((QRegularExpression(r"\b0x[0-9A-Fa-f]+[uUlL]*\b"), _NUM))
        self._rules.append((QRegularExpression(r"\b0b[01]+[uUlL]*\b"),         _NUM))
        self._rules.append((QRegularExpression(r"\b\d+(\.\d+)?[fFuUlL]*\b"),   _NUM))

        # ── Priority 3: bracket pairs ─────────────────────────────────────────────
        self._rules.append((QRegularExpression(r"[{}]"),   _BR_CURLY))
        self._rules.append((QRegularExpression(r"[\[\]]"), _BR_SQUARE))
        self._rules.append((QRegularExpression(r"[()]"),   _BR_PAREN))

        # ── Priority 4: types (override function-call color on e.g. int()) ───────
        for tp in _TYPES:
            self._rules.append((QRegularExpression(rf"\b{tp}\b"), _TYPE))

        # ── Priority 5: keywords — override function-call color on while(), for() ─
        for kw in _KEYWORDS:
            self._rules.append((QRegularExpression(rf"\b{kw}\b"), _KW))

        # ── Priority 6: uppercase macro identifiers (DDRB, F_CPU, LED_PIN …) ─────
        self._rules.append((QRegularExpression(r"\b[A-Z][A-Z0-9_]{2,}\b"), _PREP))

        # ── Priority 7: include paths — <avr/io.h> style angle-bracket paths ─────
        self._rules.append((QRegularExpression(r"<[^>\n\"]+\.h>"), _INC))

        # ── Priority 8: preprocessor directive keyword (#include, #define …) ─────
        self._rules.append((QRegularExpression(r"#\s*[a-z_]\w*"), _PP))

        # ── Priority 9: string / char literals (override everything above) ───────
        self._rules.append((QRegularExpression(r'"[^"\\]*(\\.[^"\\]*)*"'), _STR))
        self._rules.append((QRegularExpression(r"'[^'\\]*(\\.[^'\\]*)*'"), _STR))

        # ── Priority 10: single-line comments ─────────────────────────────────────
        self._rules.append((QRegularExpression(r"//[^\n]*"), _CMT))

        # Block-comment delimiters (state machine in highlightBlock)
        self._block_start = QRegularExpression(r"/\*")
        self._block_end   = QRegularExpression(r"\*/")

    def highlightBlock(self, text: str) -> None:
        for pattern, fmt in self._rules:
            it = pattern.globalMatch(text)
            while it.hasNext():
                m = it.next()
                self.setFormat(m.capturedStart(), m.capturedLength(), fmt)

        # ── Block comments ────────────────────────────────────────────────────
        self.setCurrentBlockState(0)
        start = 0

        if self.previousBlockState() != 1:
            m = self._block_start.match(text)
            start = m.capturedStart() if m.hasMatch() else -1

        while start >= 0:
            m = self._block_end.match(text, start)
            if m.hasMatch():
                length = m.capturedStart() + m.capturedLength() - start
            else:
                self.setCurrentBlockState(1)
                length = len(text) - start
            self.setFormat(start, length, _CMT)
            m2 = self._block_start.match(text, start + length)
            start = m2.capturedStart() if m2.hasMatch() else -1
