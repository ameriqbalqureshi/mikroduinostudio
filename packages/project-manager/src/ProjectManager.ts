import * as fs from 'fs';
import * as path from 'path';
import type { ProjectFile, ProjectState } from '@mikroduino/shared';
import { DEFAULT_PROJECT } from '@mikroduino/shared';
import { ProjectValidator } from './ProjectValidator.js';
import { ProjectScaffold } from './ProjectScaffold.js';

export class ProjectManager {
  private readonly _validator = new ProjectValidator();
  private readonly _scaffold  = new ProjectScaffold();

  // ── Open ──────────────────────────────────────────────────────────────────

  /** Open an existing project.mdp file. */
  open(mdpPath: string): ProjectState {
    if (!fs.existsSync(mdpPath)) {
      throw new Error(`Project file not found: ${mdpPath}`);
    }

    const raw  = fs.readFileSync(mdpPath, 'utf8');
    const file = this.parse(raw);
    const result = this._validator.validate(file);

    if (!result.valid) {
      throw new Error(`Invalid project file:\n${result.errors.join('\n')}`);
    }

    return {
      path:    path.dirname(mdpPath),
      file,
      isDirty: false,
    };
  }

  // ── Create ─────────────────────────────────────────────────────────────────

  /** Create a new project in parentDir. Returns the new ProjectState. */
  create(parentDir: string, overrides: Partial<ProjectFile> = {}): ProjectState {
    const project: ProjectFile = {
      ...DEFAULT_PROJECT,
      ...overrides,
      target: { ...DEFAULT_PROJECT.target, ...overrides.target },
      build:  { ...DEFAULT_PROJECT.build,  ...overrides.build  },
      programmer: { ...DEFAULT_PROJECT.programmer, ...overrides.programmer },
      sources:    { ...DEFAULT_PROJECT.sources,    ...overrides.sources    },
    };

    const result = this._validator.validate(project);
    if (!result.valid) {
      throw new Error(`Invalid project config:\n${result.errors.join('\n')}`);
    }

    const mdpPath = this._scaffold.create(parentDir, project);

    return {
      path:    path.dirname(mdpPath),
      file:    project,
      isDirty: false,
    };
  }

  // ── Save ──────────────────────────────────────────────────────────────────

  save(state: ProjectState): void {
    const mdpPath = path.join(state.path, 'project.mdp');
    fs.writeFileSync(mdpPath, JSON.stringify(state.file, null, 2) + '\n', 'utf8');
  }

  // ── File tree ──────────────────────────────────────────────────────────────

  /** Return a recursive listing of project source files. */
  listFiles(projectPath: string): FileNode[] {
    return this.walkDir(projectPath, projectPath, [
      'node_modules', 'build', '.git',
    ]);
  }

  /** Read a file from within the project. */
  readFile(filePath: string): string {
    return fs.readFileSync(filePath, 'utf8');
  }

  /** Write a file within the project. */
  writeFile(filePath: string, content: string): void {
    fs.writeFileSync(filePath, content, 'utf8');
  }

  /** Delete a file from the project. */
  deleteFile(filePath: string): void {
    fs.rmSync(filePath);
  }

  /** Rename / move a file. */
  renameFile(from: string, to: string): void {
    fs.renameSync(from, to);
  }

  addSourceFile(state: ProjectState, filename: string, isHeader = false): string {
    return this._scaffold.addSourceFile(state.path, filename, isHeader);
  }

  // ── Private ────────────────────────────────────────────────────────────────

  private parse(raw: string): ProjectFile {
    try {
      return JSON.parse(raw) as ProjectFile;
    } catch (e) {
      throw new Error(`Failed to parse project.mdp: ${String(e)}`);
    }
  }

  private walkDir(base: string, current: string, excludes: string[]): FileNode[] {
    const nodes: FileNode[] = [];
    let entries: fs.Dirent[];

    try {
      entries = fs.readdirSync(current, { withFileTypes: true });
    } catch {
      return nodes;
    }

    for (const entry of entries) {
      if (excludes.includes(entry.name)) continue;

      const fullPath = path.join(current, entry.name);

      if (entry.isDirectory()) {
        nodes.push({
          name:     entry.name,
          path:     fullPath,
          type:     'directory',
          children: this.walkDir(base, fullPath, excludes),
        });
      } else {
        nodes.push({
          name: entry.name,
          path: fullPath,
          type: 'file',
        });
      }
    }

    return nodes.sort((a, b) => {
      if (a.type !== b.type) return a.type === 'directory' ? -1 : 1;
      return a.name.localeCompare(b.name);
    });
  }
}

export interface FileNode {
  name: string;
  path: string;
  type: 'file' | 'directory';
  children?: FileNode[];
}
