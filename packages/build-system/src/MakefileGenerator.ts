import * as path from 'path';
import type { BuildContext } from './types.js';
import { getMCUDefinition } from '@mikroduino/shared';

export class MakefileGenerator {
  generate(ctx: BuildContext): string {
    const { project, toolchain, sdkCorePath } = ctx;
    const mcuDef = getMCUDefinition(project.target.mcu);

    const srcDir     = project.sources.srcDir;
    const includeDir = project.sources.includeDir;
    const libDir     = project.sources.libDir;
    const buildDir   = 'build';

    const defines = [
      `F_CPU=${project.target.clock}UL`,
      ...project.build.defines,
    ].map(d => `-D${d}`).join(' ');

    const optimisation = `-${project.build.optimization}`;
    const warnings     = project.build.warnings === 'none' ? '' :
                         project.build.warnings === 'extra' ? '-Wall -Wextra' : '-Wall';

    const cStd   = project.build.cStandard   ? `-std=${project.build.cStandard}`   : '-std=c11';
    const cppStd = project.build.cppStandard ? `-std=${project.build.cppStandard}` : '-std=c++17';

    const extraFlags = project.build.extraFlags.join(' ');

    const sdkInclude   = path.join(sdkCorePath, 'include').replace(/\\/g, '/');
    const sdkSrcDir    = path.join(sdkCorePath, 'src').replace(/\\/g, '/');

    const lines: string[] = [
      `# MikroDuino Makefile — generated for ${project.projectName}`,
      `# MCU: ${project.target.mcu} @ ${project.target.clock / 1_000_000} MHz`,
      `# DO NOT EDIT — regenerated on each build`,
      ``,
      `MCU      := ${mcuDef.avrdudeId}`,
      `CC       := "${toolchain.avrGcc}"`,
      `CXX      := "${toolchain.avrGpp}"`,
      `OBJCOPY  := "${toolchain.avrObjcopy}"`,
      `SIZE     := "${toolchain.avrSize}"`,
      ``,
      `TARGET   := ${buildDir}/${project.projectName}`,
      ``,
      `CFLAGS   := -mmcu=${mcuDef.avrdudeId} ${optimisation} ${warnings} ${defines} ${cStd} ${extraFlags} \\`,
      `            -I${includeDir} -I${sdkInclude} -ffunction-sections -fdata-sections`,
      `CXXFLAGS := -mmcu=${mcuDef.avrdudeId} ${optimisation} ${warnings} ${defines} ${cppStd} ${extraFlags} \\`,
      `            -I${includeDir} -I${sdkInclude} -ffunction-sections -fdata-sections \\`,
      `            -fno-exceptions -fno-rtti`,
      `LDFLAGS  := -mmcu=${mcuDef.avrdudeId} -Wl,--gc-sections -Wl,-Map=$(TARGET).map`,
      ``,
      `# Collect source files`,
      `SRC_C    := $(wildcard ${srcDir}/*.c) $(wildcard ${sdkSrcDir}/*.c)`,
      `SRC_CPP  := $(wildcard ${srcDir}/*.cpp) $(wildcard ${sdkSrcDir}/*.cpp)`,
      ``,
      `# Library sources`,
      ...project.libraries.map(lib =>
        `SRC_CPP  += $(wildcard ${libDir}/${lib}/src/*.cpp) $(wildcard ${libDir}/${lib}/src/*.c)`
      ),
      ``,
      `OBJ_C    := $(patsubst %.c,   ${buildDir}/%.o, $(notdir $(SRC_C)))`,
      `OBJ_CPP  := $(patsubst %.cpp, ${buildDir}/%.o, $(notdir $(SRC_CPP)))`,
      `OBJS     := $(OBJ_C) $(OBJ_CPP)`,
      ``,
      `# VPATH for make to find source files`,
      `VPATH    := ${srcDir}:${sdkSrcDir}`,
      ...project.libraries.map(lib =>
        `VPATH    += :${libDir}/${lib}/src`
      ),
      ``,
      `.PHONY: all clean size`,
      ``,
      `all: $(TARGET).hex size`,
      ``,
      `$(TARGET).elf: $(OBJS) | ${buildDir}`,
      `\t$(CXX) $(LDFLAGS) $^ -o $@`,
      ``,
      `$(TARGET).hex: $(TARGET).elf`,
      `\t$(OBJCOPY) -O ihex -R .eeprom $< $@`,
      ``,
      `$(buildDir)/%.o: %.c | ${buildDir}`,
      `\t$(CC) $(CFLAGS) -c $< -o $@`,
      ``,
      `$(buildDir)/%.o: %.cpp | ${buildDir}`,
      `\t$(CXX) $(CXXFLAGS) -c $< -o $@`,
      ``,
      `${buildDir}:`,
      `\t@mkdir -p $@`,
      ``,
      `size: $(TARGET).elf`,
      `\t$(SIZE) --mcu=${mcuDef.avrdudeId} --format=avr $<`,
      ``,
      `clean:`,
      `\trm -rf ${buildDir}`,
    ];

    return lines.join('\n') + '\n';
  }
}
