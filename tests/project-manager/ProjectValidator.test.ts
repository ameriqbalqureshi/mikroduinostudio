/*
 * ProjectValidator unit tests
 */

import { ProjectValidator } from '../../packages/project-manager/src/ProjectValidator.js';
import type { ProjectFile } from '../../packages/shared/src/index.js';

function assert(condition: boolean, msg: string): void {
  if (!condition) throw new Error(`FAIL: ${msg}`);
  console.log(`  ✓ ${msg}`);
}

const validator = new ProjectValidator();

const validProject: ProjectFile = {
  projectName: 'TestProject',
  version: '1.0.0',
  target:  { mcu: 'ATmega328P', clock: 16_000_000 },
  build:   { optimization: 'O2', warnings: 'all', defines: ['F_CPU=16000000UL'], extraFlags: [] },
  programmer: { type: 'USBASP', port: '', baudRate: 115200 },
  libraries: [],
  sources: { srcDir: 'src', includeDir: 'include', libDir: 'lib' },
};

console.log('ProjectValidator');

const r1 = validator.validate(validProject);
assert(r1.valid, 'valid project passes');
assert(r1.errors.length === 0, 'no errors for valid project');

const r2 = validator.validate({ ...validProject, projectName: '' });
assert(!r2.valid, 'empty name fails');

const r3 = validator.validate({ ...validProject, target: { ...validProject.target, mcu: 'ATmega999' as never } });
assert(!r3.valid, 'unsupported MCU fails');

const r4 = validator.validate({ ...validProject, target: { ...validProject.target, clock: 0 } });
assert(!r4.valid, 'zero clock fails');

const r5 = validator.validate({ ...validProject, target: { ...validProject.target, clock: 25_000_000 } });
assert(r5.valid, 'high clock valid (warning only)');
assert(r5.warnings.length > 0, 'generates warning for high clock');

console.log('\nAll tests passed.');
