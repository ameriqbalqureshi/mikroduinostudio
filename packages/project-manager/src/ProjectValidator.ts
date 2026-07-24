import { isSupportedMCU, type ProjectFile } from '@mikroduino/shared';

export interface ValidationResult {
  valid: boolean;
  errors: string[];
  warnings: string[];
}

export class ProjectValidator {
  validate(project: ProjectFile): ValidationResult {
    const errors:   string[] = [];
    const warnings: string[] = [];

    if (!project.projectName?.trim()) {
      errors.push('projectName is required.');
    } else if (!/^[\w\-. ]+$/.test(project.projectName)) {
      errors.push(`projectName "${project.projectName}" contains invalid characters.`);
    }

    if (!project.target?.mcu) {
      errors.push('target.mcu is required.');
    } else if (!isSupportedMCU(project.target.mcu)) {
      errors.push(`Unsupported MCU: ${project.target.mcu}.`);
    }

    if (!project.target?.clock || project.target.clock <= 0) {
      errors.push('target.clock must be a positive number (Hz).');
    } else if (project.target.clock > 20_000_000) {
      warnings.push(`target.clock ${project.target.clock} Hz exceeds typical AVR max (20 MHz).`);
    }

    const validOpts = ['O0','O1','O2','O3','Os','Og'];
    if (project.build?.optimization && !validOpts.includes(project.build.optimization)) {
      errors.push(`build.optimization must be one of: ${validOpts.join(', ')}.`);
    }

    const validProgrammers = ['USBASP','AVRISP','STK500','ARDUINO','DRAGON','JTAG'];
    if (project.programmer?.type && !validProgrammers.includes(project.programmer.type)) {
      warnings.push(`Unknown programmer type: ${project.programmer.type}.`);
    }

    return { valid: errors.length === 0, errors, warnings };
  }
}
