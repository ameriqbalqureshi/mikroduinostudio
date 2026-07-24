import type { ProjectFile } from '@mikroduino/shared';

export interface ToolchainPaths {
  avrGcc: string;
  avrGpp: string;
  avrObjcopy: string;
  avrSize: string;
  avrdude: string;
  make: string;
}

export interface BuildContext {
  projectPath: string;
  project: ProjectFile;
  toolchain: ToolchainPaths;
  sdkCorePath: string;
  sdkCompatPath?: string;
}

export interface BuildOptions {
  clean?: boolean;
  verbose?: boolean;
  jobs?: number;
}

export type BuildEventCallback = (event: import('@mikroduino/shared').BuildEvent) => void;
