export type BuildStatus = 'idle' | 'building' | 'success' | 'failed';
export type DiagnosticSeverity = 'error' | 'warning' | 'info' | 'note';

export interface BuildDiagnostic {
  severity: DiagnosticSeverity;
  file: string;
  line: number;
  column: number;
  message: string;
  raw: string;
}

export interface MemoryUsage {
  flash: { used: number; total: number };
  ram: { used: number; total: number };
  eeprom: { used: number; total: number };
}

export interface BuildResult {
  status: BuildStatus;
  startTime: Date;
  endTime: Date;
  durationMs: number;
  diagnostics: BuildDiagnostic[];
  memory?: MemoryUsage;
  hexFile?: string;
  elfFile?: string;
  exitCode: number;
}

export interface BuildEvent {
  type: 'stdout' | 'stderr' | 'info' | 'diagnostic' | 'memory' | 'complete';
  payload: string | BuildDiagnostic | MemoryUsage | BuildResult;
}
