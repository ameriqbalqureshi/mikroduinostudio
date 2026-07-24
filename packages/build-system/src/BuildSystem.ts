import * as fs from 'fs';
import * as path from 'path';
import { spawn } from 'child_process';
import type { BuildContext, BuildOptions, BuildEventCallback } from './types.js';
import type { BuildResult, BuildDiagnostic, BuildEvent } from '@mikroduino/shared';
import { MakefileGenerator } from './MakefileGenerator.js';
import { ToolchainManager } from './ToolchainManager.js';
import { BuildParser } from './BuildParser.js';

export class BuildSystem {
  private readonly _makefileGen  = new MakefileGenerator();
  private readonly _toolchainMgr = new ToolchainManager();
  private readonly _parser       = new BuildParser();

  async build(
    ctx: BuildContext,
    options: BuildOptions = {},
    onEvent?: BuildEventCallback
  ): Promise<BuildResult> {
    const emit = (event: BuildEvent): void => { onEvent?.(event); };

    const startTime = new Date();
    const diagnostics: BuildDiagnostic[] = [];

    emit({ type: 'info', payload: `Building ${ctx.project.projectName}...` });

    // Validate toolchain
    const toolchainErrors = this._toolchainMgr.validate(ctx.toolchain);
    if (toolchainErrors.length > 0) {
      for (const e of toolchainErrors) emit({ type: 'stderr', payload: e });
      return this.failResult(startTime, diagnostics, -1);
    }

    // Generate Makefile
    const makefile = this._makefileGen.generate(ctx);
    const makefilePath = path.join(ctx.projectPath, 'Makefile');
    fs.writeFileSync(makefilePath, makefile, 'utf8');
    emit({ type: 'info', payload: 'Makefile generated.' });

    // Ensure build directory exists
    const buildDir = path.join(ctx.projectPath, 'build');
    if (!fs.existsSync(buildDir)) fs.mkdirSync(buildDir, { recursive: true });

    // Invoke make
    const makeArgs: string[] = [];
    if (options.clean) makeArgs.push('clean', 'all');
    else makeArgs.push('all');
    if (options.verbose) makeArgs.push('V=1');
    if (options.jobs)   makeArgs.push(`-j${options.jobs}`);

    let stdout = '';
    let stderr = '';
    let exitCode = 0;

    try {
      exitCode = await this.runProcess(
        ctx.toolchain.make,
        makeArgs,
        ctx.projectPath,
        (line, isErr) => {
          if (isErr) stderr += line + '\n';
          else       stdout += line + '\n';
          emit({ type: isErr ? 'stderr' : 'stdout', payload: line });

          const diag = this._parser.parseLine(line);
          if (diag) {
            diagnostics.push(diag);
            emit({ type: 'diagnostic', payload: diag });
          }
        }
      );
    } catch (err) {
      const message = err instanceof Error ? err.message : String(err);
      emit({ type: 'stderr', payload: `Build process error: ${message}` });
      return this.failResult(startTime, diagnostics, -1);
    }

    // Parse avr-size output from stdout
    let memory: import('@mikroduino/shared').MemoryUsage | undefined;
    const sizeOutput = stdout + stderr;
    const memUsage = this._parser.parseMemoryUsage(sizeOutput);
    if (memUsage) {
      memory = memUsage;
      emit({ type: 'memory', payload: memUsage });
    }

    const endTime = new Date();
    const status = exitCode === 0 && !this._parser.hasErrors(diagnostics) ? 'success' : 'failed';

    const result: BuildResult = {
      status,
      startTime,
      endTime,
      durationMs: endTime.getTime() - startTime.getTime(),
      diagnostics,
      exitCode,
      ...(memory !== undefined && { memory }),
      ...(exitCode === 0 && {
        hexFile: path.join(buildDir, `${ctx.project.projectName}.hex`),
        elfFile: path.join(buildDir, `${ctx.project.projectName}.elf`),
      }),
    };

    emit({ type: 'complete', payload: result });
    return result;
  }

  async clean(ctx: BuildContext): Promise<void> {
    const buildDir = path.join(ctx.projectPath, 'build');
    if (fs.existsSync(buildDir)) {
      fs.rmSync(buildDir, { recursive: true, force: true });
    }
  }

  private failResult(
    startTime: Date,
    diagnostics: BuildDiagnostic[],
    exitCode: number
  ): BuildResult {
    const endTime = new Date();
    return {
      status: 'failed',
      startTime,
      endTime,
      durationMs: endTime.getTime() - startTime.getTime(),
      diagnostics,
      exitCode,
    };
  }

  private runProcess(
    cmd: string,
    args: string[],
    cwd: string,
    onLine: (line: string, isStderr: boolean) => void
  ): Promise<number> {
    return new Promise((resolve, reject) => {
      const proc = spawn(cmd, args, { cwd, shell: false });

      const onData = (isErr: boolean) => (chunk: Buffer): void => {
        const text = chunk.toString('utf8');
        for (const line of text.split('\n')) {
          const trimmed = line.trimEnd();
          if (trimmed) onLine(trimmed, isErr);
        }
      };

      proc.stdout.on('data', onData(false));
      proc.stderr.on('data', onData(true));
      proc.on('error', reject);
      proc.on('close', resolve);
    });
  }
}
