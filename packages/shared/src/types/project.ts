import type { SupportedMCU } from './mcu.js';

export type ProgrammerType =
  | 'USBASP'
  | 'AVRISP'
  | 'STK500'
  | 'ARDUINO'
  | 'DRAGON'
  | 'JTAG';

export type OptimizationLevel = 'O0' | 'O1' | 'O2' | 'O3' | 'Os' | 'Og';

export interface ProjectTarget {
  mcu: SupportedMCU;
  clock: number;          // Hz
  bootloader?: string;
}

export interface ProjectBuild {
  optimization: OptimizationLevel;
  warnings: 'none' | 'default' | 'all' | 'extra';
  defines: string[];
  extraFlags: string[];
  cStandard?: 'c99' | 'c11' | 'c17';
  cppStandard?: 'c++11' | 'c++14' | 'c++17' | 'c++20';
}

export interface ProjectProgrammer {
  type: ProgrammerType;
  port: string;
  baudRate: number;
}

export interface ProjectSources {
  srcDir: string;
  includeDir: string;
  libDir: string;
}

export interface ProjectFile {
  projectName: string;
  version: string;
  target: ProjectTarget;
  build: ProjectBuild;
  programmer: ProjectProgrammer;
  libraries: string[];
  sources: ProjectSources;
}

export interface ProjectState {
  path: string;             // absolute path to project directory
  file: ProjectFile;
  isDirty: boolean;
  lastBuilt?: Date;
}

export const DEFAULT_PROJECT: ProjectFile = {
  projectName: 'NewProject',
  version: '1.0.0',
  target: {
    mcu: 'ATmega328P',
    clock: 16_000_000,
  },
  build: {
    optimization: 'O2',
    warnings: 'all',
    defines: ['F_CPU=16000000UL'],
    extraFlags: [],
    cppStandard: 'c++17',
    cStandard: 'c11',
  },
  programmer: {
    type: 'USBASP',
    port: '',
    baudRate: 115200,
  },
  libraries: [],
  sources: {
    srcDir: 'src',
    includeDir: 'include',
    libDir: 'lib',
  },
};
