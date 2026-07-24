import * as fs from 'fs';
import * as path from 'path';
import type { ProjectFile } from '@mikroduino/shared';

const MAIN_CPP_TEMPLATE = `/*
 * {{projectName}}
 * Target: {{mcu}} @ {{clockMhz}} MHz
 *
 * MikroDuino project
 */

#include <mikroduino/mikroduino.hpp>

using namespace MikroDuino;

int main() {
    // Configure PB5 as output (built-in LED on most boards)
    GPIO::output(PB5);

    // Enable global interrupts (if needed)
    // InterruptManager::enableGlobal();

    while (true) {
        GPIO::set(PB5);
        // TODO: add delay
        GPIO::clear(PB5);
        // TODO: add delay
    }

    return 0;
}
`;

export class ProjectScaffold {
  /**
   * Create a new project directory with standard structure and template files.
   * Returns the path to the created project.mdp file.
   */
  create(parentDir: string, project: ProjectFile): string {
    const projectDir = path.join(parentDir, project.projectName);

    if (fs.existsSync(projectDir)) {
      throw new Error(`Directory already exists: ${projectDir}`);
    }

    // Create directory structure
    for (const sub of ['src', 'include', 'lib', 'build', 'assets']) {
      fs.mkdirSync(path.join(projectDir, sub), { recursive: true });
    }

    // Write main.cpp
    const mainCpp = MAIN_CPP_TEMPLATE
      .replace(/\{\{projectName\}\}/g, project.projectName)
      .replace(/\{\{mcu\}\}/g,        project.target.mcu)
      .replace(/\{\{clockMhz\}\}/g,   String(project.target.clock / 1_000_000));

    fs.writeFileSync(path.join(projectDir, 'src', 'main.cpp'), mainCpp, 'utf8');

    // Write .gitignore
    fs.writeFileSync(
      path.join(projectDir, '.gitignore'),
      'build/\nMakefile\n*.hex\n*.elf\n*.map\n*.o\n',
      'utf8'
    );

    // Write project.mdp
    const mdpPath = path.join(projectDir, 'project.mdp');
    fs.writeFileSync(mdpPath, JSON.stringify(project, null, 2) + '\n', 'utf8');

    return mdpPath;
  }

  /** Add a new source file to an existing project. */
  addSourceFile(projectDir: string, filename: string, isHeader = false): string {
    const subDir = isHeader ? 'include' : 'src';
    const filePath = path.join(projectDir, subDir, filename);

    if (fs.existsSync(filePath)) {
      throw new Error(`File already exists: ${filePath}`);
    }

    const ext = path.extname(filename).toLowerCase();
    let content = '';

    if (ext === '.hpp' || ext === '.h') {
      content = `#pragma once\n\n// ${filename}\n`;
    } else if (ext === '.cpp') {
      const headerName = path.basename(filename, '.cpp');
      const headerPath = `../include/${headerName}.hpp`;
      content = `#include "${headerPath}"\n\n`;
    }

    fs.writeFileSync(filePath, content, 'utf8');
    return filePath;
  }
}
