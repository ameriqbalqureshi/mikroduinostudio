/*
 * BuildParser unit tests
 * Run with: npx ts-node --esm tests/build-system/BuildParser.test.ts
 */

import { BuildParser } from '../../packages/build-system/src/BuildParser.js';

function assert(condition: boolean, msg: string): void {
  if (!condition) throw new Error(`FAIL: ${msg}`);
  console.log(`  ✓ ${msg}`);
}

const parser = new BuildParser();

console.log('BuildParser — parseLine()');

// Error line
const errLine = 'src/main.cpp:42:10: error: expected \';\' before \'return\'';
const diag = parser.parseLine(errLine);
assert(diag !== null, 'parses error line');
assert(diag?.severity === 'error', 'severity is error');
assert(diag?.file === 'src/main.cpp', 'file is correct');
assert(diag?.line === 42, 'line is 42');
assert(diag?.column === 10, 'column is 10');

// Warning line
const warnLine = 'include/display.hpp:7:3: warning: unused variable \'x\'';
const wdiag = parser.parseLine(warnLine);
assert(wdiag !== null, 'parses warning line');
assert(wdiag?.severity === 'warning', 'severity is warning');

// Non-diagnostic line
const infoLine = 'avr-g++ -mmcu=atmega328p -O2 src/main.cpp -o build/main.o';
assert(parser.parseLine(infoLine) === null, 'non-diagnostic returns null');

// hasErrors
assert(parser.hasErrors([diag!]), 'detects errors');
assert(!parser.hasErrors([wdiag!]), 'no errors from warning-only');

// parseMemoryUsage
console.log('\nBuildParser — parseMemoryUsage()');
const sizeOutput = `
Device: atmega328p
Program:    4096 bytes (12.5% Full)
Data:        256 bytes (12.5% Full)
EEPROM:        0 bytes (0.0% Full)
`.trim();

const mem = parser.parseMemoryUsage(sizeOutput);
assert(mem !== null, 'parses avr-size output');
assert(mem?.flash.used === 4096, 'flash used = 4096');
assert(mem?.flash.total === 32768, 'flash total = 32768');
assert(mem?.ram.used === 256, 'ram used = 256');
assert(mem?.ram.total === 2048, 'ram total = 2048');

console.log('\nAll tests passed.');
