import type { BuildDiagnostic, DiagnosticSeverity } from '@mikroduino/shared';

// avr-gcc error format:  file.cpp:42:10: error: message
// avr-gcc warning format: file.cpp:42:10: warning: message
// avr-size format:        Program:   1234 bytes (3.8% Full)
const GCC_DIAG_RE   = /^(.+?):(\d+):(\d+):\s+(error|warning|note|info):\s+(.+)$/;
const GCC_SIMPLE_RE = /^(.+?):(\d+):\s+(error|warning|note|info):\s+(.+)$/;

export class BuildParser {
  parseLine(line: string): BuildDiagnostic | null {
    let m = GCC_DIAG_RE.exec(line);
    if (m) {
      return {
        severity: m[4] as DiagnosticSeverity,
        file:     m[1],
        line:     parseInt(m[2], 10),
        column:   parseInt(m[3], 10),
        message:  m[5],
        raw:      line,
      };
    }

    m = GCC_SIMPLE_RE.exec(line);
    if (m) {
      return {
        severity: m[3] as DiagnosticSeverity,
        file:     m[1],
        line:     parseInt(m[2], 10),
        column:   0,
        message:  m[4],
        raw:      line,
      };
    }

    return null;
  }

  parseAll(output: string): BuildDiagnostic[] {
    const diagnostics: BuildDiagnostic[] = [];
    for (const line of output.split('\n')) {
      const d = this.parseLine(line.trim());
      if (d) diagnostics.push(d);
    }
    return diagnostics;
  }

  parseMemoryUsage(avrSizeOutput: string): import('@mikroduino/shared').MemoryUsage | null {
    // avr-size --mcu=... --format=avr output:
    // Device: atmega328p
    // Program:    1234 bytes (3.8% Full)
    // Data:        456 bytes (22.3% Full)
    // EEPROM:        0 bytes (0.0% Full)

    const progMatch = /Program:\s+(\d+)\s+bytes.*\((\d+\.\d+)%/.exec(avrSizeOutput);
    const dataMatch = /Data:\s+(\d+)\s+bytes.*\((\d+\.\d+)%/.exec(avrSizeOutput);
    const eepMatch  = /EEPROM:\s+(\d+)\s+bytes.*\((\d+\.\d+)%/.exec(avrSizeOutput);

    if (!progMatch || !dataMatch) return null;

    const flashUsed = parseInt(progMatch[1], 10);
    const flashPct  = parseFloat(progMatch[2]);
    const flashTotal = Math.round(flashUsed / (flashPct / 100));

    const ramUsed = parseInt(dataMatch[1], 10);
    const ramPct  = parseFloat(dataMatch[2]);
    const ramTotal = Math.round(ramUsed / (ramPct / 100));

    const eepromUsed  = eepMatch  ? parseInt(eepMatch[1],  10) : 0;
    const eepromPct   = eepMatch  ? parseFloat(eepMatch[2]) : 0;
    const eepromTotal = eepromPct > 0 ? Math.round(eepromUsed / (eepromPct / 100)) : 0;

    return {
      flash:  { used: flashUsed,  total: flashTotal  },
      ram:    { used: ramUsed,    total: ramTotal    },
      eeprom: { used: eepromUsed, total: eepromTotal },
    };
  }

  hasErrors(diagnostics: BuildDiagnostic[]): boolean {
    return diagnostics.some(d => d.severity === 'error');
  }
}
