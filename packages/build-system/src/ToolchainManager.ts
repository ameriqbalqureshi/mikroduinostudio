import * as fs from 'fs';
import * as path from 'path';
import { execSync } from 'child_process';
import type { ToolchainPaths } from './types.js';

const WIN_TOOLCHAIN_DIRS = [
  'C:\\WinAVR-20100110\\bin',
  'C:\\Program Files\\AVR Tools\\AVR Toolchain\\bin',
  'C:\\Program Files (x86)\\AVR Tools\\AVR Toolchain\\bin',
  'C:\\avr-gcc\\bin',
];

const LINUX_TOOLCHAIN_DIRS = [
  '/usr/bin',
  '/usr/local/bin',
  '/opt/avr-gcc/bin',
];

export class ToolchainManager {
  private _cached: ToolchainPaths | null = null;

  /** Detect toolchain from PATH and known install locations. */
  detect(): ToolchainPaths {
    if (this._cached) return this._cached;

    const isWin = process.platform === 'win32';
    const ext   = isWin ? '.exe' : '';
    const dirs  = isWin ? WIN_TOOLCHAIN_DIRS : LINUX_TOOLCHAIN_DIRS;

    const resolve = (name: string): string => {
      // Try PATH first
      try {
        const which = isWin ? 'where' : 'which';
        const result = execSync(`${which} ${name}${ext}`, { encoding: 'utf8' }).trim();
        if (result) return result.split('\n')[0].trim();
      } catch { /* not on PATH */ }

      // Try known dirs
      for (const dir of dirs) {
        const candidate = path.join(dir, name + ext);
        if (fs.existsSync(candidate)) return candidate;
      }

      return name + ext; // fallback — will fail at build time with clear error
    };

    const make = isWin ? this.detectMake() : 'make';

    this._cached = {
      avrGcc:    resolve('avr-gcc'),
      avrGpp:    resolve('avr-g++'),
      avrObjcopy: resolve('avr-objcopy'),
      avrSize:   resolve('avr-size'),
      avrdude:   resolve('avrdude'),
      make,
    };

    return this._cached;
  }

  /** Check that all required tools are accessible. */
  validate(paths: ToolchainPaths): string[] {
    const errors: string[] = [];
    const check = (label: string, p: string): void => {
      if (!fs.existsSync(p)) {
        errors.push(`${label} not found: ${p}`);
      }
    };

    check('avr-gcc',    paths.avrGcc);
    check('avr-g++',    paths.avrGpp);
    check('avr-objcopy', paths.avrObjcopy);
    check('avr-size',   paths.avrSize);
    check('make',       paths.make);
    // avrdude is optional until upload
    return errors;
  }

  version(avrGcc: string): string {
    try {
      return execSync(`"${avrGcc}" --version`, { encoding: 'utf8' }).split('\n')[0];
    } catch {
      return 'unknown';
    }
  }

  private detectMake(): string {
    const candidates = [
      'C:\\WinAVR-20100110\\utils\\bin\\make.exe',
      'C:\\Program Files\\Git\\usr\\bin\\make.exe',
      'C:\\msys64\\usr\\bin\\make.exe',
      'C:\\MinGW\\msys\\1.0\\bin\\make.exe',
    ];
    try {
      const r = execSync('where make.exe', { encoding: 'utf8' }).trim();
      if (r) return r.split('\n')[0].trim();
    } catch { /* not on PATH */ }

    for (const c of candidates) {
      if (fs.existsSync(c)) return c;
    }
    return 'make.exe';
  }
}
